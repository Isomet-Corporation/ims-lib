/*-----------------------------------------------------------------------------
/ Title      : Connection Manager Implementation for FTDI Connections
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/src/CM_FTDI.cpp $
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
#elif defined(__linux__)
		struct FT_Event
		{
			std::condition_variable eCondVar;
			mutable std::mutex eMutex;
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

	const std::list<std::string> CM_FTDI::Impl::SerialNumberPrefix = std::list<std::string>({ "iMS", "iDDS" });

	// Default Constructor
	CM_FTDI::CM_FTDI() : p_Impl(new CM_FTDI::Impl(this))
	{
		sendTimeout = std::chrono::milliseconds(50);
		rxTimeout = std::chrono::milliseconds(5000);
		autoFreeTimeout = std::chrono::milliseconds(30000);
	}
	 
	CM_FTDI::~CM_FTDI()
	{
		// v1.0.1 Clean up threads if destroyed unexpectedly
		if (DeviceIsOpen) this->Disconnect();

		delete p_Impl;
		p_Impl = nullptr;
	}

	const std::string& CM_FTDI::Ident() const
	{
		return p_Impl->Ident;
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
		return p_Impl->ListFtUsbDevices();
	}

	void CM_FTDI::Connect(const std::string& serial)
	{
		if (!DeviceIsOpen)
		{
			FT_STATUS ftStatus;

			// Open Device
			ftStatus = FT_OpenEx((PVOID)serial.c_str(), FT_OPEN_BY_SERIAL_NUMBER, &p_Impl->ftdiDevice);
			if (ftStatus != FT_OK) {
				// FT_Open failed
				return;
			}
			DeviceIsOpen = true;

			ftStatus = FT_Purge(p_Impl->ftdiDevice, FT_PURGE_RX | FT_PURGE_TX); // Purge both Rx and Tx buffers

			// Assign Event Handle to receive notifications from device
			DWORD EventMask = FT_EVENT_RXCHAR;
#if defined(_WIN32)
			p_Impl->hEvent = CreateEvent(
				NULL,
				false, // auto-reset event
				false, // non-signalled state
				NULL
				);
			ftStatus = FT_SetEventNotification(p_Impl->ftdiDevice, EventMask, (PVOID)p_Impl->hEvent);
#elif defined(__linux__)
			eh = new FT_Event;

			pthread_mutex_init(eh->eMutex, NULL);
			pthread_cond_init(eh->eCondVar, NULL);
			ftStatus = FT_SetEventNotification(ftdiDevice, EventMask, (PVOID)eh)
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
			p_Impl->memoryTransferThread = std::thread(&CM_FTDI::MemoryTransfer, this);

			// Start Interrupt receiving thread
			p_Impl->interruptThread = std::thread(&CM_FTDI::InterruptReceiver, this);
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
			senderThread.join();
			receiverThread.join();
			parserThread.join();
			p_Impl->memoryTransferThread.join();  // TODO: need to abort in-flight transfers
			p_Impl->interruptThread.join();

			FT_Close(p_Impl->ftdiDevice);

			// Clear Up
#if defined(__linux__)
			delete eh;
#endif
		}
	}

	// Message sending thread waits until a new message has been added to the queue
	// then sends the report data to the FTDI device
	void CM_FTDI::MessageSender()
	{
		while (DeviceIsOpen == true)
		{
			std::unique_lock<std::mutex> lck{ m_txmutex };
			m_txcond.wait_for(lck, std::chrono::milliseconds(100));
			// Unblock every 100ms to allow thread to terminate or to process any missed notifications
			if (DeviceIsOpen == false)
			{
				lck.unlock();
				break;
			}

			while (!m_queue.empty())
			{
				std::shared_ptr<Message> m = m_queue.front();
				m_queue.pop();  // delete from queue

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
							ftStatus = FT_Write(p_Impl->ftdiDevice, (LPVOID)&c, 1, (LPDWORD)&numBytesWritten);
							i -= numBytesWritten;
						} while (!i);
						break;
					}

					ftStatus = FT_Write(p_Impl->ftdiDevice, (LPVOID)&b[totalBytesWritten], (static_cast<DWORD>(b.size()) - totalBytesWritten), (LPDWORD)&numBytesWritten);
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
				{
					std::unique_lock<std::mutex> list_lck{ m_listmutex };
					m_list.push_back(m);
					list_lck.unlock();
				}

			}
			lck.unlock();
		}
	}

	void CM_FTDI::ResponseReceiver()
	{
		while (DeviceIsOpen == true)
		{
			DWORD RxBytes;
			DWORD TxBytes;
			DWORD EventStatus;
			unsigned char RxBuffer[4096]; // same size as on-chip buffer
			DWORD BytesReceived;

			// Wait for something to happen
#if defined(_WIN32)
			while (WAIT_TIMEOUT == WaitForSingleObject(p_Impl->hEvent, 100))
			{
				// Timeout every 100ms to allow threads to terminate on disconnect
				if (DeviceIsOpen == false) break;
				FT_GetStatus(p_Impl->ftdiDevice, &RxBytes, &TxBytes, &EventStatus);
				if (RxBytes > 0) break;
			}
#elif defined(__linux__)
			pthread_mutex_lock(&eh.eMutex);
			pthread_cond_wait(&eh.eCondVar, &eh.eMutex);
			pthread_mutex_unlock(&eh.eMutex);

			//std::unique_lock<std::mutex> lck{ eh->eMutex };
			//while (eh->eCondVar.wait_for(lck, std::chrono::milliseconds(100)) == std::cv_status::timeout)
			//{
			//	// Timeout every 100ms to allow threads to terminate on disconnect
			//	if (DeviceIsOpen == false) break;
			//}
			//if (DeviceIsOpen == false)
			//{
			//	lck.unlock();
			//	break;
			//}
#endif

			FT_GetStatus(p_Impl->ftdiDevice, &RxBytes, &TxBytes, &EventStatus);
			if ((EventStatus & FT_EVENT_RXCHAR) || (RxBytes > 0))
			{
				//lck.unlock();
				std::unique_lock<std::mutex> rxlck{ m_rxmutex };
				DWORD TfrBytes;
				while (RxBytes > 0)
				{
					TfrBytes = RxBytes;
					// Never request more bytes than the size of the buffer in the FTDI device (4kB)
					if (TfrBytes > 4096) TfrBytes = 4096; 
					FT_STATUS ftStatus = FT_Read(p_Impl->ftdiDevice, RxBuffer, TfrBytes, &BytesReceived);
					//std::cout << "Received: " << RxBytes << " bytes" << std::endl;
					if (ftStatus == FT_OK)  {
						if (BytesReceived)
						{
							// FT_Read OK
							for (unsigned int i = 0; i < BytesReceived; i++)
							{
								m_rxCharQueue.push(RxBuffer[i]);
								//std::cout << std::hex;
								//std::cout << std::setfill('0') << std::setw(2) << static_cast<int>(RxBuffer[i]) << " ";
							}
						}
					}
					else {
						// FT_Read Failed
						break;
					}
					RxBytes -= BytesReceived;
				}
				// Signal Parser thread
				rxlck.unlock();
				m_rxcond.notify_one();
			}
		}
	}

	bool CM_FTDI::MemoryDownload(boost::container::deque<uint8_t>& arr, uint32_t start_addr, int image_index, const std::array<uint8_t, 16>& uuid)
	{
		// Only proceed if idle
		if (p_Impl->FastTransferStatus.load() != _FastTransferStatus::IDLE) {
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
			std::unique_lock<std::mutex> tfr_lck{ p_Impl->m_tfrmutex };
			p_Impl->fti = new FastTransfer(arr, start_addr, length, image_index);
		}

		// Signal thread to do the grunt work
		p_Impl->FastTransferStatus.store(_FastTransferStatus::DOWNLOADING);
		p_Impl->m_tfrcond.notify_one();

		return true;
	}

	bool CM_FTDI::MemoryUpload(boost::container::deque<uint8_t>& arr, uint32_t start_addr, int len, int image_index, const std::array<uint8_t, 16>& uuid)
	{
		// Only proceed if idle
		if (p_Impl->FastTransferStatus.load() != _FastTransferStatus::IDLE) {
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
			std::unique_lock<std::mutex> tfr_lck{ p_Impl->m_tfrmutex };
			p_Impl->fti = new FastTransfer(arr, start_addr, len, image_index);
		}

		// Signal thread to do the grunt work
		p_Impl->FastTransferStatus.store(_FastTransferStatus::UPLOADING);
		p_Impl->m_tfrcond.notify_one();

		return true;
	}

	void CM_FTDI::MemoryTransfer()
	{
		while (DeviceIsOpen == true)
		{
			{
				std::unique_lock<std::mutex> lck{ p_Impl->m_tfrmutex };
				while (!p_Impl->m_tfrcond.wait_for(lck, std::chrono::milliseconds(100), [&] {return p_Impl->fti != nullptr; }))
				{
					if (p_Impl->FastTransferStatus.load() != _FastTransferStatus::IDLE)
					{
						break;
					}
					// Timeout every 100ms to allow threads to terminate on disconnect
					if (DeviceIsOpen == false) break;
				}
				if (DeviceIsOpen == false)
				{
					// End thread
					lck.unlock();
					break;
				}
				//PUCHAR dataBuffer;
				//try
				//{
				/*if (FastTransfer::DL_TRANSFER_SIZE > FastTransfer::UL_TRANSFER_SIZE)
				dataBuffer = new UCHAR[FastTransfer::DL_TRANSFER_SIZE];
				else
				dataBuffer = new UCHAR[FastTransfer::UL_TRANSFER_SIZE];*/
				//}
				//catch (std::bad_alloc& exc)
				//{
				//					std::cout << exc.what() << std::endl;
				//}
				LONG bytesTransferred = 0;

				if (p_Impl->FastTransferStatus.load() == _FastTransferStatus::UPLOADING) p_Impl->fti->m_data.clear();

