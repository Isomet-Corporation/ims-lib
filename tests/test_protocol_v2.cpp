// Protocol v2 (09-26-015-SPEC-001): the codec on its own, protocol negotiation, and request /
// response handling over a v2 link.
//
// The fault suites in test_rx_matching.cpp also run under v2; the tests here cover what only v2
// has: sequence numbers, COBS framing and resynchronisation, STATUS and FLAGS, the MAX_INFLIGHT
// window, payloads beyond the v1 limit, and the protocol probe.
#include "TestHelpers.h"
#include "ProtocolV2.h"

#include <algorithm>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace iMS;
using namespace iMS::test;
using namespace iMS::ProtocolV2;
using namespace std::chrono_literals;

namespace {

	Bytes Words(const std::vector<std::uint16_t>& words)
	{
		Bytes b;
		for (auto w : words) {
			b.push_back(static_cast<std::uint8_t>(w & 0xFF));
			b.push_back(static_cast<std::uint8_t>(w >> 8));
		}
		return b;
	}

	// The 16 header bytes, laid out by hand from the spec (not by the code under test)
	Bytes RawHeader(const Header& h)
	{
		return Bytes{ h.pver, h.type,
			static_cast<std::uint8_t>(h.seq & 0xFF), static_cast<std::uint8_t>(h.seq >> 8),
			h.cmd, h.subcmd, h.flags, h.status,
			static_cast<std::uint8_t>(h.addr & 0xFF), static_cast<std::uint8_t>((h.addr >> 8) & 0xFF),
			static_cast<std::uint8_t>((h.addr >> 16) & 0xFF), static_cast<std::uint8_t>(h.addr >> 24),
			static_cast<std::uint8_t>(h.len & 0xFF), static_cast<std::uint8_t>(h.len >> 8),
			static_cast<std::uint8_t>(h.rdlen & 0xFF), static_cast<std::uint8_t>(h.rdlen >> 8) };
	}

	// A wire frame from raw header and payload bytes, without checking that they agree
	Bytes WireFromRaw(Bytes raw)
	{
		const std::uint32_t crc = Crc32c(raw.data(), raw.size());
		for (int i = 0; i < 4; i++) raw.push_back(static_cast<std::uint8_t>(crc >> (8 * i)));
		Bytes wire{ Delimiter };
		const Bytes enc = CobsEncode(raw);
		wire.insert(wire.end(), enc.begin(), enc.end());
		wire.push_back(Delimiter);
		return wire;
	}

	Header Decoded(const Bytes& wire)
	{
		Header h;
		Bytes payload;
		EXPECT_EQ(DecodeWireFrame(wire, h, payload), DecodeResult::OK) << "not a valid v2 frame: " << Hex(wire);
		return h;
	}

	// A device's reply to a request header (the request need not have been sent)
	Bytes ReplyTo(Header req, Status st, const Bytes& payload = {}, std::uint8_t flags = 0)
	{
		req.type = Header::MakeType(Kind::RESPONSE, req.target());
		req.status = static_cast<std::uint8_t>(st);
		req.flags = static_cast<std::uint8_t>((req.flags & FlagRead) | flags);
		req.rdlen = 0;
		return BuildFrame(req, payload);
	}

	DeviceReport ParseV1(const Bytes& frame)
	{
		DeviceReport d;
		for (auto b : frame) d.Parse(b);
		return d;
	}

}

// =============================================================================================
// Codec
// =============================================================================================

TEST(ProtocolV2Codec, Crc32cCheckValue)
{
	const char* s = "123456789";
	EXPECT_EQ(Crc32c(reinterpret_cast<const std::uint8_t*>(s), 9), 0xE3069283u);
}

// 09-26-015-SPEC-001 section 12.1: read controller register 0, SEQ 1
TEST(ProtocolV2Codec, SpecVectorRequest)
{
	HostReport r(HostReport::Actions::CTRLR_REG, HostReport::Dir::READ, 0);
	const Bytes expected{ 0x00, 0x02, 0x02, 0x02, 0x01, 0x02, 0x11, 0x02, 0x80, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02,
		0x02, 0x05, 0x48, 0xF5, 0x81, 0x82, 0x00 };
	const Bytes wire = EncodeRequest(r.Fields(), {}, 1);
	EXPECT_EQ(wire, expected) << Hex(wire);

	// The same bytes from the header laid out by hand
	Header h;
	h.type = Header::MakeType(Kind::REQUEST, Target::CONTROLLER);
	h.seq = 1;
	h.cmd = 0x11;
	h.flags = FlagRead;
	h.rdlen = 2;
	EXPECT_EQ(WireFromRaw(RawHeader(h)), expected);
}

// Section 12.2: the OK response carrying 0x1234
TEST(ProtocolV2Codec, SpecVectorResponse)
{
	const Bytes wire{ 0x00, 0x04, 0x02, 0x01, 0x01, 0x02, 0x11, 0x02, 0x80, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x01,
		0x01, 0x07, 0x34, 0x12, 0x5C, 0x10, 0x53, 0x91, 0x00 };
	Header h;
	Bytes p;
	ASSERT_EQ(DecodeWireFrame(wire, h, p), DecodeResult::OK);
	EXPECT_EQ(h.kind(), Kind::RESPONSE);
	EXPECT_EQ(h.target(), Target::CONTROLLER);
	EXPECT_EQ(h.seq, 1);
	EXPECT_EQ(h.cmd, 0x11);
	EXPECT_EQ(h.flags, FlagRead);
	EXPECT_EQ(h.status, 0);
	EXPECT_EQ(h.len, 2);
	EXPECT_EQ(p, (Bytes{ 0x34, 0x12 }));

	Header req = h;
	req.type = Header::MakeType(Kind::REQUEST, Target::CONTROLLER);
	EXPECT_EQ(ReplyTo(req, Status::OK, { 0x34, 0x12 }), wire);
}

