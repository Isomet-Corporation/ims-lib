// Request/response matching before, during and after communication errors (protocol v1).
//
// Every test checks the same two things:
//	1. No message ever completes with another message's reply.  Each read goes to its own address
//	   and the simulated device answers with a value derived from that address, so a reply given
//	   to the wrong message shows up as a wrong value.
//	2. Communication recovers.  Requests made after the error get their own replies.
//
// Protocol v1 replies carry no sequence number.  The host matches each reply to the oldest
// outstanding request with the same report type, action and (where echoed) address.  When two
// outstanding requests are identical, only their order can tell their replies apart; the
// IdenticalRequests tests show what that means after an error.
#include "TestHelpers.h"

#include <deque>
#include <mutex>
#include <ostream>
#include <random>
#include <set>
#include <thread>
#include <vector>

using namespace iMS;
using namespace iMS::test;
using namespace std::chrono_literals;

namespace {

	// The faults the simulated device (or link) can apply to one request
	enum class Fault {
		None,            // normal reply
		NoReply,         // reply lost
		LateReply,       // reply held back and delivered just before the next request's reply
		BadCrc,          // reply corrupted in transit
		DeviceError,     // device reports an error (DATA_OK clear, address not echoed)
		NoiseBefore,     // line noise in front of the reply
		SplitReply,      // reply arrives in two pieces with a gap between them
		Truncated,       // a frame header that never completes, then nothing
		InterruptBefore, // an unsolicited interrupt in front of the reply
		StrayBefore,     // a reply to a request nobody made, in front of the real reply
		DuplicateReply,  // reply sent twice
		SendFailure,     // request never leaves the host
	};

	const char* Name(Fault f)
	{
		switch (f) {
		case Fault::None: return "None";
		case Fault::NoReply: return "NoReply";
		case Fault::LateReply: return "LateReply";
		case Fault::BadCrc: return "BadCrc";
		case Fault::DeviceError: return "DeviceError";
		case Fault::NoiseBefore: return "NoiseBefore";
		case Fault::SplitReply: return "SplitReply";
		case Fault::Truncated: return "Truncated";
		case Fault::InterruptBefore: return "InterruptBefore";
		case Fault::StrayBefore: return "StrayBefore";
		case Fault::DuplicateReply: return "DuplicateReply";
		case Fault::SendFailure: return "SendFailure";
		}
		return "?";
	}

	// gtest prints parameters with this
	[[maybe_unused]] std::ostream& operator<<(std::ostream& os, Fault f) { return os << Name(f); }

	// The status the faulted message itself should end with when requests are sent one at a time
	Message::Status ExpectedStatus(Fault f)
	{
		switch (f) {
		case Fault::NoReply:
		case Fault::LateReply:
		case Fault::Truncated:   return Message::Status::TIMEOUT_ON_RXCV;
		case Fault::BadCrc:      return Message::Status::RX_ERROR_INVALID;
		case Fault::DeviceError: return Message::Status::RX_ERROR_VALID;
		case Fault::SendFailure: return Message::Status::SEND_ERROR;
		default:                 return Message::Status::RX_OK;
		}
	}

	// The value the device returns for a read of this address
	std::uint16_t ValueFor(std::uint16_t addr)
	{
		return static_cast<std::uint16_t>(addr ^ 0xA5A5);
	}

	// Never used by any request, so a reply to it matches nothing
	constexpr std::uint16_t UnusedAddr = 0xFFF0;

	// A device that answers synth register reads, applying a planned fault to each request in turn
	class ScriptedDevice
	{
	public:
		explicit ScriptedDevice(CM_TEST& conn) : m_conn(conn) {}

		// Plan the fault for the next request the device receives (default: none)
		void Plan(Fault f)
		{
			std::scoped_lock l(m_mtx);
			m_plan.push_back(f);
		}

