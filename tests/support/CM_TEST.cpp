/*-----------------------------------------------------------------------------
/ Title      : Connection Manager for automated testing
/ Project    : Isomet Modular Synthesiser System
/----------------------------------------------------------------------------*/

#include "CM_TEST.h"
#include "ReportManipulation.h"

#include <algorithm>
#include <queue>

namespace iMS {

	namespace {
		constexpr std::uint8_t HDR_ACTION_MASK = 0x0F;
		constexpr std::uint8_t HDR_READ = 0x80;
		constexpr std::uint8_t HDR_DATA_OK = 0x40;
		constexpr std::uint8_t HDR_GENERAL_ERROR = 0x20;

		std::uint16_t Get16(const Bytes& b, std::size_t i)
		{
			return static_cast<std::uint16_t>(b[i] | (b[i + 1] << 8));
		}

		ReportTypes ResponseIdFor(std::uint8_t requestId)
		{
			return (requestId == static_cast<std::uint8_t>(ReportTypes::HOST_REPORT_ID_CTRLR))
				? ReportTypes::DEVICE_REPORT_ID_CTRLR
				: ReportTypes::DEVICE_REPORT_ID_SYNTH;
		}
	}

	CM_TEST::CM_TEST()
	{
		sendTimeout = std::chrono::milliseconds(500);
		rxTimeout = std::chrono::milliseconds(500);
		autoFreeTimeout = std::chrono::milliseconds(10000);
	}

	CM_TEST::~CM_TEST()
	{
		Disconnect();
	}

	const std::string& CM_TEST::Ident() const
	{
		return m_ident;
	}

	std::vector<std::shared_ptr<IMSSystem>> CM_TEST::Discover(const ListBase<std::string>&, std::shared_ptr<IConnectionSettings>)
	{
		return {};
	}

	void CM_TEST::Connect(const std::string&)
	{
		if (DeviceIsOpen) return;
		{
			std::scoped_lock lk(m_rxmutex);
			m_rxCharQueue.clear();
		}
		DeviceIsOpen = true;
		senderThread = std::thread(&CM_TEST::MessageSender, this);
		parserThread = std::thread(&CM_TEST::MessageListManager, this);
	}

	void CM_TEST::Disconnect()
	{
		{
			// Take both locks so neither thread can miss the change between its check and its wait
			std::scoped_lock lk(m_txmutex, m_rxmutex);
			DeviceIsOpen = false;
		}
		m_txcond.notify_all();
		m_rxcond.notify_all();
		if (senderThread.joinable()) senderThread.join();
		if (parserThread.joinable()) parserThread.join();
	}

	void CM_TEST::SetTimeouts(int send_timeout_ms, int rx_timeout_ms, int free_timeout_ms, int)
	{
		sendTimeout = std::chrono::milliseconds(send_timeout_ms);
		rxTimeout = std::chrono::milliseconds(rx_timeout_ms);
		autoFreeTimeout = std::chrono::milliseconds(free_timeout_ms);
	}

	void CM_TEST::SetResponder(Responder r)
	{
		std::scoped_lock lk(m_responderMutex);
		m_responder = std::move(r);
	}

	void CM_TEST::InjectRx(const Bytes& bytes)
	{
		{
			std::scoped_lock lk(m_rxmutex);
			m_rxCharQueue.insert(m_rxCharQueue.end(), bytes.begin(), bytes.end());
		}
		m_rxcond.notify_one();
	}

	void CM_TEST::InjectInterruptChannel(const Bytes& bytes)
	{
		auto m = std::make_shared<Message>(HostReport());
		m->setStatus(Message::Status::INTERRUPT);
		m->AddBuffer(bytes);   // before the message is registered, so the parser can't see it yet
		m_msgRegistry.addMessage(m->getMessageHandle(), m);
		m_rxcond.notify_one();
	}

	void CM_TEST::HoldTransmit(bool hold)
	{
		m_hold = hold;
		m_txcond.notify_all();
	}

	void CM_TEST::FailNextSends(int n)
	{
		m_failSends = n;
	}

	void CM_TEST::FailSendsWhere(std::function<bool(const Bytes& request)> predicate)
	{
		std::scoped_lock lk(m_failMutex);
		m_failWhere = std::move(predicate);
	}

	void CM_TEST::UseProtocolV2(std::uint16_t maxInflight, std::uint16_t maxPayload)
	{
		ProtocolV2::ProtocolInfo info;
		info.valid = true;
		info.pverMax = ProtocolV2::Version;
		info.maxInflight = maxInflight;
		info.maxPayload = maxPayload;
		UseProtocolV2(info);
	}