#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
				boost::chrono::steady_clock::time_point start = boost::chrono::steady_clock::now();
				boost::chrono::duration<double, boost::milli> copy_time(0.0);
				boost::chrono::duration<double, boost::milli> xfer_time(0.0);
#endif

				for (unsigned int i = 0; i < p_Impl->fti->m_transCount; i++) {
					// Prime DMA Transfer
					HostReport *iorpt;
					//uint32_t len = static_cast<uint32_t>(p_Impl->fti->m_transBytesRemaining);
					uint32_t addr = p_Impl->fti->m_addr + i * FastTransfer::TRANSFER_GRANULARITY;
					if (p_Impl->FastTransferStatus.load() == _FastTransferStatus::DOWNLOADING) {
						iorpt = new HostReport(HostReport::Actions::CTRLR_IMAGE, HostReport::Dir::WRITE, ((p_Impl->fti->m_currentTrans - 1) & 0xFFFF));
						if (p_Impl->fti->m_currentTrans > 0x10000) {
							ReportFields f = iorpt->Fields();
							f.context = ((p_Impl->fti->m_currentTrans - 1) >> 16);
							iorpt->Fields(f);
						}
						LONG len = FastTransfer::DL_TRANSFER_SIZE;
						if (p_Impl->fti->m_transBytesRemaining < FastTransfer::DL_TRANSFER_SIZE) len = p_Impl->fti->m_transBytesRemaining;
						auto buf_start = p_Impl->fti->m_data_it;
						auto buf_end = p_Impl->fti->m_data_it + len;
						std::vector<uint8_t> dataBuffer(buf_start, buf_end);
						//						std::copy(buf_start, buf_end, dataBuffer);
						p_Impl->fti->m_data_it += len;
						p_Impl->fti->m_transBytesRemaining -= len;
						iorpt->Payload<std::vector<uint8_t>>(dataBuffer);
						DeviceReport resp = this->SendMsgBlocking(*iorpt);
						delete iorpt;
						if (!resp.Done()) {
							break;
						}
						bytesTransferred += len;
					}
					else if (p_Impl->FastTransferStatus.load() == _FastTransferStatus::UPLOADING) {
						uint16_t len = FastTransfer::UL_TRANSFER_SIZE;
						if (p_Impl->fti->m_transBytesRemaining < FastTransfer::UL_TRANSFER_SIZE) len = p_Impl->fti->m_transBytesRemaining;
						iorpt = new HostReport(HostReport::Actions::CTRLR_IMAGE, HostReport::Dir::READ, ((p_Impl->fti->m_currentTrans - 1) & 0xFFFF));
						ReportFields f = iorpt->Fields();
						if (p_Impl->fti->m_currentTrans > 0x10000) {
							f.context = ((p_Impl->fti->m_currentTrans - 1) >> 16);
						}
						f.len = len;
						iorpt->Fields(f);
						DeviceReport resp = this->SendMsgBlocking(*iorpt);
						if (resp.Done()) {
							//							std::copy(resp.Payload<std::vector<uint8_t>>().cbegin(), resp.Payload<std::vector<uint8_t>>().cend(), dataBuffer);
							std::vector<uint8_t> vfy_data = resp.Payload<std::vector<uint8_t>>();
							p_Impl->fti->m_data.insert(p_Impl->fti->m_data.end(), vfy_data.begin(), vfy_data.end());
							//							p_Impl->fti->m_data_it += len;
							p_Impl->fti->m_transBytesRemaining -= len;
							bytesTransferred += len;
						}
						else {
							break;
						}
						delete iorpt;
					}
					// Set up index data for next DMA Transaction
					p_Impl->fti->startNextTransaction();
				}