TEST(ProtocolV2Codec, CobsRoundTrip)
{
	std::mt19937 rng(1234);
	for (std::size_t n : { 0u, 1u, 2u, 253u, 254u, 255u, 256u, 507u, 508u, 509u, 1024u, 4100u }) {
		for (int fill = 0; fill < 4; fill++) {
			Bytes d(n);
			for (auto& b : d)
				b = (fill == 0) ? 0 : (fill == 1) ? 0xFF : (fill == 2) ? static_cast<std::uint8_t>(rng() % 3) : static_cast<std::uint8_t>(rng());
			const Bytes e = CobsEncode(d);
			EXPECT_EQ(std::find(e.begin(), e.end(), 0), e.end()) << "zero in encoding, n=" << n << " fill=" << fill;
			EXPECT_LE(e.size(), n + n / 254 + 1) << "n=" << n;
			Bytes back;
			ASSERT_TRUE(CobsDecode(e.data(), e.size(), back)) << "n=" << n << " fill=" << fill;
			EXPECT_EQ(back, d) << "n=" << n << " fill=" << fill;
		}
	}
}

TEST(ProtocolV2Codec, MaxEncodedLenCoversLargestFrame)
{
	for (std::size_t payload : { 0u, 64u, 238u, 1024u, 4096u }) {
		Header h;
		const Bytes wire = BuildFrame(h, Bytes(payload, 0x5A));
		EXPECT_LE(wire.size() - 2, MaxEncodedLen(payload)) << "payload " << payload;
	}
}

TEST(ProtocolV2Codec, DecodeErrors)
{
	Header r;
	r.type = Header::MakeType(Kind::REQUEST, Target::SYNTH);
	r.cmd = 0x07;
	r.seq = 9;
	const Bytes w = ReplyTo(r, Status::OK, { 1, 2, 3, 4 });
	Header h;
	Bytes p;

	Bytes bad = w;
	bad[6] ^= 0x10;
	EXPECT_EQ(DecodeFrame(bad.data() + 1, bad.size() - 2, h, p), DecodeResult::BAD_CRC);
	EXPECT_EQ(DecodeFrame(w.data() + 1, 6, h, p), DecodeResult::TOO_SHORT);

	const Bytes cobsBad{ 0x09, 0x01, 0x02 };
	EXPECT_EQ(DecodeFrame(cobsBad.data(), cobsBad.size(), h, p), DecodeResult::BAD_COBS);

	// LEN says 3 but 4 bytes follow; the CRC is good, so the header is returned
	Header lying = r;
	lying.len = 3;
	const Bytes badLen = WireFromRaw(RawHeader(lying) + Bytes{ 1, 2, 3, 4 });
	EXPECT_EQ(DecodeWireFrame(badLen, h, p), DecodeResult::BAD_LEN);
	EXPECT_EQ(h.seq, 9);
}

TEST(ProtocolV2Codec, CommandMappingFromV1)
{
	auto cmdOf = [](HostReport::Actions a) { HostReport r(a, HostReport::Dir::WRITE, 0); return CommandFromV1(r.Fields()); };
	EXPECT_EQ(cmdOf(HostReport::Actions::PLL_REF), 0x00);
	EXPECT_EQ(cmdOf(HostReport::Actions::RF_POWER), 0x01);
	EXPECT_EQ(cmdOf(HostReport::Actions::SYNTH_REG), 0x07);
	EXPECT_EQ(cmdOf(HostReport::Actions::FW_UPGRADE), 0x20);
	EXPECT_EQ(cmdOf(HostReport::Actions::WAVE_SHAPING), 0x21);
	EXPECT_EQ(cmdOf(HostReport::Actions::CTRLR_REG), 0x11);
	EXPECT_EQ(cmdOf(HostReport::Actions::CTRLR_FW_UPGRADE), 0x30);

	HostReport fw(HostReport::Actions::FW_UPGRADE, HostReport::Dir::WRITE, 0);
	EXPECT_EQ(RequestHeaderFromV1(fw.Fields(), 0).subcmd, 0) << "extension CONTEXT bit copied to SUBCMD";
}

TEST(ProtocolV2Codec, RequestHeaderFromV1Fields)
{
	// CONTEXT is a sub-command selector for most commands, and the read length goes in RDLEN
	HostReport idx(HostReport::Actions::CTRLR_IMGIDX, HostReport::Dir::READ, 7);
	ReportFields f = idx.Fields();
	f.context = 2;
	f.len = 49;
	idx.Fields(f);
	Header h = RequestHeaderFromV1(idx.Fields(), 5);
	EXPECT_EQ(h.cmd, 0x15);
	EXPECT_EQ(h.subcmd, 2);
	EXPECT_EQ(h.addr, 7u);
	EXPECT_EQ(h.rdlen, 49);
	EXPECT_EQ(h.flags, FlagRead);
	EXPECT_EQ(h.seq, 5);
	EXPECT_EQ(h.kind(), Kind::REQUEST);
	EXPECT_EQ(h.target(), Target::CONTROLLER);

	// For image and EEPROM commands CONTEXT is address bits 23:16
	HostReport img(HostReport::Actions::CTRLR_IMAGE, HostReport::Dir::WRITE, 0x1234);
	f = img.Fields();
	f.context = 3;
	img.Fields(f);
	h = RequestHeaderFromV1(img.Fields(), 0);
	EXPECT_EQ(h.addr, 0x031234u);
	EXPECT_EQ(h.subcmd, 0);
	EXPECT_EQ(h.rdlen, 0);

	HostReport ee(HostReport::Actions::SYNTH_EEPROM, HostReport::Dir::READ, 0x0010);
	f = ee.Fields();
	f.context = 1;
	f.len = 32;
	ee.Fields(f);
	h = RequestHeaderFromV1(ee.Fields(), 0);
	EXPECT_EQ(h.cmd, 0x02);
	EXPECT_EQ(h.addr, 0x010010u);
	EXPECT_EQ(h.target(), Target::SYNTH);
}

