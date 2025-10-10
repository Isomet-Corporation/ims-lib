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
#include <cstdint>

//#define DEBUG_PRESERVE_LIST

#define DMA_PERFORMANCE_MEASUREMENT_MODE

#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
#include "boost/chrono.hpp"
#endif

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

            auto hnd = m->getMessageHandle();
            m_msgRegistry.addMessage(hnd, m);

			return hnd;
		}
		else {
			return NullMessage;
		}
	}

	DeviceReport CM_Common::SendMsgBlocking(HostReport const& Rpt)
	{
        if (DeviceIsOpen)
		{
			auto hnd = this->SendMsg(Rpt);
            auto& m = m_msgRegistry.findMessage(hnd);

            m->waitForCompletion();
            DeviceReport Resp = this->Response(hnd);
            return Resp;
		}
		else {
            return DeviceReport();
        }
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

		while (DeviceIsOpen)
		{

			// Grab any outstanding characters from the queue
            std::deque<char> localCopy;
            {
                std::unique_lock<std::mutex> lck{ m_rxmutex };

                m_rxcond.wait_for(lck, std::chrono::milliseconds(10), [&] {
                    return (!m_rxCharQueue.empty() || !DeviceIsOpen);
                });
                if (!m_rxCharQueue.empty()) {
                    std::swap(localCopy, m_rxCharQueue);
                }
                if (!DeviceIsOpen) break;
            }

			// Do some message management
            glbl_rx.insert(glbl_rx.end(),
               std::make_move_iterator(localCopy.begin()),
               std::make_move_iterator(localCopy.end()));
            localCopy.clear();

			// Look for first message in list that is waiting for characters
            m_msgRegistry.forEachMessageMutable([&](std::shared_ptr<Message>& m) {

                if (!m->isComplete())
                {
                    //std::shared_ptr<Message> m = (*it);
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
                                std::stringstream ss;
                                while (!glbl_rx.empty() && !m->Response()->UnexpectedChar() && !m->Response()->Done()) {
                                    // Retrieve the next character from the HAL queue
                                    c = glbl_rx.front();
                                    glbl_rx.pop_front();
                                    ss << std::hex << std::setfill('0') << std::setw(2) << (static_cast<unsigned int>(c)&0xFF) << " ";
                                    m->Parse(c);
                                }
//                                BOOST_LOG_SEV(lg::get(), sev::trace) << "PROC: " << ss.str();
                            }
                            else {
                                m->setStatus(Message::Status::RX_ERROR_INVALID);
                                ss.clear(); ss.str("");
                                ss << "Msg Invalid (" << m->getMessageHandle() << "): [" << m->getStatusText() << "] ";
                                BOOST_LOG_SEV(lg::get(), sev::error) << ss.str();
                                DebugLogReportTrace(m);
                                m_msgRegistry.notifyAll();
                                break;
                            }
                            if (m->getStatus() == Message::Status::SENT) m->setStatus(Message::Status::RX_PARTIAL);
                        }
                        else {
                            m->setStatus(Message::Status::RX_ERROR_INVALID);
                            ss.clear(); ss.str("");
                            ss << "Msg Invalid (" << m->getMessageHandle() << "): [" << m->getStatusText() << "] ";
                            BOOST_LOG_SEV(lg::get(), sev::error) << ss.str();
                            DebugLogReportTrace(m);
                            m_msgRegistry.notifyAll();
                            break;
                        }
                        // Handle parsing errors
                        if (m->Response()->UnexpectedChar())
                        {
                            // Since we received a byte that wasn't an ID byte, we can either reset and start again, or mark the message as failed.
                            m->ResetParser();
                            events.push_back(triggerEvents<int>(MessageEvents::UNEXPECTED_RX_CHAR, static_cast<int>(c)));
                            ss.clear(); ss.str("");
                            ss << "Unexpected Char 0x" << std::hex << std::setfill('0') << std::setw(2) << (static_cast<unsigned int>(c)&0xFF) << std::dec << " (" << m->getMessageHandle() << "): [" << m->getStatusText() << "]";
                            BOOST_LOG_SEV(lg::get(), sev::warning) << ss.str();
                            DebugLogReportTrace(m);
                            m_msgRegistry.notifyAll();
                            break;
                        }
                        if (m->Response()->Done())
                        {
                            // Look for any problems in the message transfer
                            if (m->Response()->HardwareAlarm())
                            {
                                events.push_back(triggerEvents<int>(MessageEvents::INTERLOCK_ALARM_SET, m->getMessageHandle()));
                                ss.clear(); ss.str("");
                                ss << "Msg (" << m->getMessageHandle() << "): >>> INTERLOCK ALARM <<< " << m->MsgDuration().count() << "ms";
                                BOOST_LOG_SEV(lg::get(), sev::warning) << ss.str();
                                //DebugLogReportTrace(m);
                            }

                            if ((m->Response()->GeneralError()) || (m->Response()->TxTimeout()) || (m->Response()->TxCRC()))
                            {
                                m->setStatus(Message::Status::RX_ERROR_VALID);
                                events.push_back(triggerEvents<int>(MessageEvents::RESPONSE_ERROR_VALID, m->getMessageHandle()));
                                ss.clear(); ss.str("");
                                ss << "Msg (" << m->getMessageHandle() << "): [" << m->getStatusText() << "] " << m->MsgDuration().count() << "ms";
                                BOOST_LOG_SEV(lg::get(), sev::warning) << ss.str();
                                DebugLogReportTrace(m);
                            }
                            else if (m->Response()->RxCRC())
                            {
                                m->setStatus(Message::Status::RX_ERROR_INVALID);
                                events.push_back(triggerEvents<int>(MessageEvents::RESPONSE_ERROR_CRC, m->getMessageHandle()));
                                ss.clear(); ss.str("");
                                ss << "Msg (" << m->getMessageHandle() << "): [" << m->getStatusText() << "] " << m->MsgDuration().count() << "ms";
                                BOOST_LOG_SEV(lg::get(), sev::error) << ss.str();
                                DebugLogReportTrace(m);
                            }
                            else if (m->getStatus() == Message::Status::INTERRUPT)
                            {
                                m->setStatus(Message::Status::PROCESSED_INTERRUPT);
                                // Integer parameter for interrupt divided into two 16-bit fields: interrupt type and data.
                                unsigned int param = (static_cast<unsigned int>(m->Response()->Fields().addr) << 16);
                                unsigned int param2 = 0;
                                if (m->Response()->Fields().len > 4) {
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
                                ss << "Processed Interrupt (" << m->getMessageHandle() << "): [" << m->getStatusText() << "] p0:" << param << " p1:" << param2;
                                BOOST_LOG_SEV(lg::get(), sev::info) << ss.str();
                                DebugLogReportTrace(m);
                            }
                            else
                            {
                                m->setStatus(Message::Status::RX_OK);
                                events.push_back(triggerEvents<int>(MessageEvents::RESPONSE_RECEIVED, m->getMessageHandle()));
                                ss.clear(); ss.str("");
                                ss << "Msg (" << m->getMessageHandle() << "): [" << m->getStatusText() << "] " << m->MsgDuration().count() << "ms";
                                BOOST_LOG_SEV(lg::get(), sev::info) << ss.str();
                                DebugLogReportTrace(m);
                            }
                            m_msgRegistry.notifyAll();
                            break;
                        }
                    }
                }
            });

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

			// Look for messages in the list that have timed out
            m_msgRegistry.forEachMessageMutable([&](std::shared_ptr<Message>& m) {
                if ((m->getStatus() == Message::Status::SENT) ||
                    (m->getStatus() == Message::Status::RX_PARTIAL))
                {
                    if (m->TimeElapsed() > rxTimeout)
                    {
                        m->setStatus(Message::Status::TIMEOUT_ON_RXCV);
                        events.push_back(triggerEvents<int>(MessageEvents::RESPONSE_TIMED_OUT, m->getMessageHandle()));
                        ss.clear(); ss.str("");
                        ss << "Msg RX Timeout (" << m->getMessageHandle() << "): [" << m->getStatusText() << "] ";
                        BOOST_LOG_SEV(lg::get(), sev::warning) << ss.str();
                        DebugLogReportTrace(m);
                    }
                }
                // Then look for stale messages to expire from the list
                else {
#ifndef DEBUG_PRESERVE_LIST
                    if (m->TimeElapsed() > autoFreeTimeout)
                    {
                        if (m->isComplete()) {
                            m_msgRegistry.removeMessage(m->getMessageHandle());
                        }
                    }
#endif
                }
            });

            // Trigger all the events that were initiated while we still consumed the lock
            for (std::vector<triggerEvents<int>>::const_iterator it = events.begin(); it != events.end(); ++it)
            {
                mMsgEvent.Trigger<int>(this, (*it).e, (*it).p);
            }
            events.clear();
		}
	}

	const DeviceReport CM_Common::Response(const MessageHandle h) const
	{
        auto& msg = m_msgRegistry.findMessage(h);
        if (nullptr != msg) {
            return *msg->Response();
        }
        return DeviceReport();
	}

    // Default implementations using pipelined SendMsg() calls
	bool CM_Common::MemoryDownload(boost::container::deque<uint8_t>& arr, uint32_t start_addr, int image_index, const std::array<uint8_t, 16>& uuid)
	{
		(void)uuid;
        BOOST_LOG_SEV(lg::get(), sev::trace) << "Starting memory download " << arr.size() << " bytes at address 0x" 
            << std::hex << std::setfill('0') << std::setw(2) << start_addr;

		// Only proceed if idle
		if (FastTransferStatus.load() != _FastTransferStatus::IDLE) {
			mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_NOT_IDLE, -1);
			return false;
		}
		// DMA Cannot accept addresses that aren't aligned to 64 bits
		if (start_addr & 0x7) return false;
		// Setup transfer
		int length = arr.size();
		length = (((length - 1) / DefaultPolicy::TRANSFER_UNIT) + 1) * DefaultPolicy::TRANSFER_UNIT;
		arr.resize(length);  // Increase the buffer size to the transfer granularity
		{
            DefaultPolicy policy(start_addr, image_index);
			std::unique_lock<std::mutex> tfr_lck{ m_tfrmutex };
			fti = new FastTransfer(arr, length, policy);
		}

		// Signal thread to do the grunt work
		FastTransferStatus.store(_FastTransferStatus::DOWNLOADING);
		m_tfrcond.notify_one();

		return true;
	}

	bool CM_Common::MemoryUpload(boost::container::deque<uint8_t>& arr, uint32_t start_addr, int len, int image_index, const std::array<uint8_t, 16>& uuid)
	{
		(void)uuid;

        BOOST_LOG_SEV(lg::get(), sev::trace) << "Starting memory upload idx = " << image_index << ", " << len << " bytes at address 0x" 
            << std::hex << std::setfill('0') << std::setw(2) << start_addr;

		// Only proceed if idle
		if (FastTransferStatus.load() != _FastTransferStatus::IDLE) {
			mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_NOT_IDLE, -1);
			return false;
		}

		// Get number of entries in table
		HostReport *iorpt;

		// Perform a table index read in order to set dma status in Controller ready to read back data
		iorpt = new HostReport(HostReport::Actions::CTRLR_IMGIDX, HostReport::Dir::READ, static_cast<uint16_t>(image_index));
		ReportFields f = iorpt->Fields();
		f.context = 2; // Retrieve Index
		f.len = 49;
		iorpt->Fields(f);
		DeviceReport Resp = this->SendMsgBlocking(*iorpt);
		delete iorpt;
		if (!Resp.Done()) {
			return false;
		}

		{
            DefaultPolicy policy(start_addr, image_index);
			std::unique_lock<std::mutex> tfr_lck{ m_tfrmutex };
			fti = new FastTransfer(arr, len, policy);
		}

		// Signal thread to do the grunt work
		FastTransferStatus.store(_FastTransferStatus::UPLOADING);
		m_tfrcond.notify_one();

		return true;
	}

	void CM_Common::MemoryTransfer()
	{
        unsigned int dl_max_in_flight = DefaultPolicy::DMA_MAX_TRANSACTION_SIZE / std::max<unsigned int>(1,DefaultPolicy::DL_TRANSFER_SIZE); 
        unsigned int ul_max_in_flight = DefaultPolicy::DMA_MAX_TRANSACTION_SIZE / std::max<unsigned int>(1,DefaultPolicy::UL_TRANSFER_SIZE); 
        std::deque<MessageHandle> inflight;

        // Lambda to wait for any in-flight message to complete
        auto waitForCompletion = [&](std::deque<MessageHandle>& inflight, unsigned int max_in_flight, bool expecting_more) {
            std::vector<std::vector<uint8_t>> completedPayloads;

            m_msgRegistry.waitUntil([&]() {
                bool anyRemoved = false;

                // auto t0 = std::chrono::steady_clock::now();
                for (auto it = inflight.begin(); it != inflight.end(); ) {
                    auto handle = *it;
                    std::vector<uint8_t> payload;

                    std::shared_ptr<Message> msg = m_msgRegistry.findMessage(handle);
                    
                    if (msg->isComplete() || msg == nullptr) {
                        if (FastTransferStatus.load() == _FastTransferStatus::UPLOADING && msg != nullptr) {
                            payload = msg->Response()->Payload<std::vector<uint8_t>>(); // collect locally
                        }
                        if (!payload.empty()) completedPayloads.push_back(std::move(payload));
                        it = inflight.erase(it);
                        anyRemoved = true;
                    } else {
                        ++it;
                    }
                }

                // auto t1 = std::chrono::steady_clock::now();
                // auto hold = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
                // BOOST_LOG_SEV(lg::get(), sev::trace) << "Memory Transfer held lock for " << hold << " us. AnyRemoved = " << anyRemoved << ". inflight.size() = " << inflight.size();
                return anyRemoved || (expecting_more && inflight.size() < max_in_flight);
            });

            // Append upload payloads after releasing the lock
            for (auto& p : completedPayloads) {
                fti->m_data.insert(fti->m_data.end(), p.begin(), p.end());
            }
        };       


		while (DeviceIsOpen)
		{
			{
				std::unique_lock<std::mutex> lck{ m_tfrmutex };
				m_tfrcond.wait(lck, [&] {return !DeviceIsOpen || fti != nullptr; });
                if (!DeviceIsOpen || (FastTransferStatus.load() == _FastTransferStatus::IDLE)) continue;

				unsigned int bytesTransferred = 0;

				if (FastTransferStatus.load() == _FastTransferStatus::UPLOADING) fti->m_data.clear();

#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
				boost::chrono::steady_clock::time_point start = boost::chrono::steady_clock::now();
				boost::chrono::duration<double, boost::milli> sending_time(0.0);
                auto t_pre_xfer = boost::chrono::steady_clock::now();
#endif

                bool downloading = FastTransferStatus.load() == _FastTransferStatus::DOWNLOADING;
                unsigned int max_in_flight = downloading ? dl_max_in_flight : ul_max_in_flight;
                for (unsigned int i = 0; i < fti->m_transCount; i++) {
                    if (FastTransferStatus.load() == _FastTransferStatus::IDLE)
                        break;

                    HostReport::Dir dir = downloading ? HostReport::Dir::WRITE : HostReport::Dir::READ;
                    uint32_t transfer_size = downloading ? DefaultPolicy::DL_TRANSFER_SIZE : DefaultPolicy::UL_TRANSFER_SIZE;

                    unsigned int len = std::min<unsigned int>(static_cast<unsigned int>(fti->m_transBytesRemaining),
                                        static_cast<unsigned int>(transfer_size));

                    HostReport* iorpt = new HostReport(
                        HostReport::Actions::CTRLR_IMAGE,
                        dir,
                        ((fti->m_currentTrans - 1) & 0xFFFF));

                    ReportFields f = iorpt->Fields();
                    if (fti->m_currentTrans > 0x10000) {
                        f.context = static_cast<std::uint8_t>((fti->m_currentTrans - 1) >> 16);
                    }
                    if (!downloading) f.len = static_cast<uint16_t>(len);
                    iorpt->Fields(f);

                    if (downloading) {
                        auto buf_start = fti->m_data_it;
                        auto buf_end   = fti->m_data_it + len;
                        std::vector<uint8_t> dataBuffer(buf_start, buf_end);
                        iorpt->Payload<std::vector<uint8_t>>(dataBuffer);
                        fti->m_data_it += len;
                    }

                    fti->m_transBytesRemaining -= len;
                    bytesTransferred += len;

                    // Send asynchronously
                    auto handle = this->SendMsg(*iorpt);
                    inflight.push_back(handle);

                    delete iorpt;

                    fti->startNextTransaction();

                    if (inflight.size() >= max_in_flight) {
#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
                        auto t_final = boost::chrono::steady_clock::now();
                        sending_time += (t_final - t_pre_xfer);
#endif
                        waitForCompletion(inflight, max_in_flight, true);
#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
                        t_pre_xfer = boost::chrono::steady_clock::now();
#endif
                    }
                }

                // Drain remaining messages
                while (!inflight.empty()) {
                    waitForCompletion(inflight, max_in_flight, false);
                }

#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
				auto end = boost::chrono::steady_clock::now();
				auto diff = end - start;
				double transferTime = boost::chrono::duration <double, boost::milli>(diff).count();
				double transferSpeed = (double)bytesTransferred / ((transferTime / 1000.0) * 1024 * 1024);
				BOOST_LOG_SEV(lg::get(), sev::info) << "DMA Overall Execution time " << transferTime << " ms. Calculated Transfer speed " << transferSpeed << " MB/s";
				// BOOST_LOG_SEV(lg::get(), sev::info) << "   Time spent in blocked send " << sending_time.count() << " ms.";
				// BOOST_LOG_SEV(lg::get(), sev::info) << "   Calculated overhead " << transferTime - sending_time.count() << " ms.";
				// transferSpeed = (double)bytesTransferred / ((sending_time.count() / 1000.0) * 1024 * 1024);
				// BOOST_LOG_SEV(lg::get(), sev::info) << "   USB sustained transfer speed " << transferSpeed << " MB/s";
#endif

				delete fti;
				fti = nullptr;

				FastTransferStatus.store(_FastTransferStatus::IDLE);
				mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_COMPLETE, bytesTransferred);
			}
		}

	}

	int CM_Common::MemoryProgress() {
		mMsgEvent.Trigger<int>(this, MessageEvents::NO_FAST_MEMORY_INTERFACE, -1);
		return -1;
	}

}