	void CM_TEST::UseProtocolV2(const ProtocolV2::ProtocolInfo& info)
	{
		EnableProtocolV2(info);
	}

	void CM_TEST::SetSupportsProtocolV2(bool supported)
	{
		m_supportsV2 = supported;
	}

	Bytes CM_TEST::DefaultDevice(const Bytes& request)
	{
		const Request r = ParseRequest(request);
		if (!r.valid) return {};

		const auto key = std::make_tuple(r.space, r.action, r.addr);
		std::scoped_lock lk(m_memMutex);
		if (r.read) {
			Bytes data = m_memory[key];
			data.resize(r.readLen, 0);
			return OkResponse(request, data);
		}
		m_memory[key] = r.payload;
		return OkResponse(request);
	}

	void CM_TEST::ClearDeviceMemory()
	{
		std::scoped_lock lk(m_memMutex);
		m_memory.clear();
	}

	std::vector<Bytes> CM_TEST::SentFrames() const
	{
		std::scoped_lock lk(m_sentMutex);
		return m_sent;
	}

	std::size_t CM_TEST::SentCount() const
	{
		std::scoped_lock lk(m_sentMutex);
		return m_sent.size();
	}

	void CM_TEST::ClearSentFrames()
	{
		std::scoped_lock lk(m_sentMutex);
		m_sent.clear();
	}

	bool CM_TEST::WaitForSent(std::size_t n, std::chrono::milliseconds timeout)
	{
		std::unique_lock lk(m_sentMutex);
		return m_sentCond.wait_for(lk, timeout, [&] { return m_sent.size() >= n; });
	}

	std::shared_ptr<Message> CM_TEST::FindMessage(MessageHandle h) const
	{
		return m_msgRegistry.findMessage(h);
	}

	Message::Status CM_TEST::WaitForStatus(MessageHandle h, std::chrono::milliseconds timeout)
	{
		auto m = FindMessage(h);
		if (!m) return Message::Status::CANCELLED;
		m->waitForCompletion(timeout);
		return m->getStatus();
	}

	Bytes CM_TEST::MakeFrame(ReportTypes id, std::uint8_t hdr, std::uint8_t context, std::uint16_t addr, const Bytes& payload)
	{
		const auto len = static_cast<std::uint16_t>(payload.size());
		Bytes f{ static_cast<std::uint8_t>(id), hdr, context,
			static_cast<std::uint8_t>(len & 0xFF), static_cast<std::uint8_t>(len >> 8),
			static_cast<std::uint8_t>(addr & 0xFF), static_cast<std::uint8_t>(addr >> 8) };
		f.insert(f.end(), payload.begin(), payload.end());

		std::queue<std::uint8_t> q;
		for (auto b : f) q.push(b);
		CRCGenerator crc(q);
		f.push_back(static_cast<std::uint8_t>(crc.CRC() & 0xFF));
		f.push_back(static_cast<std::uint8_t>(crc.CRC() >> 8));
		return f;
	}

	CM_TEST::Request CM_TEST::ParseRequest(const Bytes& request)
	{
		Request r;
		if (IsV2Frame(request)) {
			if (ProtocolV2::DecodeWireFrame(request, r.hdr, r.payload) != ProtocolV2::DecodeResult::OK) return r;
			r.valid = r.hdr.kind() == ProtocolV2::Kind::REQUEST;
			r.v2 = true;
			r.space = static_cast<std::uint8_t>(0x80 | static_cast<std::uint8_t>(r.hdr.target()));
			r.action = r.hdr.cmd;
			r.read = (r.hdr.flags & ProtocolV2::FlagRead) != 0;
			r.addr = r.hdr.addr;
			r.readLen = r.read ? r.hdr.rdlen : 0;
			return r;
		}

		if (request.size() < 9) return r;
		const std::uint8_t hdr = request[1];
		r.valid = true;
		r.space = request[0];
		r.action = static_cast<std::uint8_t>(hdr & HDR_ACTION_MASK);
		r.read = (hdr & HDR_READ) != 0;
		r.addr = Get16(request, 5);
		if (r.read) r.readLen = Get16(request, 3);
		else r.payload.assign(request.begin() + 7, request.end() - 2);
		return r;
	}