TEST(ProtocolV2Codec, StatusMapsToV1ResponseFlags)
{
	auto parse = [](Status st, std::uint8_t flags = 0) {
		Header h;
		h.type = Header::MakeType(Kind::RESPONSE, Target::SYNTH);
		h.cmd = 0x07;
		h.status = static_cast<std::uint8_t>(st);
		h.flags = flags;
		return ParseV1(ToV1DeviceFrame(h, { 0xAA, 0x55 }));
	};

	const DeviceReport ok = parse(Status::OK);
	EXPECT_TRUE(ok.Done());
	EXPECT_FALSE(ok.RxCRC() || ok.GeneralError() || ok.TxTimeout() || ok.TxCRC() || ok.HardwareAlarm());
	EXPECT_EQ(ok.Payload<std::uint16_t>(), 0x55AA);

	// Each error status sets exactly one of the v1 error flags
	const DeviceReport timeout = parse(Status::TARGET_TIMEOUT);
	EXPECT_TRUE(timeout.TxTimeout());
	EXPECT_FALSE(timeout.TxCRC()) << "a target timeout also reads as a CRC error";
	EXPECT_FALSE(timeout.GeneralError());
	const DeviceReport crc = parse(Status::TARGET_CRC);
	EXPECT_TRUE(crc.TxCRC());
	EXPECT_FALSE(crc.TxTimeout());
	EXPECT_FALSE(crc.GeneralError());
	for (Status st : { Status::BAD_VERSION, Status::BAD_TYPE, Status::NO_TARGET, Status::UNKNOWN_CMD, Status::BAD_ADDR,
		Status::BAD_LEN, Status::BUSY, Status::GENERAL_ERROR })
		EXPECT_TRUE(parse(st).GeneralError()) << StatusName(static_cast<std::uint8_t>(st));
	EXPECT_TRUE(parse(Status::OK, FlagAlarm).HardwareAlarm());
}

TEST(ProtocolV2Codec, DescribeForTraceLogs)
{
	HostReport r(HostReport::Actions::CTRLR_REG, HostReport::Dir::READ, 3);
	Header h;
	Bytes p;
	ASSERT_EQ(DecodeWireFrame(EncodeRequest(r.Fields(), {}, 4660), h, p), DecodeResult::OK);
	EXPECT_EQ(Describe(h, p), "SEQ 4660 request ctrlr CTRLR_REG(0x11) read addr 0x00000003 rdlen 2 len 0");

	ASSERT_EQ(DecodeWireFrame(ReplyTo(h, Status::OK, { 0x78, 0x56 }, FlagAlarm), h, p), DecodeResult::OK);
	EXPECT_EQ(Describe(h, p), "SEQ 4660 response ctrlr CTRLR_REG(0x11) read OK ALARM addr 0x00000003 len 2: 78 56");

	Header big;
	big.type = Header::MakeType(Kind::RESPONSE, Target::SYNTH);
	big.cmd = 0x3F;
	big.status = 0x85;
	big.len = 70;
	EXPECT_EQ(Describe(big, Bytes(70, 0xAB), 4),
		"SEQ 0 response synth CMD_0x3F(0x3f) write COMMAND_SPECIFIC_0x85 addr 0x00000000 len 70: ab ab ab ab ... (+66 bytes)");
}

TEST(ProtocolV2Codec, ProtocolInfoParsing)
{
	EXPECT_FALSE(ProtocolInfo::Parse(std::vector<std::uint16_t>(InfoWords, 0)).valid) << "a v1 device reads as zeros";

	const auto i = ProtocolInfo::Parse({ InfoMagic, 2, 0x0003, 0, 1024, 8, 4, 256, 2, 0 });
	EXPECT_TRUE(i.SupportsV2());
	EXPECT_EQ(i.features, 3u);
	EXPECT_EQ(i.maxPayload, 1024);
	EXPECT_EQ(i.maxInflight, 8);
	EXPECT_EQ(i.replayDepth, 4);
	EXPECT_EQ(i.relayMaxPayload, 256);
	EXPECT_EQ(i.relayMaxInflight, 2);

	const auto j = ProtocolInfo::Parse({ InfoMagic, 2, 0, 0, 0, 0, 0, 0, 0, 0 });
	EXPECT_EQ(j.maxPayload, 64) << "minimum payload enforced";
	EXPECT_EQ(j.maxInflight, 1) << "minimum window enforced";

	EXPECT_FALSE(ProtocolInfo::Parse({ InfoMagic, 1, 0, 0, 64, 1, 0, 0, 0, 0 }).SupportsV2()) << "PVER_MAX 1";
}

// =============================================================================================
// Negotiation: the v1 probe of the protocol info block
// =============================================================================================

namespace {

	const Bytes V2Info = Words({ InfoMagic, 2, 0x0001, 0, 512, 8, 0, 128, 2, 0 });
	const Bytes V1Info(InfoWords * 2, 0);