#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
				auto end = boost::chrono::steady_clock::now();
				auto diff = end - start;
				double transferTime = boost::chrono::duration <double, boost::milli>(diff).count();
				double transferSpeed = (double)bytesTransferred / ((transferTime / 1000.0) * 1024 * 1024);
				std::cout << "DMA Overall Execution time " << transferTime << " ms. Calculated Transfer speed " << transferSpeed << " MB/s" << std::endl;
				std::cout << "   Time spent in data management " << copy_time.count() << " ms." << std::endl;
				std::cout << "   Time spent on USB transfer activity " << xfer_time.count() << " ms." << std::endl;
				std::cout << "   Calculated overhead " << transferTime - copy_time.count() - xfer_time.count() << " ms." << std::endl;
				transferSpeed = (double)bytesTransferred / ((xfer_time.count() / 1000.0) * 1024 * 1024);
				std::cout << "   USB sustained transfer speed " << transferSpeed << " MB/s" << std::endl;
#endif

				//delete[] dataBuffer;
				delete p_Impl->fti;
				p_Impl->fti = nullptr;

				p_Impl->FastTransferStatus.store(_FastTransferStatus::IDLE);
				mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_COMPLETE, bytesTransferred);
			}
		}

	}

	void CM_FTDI::InterruptReceiver()
	{

	}
}

#endif
