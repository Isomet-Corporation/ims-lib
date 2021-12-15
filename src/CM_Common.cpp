/*-----------------------------------------------------------------------------
/ Title      : Connection Manager Common Functions CPP
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/src/CM_Common.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2021-12-15 10:49:10 +0000 (Wed, 15 Dec 2021) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 516 $
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

#include "IConnectionManager.h"
#include "CM_Common.h"
#include "PrivateUtil.h"  // for logging

#include <iostream>
#include <iomanip>

//#define DEBUG_PRESERVE_LIST

namespace iMS {

	void DebugLogReportTrace(std::shared_ptr<Message> msg)
	{
		std::stringstream ss;

		if ((msg->getStatus() != Message::Status::INTERRUPT) && (msg->getStatus() != Message::Status::PROCESSED_INTERRUPT)) {
			ss << "H->D: ";
			for (auto&& c : msg->SerialStream())
			{
				ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(c) << " ";
			}
			BOOST_LOG_SEV(lg::get(), sev::trace) << ss.str();
			ss.clear(); ss.str("");
		}

		ss << "D->H: " << *msg->Response();
		BOOST_LOG_SEV(lg::get(), sev::trace) << ss.str();
	}

	CM_Common::CM_Common()
	{
		//BOOST_LOG_SEV(lg::get(), sev::info) << "CM_Common Default Constructor";
	}

	void CM_Common::MessageEventSubscribe(const int message, IEventHandler* handler)
	{
		mMsgEvent.Subscribe(message, handler);
	}

	void CM_Common::MessageEventUnsubscribe(const int message, const IEventHandler* handler)
	{
		mMsgEvent.Unsubscribe(message, handler);
	}

	const bool& CM_Common::Open() const
	{
		return DeviceIsOpen;
	}

	MessageHandle CM_Common::SendMsg(HostReport const& Rpt)
	{
		if (DeviceIsOpen)
		{
			// Create a message, add report, assign it an ID, and place in queue.
			std::shared_ptr<Message> m = std::make_shared<Message>(Rpt);
			{
				std::unique_lock <std::mutex> txlck{ m_txmutex };
				m_queue.push(m);
				txlck.unlock();

				// Notify Worker
				m_txcond.notify_one();
			}
			return m->getMessageHandle();
		}
		else {
			return NullMessage;
		}
	}

	DeviceReport CM_Common::SendMsgBlocking(HostReport const& Rpt)
	{
		DeviceReport Resp;
		MessageHandle handle = this->SendMsg(Rpt);
		while (1) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			Resp = this->Response(handle);
			if (Resp.Done()) break;
			/*if (Resp.Fields().ID == ReportTypes::NULL_REPORT) {
				return DeviceReport();
			}*/
			//elapsed_tm = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tm_start);
			std::unique_lock<std::mutex> list_lck{ m_listmutex };
			for (std::list<std::shared_ptr<Message>>::const_iterator it = m_list.begin(); it != m_list.end(); ++it)
			{
				if ((*it)->getMessageHandle() == handle)
				{
					if (((*it)->getStatus() != Message::Status::UNSENT) &&
						((*it)->getStatus() != Message::Status::SENT) &&
						((*it)->getStatus() != Message::Status::RX_PARTIAL) &&
						((*it)->getStatus() != Message::Status::RX_OK) &&
						((*it)->getStatus() != Message::Status::RX_ERROR_VALID))
					{
						return Resp;
					}
				}
			}
			list_lck.unlock();
		}
		return Resp;
	}

	template <typename T, typename T2 = int>
	struct triggerEvents
	{
		MessageEvents::Events e;
		T p;
		T2 p2;
		int count;
		triggerEvents(MessageEvents::Events e, T p) : e(e), p(p), p2(0), count(1) {};
		triggerEvents(MessageEvents::Events e, T p, T2 p2) : e(e), p(p), p2(p2), count(2) {};
	};

	void CM_Common::MessageListManager()
	{
		// Record any trigger events that occur during processing
		// If we trigger while still processing, the list mutex is still locked and
		// any callback code trying to access the mutex will deadlock.
		// So record the events and trigger after the mutex is released.
		std::vector<triggerEvents<int>> events;
		//std::vector<std::uint8_t> int_data;
		std::vector<triggerEvents<int, std::vector<std::uint8_t>>> vevents;
		std::stringstream ss;
		std::deque<std::uint8_t> glbl_rx;

		while (DeviceIsOpen == true)
		{
			// Grab any outstanding characters from the queue
			std::unique_lock<std::mutex> lck{ m_rxmutex };

			m_rxcond.wait_for(lck, std::chrono::milliseconds(100));
			while (!m_rxCharQueue.empty()) {
				glbl_rx.push_back(m_rxCharQueue.front());
				m_rxCharQueue.pop();
			}

			lck.unlock();

			// Do some message management

			// Look for first message in list that is waiting for characters
			{
				std::unique_lock<std::mutex> list_lck{ m_listmutex };

				for (std::list<std::shared_ptr<Message>>::iterator it = m_list.begin(); it != m_list.end(); /*++it*/)
				{
					if (((*it)->getStatus() == Message::Status::SENT) ||
						((*it)->getStatus() == Message::Status::RX_PARTIAL) ||
						((*it)->getStatus() == Message::Status::INTERRUPT))
					{
						std::shared_ptr<Message> m = (*it);
						// Do Not add character data to both global queue (rxCharQueue) and message queue
						// Use one or the other, depending on capabilities of Connection Target
						while (!glbl_rx.empty() || m->HasData())
						{
							char c = ' ';
							// Parse a character(or buffer), if the current message is expecting more data
							if (!m->Response()->Done())
							{
								if (m->HasData()) {
									c = m->Parse();
								}
								else if (!glbl_rx.empty()) {
									// Retrieve the next character from the HAL queue
									c = glbl_rx.front();
									glbl_rx.pop_front();
									m->Parse(c);
								}
								else {
									m->setStatus(Message::Status::RX_ERROR_INVALID);
									ss.clear(); ss.str("");
									ss << "Msg Invalid (" << (*it)->getMessageHandle() << "): [" << (*it)->getStatusText() << "] ";
									BOOST_LOG_SEV(lg::get(), sev::error) << ss.str();
									DebugLogReportTrace(m);
									//std::cout << "Message Invalid: " << (*it)->getMessageHandle() << std::endl;
									break;
								}
								if (m->getStatus() == Message::Status::SENT) m->setStatus(Message::Status::RX_PARTIAL);
							}
							else {
								m->setStatus(Message::Status::RX_ERROR_INVALID);
								ss.clear(); ss.str("");
								ss << "Msg Invalid (" << (*it)->getMessageHandle() << "): [" << (*it)->getStatusText() << "] ";
								BOOST_LOG_SEV(lg::get(), sev::error) << ss.str();
								DebugLogReportTrace(m);
								//std::cout << "Message Invalid: " << (*it)->getMessageHandle() << std::endl;
								break;
							}
							// Handle parsing errors
							if (m->Response()->UnexpectedChar())
							{
								//mMsgEvent.Trigger<int>(this, MessageEvents::UNEXPECTED_RX_CHAR, static_cast<int>(c));
								events.push_back(triggerEvents<int>(MessageEvents::UNEXPECTED_RX_CHAR, static_cast<int>(c)));
								//std::cout << "Message Unexpected Char: " << (*it)->getMessageHandle() << std::endl;
								ss.clear(); ss.str("");
								ss << "Unexpected Char (" << (*it)->getMessageHandle() << "): [" << (*it)->getStatusText() << "] 0x" << std::hex << c << std::dec;
								BOOST_LOG_SEV(lg::get(), sev::warning) << ss.str();
								DebugLogReportTrace(m);
								break;
							}
							if (m->Response()->Done())
							{
								m->MarkRecdTime();
								// Look for any problems in the message transfer
								if (m->Response()->HardwareAlarm())
								{
									//									mMsgEvent.Trigger<int>(this, MessageEvents::INTERLOCK_ALARM_SET, m->getMessageHandle());
									events.push_back(triggerEvents<int>(MessageEvents::INTERLOCK_ALARM_SET, m->getMessageHandle()));
									//std::cout << "Message Rx with Interlock Alarm: " << (*it)->getMessageHandle() << std::endl;
									ss.clear(); ss.str("");
									ss << "Msg (" << (*it)->getMessageHandle() << "): >>> INTERLOCK ALARM <<< " << m->MsgDuration().count() << "ms";
									BOOST_LOG_SEV(lg::get(), sev::warning) << ss.str();
									//DebugLogReportTrace(m);
								}

								if ((m->Response()->GeneralError()) || (m->Response()->TxTimeout()) || (m->Response()->TxCRC()))
								{
									m->setStatus(Message::Status::RX_ERROR_VALID);
									//									mMsgEvent.Trigger<int>(this, MessageEvents::RESPONSE_ERROR_VALID, m->getMessageHandle());
									events.push_back(triggerEvents<int>(MessageEvents::RESPONSE_ERROR_VALID, m->getMessageHandle()));
									//std::cout << "Message Valid but Error: " << (*it)->getMessageHandle() << std::endl;
									ss.clear(); ss.str("");
									ss << "Msg (" << (*it)->getMessageHandle() << "): [" << (*it)->getStatusText() << "] " << m->MsgDuration().count() << "ms";
									BOOST_LOG_SEV(lg::get(), sev::warning) << ss.str();
									DebugLogReportTrace(m);
								}
								else if (m->Response()->RxCRC())
								{
									m->setStatus(Message::Status::RX_ERROR_INVALID);
									//									mMsgEvent.Trigger<int>(this, MessageEvents::RESPONSE_ERROR_CRC, m->getMessageHandle());
									events.push_back(triggerEvents<int>(MessageEvents::RESPONSE_ERROR_CRC, m->getMessageHandle()));
									//std::cout << "Message CRC error: " << (*it)->getMessageHandle() << std::endl;
									ss.clear(); ss.str("");
									ss << "Msg (" << (*it)->getMessageHandle() << "): [" << (*it)->getStatusText() << "] " << m->MsgDuration().count() << "ms";
									BOOST_LOG_SEV(lg::get(), sev::error) << ss.str();
									DebugLogReportTrace(m);
								}
								else if (m->getStatus() == Message::Status::INTERRUPT)
								{
									m->setStatus(Message::Status::PROCESSED_INTERRUPT);
									// Integer parameter for interrupt divided into two 16-bit fields: interrupt type and data.
									unsigned int param = (static_cast<unsigned int>(m->Response()->Fields().addr) << 16);
									unsigned int param2 = 0;
									//int_data.clear();
									if (m->Response()->Fields().len > 4) {
										//int_data = m->Response()->Payload<std::vector<std::uint8_t>>();
										vevents.push_back(triggerEvents<int, std::vector<std::uint8_t>>(MessageEvents::INTERRUPT_RECEIVED, param, m->Response()->Payload<std::vector<std::uint8_t>>()));
									}
									else {
										if (m->Response()->Fields().len >= 2) {
											param |= (m->Response()->Payload<std::vector<std::uint16_t>>().at(0));
										}
										if (m->Response()->Fields().len >= 4) {
											param2 = (m->Response()->Payload<std::vector<std::uint16_t>>().at(1));
											events.push_back(triggerEvents<int, int>(MessageEvents::INTERRUPT_RECEIVED, param, param2));
										}
										else {
											events.push_back(triggerEvents<int>(MessageEvents::INTERRUPT_RECEIVED, param));
										}
									}
									ss.clear(); ss.str("");
									ss << "Processed Interrupt (" << (*it)->getMessageHandle() << "): [" << (*it)->getStatusText() << "] p0:" << param << " p1:" << param2;
									BOOST_LOG_SEV(lg::get(), sev::info) << ss.str();
									DebugLogReportTrace(m);
								}
								else
								{
									m->setStatus(Message::Status::RX_OK);
									//									mMsgEvent.Trigger<int>(this, MessageEvents::RESPONSE_RECEIVED, m->getMessageHandle());
									events.push_back(triggerEvents<int>(MessageEvents::RESPONSE_RECEIVED, m->getMessageHandle()));
									ss.clear(); ss.str("");
									ss << "Msg (" << (*it)->getMessageHandle() << "): [" << (*it)->getStatusText() << "] " << m->MsgDuration().count() << "ms";
									BOOST_LOG_SEV(lg::get(), sev::info) << ss.str();
									DebugLogReportTrace(m);
									//std::cout << "Response received. Took " << std::dec << m->MsgDuration().count() << "ms" << std::endl;
								}
								break;
							}
						}
						// No more characters, exit message iterator
						//if (m_rxCharQueue.empty()) break;
					}
					it++;
					if ((it == m_list.end()) && !glbl_rx.empty()) {
						//						while (1);
						int a = 1;
					}
				}
				list_lck.unlock();

				// Trigger all the events that were initiated while we still consumed the lock
				for (std::vector<triggerEvents<int>>::iterator ev_it = events.begin(); ev_it != events.end(); ++ev_it)
				{
					if (ev_it->count > 1) {
						mMsgEvent.Trigger<int, int>(this, ev_it->e, ev_it->p, ev_it->p2);
					}
					else {
						mMsgEvent.Trigger<int>(this, ev_it->e, ev_it->p);
					}
				}
				events.clear();
				for (std::vector<triggerEvents<int, std::vector<std::uint8_t>>>::iterator ev_it = vevents.begin(); ev_it != vevents.end(); ++ev_it)
				{
					if (ev_it->count > 1) {
						mMsgEvent.Trigger<int, std::vector<std::uint8_t>>(this, MessageEvents::INTERRUPT_RECEIVED, ev_it->p, ev_it->p2);
					}
				}
				vevents.clear();
			}

			// Look for messages in the list that have timed out
			{
				std::unique_lock<std::mutex> list_lck{ m_listmutex };

				for (std::list<std::shared_ptr<Message>>::iterator it = m_list.begin(); it != m_list.end();)
				{
					if ((*it)->HasData()) {
						//localDataAvailable = true;
						//break;
					}
					if (((*it)->getStatus() == Message::Status::SENT) ||
						((*it)->getStatus() == Message::Status::RX_PARTIAL))
					{
						if ((*it)->TimeElapsed() > rxTimeout)
						{
							//std::cout << "Message Timeout: " << (*it)->getMessageHandle() << std::endl;
							(*it)->setStatus(Message::Status::TIMEOUT_ON_RXCV);
							//mMsgEvent.Trigger<int>(this, MessageEvents::RESPONSE_TIMED_OUT, (*it)->getMessageHandle());
							events.push_back(triggerEvents<int>(MessageEvents::RESPONSE_TIMED_OUT, (*it)->getMessageHandle()));
							//std::cout << "Message Timeout: " << (*it)->getMessageHandle() << std::endl;
							ss.clear(); ss.str("");
							ss << "Msg RX Timeout (" << (*it)->getMessageHandle() << "): [" << (*it)->getStatusText() << "] ";
							BOOST_LOG_SEV(lg::get(), sev::warning) << ss.str();
							DebugLogReportTrace(*it);
						}
						++it;
					}
					// Then look for stale messages to expire from the list
					else {
#ifndef DEBUG_PRESERVE_LIST
						if ((*it)->TimeElapsed() > autoFreeTimeout)
						{
							if ((((*it)->getStatus() >= Message::Status::RX_OK) &&
								((*it)->getStatus() <= Message::Status::RX_ERROR_INVALID)) ||
								((*it)->getStatus() == Message::Status::PROCESSED_INTERRUPT)) {
								//std::cout << "Message Deleted: " << (*it)->getMessageHandle() << std::endl;
								it = m_list.erase(it);  // Message handle deleted
							}
						}
						else
#endif
						{
							++it;
						}
					}
				}
				list_lck.unlock();

				// Trigger all the events that were initiated while we still consumed the lock
				for (std::vector<triggerEvents<int>>::const_iterator it = events.begin(); it != events.end(); ++it)
				{
					mMsgEvent.Trigger<int>(this, (*it).e, (*it).p);
				}
				events.clear();
			}

		}
	}

	const DeviceReport CM_Common::Response(const MessageHandle h) const
	{
		{
			std::unique_lock<std::mutex> list_lck{ m_listmutex };
			for (std::list<std::shared_ptr<Message>>::const_iterator it = m_list.begin(); it != m_list.end(); ++it)
			{
				if ((*it)->getMessageHandle() == h)
				{
					return *((*it)->Response());
					break;
				}
			}
			list_lck.unlock();
		}
		return DeviceReport();
	}

	// Default methods for those interfaces that do not have a memory download option
	bool CM_Common::MemoryDownload(boost::container::deque<std::uint8_t>& arr, std::uint32_t start_addr, int image_index, const std::array<std::uint8_t, 16>& uuid) {
		mMsgEvent.Trigger<int>(this, MessageEvents::NO_FAST_MEMORY_INTERFACE, -1);
		return false;
	}

	bool CM_Common::MemoryUpload(boost::container::deque<std::uint8_t>& arr, std::uint32_t start_addr, int len, int image_index, const std::array<std::uint8_t, 16>& uuid) {
		mMsgEvent.Trigger<int>(this, MessageEvents::NO_FAST_MEMORY_INTERFACE, -1);
		return false;
	}

	int CM_Common::MemoryProgress() {
		mMsgEvent.Trigger<int>(this, MessageEvents::NO_FAST_MEMORY_INTERFACE, -1);
		return -1;
	}

}