	// A device that answers only the v1 probe reads.  Empty info: that device doesn't answer.
	CM_TEST::Responder ProbeDevice(Bytes ctrlrInfo, Bytes synthInfo)
	{
		return [ctrlrInfo, synthInfo](const Bytes& req) -> Bytes {
			const CM_TEST::Request r = CM_TEST::ParseRequest(req);
			if (!r.valid || r.v2 || !r.read || r.addr != InfoBaseProvisional) return {};
			if (r.space == static_cast<std::uint8_t>(ReportTypes::HOST_REPORT_ID_CTRLR) && !ctrlrInfo.empty())
				return CM_TEST::OkResponse(req, ctrlrInfo);
			if (r.space == static_cast<std::uint8_t>(ReportTypes::HOST_REPORT_ID_SYNTH) && !synthInfo.empty())
				return CM_TEST::OkResponse(req, synthInfo);
			return {};
		};
	}

	class ProtocolNegotiation : public CmTest
	{
	protected:
		void Auto() { conn.SetProtocolPreference(IConnectionManager::ProtocolPreference::AUTO); }
	};

}

TEST_F(ProtocolNegotiation, PreferenceV1SendsNoProbe)
{
	conn.SetResponder(ProbeDevice(V2Info, V2Info));
	conn.NegotiateProtocol();
	EXPECT_EQ(conn.SentCount(), 0u);
	EXPECT_FALSE(conn.ProtocolV2Active());
}

TEST_F(ProtocolNegotiation, V1ControllerStaysV1)
{
	conn.SetResponder(ProbeDevice(V1Info, V2Info));
	Auto();
	conn.NegotiateProtocol();
	EXPECT_FALSE(conn.ProtocolV2Active());
	ASSERT_EQ(conn.SentCount(), 1u) << "a synthesiser behind a controller must not be probed";

	const CM_TEST::Request probe = CM_TEST::ParseRequest(conn.SentFrames()[0]);
	EXPECT_FALSE(probe.v2);
	EXPECT_EQ(probe.space, static_cast<std::uint8_t>(ReportTypes::HOST_REPORT_ID_CTRLR));
	EXPECT_TRUE(probe.read);
	EXPECT_EQ(probe.addr, InfoBaseProvisional);
	EXPECT_EQ(probe.readLen, InfoWords * 2);
}

TEST_F(ProtocolNegotiation, ControllerErrorReplyStaysV1)
{
	conn.SetResponder([](const Bytes& req) { return CM_TEST::ErrorResponse(req); });
	Auto();
	conn.NegotiateProtocol();
	EXPECT_FALSE(conn.ProtocolV2Active());
	EXPECT_EQ(conn.SentCount(), 1u) << "the controller answered, so the synthesiser isn't probed";
}

TEST_F(ProtocolNegotiation, V2ControllerSwitchesToV2)
{
	conn.SetResponder(ProbeDevice(V2Info, {}));
	Auto();
	conn.NegotiateProtocol();
	ASSERT_TRUE(conn.ProtocolV2Active());

	// Traffic now goes as v2 frames, and v2 replies are delivered
	conn.SetResponder(nullptr);
	auto w = conn.SendMsg(SynthRegWrite(0x10, 0x1234));
	auto r = conn.SendMsg(SynthRegRead(0x10));
	ASSERT_EQ(conn.WaitForStatus(w, 1s), Message::Status::RX_OK);
	ASSERT_EQ(conn.WaitForStatus(r, 1s), Message::Status::RX_OK);
	EXPECT_EQ(ValueOf(r), 0x1234);
	const auto sent = conn.SentFrames();
	ASSERT_EQ(sent.size(), 3u);
	EXPECT_TRUE(CM_TEST::IsV2Frame(sent[1]));
	EXPECT_TRUE(CM_TEST::IsV2Frame(sent[2]));

	// A new connection starts on v1 until it is negotiated again
	conn.Disconnect();
	conn.Connect("");
	EXPECT_FALSE(conn.ProtocolV2Active());
}

TEST_F(ProtocolNegotiation, DirectlyConnectedV2Synthesiser)
{
	conn.SetResponder(ProbeDevice({}, V2Info));
	Auto();
	conn.NegotiateProtocol();   // the controller probe times out first
	EXPECT_TRUE(conn.ProtocolV2Active());
	EXPECT_EQ(conn.SentCount(), 2u);
}

TEST_F(ProtocolNegotiation, NoDeviceAnswersStaysV1)
{
	conn.SetResponder([](const Bytes&) { return Bytes{}; });
	Auto();
	conn.NegotiateProtocol();
	EXPECT_FALSE(conn.ProtocolV2Active());
	EXPECT_EQ(conn.SentCount(), 2u);
}

TEST_F(ProtocolNegotiation, ConnectionTypeWithoutV2SendsNoProbe)
{
	conn.SetSupportsProtocolV2(false);
	conn.SetResponder(ProbeDevice(V2Info, V2Info));
	Auto();
	conn.NegotiateProtocol();
	EXPECT_FALSE(conn.ProtocolV2Active());
	EXPECT_EQ(conn.SentCount(), 0u);
}

// =============================================================================================
// A v2 link.  The device doesn't answer by itself: each test injects the replies it wants.
// =============================================================================================

namespace {

	class ProtocolV2Link : public CmTest
	{
	protected:
		void SetUp() override
		{
			CmTest::SetUp();
			conn.UseProtocolV2(32);
			conn.SetResponder([](const Bytes&) { return Bytes{}; });
		}

