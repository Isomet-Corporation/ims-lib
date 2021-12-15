/*-----------------------------------------------------------------------------
/ Title      : Connection Manager Header - Common Interface
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/h/CM_Common.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2021-09-13 10:43:17 +0100 (Mon, 13 Sep 2021) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 501 $
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

#ifndef IMS_CM_COMMON_H__
#define IMS_CM_COMMON_H__

#include "IConnectionManager.h"

#include <list>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <queue>
#include <array>

namespace iMS {

	// This is another ABC that inherits its interface and adds some data and member functions
	//	that are common to all CM types
	class CM_Common : public IConnectionManager
	{
	public:
		CM_Common();

		void MessageEventSubscribe(const int message, IEventHandler* handler);
		void MessageEventUnsubscribe(const int message, const IEventHandler* handler);
		bool MemoryDownload(boost::container::deque<std::uint8_t>& arr, std::uint32_t start_addr, int image_index, const std::array<std::uint8_t, 16>& uuid);
		bool MemoryUpload(boost::container::deque<std::uint8_t>& arr, std::uint32_t start_addr, int len, int image_index, const std::array<std::uint8_t, 16>& uuid);
		int MemoryProgress();

		// Send an I/O Report
		virtual MessageHandle SendMsg(HostReport const& Rpt);
		virtual DeviceReport SendMsgBlocking(HostReport const& Rpt);

		const DeviceReport Response(const MessageHandle) const;
		const bool& Open() const;

	protected:
		MessageEventTrigger mMsgEvent;

		bool DeviceIsOpen{ false };

		// Message Sending Thread
		std::chrono::milliseconds sendTimeout;
		std::thread senderThread;
		std::queue<std::shared_ptr<Message>> m_queue;
		mutable std::mutex m_txmutex;
		std::condition_variable m_txcond;
		virtual void MessageSender() = 0;

		// Message Receiving Thread
		std::thread receiverThread;
		std::queue<char> m_rxCharQueue;
		std::list<std::shared_ptr<Message>> m_list;
		mutable std::mutex m_rxmutex;
		std::condition_variable m_rxcond;
		virtual void ResponseReceiver() = 0;

		// Message List Manager Thread
		std::thread parserThread;
		std::chrono::milliseconds rxTimeout;
		std::chrono::milliseconds autoFreeTimeout;
		mutable std::mutex m_listmutex;
		virtual void MessageListManager();
	};

	enum class _FastTransferStatus {
		IDLE,
		DOWNLOADING,
		UPLOADING
	};


}
#endif