	Bytes CM_TEST::OkResponse(const Bytes& request, const Bytes& payload)
	{
		if (IsV2Frame(request)) return V2Response(request, ProtocolV2::Status::OK, payload);
		return MakeFrame(ResponseIdFor(request[0]),
			static_cast<std::uint8_t>(HDR_DATA_OK | (request[1] & HDR_ACTION_MASK)),
			request[2], Get16(request, 5), payload);
	}

	Bytes CM_TEST::ErrorResponse(const Bytes& request)
	{
		if (IsV2Frame(request)) return V2Response(request, ProtocolV2::Status::GENERAL_ERROR);
		return MakeFrame(ResponseIdFor(request[0]),
			static_cast<std::uint8_t>(HDR_GENERAL_ERROR | (request[1] & HDR_ACTION_MASK)),
			request[2], Get16(request, 5));
	}

	Bytes CM_TEST::V2Response(const Bytes& request, ProtocolV2::Status status, const Bytes& payload, std::uint8_t flags)
	{
		ProtocolV2::Header h;
		Bytes ignored;
		if (ProtocolV2::DecodeWireFrame(request, h, ignored) != ProtocolV2::DecodeResult::OK) return {};
		h.type = ProtocolV2::Header::MakeType(ProtocolV2::Kind::RESPONSE, h.target());
		h.status = static_cast<std::uint8_t>(status);
		h.flags = static_cast<std::uint8_t>((h.flags & ProtocolV2::FlagRead) | flags);
		h.rdlen = 0;
		return ProtocolV2::BuildFrame(h, payload);
	}

	Bytes CM_TEST::Corrupt(const Bytes& frame)
	{
		Bytes bad = frame;
		if (IsV2Frame(frame) && frame.size() > 2) {
			// The last encoded byte before the closing delimiter; never made zero, so the framing
			//	is kept and only the CRC (or COBS) check fails
			std::uint8_t& b = bad[bad.size() - 2];
			b = (b == 0xFF) ? 0x01 : static_cast<std::uint8_t>(b + 1);
		}
		else if (!bad.empty()) {
			bad.back() ^= 0xFF;
		}
		return bad;
	}

	Bytes CM_TEST::InterruptFrame(std::uint16_t addr, const Bytes& payload)
	{
		return MakeFrame(ReportTypes::INTERRUPT_REPORT_ID_CTRLR, HDR_DATA_OK, 0, addr, payload);
	}

	Bytes CM_TEST::InterruptFrameV2(std::uint16_t seq, std::uint16_t addr, const Bytes& payload)
	{
		ProtocolV2::Header h;
		h.type = ProtocolV2::Header::MakeType(ProtocolV2::Kind::INTERRUPT, ProtocolV2::Target::CONTROLLER);
		h.seq = seq;
		h.cmd = ProtocolV2::InterruptCmd;
		h.addr = addr;
		return ProtocolV2::BuildFrame(h, payload);
	}

	void CM_TEST::MessageSender()
	{
		while (DeviceIsOpen)
		{
			std::shared_ptr<Message> m;
			{
				std::unique_lock<std::mutex> lck{ m_txmutex };
				m_txcond.wait(lck, [&] { return !DeviceIsOpen || (!m_queue.empty() && !m_hold); });
				if (!DeviceIsOpen) break;
				m = m_queue.front();
				m_queue.pop();
			}

			const Bytes request = m->SerialStream();

			bool fail = false;
			if (m_failSends > 0) {
				m_failSends--;
				fail = true;
			}
			else {
				std::scoped_lock lk(m_failMutex);
				fail = m_failWhere && m_failWhere(request);
			}
			if (fail) {
				m->setStatus(Message::Status::SEND_ERROR);
				mMsgEvent.Trigger<int>(this, MessageEvents::SEND_ERROR, m->getMessageHandle());
				continue;
			}

			m->setStatus(Message::Status::SENT);

			// Called in place, not copied, so a responder that keeps state (a counter, a held
			//	reply) sees every request.  The lock also stops SetResponder swapping it mid-call.
			Bytes reply;
			{
				std::scoped_lock lk(m_responderMutex);
				reply = m_responder ? m_responder(request) : DefaultDevice(request);
			}

			// Recorded after the responder has run, so a test that waits for a frame to be
			//	sent also sees any state its responder set up for that frame
			{
				std::scoped_lock lk(m_sentMutex);
				m_sent.push_back(request);
			}
			m_sentCond.notify_all();

			if (!reply.empty()) InjectRx(reply);

			AwaitCompletionIfSerialised(m);
		}
	}

	void CM_TEST::ResponseReceiver()
	{
		// Device bytes arrive through InjectRx(); no receiver thread is needed
	}

}