		// The request header of the n-th frame sent (from 0), waiting for it to be sent
		Header SentHeader(std::size_t n)
		{
			EXPECT_TRUE(conn.WaitForSent(n + 1, 1s)) << "frame " << n << " not sent";
			const auto sent = conn.SentFrames();
			if (sent.size() <= n) return Header();
			return Decoded(sent[n]);
		}

		std::uint16_t Value(MessageHandle h) { return ValueOf(h); }
	};

}

TEST_F(ProtocolV2Link, RequestsCarryConsecutiveSequenceNumbers)
{
	conn.SendMsg(CtrlrRegRead(0x10));
	conn.SendMsg(SynthRegRead(0x20));
	conn.SendMsg(LutWrite(0x30));

	const Header a = SentHeader(0), b = SentHeader(1), c = SentHeader(2);
	EXPECT_EQ(b.seq, static_cast<std::uint16_t>(a.seq + 1));
	EXPECT_EQ(c.seq, static_cast<std::uint16_t>(b.seq + 1));

	EXPECT_EQ(a.kind(), Kind::REQUEST);
	EXPECT_EQ(a.target(), Target::CONTROLLER);
	EXPECT_EQ(a.cmd, 0x11);
	EXPECT_EQ(a.flags, FlagRead);
	EXPECT_EQ(a.addr, 0x10u);
	EXPECT_EQ(a.rdlen, 2);

	EXPECT_EQ(b.target(), Target::SYNTH);
	EXPECT_EQ(b.cmd, 0x07);

	EXPECT_EQ(c.flags & FlagRead, 0);
	EXPECT_EQ(c.len, 8);
	EXPECT_EQ(c.rdlen, 0);
}

TEST_F(ProtocolV2Link, OutOfOrderRepliesMatchedBySequenceNumber)
{
	auto a = conn.SendMsg(CtrlrRegRead(0x10));
	auto b = conn.SendMsg(CtrlrRegRead(0x11));
	auto c = conn.SendMsg(CtrlrRegRead(0x12));
	const Header ra = SentHeader(0), rb = SentHeader(1), rc = SentHeader(2);

	conn.InjectRx(ReplyTo(rc, Status::OK, Value16(3)) + ReplyTo(ra, Status::OK, Value16(1)) + ReplyTo(rb, Status::OK, Value16(2)));
	ASSERT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_OK);
	ASSERT_EQ(conn.WaitForStatus(b, 1s), Message::Status::RX_OK);
	ASSERT_EQ(conn.WaitForStatus(c, 1s), Message::Status::RX_OK);
	EXPECT_EQ(Value(a), 1);
	EXPECT_EQ(Value(b), 2);
	EXPECT_EQ(Value(c), 3);
}

// Identical requests outstanding together, answered in reverse order.  v1 can only tell their
// replies apart by order; v2 gives each its own.
TEST_F(ProtocolV2Link, IdenticalRequestsAnsweredInReverseOrder)
{
	std::vector<MessageHandle> h;
	for (int i = 0; i < 4; i++) h.push_back(conn.SendMsg(SynthRegRead(0x40)));
	std::vector<Header> req;
	for (std::size_t i = 0; i < 4; i++) req.push_back(SentHeader(i));

	Bytes replies;
	for (int i = 3; i >= 0; i--) replies = replies + ReplyTo(req[i], Status::OK, Value16(static_cast<std::uint16_t>(0x100 + i)));
	conn.InjectRx(replies);

	for (int i = 0; i < 4; i++) {
		ASSERT_EQ(conn.WaitForStatus(h[i], 1s), Message::Status::RX_OK) << i;
		EXPECT_EQ(Value(h[i]), 0x100 + i) << i;
	}
}

// A reply that comes after its request timed out is absorbed, never given to another message,
// and the timed-out request's SEQ isn't reused while the late reply can still come
TEST_F(ProtocolV2Link, LateReplyNeverCreditedToAnotherMessage)
{
	auto a = conn.SendMsg(SynthRegRead(0x40));
	const Header ra = SentHeader(0);
	EXPECT_EQ(conn.WaitForStatus(a, 1s), Message::Status::TIMEOUT_ON_RXCV);

	auto b = conn.SendMsg(SynthRegRead(0x40));
	const Header rb = SentHeader(1);
	EXPECT_NE(rb.seq, ra.seq);

	conn.InjectRx(ReplyTo(ra, Status::OK, Value16(0xDEAD)) + ReplyTo(rb, Status::OK, Value16(0x2222)));
	ASSERT_EQ(conn.WaitForStatus(b, 1s), Message::Status::RX_OK);
	EXPECT_EQ(Value(b), 0x2222);
	EXPECT_EQ(conn.FindMessage(a)->getStatus(), Message::Status::TIMEOUT_ON_RXCV);
}

// v2 doesn't take one reply as proof that earlier replies were lost: replies may come in any
// order, so a request with no reply waits for its full timeout
TEST_F(ProtocolV2Link, MissingReplyWaitsForItsTimeout)
{
	auto a = conn.SendMsg(SynthRegRead(0x40));
	auto b = conn.SendMsg(SynthRegRead(0x41));
	SentHeader(0);
	conn.InjectRx(ReplyTo(SentHeader(1), Status::OK, Value16(1)));
	ASSERT_EQ(conn.WaitForStatus(b, 1s), Message::Status::RX_OK);
	EXPECT_EQ(conn.FindMessage(a)->getStatus(), Message::Status::SENT);
	EXPECT_EQ(conn.WaitForStatus(a, 1s), Message::Status::TIMEOUT_ON_RXCV);
}

