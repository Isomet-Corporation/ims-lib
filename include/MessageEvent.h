/*-----------------------------------------------------------------------------
/ Title      : Message Events Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/EventManager/h/MessageEvent.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2017-09-11 23:55:34 +0100 (Mon, 11 Sep 2017) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 300 $
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

#ifndef IMS_MESSAGE_EVENT_H
#define IMS_MESSAGE_EVENT_H

#include "IEventTrigger.h"

namespace iMS {

	class MessageEvents {
	public:
		enum Events {
			DEVICE_NOT_AVAILABLE,
			TIMED_OUT_ON_SEND,
			SEND_ERROR,
			RESPONSE_RECEIVED,
			RESPONSE_TIMED_OUT,
			RESPONSE_ERROR_VALID,
			RESPONSE_ERROR_INVALID,
			RESPONSE_ERROR_CRC,
			INTERLOCK_ALARM_SET,
			UNEXPECTED_RX_CHAR,
			INTERRUPT_RECEIVED,
			NO_FAST_MEMORY_INTERFACE,
			MEMORY_TRANSFER_NOT_IDLE,
			MEMORY_TRANSFER_COMPLETE,
			MEMORY_TRANSFER_ERROR,
			Count
		};
	};

	class MessageEventTrigger :
		public IEventTrigger
	{
	public:
		MessageEventTrigger() { updateCount(MessageEvents::Count); }
		~MessageEventTrigger() {};
	};

}

#endif