		Bytes operator()(const Bytes& req)
		{
			Fault f = Fault::None;
			Bytes held;
			{
				std::scoped_lock l(m_mtx);
				if (!m_plan.empty()) { f = m_plan.front(); m_plan.pop_front(); }
				std::swap(held, m_held);
			}

			const std::uint16_t addr = static_cast<std::uint16_t>(req[5] | (req[6] << 8));
			const bool read = (req[1] & 0x80) != 0;
			const Bytes reply = read ? CM_TEST::OkResponse(req, Value16(ValueFor(addr))) : CM_TEST::OkResponse(req);

			switch (f) {
			case Fault::NoReply:
				return held;
			case Fault::LateReply: {
				std::scoped_lock l(m_mtx);
				m_held = reply;
				return held;
			}
			case Fault::BadCrc: {
				Bytes bad = reply;
				bad.back() ^= 0xFF;
				return held + bad;
			}
			case Fault::DeviceError:
				return held + CM_TEST::ErrorResponse(req);
			case Fault::NoiseBefore:
				return held + Bytes{ 0xAA, 0x55, 0x00, 0xFF, 0x7E } + reply;
			case Fault::SplitReply:
				m_conn.InjectRx(held + Bytes(reply.begin(), reply.begin() + 5));
				std::this_thread::sleep_for(30ms);
				return Bytes(reply.begin() + 5, reply.end());
			case Fault::Truncated:
				// A synth reply header announcing 255 data bytes that never arrive
				return held + Bytes{ 0x02, 0x47, 0x00, 0xFF, 0x00 };
			case Fault::InterruptBefore:
				return held + CM_TEST::InterruptFrame(0x0005, { 0x01, 0x00 }) + reply;
			case Fault::StrayBefore: {
				Bytes stray = req;
				stray[5] = UnusedAddr & 0xFF;
				stray[6] = UnusedAddr >> 8;
				return held + CM_TEST::OkResponse(stray, Value16(0xDEAD)) + reply;
			}
			case Fault::DuplicateReply:
				return held + reply + reply;
			default:
				return held + reply;
			}
		}

	private:
		CM_TEST& m_conn;
		std::mutex m_mtx;
		std::deque<Fault> m_plan;
		Bytes m_held;
	};

	class RxMatching : public CmTest
	{
	protected:
		ScriptedDevice device{ conn };
		std::uint16_t nextAddr = 0x0100;

		std::mutex failMutex;
		std::set<std::uint16_t> failAddrs;

		void SetUp() override
		{
			CmTest::SetUp();
			conn.SetResponder([this](const Bytes& req) { return device(req); });
			conn.FailSendsWhere([this](const Bytes& req) {
				std::scoped_lock l(failMutex);
				return failAddrs.count(static_cast<std::uint16_t>(req[5] | (req[6] << 8))) > 0;
			});
		}

		// For requests sent one at a time: the status the next request should end with, given
		//	its fault and what came before.
		//
		//	A request that timed out with no reply stays first in line for a late reply (up to 5 s).
		//	A reply with a bad CRC goes to the oldest outstanding request, and an error reply
		//	doesn't echo the address.  So if one of these follows a request still waiting for its
		//	reply, the earlier request takes it as its late reply, and the new request times out
		//	(and is then the one waiting).  No data is misattributed either way.
		Message::Status ExpectSequential(Fault f)
		{
			Message::Status s = ExpectedStatus(f);
			switch (f) {
			case Fault::NoReply:
			case Fault::Truncated:
				waitingForLateReply = true;
				break;
			case Fault::BadCrc:
			case Fault::DeviceError:
				if (waitingForLateReply) s = Message::Status::TIMEOUT_ON_RXCV;
				break;
			case Fault::SendFailure:
				break;
			default:
				// A reply that matches by address shows the earlier reply was lost; a late reply
				//	(LateReply) is delivered just before the next request's reply and absorbed
				waitingForLateReply = false;
				break;
			}
			return s;
		}
		bool waitingForLateReply = false;

		struct Sent {
			MessageHandle handle;
			std::uint16_t addr;
			Fault fault;
		};

		// Send a read of a new address with the given fault applied
		Sent Read(Fault f = Fault::None)
		{
			const std::uint16_t addr = nextAddr++;
			if (f == Fault::SendFailure) {
				std::scoped_lock l(failMutex);
				failAddrs.insert(addr);
			}
			else device.Plan(f);
			return { conn.SendMsg(SynthRegRead(addr)), addr, f };
		}

		// Send a read and wait for it to complete
		Sent ReadAndWait(Fault f = Fault::None)
		{
			Sent s = Read(f);
			conn.WaitForStatus(s.handle, 2s);
			return s;
		}

