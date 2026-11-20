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

	Bytes CM_TEST::DefaultDevice(const Bytes& request)
	{
		if (request.size() < 9) return {};

		const std::uint8_t id = request[0];
		const std::uint8_t hdr = request[1];
		const std::uint16_t len = Get16(request, 3);
		const std::uint16_t addr = Get16(request, 5);
		const auto key = std::make_tuple(id, static_cast<std::uint8_t>(hdr & HDR_ACTION_MASK), addr);

		std::scoped_lock lk(m_memMutex);
		if (hdr & HDR_READ) {
			Bytes data = m_memory[key];
			data.resize(len, 0);
			return OkResponse(request, data);
		}
		m_memory[key] = Bytes(request.begin() + 7, request.end() - 2);
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

	Bytes CM_TEST::OkResponse(const Bytes& request, const Bytes& payload)
	{
		return MakeFrame(ResponseIdFor(request[0]),
			static_cast<std::uint8_t>(HDR_DATA_OK | (request[1] & HDR_ACTION_MASK)),
			request[2], Get16(request, 5), payload);
	}

	Bytes CM_TEST::ErrorResponse(const Bytes& request)
	{
		return MakeFrame(ResponseIdFor(request[0]),
			static_cast<std::uint8_t>(HDR_GENERAL_ERROR | (request[1] & HDR_ACTION_MASK)),
			request[2], Get16(request, 5));
	}

	Bytes CM_TEST::InterruptFrame(std::uint16_t addr, const Bytes& payload)
	{
		return MakeFrame(ReportTypes::INTERRUPT_REPORT_ID_CTRLR, HDR_DATA_OK, 0, addr, payload);
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

			Responder responder;
			{
				std::scoped_lock lk(m_responderMutex);
				responder = m_responder;
			}
			const Bytes reply = responder ? responder(request) : DefaultDevice(request);

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