TEST_F(ProtocolV2Link, ResynchronisesAfterCorruption)
{
	auto a = conn.SendMsg(CtrlrRegRead(0x30));
	auto b = conn.SendMsg(CtrlrRegRead(0x31));
	auto c = conn.SendMsg(CtrlrRegRead(0x32));
	const Header ra = SentHeader(0), rb = SentHeader(1), rc = SentHeader(2);

	const Bytes damaged = CM_TEST::Corrupt(ReplyTo(ra, Status::OK, Value16(0x0A)));
	Bytes unclosed = ReplyTo(rb, Status::OK, Value16(0x0B));
	unclosed.pop_back();                                       // closing delimiter lost
	conn.InjectRx(damaged + Bytes{ 0x11, 0x22, 0x33 } + unclosed + ReplyTo(rc, Status::OK, Value16(0x0D)));

	ASSERT_EQ(conn.WaitForStatus(c, 1s), Message::Status::RX_OK) << "frame after corruption, junk and a lost delimiter";
	EXPECT_EQ(Value(c), 0x0D);
	ASSERT_EQ(conn.WaitForStatus(b, 1s), Message::Status::RX_OK) << "frame whose closing delimiter was lost";
	EXPECT_EQ(Value(b), 0x0B);
	EXPECT_EQ(conn.WaitForStatus(a, 1s), Message::Status::TIMEOUT_ON_RXCV) << "damaged frame delivered";
}

TEST_F(ProtocolV2Link, FrameArrivingOneByteAtATime)
{
	// Sleeps on Windows last a timer tick (about 15 ms), so allow for the whole frame taking
	//	longer than the fixture's RX timeout
	conn.SetTimeouts(500, 2000, 10000, 0);
	auto a = conn.SendMsg(CtrlrRegRead(0x40));
	for (auto x : ReplyTo(SentHeader(0), Status::OK, Value16(0x44))) {
		conn.InjectRx({ x });
		std::this_thread::sleep_for(2ms);
	}
	ASSERT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_OK);
	EXPECT_EQ(Value(a), 0x44);
}

// A run of bytes longer than any valid frame, with no delimiter, is thrown away without waiting
// for the stall timeout, and the next frame is received
TEST_F(ProtocolV2Link, OversizedFrameDiscarded)
{
	conn.InjectRx(Bytes{ Delimiter } + Bytes(MaxEncodedLen(1024) + 100, 0x55));
	EXPECT_TRUE(events.WaitFor(MessageEvents::UNEXPECTED_RX_CHAR, 1, 150ms));

	auto a = conn.SendMsg(CtrlrRegRead(0x41));
	conn.InjectRx(ReplyTo(SentHeader(0), Status::OK, Value16(0x41)));
	ASSERT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_OK);
}

// Delimiters on their own (idle fill, or the end of the last frame) aren't reported as errors
TEST_F(ProtocolV2Link, IdleDelimitersAreNotErrors)
{
	conn.InjectRx(Bytes{ Delimiter, Delimiter, Delimiter });
	std::this_thread::sleep_for(std::chrono::milliseconds(2 * RxTimeoutMs + 100));
	EXPECT_EQ(events.Count(MessageEvents::UNEXPECTED_RX_CHAR), 0);

	auto a = conn.SendMsg(CtrlrRegRead(0x42));
	conn.InjectRx(ReplyTo(SentHeader(0), Status::OK, Value16(0x42)));
	ASSERT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_OK);
}

// A frame whose LEN disagrees with its payload is discarded even though its CRC is good
TEST_F(ProtocolV2Link, FrameWithWrongLengthDiscarded)
{
	auto a = conn.SendMsg(SynthRegRead(0x50));
	Header r = SentHeader(0);
	r.type = Header::MakeType(Kind::RESPONSE, r.target());
	r.rdlen = 0;
	r.len = 4;
	conn.InjectRx(WireFromRaw(RawHeader(r) + Value16(0xDEAD)));
	std::this_thread::sleep_for(50ms);
	EXPECT_EQ(conn.FindMessage(a)->getStatus(), Message::Status::SENT);

	r.type = Header::MakeType(Kind::REQUEST, r.target());
	conn.InjectRx(ReplyTo(r, Status::OK, Value16(0x5050)));
	ASSERT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_OK);
	EXPECT_EQ(Value(a), 0x5050);
}

// A response whose SEQ matches but whose CMD or target doesn't is rejected
TEST_F(ProtocolV2Link, ReplyWithWrongCommandOrTargetRejected)
{
	auto a = conn.SendMsg(CtrlrRegRead(0x60));
	const Header req = SentHeader(0);

	Header wrongCmd = req;
	wrongCmd.cmd = 0x12;
	Header wrongTarget = req;
	wrongTarget.type = Header::MakeType(Kind::REQUEST, Target::SYNTH);
	conn.InjectRx(ReplyTo(wrongCmd, Status::OK, Value16(0xDEAD)) + ReplyTo(wrongTarget, Status::OK, Value16(0xDEAD)));
	std::this_thread::sleep_for(50ms);
	EXPECT_EQ(conn.FindMessage(a)->getStatus(), Message::Status::SENT);

	conn.InjectRx(ReplyTo(req, Status::OK, Value16(0x6060)));
	ASSERT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_OK);
	EXPECT_EQ(Value(a), 0x6060);
}