		// Never: a message completing with a reply meant for another
		void ExpectNotMisattributed(const Sent& s)
		{
			auto m = conn.FindMessage(s.handle);
			ASSERT_NE(m, nullptr);
			if (m->getStatus() == Message::Status::RX_OK) {
				EXPECT_EQ(m->Response().Fields().addr, s.addr) << "message " << s.handle << " got another request's reply";
				EXPECT_EQ(ValueOf(s.handle), ValueFor(s.addr)) << "message " << s.handle << " got another request's data";
			}
		}

		// The read completed normally, with its own data
		void ExpectGood(const Sent& s)
		{
			EXPECT_EQ(conn.WaitForStatus(s.handle, 2s), Message::Status::RX_OK) << "read of 0x" << std::hex << s.addr;
			ExpectNotMisattributed(s);
		}
	};

}

// ---------------------------------------------------------------------------------------------
// One fault in a sequence of requests sent one at a time: good before, the expected outcome for
// the faulted request, good after.
// ---------------------------------------------------------------------------------------------

class RxMatchingSequential : public RxMatching, public ::testing::WithParamInterface<Fault> {};

TEST_P(RxMatchingSequential, RecoversAfterFault)
{
	const Fault fault = GetParam();

	for (int i = 0; i < 3; i++) ExpectGood(ReadAndWait());

	Sent faulted = ReadAndWait(fault);
	EXPECT_EQ(conn.FindMessage(faulted.handle)->getStatus(), ExpectedStatus(fault));
	ExpectNotMisattributed(faulted);

	// The next request is the one most at risk: a late or stray reply could be taken for its own
	for (int i = 0; i < 3; i++) ExpectGood(ReadAndWait());

	if (fault == Fault::InterruptBefore) EXPECT_TRUE(events.WaitFor(MessageEvents::INTERRUPT_RECEIVED, 1));
}

INSTANTIATE_TEST_SUITE_P(AllFaults, RxMatchingSequential,
	::testing::Values(Fault::None, Fault::NoReply, Fault::LateReply, Fault::BadCrc, Fault::DeviceError,
		Fault::NoiseBefore, Fault::SplitReply, Fault::Truncated, Fault::InterruptBefore,
		Fault::StrayBefore, Fault::DuplicateReply, Fault::SendFailure),
	[](const ::testing::TestParamInfo<Fault>& info) { return std::string(Name(info.param)); });

// ---------------------------------------------------------------------------------------------
// One fault in the middle of a burst of requests sent without waiting (several outstanding at
// once).  Requests before the fault complete normally, nothing is misattributed, and a second
// burst afterwards is entirely good.
// ---------------------------------------------------------------------------------------------

class RxMatchingPipelined : public RxMatching, public ::testing::WithParamInterface<Fault> {};

TEST_P(RxMatchingPipelined, RecoversAfterFault)
{
	const Fault fault = GetParam();
	constexpr int BurstSize = 16;
	constexpr int FaultAt = 8;

	std::vector<Sent> burst1;
	for (int i = 0; i < BurstSize; i++) burst1.push_back(Read(i == FaultAt ? fault : Fault::None));
	for (auto& s : burst1) conn.WaitForStatus(s.handle, 2s);

	for (int i = 0; i < FaultAt; i++) ExpectGood(burst1[i]);
	for (auto& s : burst1) ExpectNotMisattributed(s);

	// Let any stalled partial frame be cleared before the next burst
	std::this_thread::sleep_for(std::chrono::milliseconds(2 * RxTimeoutMs));

	std::vector<Sent> burst2;
	for (int i = 0; i < BurstSize; i++) burst2.push_back(Read());
	for (auto& s : burst2) ExpectGood(s);
}

INSTANTIATE_TEST_SUITE_P(AllFaults, RxMatchingPipelined,
	::testing::Values(Fault::None, Fault::NoReply, Fault::LateReply, Fault::BadCrc, Fault::DeviceError,
		Fault::NoiseBefore, Fault::SplitReply, Fault::Truncated, Fault::InterruptBefore,
		Fault::StrayBefore, Fault::DuplicateReply, Fault::SendFailure),
	[](const ::testing::TestParamInfo<Fault>& info) { return std::string(Name(info.param)); });

// ---------------------------------------------------------------------------------------------
// Specific situations
// ---------------------------------------------------------------------------------------------

// A lost reply is detected as soon as the next request's reply arrives, without waiting for the
// timeout, and the following requests are unaffected
TEST_F(RxMatching, LostReplyDetectedByNextReply)
{
	ExpectGood(ReadAndWait());

	Sent lost = Read(Fault::NoReply);
	Sent next = Read();
	ExpectGood(next);
	EXPECT_EQ(conn.WaitForStatus(lost.handle, 50ms), Message::Status::TIMEOUT_ON_RXCV);

	ExpectGood(ReadAndWait());
}

