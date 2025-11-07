/*-----------------------------------------------------------------------------
/ Title      : Connection Manager Implementation for Cypress USB Connections
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/src/CM_CYUSB.cpp $
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
#include <vector>
#include <map>
#include <string>
//#include <iostream>
#include <chrono>
#include <atomic>

#include "boost/chrono.hpp"
#include "windows.h"

#include "CM_CYUSB.h"
#include "IMSSystem.h"

#if defined _WIN32 && defined _DEBUG
#include "crtdbg.h"
#define DEBUG_NEW new( _NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

//#define DMA_PERFORMANCE_MEASUREMENT_MODE

namespace iMS {
	const uint16_t ImageDMA_Download = 0;
	const uint16_t ImageDMA_Upload = 1;

	class CM_CYUSB::USBContext
	{
	public:
		static const int MaxPacketSize = 4 * (((IOReport::PAYLOAD_MAX_LENGTH + IOReport::OVERHEAD_MAX_LENGTH) / 4) + 1);
		static const int MaxUSBBufferSize = 1024;

		OVERLAPPED *OvLap;
		PUCHAR context;
		LONG bufLen;
		std::vector<uint8_t> *Buffer;
		MessageHandle handle;

		USBContext();
		~USBContext();
	};

	CM_CYUSB::USBContext::USBContext()
	{
		OvLap = new OVERLAPPED();
		// Manual reset event (remains signalled until reset by ResponseReceiver code)
		OvLap->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	CM_CYUSB::USBContext::~USBContext()
	{
		CloseHandle(OvLap->hEvent);
		delete OvLap;
	}

	// All private data and member functions contained within Impl class
	class CM_CYUSB::Impl
	{
	public:
		Impl::Impl(IConnectionManager* parent) : m_parent(parent) {}
		~Impl();

		const std::string Ident = "CM_USBSS";

		// Only accept FTDI Device number with Serial Numbers prefixed with this string
		const std::string SerialNumberPrefix = "iMS";

		CCyUSBDevice	*USBDevice = nullptr;
		std::vector<std::shared_ptr<IMSSystem>> ListCyUsbDevices();
		std::map<std::string, int> cyusb_list;
		struct endpoints
		{
			CCyUSBEndPoint *cInEpt = nullptr;
			CCyUSBEndPoint *cOutEpt = nullptr;
			CCyUSBEndPoint *bInEpt = nullptr;
			CCyUSBEndPoint *bOutEpt = nullptr;
			CCyUSBEndPoint *iInEpt = nullptr;
		} Ept;

		void expandBuffer(std::vector<uint8_t>& buf);
		void condenseBuffer(std::vector<uint8_t>& buf);
		HANDLE *getRxBufHandles();

		std::deque<std::shared_ptr<USBContext>> m_rxBuf;
		int m_rxbuf_size;
		mutable std::mutex m_rxBufmutex;
		std::condition_variable m_rxBufcond;

        FastTransfer* m_fti = nullptr;

		// Interrupt receiving thread
		std::thread interruptThread;
		std::shared_ptr<std::vector<uint8_t>> interruptData;

	private:
		IConnectionManager * m_parent;
		std::vector<HANDLE> rxBufHandles;
	};

	CM_CYUSB::Impl::~Impl() {
		if (USBDevice != nullptr) {
			if (USBDevice->IsOpen())
				USBDevice->Close();
			delete USBDevice;
			USBDevice = nullptr;
		}
	}

	// Default Constructor
	CM_CYUSB::CM_CYUSB() 
		 : mImpl(new CM_CYUSB::Impl(this))
	{
		sendTimeout = std::chrono::milliseconds(500);
		rxTimeout = std::chrono::milliseconds(10000);
		autoFreeTimeout = std::chrono::milliseconds(30000);
	}

	const std::string& CM_CYUSB::Ident() const
	{
		return mImpl->Ident;
	}

	// Data path from USB device to FPGA is 32-bits and messages are byte-oriented.
	// Add sufficient zeros to make up to a modulo-4 byte transfer
	void CM_CYUSB::Impl::expandBuffer(std::vector<uint8_t>& buf)
	{
		while (buf.size() % 4) buf.push_back(0);
		// For pre v1.1.23 iMSP where only bottom byte was used
		/*for (std::vector<uint8_t>::iterator it = buf.begin(); it != buf.end(); ++it)
		{
			it = buf.emplace(++it, 0);
			it = buf.emplace(++it, 0);
			it = buf.emplace(++it, 0);
		}*/
	}

	// Inverse operation for returned data
	void CM_CYUSB::Impl::condenseBuffer(std::vector<uint8_t>& buf)
	{
		// For pre v1.1.23 iMSP where only bottom byte was used
		/*while (buf.size() % 4) buf.push_back(0);
		for (std::vector<uint8_t>::iterator it = buf.begin(); it != buf.end(); )
		{
			++it;
			it = buf.erase(it);
			it = buf.erase(it);
			it = buf.erase(it);
		}*/
	}

	HANDLE *CM_CYUSB::Impl::getRxBufHandles()
	{
		rxBufHandles.clear();
		m_rxbuf_size = 0;

		{
			//std::unique_lock<std::mutex> lck{ m_rxBufmutex };
			if (m_rxBuf.empty()) return nullptr;
			for (std::deque<std::shared_ptr<USBContext>>::const_iterator it = m_rxBuf.begin(); it != m_rxBuf.end(); ++it)
			{
				rxBufHandles.push_back((*it)->OvLap->hEvent);
				m_rxbuf_size++;
			}
			//m_rxbuf_size = m_rxBuf.size();
		}
//		if (m_rxbuf_size > 0) {
			return (&(rxBufHandles[0]));
//		}
//		else {
//			return nullptr;
//		}
	}

	CM_CYUSB::~CM_CYUSB()
	{
		if (DeviceIsOpen) this->Disconnect();

		delete mImpl;
		mImpl = NULL;
	}

	std::vector<std::shared_ptr<IMSSystem>> CM_CYUSB::Impl::ListCyUsbDevices()
	{
		int ftStatus = 0;
		DWORD numOfDevices = 0;

		// If we have an open connection, we can't allow the driver to be opened again to look for devices
		if (m_parent->Open()) return (std::vector<std::shared_ptr<IMSSystem>>());
		if (USBDevice != nullptr) {
			delete USBDevice;
			USBDevice = nullptr;
		}

		std::vector<std::shared_ptr<IMSSystem>> IMSList;
		cyusb_list.clear();

		// Open USB driver and get number of CYUSB Devices attached to system
		USBDevice = new CCyUSBDevice(NULL, CYUSBDRV_GUID, FALSE);
		int nDeviceCount = USBDevice->DeviceCount();

		for (int nCount = 0; nCount < nDeviceCount; nCount++)
		{
			if (!USBDevice->Open(nCount)) {
				USBDevice->Reset();
				USBDevice->Open(nCount);
			}

			int interfaces = USBDevice->AltIntfcCount() + 1;
			for (int nDeviceInterfaces = 0; nDeviceInterfaces < interfaces; nDeviceInterfaces++)
			{
				USBDevice->SetAltIntfc(nDeviceInterfaces);
				int eptCnt = USBDevice->EndPointCount();

				// There should be at least 4 endpoints in a Cypress connection: Control In/Out and Bulk In/Out plus an optional Interrupt In
				if (eptCnt >= 4)
				{
					// Check endpoints and retrieve serial number
					bool controlIn = false;
					bool controlOut = false;
					bool bulkIn = false;
					bool bulkOut = false;
					bool intIn = false;
					for (int endPoint = 0; endPoint < eptCnt; endPoint++)
					{
						CCyUSBEndPoint *ept = USBDevice->EndPoints[endPoint];
						if (ept->Attributes == 2) {
							if (ept->bIn && ept->Address == 0x81) controlIn = true;
							if (!ept->bIn && ept->Address == 0x01) controlOut = true;
							if (ept->bIn && ept->Address == 0x82) bulkIn = true;
							if (!ept->bIn && ept->Address == 0x02) bulkOut = true;
						}
						if (ept->Attributes == 3) {
							if (ept->bIn && ept->Address == 0x83) intIn = true;
						}
					}
					if (!controlIn || !controlOut || !bulkIn || !bulkOut) continue;
					
					// Store serial number along with USB Device count index in cyusb_list map
					const std::string false_serial("iMSP9999");
					cyusb_list[false_serial] = nCount;

					// Then open device and send a Magic Number query message
					//ftStatus = FT_OpenEx(devInfo.SerialNumber, FT_OPEN_BY_SERIAL_NUMBER, &ftdiDevice);
					m_parent->Connect(false_serial);
					cyusb_list.erase(false_serial);

					HostReport *iorpt;
					iorpt = new HostReport(HostReport::Actions::CTRLR_SETTINGS, HostReport::Dir::READ, 0);
					ReportFields fields = iorpt->Fields();
					fields.len = 16; // Read first 16 bytes of controller settings
					iorpt->Fields(fields);
					DeviceReport Resp = m_parent->SendMsgBlocking(*iorpt);
					delete iorpt;

					if (!Resp.Done()) {
						m_parent->Disconnect();
						continue;
					}

					const std::string serial = Resp.Payload<std::string>() + ":USB";

					// Store serial number along with USB Device count index in cyusb_list map
					cyusb_list[serial] = nCount;

					if (m_parent->Open())
					{
						// Found a suitable connection interface, let's query its contents.
						// An FTDI based board can have a maximum of one controller and one synthesiser.
					    auto thisiMS = IMSSystem::Create (m_parent, serial);

						thisiMS->Initialise();

						if (thisiMS->Ctlr().IsValid() || thisiMS->Synth().IsValid()) {
							IMSList.push_back(thisiMS);
						}
					}

					m_parent->Disconnect();
				}
			}
			USBDevice->Close();
		}
		return IMSList;
	}

	std::vector<std::shared_ptr<IMSSystem>> CM_CYUSB::Discover(const ListBase<std::string>& PortMask)
	{
		return mImpl->ListCyUsbDevices();
	}

	void CM_CYUSB::Connect(const std::string& serial)
	{
		if (!DeviceIsOpen)
		{
			// If connecting without first scanning system, do a scan here to populate cyusb map
			if (mImpl->USBDevice == nullptr) mImpl->ListCyUsbDevices();

			// Exit if no device found
			if (mImpl->cyusb_list.empty()) return;

			//  Attempt to open device
			if (mImpl->USBDevice->DeviceCount() && !mImpl->USBDevice->Open(mImpl->cyusb_list[serial])) {
				mImpl->USBDevice->Reset();
				mImpl->USBDevice->Open(mImpl->cyusb_list[serial]);
			}

			if (!mImpl->USBDevice->IsOpen()) return;

			// Populate endpoint array
			for (int endPoint = 0; endPoint < mImpl->USBDevice->EndPointCount(); endPoint++)
			{
				CCyUSBEndPoint *ept = mImpl->USBDevice->EndPoints[endPoint];
				if (ept->Attributes == 2) {
					if (ept->bIn && ept->Address == 0x81) mImpl->Ept.cInEpt = ept;
					if (!ept->bIn && ept->Address == 0x01) mImpl->Ept.cOutEpt = ept;
					if (ept->bIn && ept->Address == 0x82) mImpl->Ept.bInEpt = ept;
					if (!ept->bIn && ept->Address == 0x02) mImpl->Ept.bOutEpt = ept;
				}
				if (ept->Attributes == 3) {
					if (ept->bIn && ept->Address == 0x83) mImpl->Ept.iInEpt = ept;
				}

				// Set Timeout
				ept->TimeOut = 1000;  // 1sec
			}
			if ((mImpl->Ept.cInEpt == nullptr) || (mImpl->Ept.cOutEpt == nullptr) || (mImpl->Ept.bInEpt == nullptr) || (mImpl->Ept.bOutEpt == nullptr))
			{
				mImpl->USBDevice->Close();
				mImpl->Ept.cInEpt = nullptr;
				mImpl->Ept.cOutEpt = nullptr;
				mImpl->Ept.bInEpt = nullptr;
				mImpl->Ept.bOutEpt = nullptr;
				return;
			}
			DeviceIsOpen = true;

			// Clear Message Lists
            m_msgRegistry.clear();
			while (!m_queue.empty()) m_queue.pop();
			while (!mImpl->m_rxBuf.empty()) mImpl->m_rxBuf.pop_front();

			// Start Report Sending Thread
			senderThread = std::thread(&CM_CYUSB::MessageSender, this);

			// Start Response Receiver Thread
			receiverThread = std::thread(&CM_CYUSB::ResponseReceiver, this);

			// Start Response Parser Thread
			parserThread = std::thread(&CM_CYUSB::MessageListManager, this);

			// Start Memory Transferer Thread
			memoryTransferThread = std::thread(&CM_CYUSB::MemoryTransfer, this);

			// Start Interrupt receiving thread
			mImpl->interruptThread = std::thread(&CM_CYUSB::InterruptReceiver, this);

		}
	}

	void CM_CYUSB::Disconnect()
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
                m_msgRegistry.forEachMessage([&](const std::shared_ptr<Message>& msg) {
                    if (!msg->isComplete())
					{
						msg_waiting = true;
					}
                });
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
			senderThread.join();
			receiverThread.join();
			parserThread.join();
			memoryTransferThread.join();  // TODO: need to abort in-flight transfers
			mImpl->interruptThread.join();

			mImpl->USBDevice->Close();

			// Clear Up
		}

	}

	void CM_CYUSB::SetTimeouts(int send_timeout_ms, int rx_timeout_ms, int free_timeout_ms, int discover_timeout_ms)
	{
		sendTimeout = std::chrono::milliseconds(send_timeout_ms);
		rxTimeout = std::chrono::milliseconds(rx_timeout_ms);
		autoFreeTimeout = std::chrono::milliseconds(free_timeout_ms);
	}

	void CM_CYUSB::MessageSender()
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
            std::vector<uint8_t> b = m->SerialStream();
            mImpl->expandBuffer(b);

            std::shared_ptr<USBContext> inContext = std::make_shared<USBContext>();
            inContext->handle = m->getMessageHandle();
            inContext->Buffer = new std::vector<uint8_t>(USBContext::MaxPacketSize, 0);  // Create a default buffer to write received data to
            inContext->bufLen = USBContext::MaxPacketSize;

            {
                std::unique_lock<std::mutex> rxlck{ mImpl->m_rxBufmutex };
                inContext->context = mImpl->Ept.cInEpt->BeginDataXfer((PUCHAR)&((*(inContext->Buffer))[0]), inContext->bufLen, inContext->OvLap);
                mImpl->m_rxBuf.push_back(inContext);
            }

            USBContext outContext;
            outContext.Buffer = &b;
            outContext.bufLen = b.size();
            outContext.context = mImpl->Ept.cOutEpt->BeginDataXfer((PUCHAR)&((*(outContext.Buffer))[0]), outContext.bufLen, outContext.OvLap);
            
            std::chrono::time_point<std::chrono::high_resolution_clock> tm_start = std::chrono::high_resolution_clock::now();
            do
            {
                if (mImpl->Ept.cOutEpt->NtStatus || mImpl->Ept.cOutEpt->UsbdStatus)
                {
                    m->setStatus(Message::Status::SEND_ERROR);
                    mMsgEvent.Trigger<int>(this, MessageEvents::SEND_ERROR, m->getMessageHandle());
                    break;
                }

                if ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tm_start)) > sendTimeout)
                {
                    // Do something with timeout
                    m->setStatus(Message::Status::TIMEOUT_ON_SEND);
                    mMsgEvent.Trigger<int>(this, MessageEvents::TIMED_OUT_ON_SEND, m->getMessageHandle());  // Notify listeners

                    // Abort transfer
                    mImpl->Ept.cOutEpt->Abort();
                    if (mImpl->Ept.cOutEpt->LastError == ERROR_IO_PENDING)
                        WaitForSingleObject(outContext.OvLap->hEvent, 100);

                    // If there is anything connected still, we need to flush through the buffer
                    std::vector<uint8_t> flush (USBContext::MaxPacketSize);
                    LONG flush_size = USBContext::MaxPacketSize;
                    mImpl->Ept.cOutEpt->XferData((PUCHAR)&(flush[0]), flush_size);
                    break;
                }

                if (!mImpl->Ept.cOutEpt->WaitForXfer(outContext.OvLap, 10))
                {
                    // Xfer returned without completing, loop around and retry
                }
            } while (!HasOverlappedIoCompleted(outContext.OvLap));

            mImpl->Ept.cOutEpt->FinishDataXfer((PUCHAR)&((*(outContext.Buffer))[0]), outContext.bufLen, outContext.OvLap, outContext.context);
            if (m->getStatus() == Message::Status::UNSENT) {
                m->setStatus(Message::Status::SENT);
            }
            ResetEvent(outContext.OvLap->hEvent);

            // Indicate to receive thread that a receive transfer has been started
            if (m->getStatus() == Message::Status::SENT) {
                mImpl->m_rxBufcond.notify_one();
            }
		}
	}

	void CM_CYUSB::ResponseReceiver()
	{
		while (DeviceIsOpen == true)
		{
			{
				std::unique_lock<std::mutex> lck{ mImpl->m_rxBufmutex };
				//while (mImpl->m_rxBufcond.wait_for(lck, std::chrono::milliseconds(100)) == std::cv_status::timeout)
				//{
				//	// Timeout every 100ms to allow threads to terminate on disconnect
				//	if (DeviceIsOpen == false) break;
				//}
				mImpl->m_rxBufcond.wait_for(lck, std::chrono::milliseconds(100));

#if defined(_WIN32)
				// Determine how many buffers we are waiting for, then wait until at least one of them has been signalled
				HANDLE *h;
				//do {
				h = mImpl->getRxBufHandles();
				DWORD ret;
				if (mImpl->m_rxbuf_size == 0) {
					if (DeviceIsOpen == false)
					{
						// End thread
						lck.unlock();
						break;
					}
					else continue;
				}
				else if (mImpl->m_rxbuf_size == 1) {
					ret = WaitForSingleObject(mImpl->m_rxBuf.front()->OvLap->hEvent, 10);
					if (WAIT_TIMEOUT <= ret) continue;
				}
				else {
					ret = WaitForMultipleObjects(mImpl->m_rxbuf_size, h, FALSE, 10);
					// WAIT_OBJECT_n and WAIT_ABANDONED_n satisfy this condition
					if (WAIT_TIMEOUT <= ret) continue;
				}
				//} while (1);
				//if (mImpl->m_rxbuf_size == 0) continue;
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


				for (std::deque<std::shared_ptr<USBContext>>::iterator usb_it = mImpl->m_rxBuf.begin(); usb_it != mImpl->m_rxBuf.end();)
				{
					if (HasOverlappedIoCompleted((*usb_it)->OvLap))
					{
						MessageHandle msgHnd = (*usb_it)->handle;

						// Read completed but message may not have been added to list from sender thread yet, look for it first before reading into buffer.
                        auto& msg = m_msgRegistry.findMessage(msgHnd);
						if (msg != nullptr)
						{
							mImpl->Ept.cInEpt->FinishDataXfer((PUCHAR)&((*((*usb_it)->Buffer))[0]), (*usb_it)->bufLen, (*usb_it)->OvLap, (*usb_it)->context);
							ResetEvent((*usb_it)->OvLap->hEvent);

							(*(*usb_it)->Buffer).resize((*usb_it)->bufLen);
							mImpl->condenseBuffer(*(*usb_it)->Buffer);
							msg->AddBuffer(*(*usb_it)->Buffer);

							delete (*usb_it)->Buffer;
							usb_it = mImpl->m_rxBuf.erase(usb_it);
							//break;
						}
						//}
					}
					else { ++usb_it; }
				}
				lck.unlock();
			}

			// Signal Parser thread
			m_rxcond.notify_one();
		}
	}

	bool CM_CYUSB::MemoryDownload(boost::container::deque<uint8_t>& arr, uint32_t start_addr, int image_index, const std::array<uint8_t, 16>& uuid)
	{
		(void)image_index;
		(void)uuid;

 		// Only proceed if idle
		if (FastTransferStatus.load() != _FastTransferStatus::IDLE) {
			mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_NOT_IDLE, -1);
			return false;
		}
		// DMA Cannot accept addresses that aren't aligned to 64 bits
		if (start_addr & 0x7) return false;
		// Setup transfer
		int length = arr.size();
		length = (((length - 1) / CYUSB_Policy::TRANSFER_GRANULARITY) + 1) * CYUSB_Policy::TRANSFER_GRANULARITY;
		arr.resize(length);  // Increase the buffer size to the transfer granularity
		{
            CYUSB_Policy policy(start_addr);
			std::unique_lock<std::mutex> tfr_lck{ m_tfrmutex };
			mImpl->m_fti = new FastTransfer(arr, length, policy);
		}

		// Signal thread to do the grunt work
		FastTransferStatus.store(_FastTransferStatus::DOWNLOADING);
		m_tfrcond.notify_one();

		return true; 
	}

	bool CM_CYUSB::MemoryUpload(boost::container::deque<uint8_t>& arr, uint32_t start_addr, int len, int image_index, const std::array<uint8_t, 16>& uuid)
	{
		(void)image_index;
		(void)uuid;

		// Only proceed if idle
		if (FastTransferStatus.load() != _FastTransferStatus::IDLE) {
			mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_NOT_IDLE, -1);
			return false;
		}
		// DMA Cannot accept addresses that aren't aligned to 64 bits
		if (start_addr & 0x7) return false;
		// Setup transfer
		int length = (((len - 1) / CYUSB_Policy::TRANSFER_GRANULARITY) + 1) * CYUSB_Policy::TRANSFER_GRANULARITY;

		{
            CYUSB_Policy policy(start_addr);
			std::unique_lock<std::mutex> tfr_lck{ m_tfrmutex };
			mImpl->m_fti = new FastTransfer(arr, length, policy);
		}

		// Signal thread to do the grunt work
		FastTransferStatus.store(_FastTransferStatus::UPLOADING);
		m_tfrcond.notify_one();

		return true;
	}
	
	int CM_CYUSB::MemoryProgress()
	{
		return 1;
	}

	void CM_CYUSB::MemoryTransfer()
	{
		while (DeviceIsOpen == true)
		{
			{
				std::unique_lock<std::mutex> lck{ m_tfrmutex };
				while (!m_tfrcond.wait_for(lck, std::chrono::milliseconds(100), [&] {return mImpl->m_fti != nullptr; }))
				{
					if (FastTransferStatus.load() != _FastTransferStatus::IDLE)
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
				PUCHAR dataBuffer;
				//try
				//{
					if (CYUSB_Policy::DL_TRANSFER_SIZE > CYUSB_Policy::UL_TRANSFER_SIZE)
						dataBuffer = new UCHAR[CYUSB_Policy::DL_TRANSFER_SIZE];
					else
						dataBuffer = new UCHAR[CYUSB_Policy::UL_TRANSFER_SIZE];
				//}
				//catch (std::bad_alloc& exc)
				//{
//					std::cout << exc.what() << std::endl;
				//}
				LONG bytesTransferred = 0;

				if (FastTransferStatus.load() == _FastTransferStatus::UPLOADING) mImpl->m_fti->m_data.clear();

#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
				boost::chrono::steady_clock::time_point start = boost::chrono::steady_clock::now();
				boost::chrono::duration<double, boost::milli> copy_time(0.0);
				boost::chrono::duration<double, boost::milli> xfer_time(0.0);
#endif

				for (int i = 0; i < mImpl->m_fti->m_transCount; i++) {
					// Prime DMA Transfer
					HostReport *iorpt;
					uint32_t len = static_cast<uint32_t>(mImpl->m_fti->m_transBytesRemaining);
					uint32_t addr = mImpl->m_fti->m_policy.addr + i * CYUSB_Policy::TRANSFER_UNIT;
					if (FastTransferStatus.load() == _FastTransferStatus::DOWNLOADING) {
						iorpt = new HostReport(HostReport::Actions::CTRLR_IMGDMA, HostReport::Dir::WRITE, ImageDMA_Download);
						iorpt->Payload<std::vector<uint32_t>>({ len, addr });
						DeviceReport resp = this->SendMsgBlocking(*iorpt);
						if (!resp.Done()) break;
						delete iorpt;

						// Copy a buffer's worth of data
						while (mImpl->m_fti->m_transBytesRemaining > 0) {
#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
							boost::chrono::steady_clock::time_point t_pre_copy = boost::chrono::steady_clock::now();
#endif
							LONG dl_len = CYUSB_Policy::DL_TRANSFER_SIZE;
							if (mImpl->m_fti->m_transBytesRemaining < CYUSB_Policy::DL_TRANSFER_SIZE) dl_len = mImpl->m_fti->m_transBytesRemaining;
							auto buf_start = mImpl->m_fti->m_data_it;
							auto buf_end = mImpl->m_fti->m_data_it + dl_len;
							std::copy(buf_start, buf_end, dataBuffer);
							mImpl->m_fti->m_data_it += dl_len;
							mImpl->m_fti->m_transBytesRemaining -= dl_len;
#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
							boost::chrono::steady_clock::time_point t_pre_xfer = boost::chrono::steady_clock::now();
#endif
							PUCHAR tfr_ptr = dataBuffer;
							LONG tfr_len;
							do {
								tfr_len = dl_len;
								if (tfr_len < CYUSB_Policy::TRANSFER_GRANULARITY) tfr_len = CYUSB_Policy::TRANSFER_GRANULARITY;
								mImpl->Ept.bOutEpt->XferData(tfr_ptr, tfr_len);
								tfr_ptr += tfr_len;
								bytesTransferred += tfr_len;
							} while ((dl_len -= tfr_len) > 0);
#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
							boost::chrono::steady_clock::time_point t_final = boost::chrono::steady_clock::now();
							copy_time += (t_pre_xfer - t_pre_copy);
							xfer_time += (t_final - t_pre_xfer);
#endif
						}
					}
					else if (FastTransferStatus.load() == _FastTransferStatus::UPLOADING) {
						iorpt = new HostReport(HostReport::Actions::CTRLR_IMGDMA, HostReport::Dir::WRITE, ImageDMA_Upload);
						iorpt->Payload<std::vector<uint32_t>>({ len, addr });
						DeviceReport resp = this->SendMsgBlocking(*iorpt);
						if (!resp.Done()) break;
						delete iorpt;

						// Retrieve a buffer's worth of data
						while (mImpl->m_fti->m_transBytesRemaining > 0) {
#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
							boost::chrono::steady_clock::time_point t_pre_xfer = boost::chrono::steady_clock::now();
#endif
							LONG ul_len = CYUSB_Policy::UL_TRANSFER_SIZE;
							if (mImpl->m_fti->m_transBytesRemaining < CYUSB_Policy::UL_TRANSFER_SIZE) ul_len = mImpl->m_fti->m_transBytesRemaining;
							mImpl->m_fti->m_transBytesRemaining -= ul_len;

							PUCHAR tfr_ptr = dataBuffer;
							LONG tfr_len;
							LONG buf_len = ul_len;
							do {
								tfr_len = ul_len;
								if (tfr_len < CYUSB_Policy::TRANSFER_GRANULARITY) tfr_len = CYUSB_Policy::TRANSFER_GRANULARITY;
								mImpl->Ept.bInEpt->XferData(tfr_ptr, tfr_len);
								tfr_ptr += tfr_len;
								bytesTransferred += tfr_len;
							} while ((ul_len -= tfr_len) > 0);
#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
							boost::chrono::steady_clock::time_point t_pre_copy = boost::chrono::steady_clock::now();
#endif
							mImpl->m_fti->m_data.insert(mImpl->m_fti->m_data.end(), dataBuffer, dataBuffer + buf_len);
							//mImpl->m_fti->m_data_it += len;
#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
							boost::chrono::steady_clock::time_point t_final = boost::chrono::steady_clock::now();
							copy_time += (t_final - t_pre_copy);
							xfer_time += (t_pre_copy - t_pre_xfer);
#endif
						}
					}
					// Set up index data for next DMA Transaction
					mImpl->m_fti->startNextTransaction();
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

				delete[] dataBuffer;
				delete mImpl->m_fti;
				mImpl->m_fti = nullptr;

				FastTransferStatus.store(_FastTransferStatus::IDLE);
				mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_COMPLETE, bytesTransferred);
			}
		}
	}

	void CM_CYUSB::InterruptReceiver()
	{
		while (DeviceIsOpen == true)
		{
			// Not required to have interrupt functionality
			if (mImpl->Ept.iInEpt == nullptr) break;

			std::vector<uint8_t> interruptData(64, 0);
			LONG bufLen = 64;

			mImpl->Ept.iInEpt->TimeOut = 100;
			while (!mImpl->Ept.iInEpt->XferData((PUCHAR)&(interruptData[0]), bufLen))
			{
				// Timeout.
				if (DeviceIsOpen == false) break;
				// Reinitialise
				bufLen = interruptData.size();
			}
			if (DeviceIsOpen == false) break;

			//std::cout << "Received an interrupt!" << std::endl;
			std::shared_ptr<Message> m = std::make_shared<Message>(HostReport());
			m->setStatus(Message::Status::INTERRUPT);
			interruptData.resize(bufLen);
			m->AddBuffer(interruptData);

			// Place in list for processing by receive thread
            m_msgRegistry.addMessage(m->getMessageHandle(), m);        

            // Signal Parser thread
			m_rxcond.notify_one();
		}
	}

}

#endif
