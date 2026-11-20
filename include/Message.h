/*-----------------------------------------------------------------------------
/ Title      : Messaging Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Messaging/h/Message.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2017-11-15 13:31:25 +0000 (Wed, 15 Nov 2017) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 314 $
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

#ifndef IMS_MESSAGE_H__
#define IMS_MESSAGE_H__

#include "DeviceReport.h"
#include "HostReport.h"

#include <chrono>
#include <queue>
#include <condition_variable>
#include <shared_mutex>
#include <atomic>

namespace iMS
{
	typedef int MessageHandle;
	const MessageHandle NullMessage = -1;

	class Message
	{
	public:
		Message(HostReport const& Rpt);
		~Message();

		//bool operator == (const Message m);

		// Don't forget to update strings in .cpp
		enum class Status { UNSENT, SENT, SEND_ERROR, TIMEOUT_ON_SEND, RX_PARTIAL, INTERRUPT, RX_OK, CANCELLED, TIMEOUT_ON_RXCV, RX_ERROR_VALID, RX_ERROR_INVALID, PROCESSED_INTERRUPT };

		void setStatus(const Status s);
		Status getStatus() const;
		std::string getStatusText() const;
		MessageHandle getMessageHandle() const;
        bool isComplete() const;
		std::chrono::milliseconds TimeElapsed() const;
		std::chrono::milliseconds MsgDuration() const;
		std::chrono::milliseconds RxTimeSince(std::chrono::time_point<std::chrono::high_resolution_clock>& t) const;
        bool waitForCompletion(std::chrono::milliseconds timeout);
        void waitForCompletion();

		// Mirror Host & Device Report methods.  SerialStream() returns the bytes to transmit: the
		// wire frame set by SetWireStream() if any (protocol v2), otherwise the v1 serialised report.
		const std::vector<std::uint8_t>& SerialStream();
		void SetWireStream(std::vector<std::uint8_t> wire);

		// Protocol v2 sequence number, or -1 for a v1 message
		void setSeq(int seq);
		int getSeq() const;

		// Protocol v2: decoded description and raw wire bytes of the response as received, for
		//	trace logging (set and read by the connection's receive thread)
		void setRxTrace(std::string desc, std::vector<std::uint8_t> wire);
		const std::string& getRxTrace() const;
		const std::vector<std::uint8_t>& getRxWire() const;

		// The request payload (as given by the caller, not truncated)
		std::vector<std::uint8_t> RequestPayload() const;
		void Parse(const std::uint8_t rxchar);
		char Parse();
		void ResetParser();

		// Allow Connection Manager to commit a buffer of unparsed bytes to the message
		// (Only used by CMs that can uniquely identify received chars as belonging to this message)
		void AddBuffer(const std::vector<std::uint8_t>& buf);
		bool HasData() const;

		// Return the Received Report to allow user to access data
		DeviceReport Response() const;

		// Header fields of the request (ID, hdr, len, addr), fixed at construction.  Used to check
		// that a received response belongs to this message.
		const ReportFields& RequestFields() const;

	private:
		static const char * StatusEnumStrings[] ;

		void markSendTime();
		void markRecdTime();

		//Message(const Message&);  // Prevent copying
		HostReport m_rpt;
		ReportFields m_reqFields;
		DeviceReport m_resp;
		MessageHandle m_id;
		static std::atomic<MessageHandle> mIDCount;
		std::atomic<Status> m_status{Status::UNSENT};
		std::chrono::time_point<std::chrono::high_resolution_clock> m_tm_sent;
		std::chrono::time_point<std::chrono::high_resolution_clock> m_tm_recd;
		std::deque<std::uint8_t> unparsed_buf;
		std::vector<std::uint8_t> m_wire;
		std::atomic<int> m_seq{ -1 };
		std::string m_rxTrace;
		std::vector<std::uint8_t> m_rxWire;

        // Synchronisation
        mutable std::shared_mutex m_waitMutex;
		mutable std::mutex m_parseMutex;
		std::condition_variable_any m_cv;
	};

}

#endif