// A bad-CRC or device-error reply that follows a timed-out request (still waiting for its late
// reply) is taken by that earlier request, so the new request times out instead of reporting
// the error.  Neither request gets wrong data, and the next request is good.
TEST_F(RxMatching, ErrorReplyAfterTimeoutGoesToTimedOutRequest)
{
	for (Fault f : { Fault::BadCrc, Fault::DeviceError })
	{
		ExpectGood(ReadAndWait());

		Sent lost = ReadAndWait(Fault::NoReply);
		EXPECT_EQ(conn.FindMessage(lost.handle)->getStatus(), Message::Status::TIMEOUT_ON_RXCV);

		Sent err = ReadAndWait(f);
		EXPECT_EQ(conn.FindMessage(err.handle)->getStatus(), Message::Status::TIMEOUT_ON_RXCV) << Name(f);
		ExpectNotMisattributed(err);

		ExpectGood(ReadAndWait());
		ExpectGood(ReadAndWait());
	}
}

// Several errors in a row, then normal traffic
TEST_F(RxMatching, RecoversAfterConsecutiveFaults)
{
	ExpectGood(ReadAndWait());

	for (Fault f : { Fault::NoReply, Fault::BadCrc, Fault::LateReply, Fault::Truncated, Fault::DeviceError, Fault::LateReply })
	{
		const auto expected = ExpectSequential(f);
		Sent s = ReadAndWait(f);
		EXPECT_EQ(conn.FindMessage(s.handle)->getStatus(), expected) << Name(f);
		ExpectNotMisattributed(s);
	}

	for (int i = 0; i < 5; i++) ExpectGood(ReadAndWait());
}

// The pattern seen in Ethernet captures: a burst of writes whose last replies stall past the
// timeout, then arrive together with the replies to the next burst.  The second burst must be
// matched to its own replies.
TEST_F(RxMatching, StalledBurstTailThenNextBurst)
{
	conn.SetResponder([](const Bytes&) { return Bytes{}; });

	std::vector<MessageHandle> burst1, burst2;
	std::vector<Bytes> sent;
	for (int i = 0; i < 64; i++) burst1.push_back(conn.SendMsg(LutWrite(static_cast<std::uint16_t>(8 * i))));
	ASSERT_TRUE(conn.WaitForSent(64, 1s));
	sent = conn.SentFrames();

	// The first 36 replies arrive on time; the rest stall
	Bytes first;
	for (int i = 0; i < 36; i++) first = first + CM_TEST::OkResponse(sent[i]);
	conn.InjectRx(first);
	std::this_thread::sleep_for(std::chrono::milliseconds(RxTimeoutMs + 150));

	for (int i = 0; i < 64; i++) burst2.push_back(conn.SendMsg(LutWrite(static_cast<std::uint16_t>(8 * (64 + i)))));
	ASSERT_TRUE(conn.WaitForSent(128, 1s));
	sent = conn.SentFrames();

	// The stalled replies, then the second burst's replies, in one block
	Bytes rest;
	for (int i = 36; i < 128; i++) rest = rest + CM_TEST::OkResponse(sent[i]);
	conn.InjectRx(rest);

	for (int i = 0; i < 36; i++) EXPECT_EQ(conn.WaitForStatus(burst1[i], 1s), Message::Status::RX_OK) << i;
	for (int i = 36; i < 64; i++) EXPECT_EQ(conn.WaitForStatus(burst1[i], 1s), Message::Status::TIMEOUT_ON_RXCV) << i;
	for (int i = 0; i < 64; i++) {
		ASSERT_EQ(conn.WaitForStatus(burst2[i], 1s), Message::Status::RX_OK) << i;
		EXPECT_EQ(conn.Response(burst2[i]).Fields().addr, 8 * (64 + i)) << i;
	}
}

// With message serialisation on (one request in flight, as used for older iMSP firmware), a
// timeout in the middle doesn't hold up or confuse the requests after it
TEST_F(RxMatching, SerialisedRecoversAfterFaults)
{
	conn.SetMessageSerialisation(true);

	ExpectGood(ReadAndWait());
	for (Fault f : { Fault::NoReply, Fault::LateReply, Fault::BadCrc, Fault::Truncated })
	{
		Sent s = Read(f);
		Sent next = Read();
		EXPECT_EQ(conn.WaitForStatus(s.handle, 2s), ExpectedStatus(f)) << Name(f);
		ExpectNotMisattributed(s);
		ExpectGood(next);
	}
}

