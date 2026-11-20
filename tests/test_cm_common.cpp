// CM_Common behaviour exercised through the CM_TEST simulated connection
#include "TestHelpers.h"

#include <mutex>
#include <thread>

using namespace iMS;
using namespace iMS::test;
using namespace std::chrono_literals;

TEST(CmTestLifecycle, SendWhileClosedReturnsNullMessage)
{
	CM_TEST conn;
	EXPECT_FALSE(conn.Open());
	EXPECT_EQ(conn.SendMsg(LutWrite(0)), NullMessage);
}

TEST(CmTestLifecycle, ConnectAndDisconnect)
{
	CM_TEST conn;
	conn.Connect("");
	EXPECT_TRUE(conn.Open());
	conn.Disconnect();
	EXPECT_FALSE(conn.Open());
	// Reconnecting works
	conn.Connect("");
	EXPECT_TRUE(conn.Open());
}

TEST_F(CmTest, RecordsWireBytesOfEachMessage)
{
	HostReport r = LutWrite(0x10, { 1, 2, 3, 4 });
	const Bytes expected = r.SerialStream();

	auto h = conn.SendMsg(r);
	EXPECT_EQ(conn.WaitForStatus(h, 1s), Message::Status::RX_OK);

	auto sent = conn.SentFrames();
	ASSERT_EQ(sent.size(), 1u);
	EXPECT_EQ(sent[0], expected);
}

TEST_F(CmTest, DefaultDeviceReadsBackWrittenRegister)
{
	EXPECT_EQ(conn.WaitForStatus(conn.SendMsg(SynthRegWrite(0x40, 0xBEEF)), 1s), Message::Status::RX_OK);

	DeviceReport resp = conn.SendMsgBlocking(SynthRegRead(0x40));
	EXPECT_TRUE(resp.Done());
	EXPECT_EQ(resp.Fields().addr, 0x40);
	EXPECT_EQ(resp.Payload<std::vector<std::uint16_t>>().at(0), 0xBEEF);
	EXPECT_TRUE(events.WaitFor(MessageEvents::RESPONSE_RECEIVED, 2));
}

TEST_F(CmTest, MissingResponseTimesOut)
{
	conn.SetResponder([](const Bytes&) { return Bytes{}; });

	auto h = conn.SendMsg(LutWrite(0x10));
	EXPECT_EQ(conn.WaitForStatus(h, 2s), Message::Status::TIMEOUT_ON_RXCV);
	EXPECT_TRUE(events.WaitFor(MessageEvents::RESPONSE_TIMED_OUT, 1));
}

TEST_F(CmTest, ErrorResponseIsReported)
{
	conn.SetResponder([](const Bytes& req) { return CM_TEST::ErrorResponse(req); });

	auto h = conn.SendMsg(LutWrite(0x10));
	EXPECT_EQ(conn.WaitForStatus(h, 1s), Message::Status::RX_ERROR_VALID);
	EXPECT_TRUE(events.WaitFor(MessageEvents::RESPONSE_ERROR_VALID, 1));
}

TEST_F(CmTest, SendErrorIsReported)
{
	conn.FailNextSends(1);

	auto h = conn.SendMsg(LutWrite(0x10));
	EXPECT_EQ(conn.WaitForStatus(h, 1s), Message::Status::SEND_ERROR);
	EXPECT_TRUE(events.WaitFor(MessageEvents::SEND_ERROR, 1));

	// The connection carries on afterwards
	EXPECT_EQ(conn.WaitForStatus(conn.SendMsg(LutWrite(0x18)), 1s), Message::Status::RX_OK);
}

