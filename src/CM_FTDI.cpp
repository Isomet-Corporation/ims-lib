/*-----------------------------------------------------------------------------
/ Title      : Connection Manager Implementation for FTDI Connections
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/src/CM_FTDI.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2023-11-24 08:01:48 +0000 (Fri, 24 Nov 2023) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 589 $
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

#if defined(_WIN32)

#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <atomic>

#include "CM_FTDI.h"
#include "IMSSystem.h"
#include "PrivateUtil.h"

#define DMA_PERFORMANCE_MEASUREMENT_MODE

#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
#include "boost/chrono.hpp"
#endif

using HRClock = std::chrono::high_resolution_clock;
using us = std::chrono::microseconds;

// Workaround because FTDI Driver is not compatible with VC14.0 (2015)
#if (_MSC_VER >= 1900)
extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }
#endif

#if defined _WIN32 && defined _DEBUG
#include "crtdbg.h"
#define DEBUG_NEW new( _NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace iMS {

	class CM_FTDI::FastTransfer
	{
	public:
		FastTransfer::FastTransfer(boost::container::deque<uint8_t>& data, const uint32_t addr, const int len, const int index) :
			m_data(data), m_addr(addr), m_len(len), m_idx(index), m_transCount(((m_len - 1) / TRANSFER_GRANULARITY) + 1) {
			m_data_it = m_data.cbegin();
			m_currentTrans = 0;
			startNextTransaction();
		}

		static const int DL_TRANSFER_SIZE = 64;
		static const int UL_TRANSFER_SIZE = 64;
		static const int TRANSFER_GRANULARITY = 64;
		//static const long DMA_MAX_TRANSACTION_SIZE = 1048576;
		//static const int TRANSFER_QUEUE_SZ = 16;

		boost::container::deque<uint8_t>& m_data;
		const uint32_t m_addr;
		const int m_len;
		const int m_idx;

		boost::container::deque<uint8_t>::const_iterator m_data_it;

		void startNextTransaction() {
			if (m_currentTrans < m_transCount) {
				m_currentTrans++;
				m_transBytesRemaining = (m_currentTrans == m_transCount) ?
					(m_len - ((m_currentTrans - 1) * TRANSFER_GRANULARITY)) : TRANSFER_GRANULARITY;
			}
		}

		const unsigned int m_transCount;
		unsigned int m_currentTrans;
		unsigned int m_transBytesRemaining;
	};

	class CM_FTDI::Impl
	{
	public:
		Impl::Impl(IConnectionManager* parent) : m_parent(parent) {}

		// Only accept FTDI Device number with Serial Numbers prefixed with these strings
		static const std::list<std::string> SerialNumberPrefix;

		const std::string Ident = "CM_USBLITE";

		FT_HANDLE ftdiDevice;
		std::vector<IMSSystem> ListFtUsbDevices();

		// Event used to receive notification from FTDI Device
#if defined(_WIN32)
		HANDLE hEvent;
        HANDLE hShutdown;
#elif defined(__linux__)
        struct FT_Event {
            pthread_cond_t eCondVar;
            pthread_mutex_t eMutex;
        };
        typedef FT_Event* FT_Event_Handle;
		FT_Event_Handle eh;
#endif
		std::atomic<_FastTransferStatus> FastTransferStatus{ _FastTransferStatus::IDLE };
		FastTransfer *fti = nullptr;

		// Memory Transfer Thread
		std::thread memoryTransferThread;
		mutable std::mutex m_tfrmutex;
		std::condition_variable m_tfrcond;

		// Interrupt receiving thread
		std::thread interruptThread;
		std::shared_ptr<std::vector<uint8_t>> interruptData;

	private:
		IConnectionManager * m_parent;
	};

	const std::list<std::string> CM_FTDI::Impl::SerialNumberPrefix = std::list<std::string>({ "iMS", "iDDS", "iCSA" });

	// Default Constructor
	CM_FTDI::CM_FTDI() : pImpl(new CM_FTDI::Impl(this))
	{
		sendTimeout = std::chrono::milliseconds(50);
		rxTimeout = std::chrono::milliseconds(5000);
		autoFreeTimeout = std::chrono::milliseconds(30000);
	}
	 
	CM_FTDI::~CM_FTDI()
	{
		// v1.0.1 Clean up threads if destroyed unexpectedly
		if (DeviceIsOpen) this->Disconnect();

		delete pImpl;
		pImpl = nullptr;
	}

	const std::string& CM_FTDI::Ident() const
	{
		return pImpl->Ident;
	}

	std::vector<IMSSystem> CM_FTDI::Impl::ListFtUsbDevices()
	{
		FT_STATUS ftStatus = 0;
		DWORD numOfDevices = 0;

		std::vector<IMSSystem> IMSList;

		// Get number of FTDI Devices attached to system
		ftStatus = FT_CreateDeviceInfoList(&numOfDevices);

		for (DWORD iDev = 0; iDev < numOfDevices; ++iDev)
		{
			FT_DEVICE_LIST_INFO_NODE devInfo;
			memset(&devInfo, 0, sizeof(devInfo));

			ftStatus = FT_GetDeviceInfoDetail(iDev, &devInfo.Flags, &devInfo.Type, &devInfo.ID, &devInfo.LocId,
				devInfo.SerialNumber,
				devInfo.Description,
				&devInfo.ftHandle);

			if (FT_OK == ftStatus)
			{
				const std::string serial = devInfo.SerialNumber;
				const int flags = devInfo.Flags;  // 0 = closed, 1 = open

				// Check to see if Serial Number begins with one of the approved strings and device hasn't been opened by another process
				bool prefix_match = false;
				for (auto& prefix : SerialNumberPrefix) {
					if ((prefix.length() <= serial.length()) &&
						std::equal(prefix.begin(), prefix.end(), serial.begin()))
						prefix_match = true;
				}
				if (prefix_match &&	!(flags & 0x01))
				{
					// Then open device and send a Magic Number query message
					//ftStatus = FT_OpenEx(devInfo.SerialNumber, FT_OPEN_BY_SERIAL_NUMBER, &ftdiDevice);
					m_parent->Connect(serial);

					if (m_parent->Open())
					{
						// Found a suitable connection interface, let's query its contents.
						// An FTDI based board can have a maximum of one controller and one synthesiser.
						IMSSystem thisiMS(m_parent, devInfo.SerialNumber);

						thisiMS.Initialise();

						if (thisiMS.Ctlr().IsValid() || thisiMS.Synth().IsValid()) {
							IMSList.push_back(thisiMS);
						}
					}

					//FT_Close(ftdiDevice);
					m_parent->Disconnect();
				}
			}
		}
		return IMSList;
	}

	std::vector<IMSSystem> CM_FTDI::Discover(const ListBase<std::string>& PortMask)
	{
//		std::cout << "CM_FTDI::Discover()" << std::endl;
		return pImpl->ListFtUsbDevices();
	}

	void CM_FTDI::Connect(const std::string& serial)
	{
		if (!DeviceIsOpen)
		{
			FT_STATUS ftStatus;

			// Open Device
			ftStatus = FT_OpenEx((PVOID)serial.c_str(), FT_OPEN_BY_SERIAL_NUMBER, &pImpl->ftdiDevice);
			if (ftStatus != FT_OK) {
				// FT_Open failed
				return;
			}
			DeviceIsOpen = true;

			ftStatus = FT_Purge(pImpl->ftdiDevice, FT_PURGE_RX | FT_PURGE_TX); // Purge both Rx and Tx buffers

            FT_SetLatencyTimer(pImpl->ftdiDevice, 1); // in ms

			// Assign Event Handle to receive notifications from device
			DWORD EventMask = FT_EVENT_RXCHAR;
#if defined(_WIN32)
			pImpl->hEvent = CreateEvent(
				NULL,
				false, // auto-reset event
				false, // non-signalled state
				NULL
				);
			ftStatus = FT_SetEventNotification(pImpl->ftdiDevice, EventMask, (PVOID)pImpl->hEvent);

            // Used to unblock the waiting threads
            pImpl->hShutdown = CreateEvent(NULL, TRUE, FALSE, NULL);
#elif defined(__linux__)
            // allocate and initialise POSIX event object
            pImpl->eh = new FT_Event;
            if (pthread_mutex_init(&pImpl->eh->eMutex, NULL) != 0) {
                // handle error...
            }
            if (pthread_cond_init(&pImpl->eh->eCondVar, NULL) != 0) {
                // handle error...
            }

            ftStatus = FT_SetEventNotification(pImpl->ftdiDevice, EventMask, (PVOID)pImpl->eh);
            // FT_SetEventNotification returns immediately.  When RXCHAR arrives, D2XX should signal the condvar.
#endif
			// Clear Message Lists
			m_list.clear();
			while (!m_queue.empty()) m_queue.pop();
			while (!m_rxCharQueue.empty()) m_rxCharQueue.pop();

			// Start Report Sending Thread
			senderThread = std::thread(&CM_FTDI::MessageSender, this);

			// Start Response Receiver Thread
			receiverThread = std::thread(&CM_FTDI::ResponseReceiver, this);

			// Start Response Parser Thread
			parserThread = std::thread(&CM_FTDI::MessageListManager, this);

			// Start Memory Transferer Thread
			pImpl->memoryTransferThread = std::thread(&CM_FTDI::MemoryTransfer, this);

			// Start Interrupt receiving thread
			pImpl->interruptThread = std::thread(&CM_FTDI::InterruptReceiver, this);
		}
	}

	void CM_FTDI::Disconnect()
	{
		if (DeviceIsOpen)
		{
				// Disable Interrupts
			HostReport *iorpt;
			iorpt = new HostReport(HostReport::Actions::CTRLR_INTREN, HostReport::Dir::WRITE, 0);
			iorpt->Payload<int>(0);
			ReportFields f = iorpt->Fields();
			f.len = sizeof(int);
			iorpt->Fields(f);
			this->SendMsg(*iorpt);
			delete iorpt;

			bool msg_pending{ true };
			while (msg_pending)
			{
				std::unique_lock <std::mutex> txlck{ m_txmutex };
				msg_pending = !m_queue.empty(); // wait for all messages to be sent
				txlck.unlock();
			}

			// wait for all messages to have been processed
			bool msg_waiting{ false };
			do
			{
				std::unique_lock<std::mutex> list_lck{ m_listmutex };
				for (std::list<std::shared_ptr<Message>>::iterator it = m_list.begin(); it != m_list.end(); ++it)
				{
					std::shared_ptr<Message> msg = (*it);
					if ((msg->getStatus() == Message::Status::SENT) ||
						(msg->getStatus() == Message::Status::RX_PARTIAL) )
					{
						msg_waiting = true;
					}
				}
				list_lck.unlock();
				if (msg_waiting)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(25));
					msg_waiting = false;
				}
				else {
					break;
				}
			} while (1);

			// Stop Threads
			DeviceIsOpen = false;  // must set this to cancel threads
            m_txcond.notify_all();
#if defined(_WIN32)
            SetEvent(pImpl->hShutdown);
#elif defined(__linux__)
            if (pImpl->eh) {
                // Wake the waiting ResponseReceiver loop
                pthread_mutex_lock(&pImpl->eh->eMutex);
                pthread_cond_broadcast(&pImpl->eh->eCondVar);
                pthread_mutex_unlock(&pImpl->eh->eMutex);
            }
#endif
			senderThread.join();
			receiverThread.join();
			parserThread.join();
			pImpl->memoryTransferThread.join();  // TODO: need to abort in-flight transfers
			pImpl->interruptThread.join();

			FT_Close(pImpl->ftdiDevice);

			// Clear Up
#if defined(__linux__)
            if (pImpl->eh) {
                pthread_cond_destroy(&pImpl->eh->eCondVar);
                pthread_mutex_destroy(&pImpl->eh->eMutex);
                delete pImpl->eh;
                pImpl->eh = nullptr;
            }
#endif
		}
	}

	void CM_FTDI::SetTimeouts(int send_timeout_ms, int rx_timeout_ms, int free_timeout_ms, int discover_timeout_ms)
	{
		sendTimeout = std::chrono::milliseconds(send_timeout_ms);
		rxTimeout = std::chrono::milliseconds(rx_timeout_ms);
		autoFreeTimeout = std::chrono::milliseconds(free_timeout_ms);
	}

	// Message sending thread waits until a new message has been added to the queue
	// then sends the report data to the FTDI device
	void CM_FTDI::MessageSender()
	{
		while (DeviceIsOpen == true)
		{
            std::shared_ptr<Message> m;
            {
                std::unique_lock<std::mutex> lck{ m_txmutex };
                m_txcond.wait(lck, [&] {return !DeviceIsOpen || !m_queue.empty(); });

                // Allow thread to terminate or to process any notifications
                if (!DeviceIsOpen) break;

                m = m_queue.front();
                m_queue.pop();  // delete from queue
            }

            // Get HostReport bytes and send to device
            DWORD numBytesWritten = 0, totalBytesWritten = 0;
            const std::vector<uint8_t>& b = m->SerialStream();
            FT_STATUS ftStatus;
            //			ftStatus = FT_Purge(ftdiDevice, FT_PURGE_RX | FT_PURGE_TX); // Purge both Rx and Tx buffers

            std::chrono::time_point<std::chrono::high_resolution_clock> tm_start = std::chrono::high_resolution_clock::now();
            do
            {
                if ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tm_start)) > sendTimeout)
                {
                    // Do something with timeout
                    m->setStatus(Message::Status::TIMEOUT_ON_SEND);
                    mMsgEvent.Trigger<int>(this, MessageEvents::TIMED_OUT_ON_SEND, m->getMessageHandle());  // Notify listeners

                    // If there is anything connected still, we need to flush through the buffer
                    int i = 73; // max msg size
                    const char c = 0;
                    do {
                        ftStatus = FT_Write(pImpl->ftdiDevice, (LPVOID)&c, 1, (LPDWORD)&numBytesWritten);
                        i -= numBytesWritten;
                    } while (!i);
                    break;
                }

                ftStatus = FT_Write(pImpl->ftdiDevice, (LPVOID)&b[totalBytesWritten], (static_cast<DWORD>(b.size()) - totalBytesWritten), (LPDWORD)&numBytesWritten);
                totalBytesWritten += numBytesWritten;
                if (ftStatus != FT_OK)
                {
                    // Do something with FTDI failure
                    m->setStatus(Message::Status::SEND_ERROR);
                    mMsgEvent.Trigger<int>(this, MessageEvents::SEND_ERROR, m->getMessageHandle());
                    break;
                }
            } while (totalBytesWritten < b.size());

            if (m->getStatus() == Message::Status::UNSENT) {
                m->setStatus(Message::Status::SENT);
            }

            m->MarkSendTime();
            
            // Place in list for processing by receive thread
            AddMsgToListWithNotify(m);
		}
	}

	void CM_FTDI::ResponseReceiver()
	{
        unsigned char RxBuffer[4096]; // same size as on-chip buffer
#if defined(_WIN32)
        HANDLE waitHandles[2] = { pImpl->hEvent, pImpl->hShutdown };

        while (DeviceIsOpen)
		{
			// Wait for something to happen
            DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
            if (!DeviceIsOpen) break;  // check shutdown

            if (waitResult == WAIT_OBJECT_0) {
                DWORD BytesAvailable = 0, eventStatus, txBytes;

			    FT_STATUS status = FT_GetStatus(pImpl->ftdiDevice, &BytesAvailable, &txBytes, &eventStatus);
                if (status != FT_OK || BytesAvailable == 0) continue;

                DWORD BytesRead = 0;
                if (BytesAvailable > sizeof(RxBuffer)) BytesAvailable = sizeof(RxBuffer);

                if (FT_Read(pImpl->ftdiDevice, RxBuffer, BytesAvailable, &BytesRead) == FT_OK && BytesRead > 0) {
                    {
                        std::scoped_lock lock(m_rxmutex);
                        for (DWORD i = 0; i < BytesRead; i++) {
                            m_rxCharQueue.push(RxBuffer[i]);
                        }
                    }
                    m_rxcond.notify_one();
                }
            }            
		}
#elif defined(__linux__)

        // Linux: wait on the FT_Event condvar which FTDI will signal
        FT_Event *eh = pImpl->eh;

        while (DeviceIsOpen) {
            // lock and wait until we are signalled or shutdown
            pthread_mutex_lock(&eh->eMutex);

            // Wait (unblocked either by FTDI signalling or by Disconnect() calling pthread_cond_broadcast)
            pthread_cond_wait(&eh->eCondVar, &eh->eMutex);

            // If DeviceIsOpen was cleared during shutdown and Disconnect() called broadcast, exit loop.
            if (!DeviceIsOpen) {
                pthread_mutex_unlock(&eh->eMutex);
                break;
            }

            pthread_mutex_unlock(&eh->eMutex);

            // Now check FTDI status and read any available bytes
            DWORD BytesAvailable = 0, eventStatus = 0, txBytes = 0;
            FT_STATUS status = FT_GetStatus(pImpl->ftdiDevice, &BytesAvailable, &txBytes, &eventStatus);
            if (status != FT_OK) {
                // handle error (optionally break)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            if (BytesAvailable == 0) {
                // nothing to read — go back to waiting
                continue;
            }

            DWORD BytesRead = 0;
            if (BytesAvailable > sizeof(RxBuffer)) BytesAvailable = sizeof(RxBuffer);

            if (FT_Read(pImpl->ftdiDevice, RxBuffer, BytesAvailable, &BytesRead) == FT_OK && BytesRead > 0) {
                {
                    std::scoped_lock lock(m_rxmutex);
                    for (DWORD i = 0; i < BytesRead; ++i) m_rxCharQueue.push(RxBuffer[i]);
                }
                m_rxcond.notify_one();
            }
        } // while(DeviceIsOpen)

#endif
	}

	bool CM_RS422::MemoryDownload(boost::container::deque<uint8_t>& arr, uint32_t start_addr, int image_index, const std::array<uint8_t, 16>& uuid)
	{
		(void)uuid;
        BOOST_LOG_SEV(lg::get(), sev::trace) << "Starting memory download idx = " << image_index << ", " << arr.size() << " bytes at address 0x" 
            << std::hex << std::setfill('0') << std::setw(2) << start_addr;

		// Only proceed if idle
		if (pImpl->FastTransferStatus.load() != _FastTransferStatus::IDLE) {
			mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_NOT_IDLE, -1);
			return false;
		}
		// DMA Cannot accept addresses that aren't aligned to 64 bits
		if (start_addr & 0x7) return false;
		// Setup transfer
		int length = arr.size();
		length = (((length - 1) / FastTransfer::TRANSFER_GRANULARITY) + 1) * FastTransfer::TRANSFER_GRANULARITY;
		arr.resize(length);  // Increase the buffer size to the transfer granularity
		{
			std::unique_lock<std::mutex> tfr_lck{ pImpl->m_tfrmutex };
			pImpl->fti = new FastTransfer(arr, start_addr, length, image_index);
		}

		// Signal thread to do the grunt work
		pImpl->FastTransferStatus.store(_FastTransferStatus::DOWNLOADING);
		pImpl->m_tfrcond.notify_one();

		return true;
	}

	bool CM_RS422::MemoryUpload(boost::container::deque<uint8_t>& arr, uint32_t start_addr, int len, int image_index, const std::array<uint8_t, 16>& uuid)
	{
		(void)uuid;

        BOOST_LOG_SEV(lg::get(), sev::trace) << "Starting memory upload idx = " << image_index << ", " << len << " bytes at address 0x" 
            << std::hex << std::setfill('0') << std::setw(2) << start_addr;

		// Only proceed if idle
		if (pImpl->FastTransferStatus.load() != _FastTransferStatus::IDLE) {
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
			std::unique_lock<std::mutex> tfr_lck{ pImpl->m_tfrmutex };
			pImpl->fti = new FastTransfer(arr, start_addr, len, image_index);
		}

		// Signal thread to do the grunt work
		pImpl->FastTransferStatus.store(_FastTransferStatus::UPLOADING);
		pImpl->m_tfrcond.notify_one();

		return true;
	}

	void CM_RS422::MemoryTransfer()
	{
        unsigned int dl_max_in_flight = FastTransfer::DMA_MAX_TRANSACTION_SIZE / std::max<unsigned int>(1,FastTransfer::DL_TRANSFER_SIZE); 
        unsigned int ul_max_in_flight = FastTransfer::DMA_MAX_TRANSACTION_SIZE / std::max<unsigned int>(1,FastTransfer::UL_TRANSFER_SIZE); 
        std::deque<MessageHandle> inflight;

        // Lambda to wait for any in-flight message to complete
        auto waitForCompletion = [&](std::deque<MessageHandle>& inflight, unsigned int max_in_flight) {
            std::vector<std::vector<uint8_t>> completedPayloads;

            std::unique_lock<std::mutex> lock(m_listmutex);
            m_listcv.wait(lock, [&]() {
                bool anyRemoved = false;

                for (auto it = inflight.begin(); it != inflight.end(); ) {
                    auto handle = *it;
                    bool done = false;
                    std::vector<uint8_t> payload;

                    for (const auto& msg : m_list) {
                        if (msg->getMessageHandle() == handle) {
                            auto resp = msg->Response();
                            if (resp->Done()) {
                                if (pImpl->FastTransferStatus.load() == _FastTransferStatus::UPLOADING) {
                                    payload = resp->Payload<std::vector<uint8_t>>(); // collect locally
                                }
                                done = true;
                                break;
                            }
                            auto status = msg->getStatus();
                            if (status != Message::Status::UNSENT &&
                                status != Message::Status::SENT &&
                                status != Message::Status::RX_PARTIAL &&
                                status != Message::Status::RX_OK &&
                                status != Message::Status::RX_ERROR_VALID)
                            {
                                done = true;
                                break;
                            }
                        }
                    }

                    if (done) {
                        if (!payload.empty()) completedPayloads.push_back(std::move(payload));
                        it = inflight.erase(it);
                        anyRemoved = true;
                    } else {
                        ++it;
                    }
                }

                return anyRemoved || inflight.size() < max_in_flight;
            });
                lock.unlock();

            // Append upload payloads after releasing the lock
            for (auto& p : completedPayloads) {
                pImpl->fti->m_data.insert(pImpl->fti->m_data.end(), p.begin(), p.end());
            }
        };       


		while (DeviceIsOpen)
		{
			{
				std::unique_lock<std::mutex> lck{ pImpl->m_tfrmutex };
				pImpl->m_tfrcond.wait(lck, [&] {return !DeviceIsOpen || pImpl->fti != nullptr; });
                if (!DeviceIsOpen || (pImpl->FastTransferStatus.load() == _FastTransferStatus::IDLE)) continue;

				LONG bytesTransferred = 0;

				if (pImpl->FastTransferStatus.load() == _FastTransferStatus::UPLOADING) pImpl->fti->m_data.clear();

#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
				boost::chrono::steady_clock::time_point start = boost::chrono::steady_clock::now();
				boost::chrono::duration<double, boost::milli> sending_time(0.0);
                auto t_pre_xfer = boost::chrono::steady_clock::now();
#endif

                bool downloading = pImpl->FastTransferStatus.load() == _FastTransferStatus::DOWNLOADING;
                unsigned int max_in_flight = downloading ? dl_max_in_flight : ul_max_in_flight;
                for (unsigned int i = 0; i < pImpl->fti->m_transCount; i++) {
                    if (pImpl->FastTransferStatus.load() == _FastTransferStatus::IDLE)
                        break;

                    HostReport::Dir dir = downloading ? HostReport::Dir::WRITE : HostReport::Dir::READ;
                    uint32_t transfer_size = downloading ? FastTransfer::DL_TRANSFER_SIZE : FastTransfer::UL_TRANSFER_SIZE;

                    LONG len = std::min<LONG>(static_cast<LONG>(pImpl->fti->m_transBytesRemaining),
                                        static_cast<LONG>(transfer_size));

                    HostReport* iorpt = new HostReport(
                        HostReport::Actions::CTRLR_IMAGE,
                        dir,
                        ((pImpl->fti->m_currentTrans - 1) & 0xFFFF));

                    ReportFields f = iorpt->Fields();
                    if (pImpl->fti->m_currentTrans > 0x10000) {
                        f.context = static_cast<std::uint8_t>((pImpl->fti->m_currentTrans - 1) >> 16);
                    }
                    if (!downloading) f.len = static_cast<uint16_t>(len);
                    iorpt->Fields(f);

                    if (downloading) {
                        auto buf_start = pImpl->fti->m_data_it;
                        auto buf_end   = pImpl->fti->m_data_it + len;
                        std::vector<uint8_t> dataBuffer(buf_start, buf_end);
                        iorpt->Payload<std::vector<uint8_t>>(dataBuffer);
                        pImpl->fti->m_data_it += len;
                    }

                    pImpl->fti->m_transBytesRemaining -= len;
                    bytesTransferred += len;

                    // Send asynchronously
                    auto handle = this->SendMsg(*iorpt);
                    inflight.push_back(handle);

                    delete iorpt;

                    pImpl->fti->startNextTransaction();

                    if (inflight.size() >= max_in_flight) {
#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
                        auto t_final = boost::chrono::steady_clock::now();
                        sending_time += (t_final - t_pre_xfer);
#endif
                        waitForCompletion(inflight, max_in_flight);
#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
                        t_pre_xfer = boost::chrono::steady_clock::now();
#endif
                    }
                }

                // Drain remaining messages
                while (!inflight.empty()) {
                    waitForCompletion(inflight, max_in_flight);
                }

#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
				auto end = boost::chrono::steady_clock::now();
				auto diff = end - start;
				double transferTime = boost::chrono::duration <double, boost::milli>(diff).count();
				double transferSpeed = (double)bytesTransferred / ((transferTime / 1000.0) * 1024 * 1024);
				BOOST_LOG_SEV(lg::get(), sev::info) << "DMA Overall Execution time " << transferTime << " ms. Calculated Transfer speed " << transferSpeed << " MB/s";
				BOOST_LOG_SEV(lg::get(), sev::info) << "   Time spent in blocked send " << sending_time.count() << " ms.";
				BOOST_LOG_SEV(lg::get(), sev::info) << "   Calculated overhead " << transferTime - sending_time.count() << " ms.";
				transferSpeed = (double)bytesTransferred / ((sending_time.count() / 1000.0) * 1024 * 1024);
				BOOST_LOG_SEV(lg::get(), sev::info) << "   USB sustained transfer speed " << transferSpeed << " MB/s";
#endif

				//delete[] dataBuffer;
				delete pImpl->fti;
				pImpl->fti = nullptr;

				pImpl->FastTransferStatus.store(_FastTransferStatus::IDLE);
				mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_COMPLETE, bytesTransferred);
			}
		}

	}

	void CM_FTDI::InterruptReceiver()
	{

	}
}

#endif