// Controller register replies echo the address and are checked.  Other controller replies
// don't, so only their action is checked.
TEST_F(RxMatching, ControllerRepliesAfterWrongAddress)
{
	conn.SetResponder([](const Bytes&) { return Bytes{}; });

	auto reg = conn.SendMsg(CtrlrWrite(HostReport::Actions::CTRLR_REG, 0x10));
	ASSERT_TRUE(conn.WaitForSent(1, 1s));
	const Bytes req = conn.SentFrames()[0];

	Bytes wrongAddr = req;
	wrongAddr[5] = 0x11;
	conn.InjectRx(CM_TEST::OkResponse(wrongAddr));
	std::this_thread::sleep_for(50ms);
	EXPECT_EQ(conn.FindMessage(reg)->getStatus(), Message::Status::SENT) << "reply for another register accepted";

	conn.InjectRx(CM_TEST::OkResponse(req));
	EXPECT_EQ(conn.WaitForStatus(reg, 1s), Message::Status::RX_OK);
}

// Randomised faults over a long sequence.  Fixed seed, so any failure is reproducible.
TEST_F(RxMatching, RandomFaultsSequential)
{
	std::mt19937 rng(20261120);
	const Fault faults[] = { Fault::NoReply, Fault::LateReply, Fault::BadCrc, Fault::DeviceError,
		Fault::NoiseBefore, Fault::SplitReply, Fault::Truncated, Fault::InterruptBefore,
		Fault::StrayBefore, Fault::DuplicateReply, Fault::SendFailure };
	std::uniform_int_distribution<int> pick(0, 3 * static_cast<int>(std::size(faults)) - 1);

	int faulted = 0;
	for (int i = 0; i < 150; i++) {
		const int p = pick(rng);
		const Fault f = (p < static_cast<int>(std::size(faults))) ? faults[p] : Fault::None;   // about 1 in 3 faulted
		if (f != Fault::None) faulted++;

		const auto expected = ExpectSequential(f);
		Sent s = ReadAndWait(f);
		EXPECT_EQ(conn.FindMessage(s.handle)->getStatus(), expected) << "request " << i << " (" << Name(f) << ")";
		ExpectNotMisattributed(s);
	}
	EXPECT_GT(faulted, 30);

	for (int i = 0; i < 5; i++) ExpectGood(ReadAndWait());
}

// Randomised faults in bursts of outstanding requests.  Outcomes of individual faulted requests
// depend on what else is in flight, so only the two invariants are checked: nothing is
// misattributed, and a clean burst afterwards is entirely good.
TEST_F(RxMatching, RandomFaultsPipelined)
{
	std::mt19937 rng(1120);
	const Fault faults[] = { Fault::NoReply, Fault::LateReply, Fault::BadCrc, Fault::DeviceError,
		Fault::NoiseBefore, Fault::SplitReply, Fault::Truncated, Fault::InterruptBefore,
		Fault::StrayBefore, Fault::DuplicateReply, Fault::SendFailure };
	std::uniform_int_distribution<int> pick(0, 4 * static_cast<int>(std::size(faults)) - 1);

	for (int b = 0; b < 6; b++) {
		std::vector<Sent> burst;
		for (int i = 0; i < 24; i++) {
			const int p = pick(rng);
			burst.push_back(Read((p < static_cast<int>(std::size(faults))) ? faults[p] : Fault::None));
		}
		for (auto& s : burst) {
			conn.WaitForStatus(s.handle, 2s);
			ExpectNotMisattributed(s);
		}
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(2 * RxTimeoutMs));
	std::vector<Sent> clean;
	for (int i = 0; i < 24; i++) clean.push_back(Read());
	for (auto& s : clean) ExpectGood(s);
}

// ---------------------------------------------------------------------------------------------
// Identical requests: the same read sent twice can only be told apart by order
// ---------------------------------------------------------------------------------------------