// Replies with an unknown SEQ, and frames that aren't responses, complete nothing
TEST_F(ProtocolV2Link, UnknownSequenceNumberAndWrongKindIgnored)
{
	auto a = conn.SendMsg(CtrlrRegRead(0x61));
	const Header req = SentHeader(0);

	Header stray = req;
	stray.seq = static_cast<std::uint16_t>(req.seq + 0x8000);
	Header echo = req;   // the request itself, echoed back
	conn.InjectRx(ReplyTo(stray, Status::OK, Value16(0xDEAD)) + BuildFrame(echo, {}));
	std::this_thread::sleep_for(50ms);
	EXPECT_EQ(conn.FindMessage(a)->getStatus(), Message::Status::SENT);

	conn.InjectRx(ReplyTo(req, Status::OK, Value16(0x6161)));
	ASSERT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_OK);
	EXPECT_EQ(Value(a), 0x6161);
}

// A device replying in v1 format to a v2 request: nothing is misread, the request times out
TEST_F(ProtocolV2Link, V1ReplyToV2RequestIgnored)
{
	auto a = conn.SendMsg(CtrlrRegRead(0x62));
	SentHeader(0);
	conn.InjectRx(CM_TEST::MakeFrame(ReportTypes::DEVICE_REPORT_ID_CTRLR, 0xC1, 0, 0x62, Value16(0xDEAD)));
	EXPECT_EQ(conn.WaitForStatus(a, 1s), Message::Status::TIMEOUT_ON_RXCV);
	EXPECT_TRUE(events.WaitFor(MessageEvents::UNEXPECTED_RX_CHAR, 1));
}

// Header bytes 0-3 (PVER, TYPE, SEQ) are fixed in every protocol version, so a reply in another
// version still completes its own request, but as an error: the rest of it can't be read
TEST_F(ProtocolV2Link, ReplyInOtherProtocolVersionIsErrorOnItsRequest)
{
	auto a = conn.SendMsg(CtrlrRegRead(0x63));
	auto b = conn.SendMsg(CtrlrRegRead(0x64));
	Header ra = SentHeader(0);
	const Header rb = SentHeader(1);
	ra.pver = 3;
	conn.InjectRx(ReplyTo(ra, Status::OK, Value16(0xDEAD)) + ReplyTo(rb, Status::OK, Value16(0x6464)));
	EXPECT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_ERROR_VALID);
	EXPECT_TRUE(conn.Response(a).GeneralError());
	ASSERT_EQ(conn.WaitForStatus(b, 1s), Message::Status::RX_OK);
	EXPECT_EQ(Value(b), 0x6464);
}

TEST_F(ProtocolV2Link, ReplyKeptForTraceLog)
{
	auto a = conn.SendMsg(CtrlrRegRead(0x64));
	const Bytes reply = ReplyTo(SentHeader(0), Status::OK, Value16(0x6464));
	conn.InjectRx(reply);
	ASSERT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_OK);
	auto m = conn.FindMessage(a);
	EXPECT_EQ(m->getRxWire(), reply);
	EXPECT_NE(m->getRxTrace().find("CTRLR_REG(0x11) read OK"), std::string::npos) << m->getRxTrace();
	EXPECT_GE(m->getSeq(), 0);
}

TEST_F(ProtocolV2Link, AlarmFlagRaisesEvent)
{
	auto a = conn.SendMsg(SynthRegRead(0x65));
	conn.InjectRx(ReplyTo(SentHeader(0), Status::OK, Value16(0x6565), FlagAlarm));
	ASSERT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_OK);
	EXPECT_EQ(Value(a), 0x6565);
	EXPECT_TRUE(conn.Response(a).HardwareAlarm());
	EXPECT_TRUE(events.WaitFor(MessageEvents::INTERLOCK_ALARM_SET, 1));
}

// Interrupts: v2 frames (one SEQ missing between them) and a v1 frame in the same stream as
// replies.  All are raised, and the reply after them is delivered.
TEST_F(ProtocolV2Link, InterruptsInReplyStream)
{
	auto a = conn.SendMsg(CtrlrRegRead(0x70));
	const Header req = SentHeader(0);
	conn.InjectRx(CM_TEST::InterruptFrameV2(100, 3, { 0x01, 0x00 }) + CM_TEST::InterruptFrameV2(102, 3, { 0x02, 0x00 })
		+ CM_TEST::InterruptFrame(3, { 0x03, 0x00 }) + ReplyTo(req, Status::OK, Value16(7)));
	ASSERT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_OK);
	EXPECT_EQ(Value(a), 7);
	EXPECT_TRUE(events.WaitFor(MessageEvents::INTERRUPT_RECEIVED, 3));
}

// No more than MAX_INFLIGHT requests are outstanding at once
TEST_F(ProtocolV2Link, MaxInflightWindow)
{
	conn.UseProtocolV2(2);
	std::vector<MessageHandle> h;
	for (int i = 0; i < 4; i++) h.push_back(conn.SendMsg(CtrlrRegRead(static_cast<std::uint16_t>(0x80 + i))));

	ASSERT_TRUE(conn.WaitForSent(2, 1s));
	std::this_thread::sleep_for(50ms);
	EXPECT_EQ(conn.SentCount(), 2u) << "more than two requests in flight";

	conn.InjectRx(ReplyTo(SentHeader(0), Status::OK, Value16(0)));
	ASSERT_TRUE(conn.WaitForSent(3, 1s));
	std::this_thread::sleep_for(50ms);
	EXPECT_EQ(conn.SentCount(), 3u) << "one reply should release exactly one more request";

	conn.InjectRx(ReplyTo(SentHeader(1), Status::OK, Value16(1)) + ReplyTo(SentHeader(2), Status::OK, Value16(2)));
	conn.InjectRx(ReplyTo(SentHeader(3), Status::OK, Value16(3)));
	for (int i = 0; i < 4; i++) {
		ASSERT_EQ(conn.WaitForStatus(h[i], 1s), Message::Status::RX_OK) << i;
		EXPECT_EQ(Value(h[i]), i) << i;
	}
}