TEST_F(CmTest, MergedResponsesMatchTheirMessages)
{
	// Collect the replies and deliver them as one block, as a TCP read might
	std::vector<Bytes> replies;
	std::mutex mtx;
	conn.SetResponder([&](const Bytes& req) {
		std::scoped_lock l(mtx);
		replies.push_back(CM_TEST::OkResponse(req));
		return Bytes{};
	});

	auto a = conn.SendMsg(LutWrite(0x10));
	auto b = conn.SendMsg(LutWrite(0x18));
	auto c = conn.SendMsg(LutWrite(0x20));
	ASSERT_TRUE(conn.WaitForSent(3, 1s));

	Bytes block;
	{
		std::scoped_lock l(mtx);
		for (auto& r : replies) block.insert(block.end(), r.begin(), r.end());
	}
	conn.InjectRx(block);

	EXPECT_EQ(conn.WaitForStatus(a, 1s), Message::Status::RX_OK);
	EXPECT_EQ(conn.WaitForStatus(b, 1s), Message::Status::RX_OK);
	EXPECT_EQ(conn.WaitForStatus(c, 1s), Message::Status::RX_OK);
}

TEST_F(CmTest, SplitResponseWithLeadingGarbage)
{
	Bytes reply;
	conn.SetResponder([&](const Bytes& req) {
		reply = CM_TEST::OkResponse(req, { 0x34, 0x12 });
		return Bytes{};
	});

	auto h = conn.SendMsg(SynthRegRead(0x40));
	ASSERT_TRUE(conn.WaitForSent(1, 1s));

	conn.InjectRx(Bytes{ 0xAA, 0xBB });
	conn.InjectRx(Bytes(reply.begin(), reply.begin() + 4));
	std::this_thread::sleep_for(30ms);
	conn.InjectRx(Bytes(reply.begin() + 4, reply.end()));

	ASSERT_EQ(conn.WaitForStatus(h, 1s), Message::Status::RX_OK);
	EXPECT_EQ(conn.Response(h).Payload<std::vector<std::uint16_t>>().at(0), 0x1234);
}

TEST_F(CmTest, ResponseWithBadCrcIsRejected)
{
	conn.SetResponder([](const Bytes& req) {
		Bytes r = CM_TEST::OkResponse(req);
		r.back() ^= 0xFF;
		return r;
	});

	auto h = conn.SendMsg(LutWrite(0x10));
	EXPECT_EQ(conn.WaitForStatus(h, 2s), Message::Status::RX_ERROR_INVALID);
	EXPECT_TRUE(events.WaitFor(MessageEvents::RESPONSE_ERROR_CRC, 1));
}

TEST_F(CmTest, InterruptRaisesEvent)
{
	conn.InjectRx(CM_TEST::InterruptFrame(0x0003, { 0x01, 0x00 }));

	EXPECT_TRUE(events.WaitFor(MessageEvents::INTERRUPT_RECEIVED, 1));
}

TEST_F(CmTest, HeldMessagesStayUnsentUntilReleased)
{
	conn.HoldTransmit(true);
	auto h = conn.SendMsg(LutWrite(0x10));
	std::this_thread::sleep_for(50ms);
	EXPECT_EQ(conn.SentCount(), 0u);
	EXPECT_EQ(conn.FindMessage(h)->getStatus(), Message::Status::UNSENT);

	conn.HoldTransmit(false);
	EXPECT_EQ(conn.WaitForStatus(h, 1s), Message::Status::RX_OK);
}

TEST_F(CmTest, SerialisedSenderWaitsForEachResponse)
{
	conn.SetMessageSerialisation(true);

	// No replies: with serialisation on, the second message must not go out
	// until the first has completed (here, by timing out)
	conn.SetResponder([](const Bytes&) { return Bytes{}; });
	auto a = conn.SendMsg(LutWrite(0x10));
	auto b = conn.SendMsg(LutWrite(0x18));

	ASSERT_TRUE(conn.WaitForSent(1, 1s));
	std::this_thread::sleep_for(50ms);
	EXPECT_EQ(conn.SentCount(), 1u);

	EXPECT_EQ(conn.WaitForStatus(a, 2s), Message::Status::TIMEOUT_ON_RXCV);
	ASSERT_TRUE(conn.WaitForSent(2, 1s));
	EXPECT_EQ(conn.WaitForStatus(b, 2s), Message::Status::TIMEOUT_ON_RXCV);
}
