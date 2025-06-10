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
		enum class Status { UNSENT, SENT, SEND_ERROR, TIMEOUT_ON_SEND, RX_PARTIAL, RX_OK, CANCELLED, TIMEOUT_ON_RXCV, RX_ERROR_VALID, RX_ERROR_INVALID, INTERRUPT, PROCESSED_INTERRUPT };

		void MarkSendTime();
		void MarkRecdTime();
		void setStatus(const Status s);
		Status getStatus() const;
		std::string getStatusText() const;
		MessageHandle getMessageHandle() const;
		std::chrono::milliseconds TimeElapsed() const;
		std::chrono::milliseconds MsgDuration() const;
		std::chrono::milliseconds RxTimeSince(std::chrono::time_point<std::chrono::high_resolution_clock>& t) const;

		// Mirror Host & Device Report methods
		const std::vector<std::uint8_t>& SerialStream();
		void Parse(const std::uint8_t rxchar);
		char Parse();
		void ResetParser();

		// Allow Connection Manager to commit a buffer of unparsed bytes to the message
		// (Only used by CMs that can uniquely identify received chars as belonging to this message)
		void AddBuffer(const std::vector<std::uint8_t>& buf);
		bool HasData() const;

		// Return a pointer to the Received Report to allow user to access data
		const DeviceReport* Response() const;

	private:
		static const char * StatusEnumStrings[] ;

		//Message(const Message&);  // Prevent copying
		HostReport m_rpt;
		DeviceReport m_resp;
		MessageHandle m_id;
		static MessageHandle mIDCount;
		Status m_status;
		std::chrono::time_point<std::chrono::high_resolution_clock> m_tm_sent;
		std::chrono::time_point<std::chrono::high_resolution_clock> m_tm_recd;
		std::deque<std::uint8_t> unparsed_buf;
	};

}

#endif
