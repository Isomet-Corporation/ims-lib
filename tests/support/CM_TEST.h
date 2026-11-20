/*-----------------------------------------------------------------------------
/ Title      : Connection Manager for automated testing
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ Description:
/   CM_TEST is a connection manager that talks to a simulated device instead of
/   real hardware.  It uses the CM_Common message machinery unchanged (send
/   queue, message registry, response matching, timeouts and events), so tests
/   exercise the same code paths as CM_ENET, CM_FTDI and the other modules.
/
/   - Every message sent is recorded as the bytes that would go on the wire.
/   - A responder callback plays the part of the device.  It receives each
/     request frame and returns the bytes the device sends back (or nothing,
/     to simulate a lost response).  The default responder is a small register
/     map: writes are stored and acknowledged, reads return what was written.
/   - Tests can also inject arbitrary device bytes at any time (split or merged
/     frames, garbage, corrupted CRCs, interrupts).
/
/   - Protocol v2 (09-26-015-SPEC-001) can be switched on without the probe
/     (UseProtocolV2).  The frame helpers answer in the protocol of the request
/     they are given, so the same responder works for both protocols.
/
/   CM_TEST is built only into the unit tests, not into the shipped library.
/----------------------------------------------------------------------------*/

#ifndef IMS_CM_TEST_H__
#define IMS_CM_TEST_H__

#include "CM_Common.h"
#include "ProtocolV2.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>
#include <tuple>
#include <vector>

namespace iMS {

	using Bytes = std::vector<std::uint8_t>;

	class CM_TEST : public CM_Common
	{
	public:
		// Called on the sender thread with the wire bytes of each request.  The returned bytes
		//	are delivered to the host as the device's reply; return an empty vector for no reply.
		using Responder = std::function<Bytes(const Bytes& request)>;

		CM_TEST();
		~CM_TEST() override;

		// IConnectionManager
		const std::string& Ident() const override;
		std::vector<std::shared_ptr<IMSSystem>> Discover(const ListBase<std::string>& PortMask, std::shared_ptr<IConnectionSettings>) override;
		void Connect(const std::string& serial) override;
		void Disconnect() override;
		void SetTimeouts(int send_timeout_ms, int rx_timeout_ms, int free_timeout_ms, int discover_timeout_ms) override;

		// ---- Protocol ----
		// Switch this connection to protocol v2 with the given device limits, as if the probe had
		//	found a v2 device.  Call after Connect(); a new connection starts on v1 again.
		void UseProtocolV2(std::uint16_t maxInflight = 16, std::uint16_t maxPayload = 1024);
		void UseProtocolV2(const ProtocolV2::ProtocolInfo& info);
		using CM_Common::ProtocolV2Active;
		// Pretend to be a connection type that can't carry v2 (as CM_CYUSB)
		void SetSupportsProtocolV2(bool supported);

		// ---- Simulated device ----
		// Replace the device simulation.  Pass nullptr to restore the default register map.
		void SetResponder(Responder r);
		// Deliver bytes to the host as if they had come from the device
		void InjectRx(const Bytes& bytes);
		// Deliver an interrupt on its own channel, as CM_ENET does with its interrupt socket,
		//	rather than in the shared reply stream
		void InjectInterruptChannel(const Bytes& bytes);
		// Pause transmission: messages stay queued (UNSENT) until released
		void HoldTransmit(bool hold);
		// Fail the next n transmissions with SEND_ERROR
		void FailNextSends(int n);
		// Fail every transmission whose request bytes the predicate accepts (nullptr: none)
		void FailSendsWhere(std::function<bool(const Bytes& request)> predicate);

		// The default responder: an OK reply to every request.  Writes are stored per
		//	(report ID, action, address); reads return the stored bytes, zero-filled to the
		//	requested length.
		Bytes DefaultDevice(const Bytes& request);
		void ClearDeviceMemory();

		// ---- Inspection ----
		std::vector<Bytes> SentFrames() const;
		std::size_t SentCount() const;
		void ClearSentFrames();
		// Wait until at least n frames have been transmitted
		bool WaitForSent(std::size_t n, std::chrono::milliseconds timeout);
		std::shared_ptr<Message> FindMessage(MessageHandle h) const;
		// Wait for a message to complete and return its final status
		Message::Status WaitForStatus(MessageHandle h, std::chrono::milliseconds timeout);

		// ---- Frame helpers ----
		// A request as the device sees it, in either protocol
		struct Request {
			bool valid{ false };
			bool v2{ false };
			std::uint8_t space{ 0 };     // v1: report ID; v2: 0x80 | target
			std::uint8_t action{ 0 };    // v1: HDR action nibble; v2: CMD
			bool read{ false };
			std::uint32_t addr{ 0 };
			std::uint16_t readLen{ 0 }; // bytes requested by a read
			Bytes payload;               // data carried by a write
			ProtocolV2::Header hdr;      // v2 only
		};
		static Request ParseRequest(const Bytes& request);
		static bool IsV2Frame(const Bytes& frame) { return !frame.empty() && frame[0] == ProtocolV2::Delimiter; }

		// Build a complete v1 frame (header, payload, CRC) as the device would send it
		static Bytes MakeFrame(ReportTypes id, std::uint8_t hdr, std::uint8_t context, std::uint16_t addr, const Bytes& payload = {});
		// An OK reply to a request frame, carrying the given payload, in the request's protocol
		static Bytes OkResponse(const Bytes& request, const Bytes& payload = {});
		// An error reply to a request frame: v1 DATA_OK clear and general error set; v2 STATUS
		//	GENERAL_ERROR
		static Bytes ErrorResponse(const Bytes& request);
		// A v2 reply to a v2 request with the given STATUS and FLAGS (the READ flag is copied
		//	from the request).  Empty if the request isn't a valid v2 frame.
		static Bytes V2Response(const Bytes& request, ProtocolV2::Status status, const Bytes& payload = {}, std::uint8_t flags = 0);
		// The same reply with a damaged CRC (v1) or a damaged encoded byte (v2)
		static Bytes Corrupt(const Bytes& frame);
		// An unsolicited controller interrupt: v1 frame, and v2 frame with its own SEQ
		static Bytes InterruptFrame(std::uint16_t addr, const Bytes& payload);
		static Bytes InterruptFrameV2(std::uint16_t seq, std::uint16_t addr, const Bytes& payload);

	protected:
		void MessageSender() override;
		bool SupportsProtocolV2() const override { return m_supportsV2; }
		void ResponseReceiver() override;

	private:
		const std::string m_ident{ "CM_TEST" };

		mutable std::mutex m_sentMutex;
		std::condition_variable m_sentCond;
		std::vector<Bytes> m_sent;

		std::mutex m_responderMutex;
		Responder m_responder;

		std::mutex m_memMutex;
		std::map<std::tuple<std::uint8_t, std::uint8_t, std::uint32_t>, Bytes> m_memory;

		std::atomic<bool> m_supportsV2{ true };

		std::atomic<bool> m_hold{ false };
		std::atomic<int> m_failSends{ 0 };
		std::mutex m_failMutex;
		std::function<bool(const Bytes&)> m_failWhere;
	};

}

#endif
