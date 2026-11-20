/*-----------------------------------------------------------------------------
/ Title      : I/O Messages Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Messaging/src/Message.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2020-07-30 21:50:24 +0100 (Thu, 30 Jul 2020) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 465 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2015 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2015-04-09  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

#include "Message.h"

#if defined _WIN32 && defined _DEBUG
#include "crtdbg.h"
#define DEBUG_NEW new( _NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace iMS
{

	Message::Message(HostReport const& Rpt)
	{
		// Copy report into local object
		this->m_rpt = Rpt;
		this->m_reqFields = Rpt.Fields();

		// Unique ID integer for each new instance
		m_id = mIDCount++;

		m_status.store(Status::UNSENT, std::memory_order_release);
	}

	Message::~Message()
	{
	}

	const char * Message::StatusEnumStrings[] = {
		"UNSENT",
		"SENT",
		"SEND_ERROR",
		"TIMEOUT_ON_SEND",
		"RX_PARTIAL",
		"INTERRUPT",
		"RX_OK",
		"CANCELLED",
		"TIMEOUT_ON_RXCV",
		"RX_ERROR_VALID",
		"RX_ERROR_INVALID",
		"PROCESSED_INTERRUPT"
	};

	// Initialise ID Counter
	std::atomic<MessageHandle> Message::mIDCount{ 1 };
	
	void Message::markSendTime()
	{
		m_tm_sent = std::chrono::high_resolution_clock::now();
	}

	void Message::markRecdTime()
	{
		m_tm_recd = std::chrono::high_resolution_clock::now();
	}

	static bool isTerminal(Message::Status s)
	{
		return s != Message::Status::UNSENT &&
			s != Message::Status::SENT &&
			s != Message::Status::RX_PARTIAL &&
			s != Message::Status::INTERRUPT;
	}

	void Message::setStatus(const Message::Status s)
	{
        if ((m_status.load(std::memory_order_acquire) == Status::UNSENT) && (s > Status::UNSENT)) {
            markSendTime();
        }
        bool c = isComplete();
		m_status.store(s, std::memory_order_release);
        if (!c && isComplete()) {
            markRecdTime();
            // Briefly take the wait mutex so that a waiter which has just evaluated the
            // predicate (and found it false) is guaranteed to be blocked in wait() before
            // we notify.  Without this, the notification can be lost.
            { std::unique_lock<std::shared_mutex> lk(m_waitMutex); }
            m_cv.notify_all();
        }
	}

	Message::Status Message::getStatus() const
	{
        return m_status.load(std::memory_order_acquire);
	}

    bool Message::isComplete() const {
        auto s = this->getStatus();
        return isTerminal(s);
    }

    // Wait until status changes to RX_OK or timeout
    bool Message::waitForCompletion(std::chrono::milliseconds timeout) {
        std::shared_lock lock(m_waitMutex);
        return m_cv.wait_for(lock, timeout, [&]{ 
			return isTerminal(m_status.load(std::memory_order_acquire));
        });
    }

    void Message::waitForCompletion() {
        std::shared_lock lock(m_waitMutex);
        m_cv.wait(lock, [&]{ 
			return isTerminal(m_status.load(std::memory_order_acquire));
        });
    }

	std::string Message::getStatusText() const
	{
		return std::string(Message::StatusEnumStrings[(int)getStatus()]);
	}

	MessageHandle Message::getMessageHandle() const
	{
		return m_id;
	}

	std::chrono::milliseconds Message::MsgDuration() const
	{
		return (std::chrono::duration_cast<std::chrono::milliseconds>(m_tm_recd - m_tm_sent));
	}

	std::chrono::milliseconds Message::TimeElapsed() const
	{
		std::chrono::time_point<std::chrono::high_resolution_clock> m_tm_now = std::chrono::high_resolution_clock::now();
		return (std::chrono::duration_cast<std::chrono::milliseconds>(m_tm_now - m_tm_sent));
	}

	std::chrono::milliseconds Message::RxTimeSince(std::chrono::time_point<std::chrono::high_resolution_clock>& t) const
	{
		return (std::chrono::duration_cast<std::chrono::milliseconds>(m_tm_recd - t));
	}

	const std::vector<std::uint8_t>& Message::SerialStream()
	{
		if (!m_wire.empty()) return m_wire;
		return m_rpt.SerialStream();
	}

	// Must be called before the message is queued for transmission
	void Message::SetWireStream(std::vector<std::uint8_t> wire)
	{
		m_wire = std::move(wire);
	}

	void Message::setSeq(int seq)
	{
		m_seq.store(seq);
	}

	int Message::getSeq() const
	{
		return m_seq.load();
	}

	std::vector<std::uint8_t> Message::RequestPayload() const
	{
		return m_rpt.Payload<std::vector<std::uint8_t>>();
	}

	void Message::Parse(const std::uint8_t rxchar)
	{
        {
            std::unique_lock<std::mutex> lock(m_parseMutex);
            m_resp.Parse(rxchar);
        }
	}

	char Message::Parse()
	{
		char c = ' ';
		while (!unparsed_buf.empty())
		{
			c = unparsed_buf.front();
			unparsed_buf.pop_front();
			m_resp.Parse(c);
		}
		return c;
	}

	void Message::ResetParser()
	{
		m_resp.ResetParser();
	}

	DeviceReport Message::Response() const
	{
        std::unique_lock<std::mutex> lock(m_parseMutex);
		return m_resp;
	}

    // AddBuffer is not synchronised so it must be called from the same thread that does the parsing
	void Message::AddBuffer(const std::vector<std::uint8_t>& buf)
	{
		// Add the input buffer to the internal queue
		for (std::vector<std::uint8_t>::const_iterator it = buf.begin(); it != buf.end(); ++it)
		{
			unparsed_buf.push_back(*it);
		}
	}

	const ReportFields& Message::RequestFields() const
	{
		return m_reqFields;
	}

	bool Message::HasData() const
	{
		return (!unparsed_buf.empty());
	}
}