// A late reply to the first request arrives just before the reply to the second.  The second
// request gets its own data, not the late reply (the misalignment fixed in 2.0.10).
TEST_F(RxMatching, IdenticalRequestsLateReplyGoesToItsOwnRequest)
{
	conn.SetResponder([](const Bytes&) { return Bytes{}; });

	auto first = conn.SendMsg(SynthRegRead(0x40));
	ASSERT_TRUE(conn.WaitForSent(1, 1s));
	const Bytes req = conn.SentFrames()[0];
	EXPECT_EQ(conn.WaitForStatus(first, 2s), Message::Status::TIMEOUT_ON_RXCV);

	auto second = conn.SendMsg(SynthRegRead(0x40));
	ASSERT_TRUE(conn.WaitForSent(2, 1s));
	conn.InjectRx(CM_TEST::OkResponse(req, Value16(0xAAAA)) + CM_TEST::OkResponse(req, Value16(0xBBBB)));

	ASSERT_EQ(conn.WaitForStatus(second, 1s), Message::Status::RX_OK);
	EXPECT_EQ(ValueOf(second), 0xBBBB);
	EXPECT_TRUE(events.WaitFor(MessageEvents::RESPONSE_TIMED_OUT, 1));
	EXPECT_EQ(events.Count(MessageEvents::RESPONSE_TIMED_OUT), 1);
}

namespace {

	// A device answering reads of one register: request n (counting from 0) gets the value
	//	0x1000 + n, so each reply identifies the request it was sent for.  The first request's
	//	reply is lost.
	struct PollingDevice {
		int n = 0;
		Bytes operator()(const Bytes& req)
		{
			const int k = n++;
			if (k == 0) return {};
			return CM_TEST::OkResponse(req, Value16(static_cast<std::uint16_t>(0x1000 + k)));
		}
	};

}

// CURRENT BEHAVIOUR (protocol v1 limitation).  A polling loop reads the same register, one
// request at a time, and one reply is lost.  Each later reply can't be told apart from a late
// reply to the request before it, so it is taken as that late reply, and every request in the
// loop times out.  No data is misattributed, but the loop doesn't recover until a different
// request is made, or polling pauses for longer than the late-reply window (about 5 s).
// Protocol v2 sequence numbers remove the ambiguity.  See also the disabled test below.
TEST_F(RxMatching, IdenticalRequestsAfterLostReplyAllTimeOut)
{
	conn.SetResponder(PollingDevice{});

	for (int k = 0; k < 6; k++) {
		auto h = conn.SendMsg(SynthRegRead(0x40));
		const auto status = conn.WaitForStatus(h, 2s);
		EXPECT_EQ(status, Message::Status::TIMEOUT_ON_RXCV) << "request " << k << ": v1 behaviour changed, review this test";
		if (status == Message::Status::RX_OK) EXPECT_EQ(ValueOf(h), 0x1000 + k) << "request " << k << " got another request's data";
	}

	// A different request breaks the chain: its reply shows the earlier one was lost
	conn.SetResponder([](const Bytes& req) { return CM_TEST::OkResponse(req, Value16(0x2222)); });
	auto other = conn.SendMsg(SynthRegRead(0x41));
	ASSERT_EQ(conn.WaitForStatus(other, 2s), Message::Status::RX_OK);
	EXPECT_EQ(ValueOf(other), 0x2222);

	auto again = conn.SendMsg(SynthRegRead(0x40));
	ASSERT_EQ(conn.WaitForStatus(again, 2s), Message::Status::RX_OK);
	EXPECT_EQ(ValueOf(again), 0x2222);
}

// DESIRED BEHAVIOUR, not met by protocol v1 today: after one lost reply, a polling loop of
// identical requests recovers at the next request, and each request gets its own reply.
// Enable once the host can tell these replies apart (protocol v2 sequence numbers, or a v1
// matching change).
TEST_F(RxMatching, DISABLED_IdenticalRequestsRecoverAfterLostReply)
{
	conn.SetResponder(PollingDevice{});

	auto first = conn.SendMsg(SynthRegRead(0x40));
	EXPECT_EQ(conn.WaitForStatus(first, 2s), Message::Status::TIMEOUT_ON_RXCV);

	for (int k = 1; k < 6; k++) {
		auto h = conn.SendMsg(SynthRegRead(0x40));
		ASSERT_EQ(conn.WaitForStatus(h, 2s), Message::Status::RX_OK) << "request " << k;
		EXPECT_EQ(ValueOf(h), 0x1000 + k) << "request " << k;
	}
}