// A request that times out frees its place in the window
TEST_F(ProtocolV2Link, TimeoutFreesWindowPlace)
{
	conn.UseProtocolV2(1);
	auto a = conn.SendMsg(CtrlrRegRead(0x90));
	auto b = conn.SendMsg(CtrlrRegRead(0x91));
	SentHeader(0);
	EXPECT_EQ(conn.WaitForStatus(a, 1s), Message::Status::TIMEOUT_ON_RXCV);
	conn.InjectRx(ReplyTo(SentHeader(1), Status::OK, Value16(0x91)));
	ASSERT_EQ(conn.WaitForStatus(b, 1s), Message::Status::RX_OK);
	EXPECT_EQ(Value(b), 0x91);
}

// Payloads beyond the v1 64-byte limit go in one frame
TEST_F(ProtocolV2Link, LargePayloadSentWhole)
{
	HostReport r(HostReport::Actions::CTRLR_IMAGE, HostReport::Dir::WRITE, 0);
	const Bytes data(300, 0x5A);
	r.Payload<Bytes>(data);
	auto a = conn.SendMsg(r);

	ASSERT_TRUE(conn.WaitForSent(1, 1s));
	Header h;
	Bytes payload;
	ASSERT_EQ(DecodeWireFrame(conn.SentFrames()[0], h, payload), DecodeResult::OK);
	EXPECT_EQ(h.len, 300);
	EXPECT_EQ(payload, data);

	conn.InjectRx(ReplyTo(h, Status::OK));
	EXPECT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_OK);
}

// ---------------------------------------------------------------------------------------------
// Device STATUS values, end to end
// ---------------------------------------------------------------------------------------------

namespace {

	enum class Flag { None, TxTimeout, TxCrc, GeneralError };

	struct StatusCase {
		Status status;
		Message::Status expected;
		Flag flag;
	};

	[[maybe_unused]] std::ostream& operator<<(std::ostream& os, const StatusCase& c)
	{
		return os << StatusName(static_cast<std::uint8_t>(c.status));
	}

	class ProtocolV2Status : public ProtocolV2Link, public ::testing::WithParamInterface<StatusCase> {};

}

TEST_P(ProtocolV2Status, ReportedOnTheRightRequest)
{
	const StatusCase c = GetParam();
	auto before = conn.SendMsg(SynthRegRead(0x10));
	auto faulted = conn.SendMsg(SynthRegRead(0x11));
	auto after = conn.SendMsg(SynthRegRead(0x12));
	const Header r0 = SentHeader(0), r1 = SentHeader(1), r2 = SentHeader(2);

	const Bytes payload = (c.status == Status::OK) ? Value16(0x1111) : Bytes{};
	conn.InjectRx(ReplyTo(r1, c.status, payload) + ReplyTo(r0, Status::OK, Value16(0x1010)) + ReplyTo(r2, Status::OK, Value16(0x1212)));

	EXPECT_EQ(conn.WaitForStatus(faulted, 1s), c.expected);
	const DeviceReport d = conn.Response(faulted);
	EXPECT_EQ(d.TxTimeout(), c.flag == Flag::TxTimeout);
	EXPECT_EQ(d.TxCRC(), c.flag == Flag::TxCrc);
	EXPECT_EQ(d.GeneralError(), c.flag == Flag::GeneralError);
	if (c.expected != Message::Status::RX_OK) EXPECT_TRUE(events.WaitFor(MessageEvents::RESPONSE_ERROR_VALID, 1));

	ASSERT_EQ(conn.WaitForStatus(before, 1s), Message::Status::RX_OK);
	ASSERT_EQ(conn.WaitForStatus(after, 1s), Message::Status::RX_OK);
	EXPECT_EQ(Value(before), 0x1010);
	EXPECT_EQ(Value(after), 0x1212);
}

INSTANTIATE_TEST_SUITE_P(AllStatuses, ProtocolV2Status,
	::testing::Values(
		StatusCase{ Status::OK, Message::Status::RX_OK, Flag::None },
		StatusCase{ Status::TARGET_TIMEOUT, Message::Status::RX_ERROR_VALID, Flag::TxTimeout },
		StatusCase{ Status::TARGET_CRC, Message::Status::RX_ERROR_VALID, Flag::TxCrc },
		StatusCase{ Status::BAD_VERSION, Message::Status::RX_ERROR_VALID, Flag::GeneralError },
		StatusCase{ Status::BAD_TYPE, Message::Status::RX_ERROR_VALID, Flag::GeneralError },
		StatusCase{ Status::NO_TARGET, Message::Status::RX_ERROR_VALID, Flag::GeneralError },
		StatusCase{ Status::UNKNOWN_CMD, Message::Status::RX_ERROR_VALID, Flag::GeneralError },
		StatusCase{ Status::BAD_ADDR, Message::Status::RX_ERROR_VALID, Flag::GeneralError },
		StatusCase{ Status::BAD_LEN, Message::Status::RX_ERROR_VALID, Flag::GeneralError },
		StatusCase{ Status::BUSY, Message::Status::RX_ERROR_VALID, Flag::GeneralError },
		StatusCase{ Status::GENERAL_ERROR, Message::Status::RX_ERROR_VALID, Flag::GeneralError }),
	[](const ::testing::TestParamInfo<StatusCase>& info) { return StatusName(static_cast<std::uint8_t>(info.param.status)); });
