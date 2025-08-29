/*-----------------------------------------------------------------------------
/ Title      : Connection Manager Implementation for RS422 Connections
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/src/CM_RS422.cpp $
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

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <codecvt>
#include <atomic>

#include "CM_RS422.h"
#include "IMSSystem.h"

#ifdef WIN32
#define CENUMERATESERIAL_USE_STL
#include "atlbase.h"
#include "setupapi.h"
#include "enumser.h"
#endif

#if defined _WIN32 && defined _DEBUG
#include "crtdbg.h"
#define DEBUG_NEW new( _NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace iMS {

	class CM_RS422::FastTransfer
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


	// All private data and member functions contained within Impl class
	class CM_RS422::Impl
	{
	public:
		Impl(IConnectionManager* parent);
		~Impl();

		static const int MaxMessageSize = 73;
		static const int RxBufferSize = 4096;

		const std::string Ident = "CM_SERIAL";
		ListBase<std::string> PortMask;

		std::vector<IMSSystem> ListConnectedDevices();

		std::map<std::string, std::string>* rs422_list = nullptr;
		HANDLE fp;

		/*std::deque<std::shared_ptr<CM_RS422::MsgContext>> m_rxBuf;
		int m_rxbuf_size;
		mutable std::mutex m_rxBufmutex;
		std::condition_variable m_rxBufcond;*/
		mutable std::timed_mutex m_rdwrmutex;

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

	CM_RS422::Impl::Impl(IConnectionManager* parent) : m_parent(parent)
	{
	}

	CM_RS422::Impl::~Impl()
	{
		if (rs422_list != nullptr) {
			delete rs422_list;
			rs422_list = nullptr;
		}
	}
	
	std::vector<IMSSystem> CM_RS422::Impl::ListConnectedDevices()
	{
		//CEnumerateSerial::CPortsArray ports;
		CEnumerateSerial::CNamesArray names;
		std::vector<IMSSystem> IMSList;
		std::string port;

		// If we have an open connection, we can't allow the driver to be opened again to look for devices
		if (m_parent->Open()) return (std::vector<IMSSystem>());

#ifdef WIN32
		// Get available ports
		if (CEnumerateSerial::UsingRegistry(names))
		{
#ifdef CENUMERATESERIAL_USE_STL
			for (int i = 0; i < (int)names.size(); i++) {
				//				_tprintf(_T("%s\n"), names[i].c_str());
//				swprintf_s(port, L"\\\\.\\%s", names[i].c_str());       //create a comport name, from COM0 to COM31.  Port numbers greater than 9 require backslash prefix
#else
			for (i = 0; i<names.GetSize(); i++) {
				//				_tprintf(_T("%s\n"), names[i].operator LPCTSTR());
				//swprintf_s(port, L"\\\\.\\%s",  names[i].operator LPCTSTR());       //create a comport name, from COM0 to COM31.  Port numbers greater than 9 require backslash prefix
#endif
#ifdef _UNICODE
				using convert_type = std::codecvt_utf8<wchar_t>;
				std::wstring_convert<convert_type, wchar_t> converter;
				port = converter.to_bytes(names[i]);
#else
				port = names[i];
#endif
				if (!this->PortMask.empty())
				{
					// User supplied a mask list. Check if this interface is included
					bool match = false;
					for (std::list<std::string>::const_iterator it = this->PortMask.cbegin(); it != this->PortMask.cend(); ++it)
					{
						if (port == *it) {
							match = true;
							break;
						}
					}
					if (!match) continue;
				}

				const std::string false_serial("iMSP9999");
				(*rs422_list)[false_serial] = port;

				// Then open device and send a Magic Number query message
				//ftStatus = FT_OpenEx(devInfo.SerialNumber, FT_OPEN_BY_SERIAL_NUMBER, &ftdiDevice);
				m_parent->Connect(false_serial);
				rs422_list->erase(false_serial);

				if (!m_parent->Open())
					continue;

				HostReport *iorpt;
				iorpt = new HostReport(HostReport::Actions::CTRLR_SETTINGS, HostReport::Dir::READ, 0);
				ReportFields fields = iorpt->Fields();
				fields.len = 16; // Read first 16 bytes of controller settings
				iorpt->Fields(fields);
				DeviceReport Resp = m_parent->SendMsgBlocking(*iorpt);

				if (!Resp.Done()) {
					m_parent->Disconnect();
					continue;
				}

				const std::string serial = Resp.Payload<std::string>() + ":" + port;

				// Store serial number along with USB Device count index in cyusb_list map
				(*rs422_list)[serial] = port;

				if (m_parent->Open())
				{
					// Found a suitable connection interface, let's query its contents.
					// An FTDI based board can have a maximum of one controller and one synthesiser.
					IMSSystem thisiMS(m_parent, serial);

					thisiMS.Initialise();

					if (thisiMS.Ctlr().IsValid() || thisiMS.Synth().IsValid()) {
						IMSList.push_back(thisiMS);
					}
				}

				m_parent->Disconnect();
			}
		}
		//else
			//_tprintf(_T("CEnumerateSerial::UsingRegistry failed, Error:%u\n"), GetLastError());

#endif
		return IMSList;
	}

	// Default Constructor
	CM_RS422::CM_RS422() : mImpl(new CM_RS422::Impl(this))
	{
		sendTimeout = std::chrono::milliseconds(1000);  // needs to be a bit longer than some of the other CMs otherwise connection attempt can fail
		rxTimeout = std::chrono::milliseconds(5000);
		autoFreeTimeout = std::chrono::milliseconds(30000);
	}

	CM_RS422::~CM_RS422()
	{
		this->Disconnect();
		 
		delete mImpl;
		mImpl = NULL;
	}

	const std::string& CM_RS422::Ident() const
	{
		return mImpl->Ident;
	}

	std::vector<IMSSystem> CM_RS422::Discover(const ListBase<std::string>& PortMask)
	{
//		std::cout << "CM_RS422::Discover()" << std::endl;
		mImpl->PortMask = PortMask;
		mImpl->rs422_list = new std::map<std::string, std::string>();
		std::vector<IMSSystem> v = mImpl->ListConnectedDevices();
		return v;
	}

	void CM_RS422::Connect(const std::string& serial)
	{
		if (!DeviceIsOpen)
		{
			// If connecting without first performing a scan, carry it out here
			if (mImpl->rs422_list == nullptr) {
				mImpl->rs422_list = new std::map<std::string, std::string>();
                mImpl->ListConnectedDevices();
            }

			if (mImpl->fp != NULL)
				this->Disconnect();

			std::string portname;
			// Remove trailing interface descriptor, if present
			std::string serial_ = serial.substr(0, serial.find_first_of(":"));

			// Look for serial number in internal list
			if (mImpl->rs422_list->count(serial_) > 0) {
				// Found it
				portname = (*mImpl->rs422_list)[serial_];
			}
			else {
				// search for the word "COM" and if found, substring from it to the end of the string.
				std::size_t found = serial.find("COM");
				if (found != std::string::npos) {
					portname = serial.substr(found, std::string::npos);
				}
			}

			if (portname.empty()) return;

			portname.insert(0, "\\\\.\\");
#ifdef UNICODE
			const char* cs = portname.c_str();
			size_t wn = mbsrtowcs(NULL, &cs, 0, NULL);
			if (wn == size_t(-1)) return;

			wchar_t* port = new wchar_t[wn + 1]();
			wn = mbsrtowcs(port, &cs, wn + 1, NULL);
#else 
			const char* port = portname.c_str();
#endif

			mImpl->fp = CreateFile(port,
				GENERIC_READ | GENERIC_WRITE,
				0, /* exclusive access */
				NULL, /* no security attrs */
				OPEN_EXISTING,
				FILE_FLAG_OVERLAPPED,   /* Or NULL for non-overlapped */
				NULL
				);
#ifdef UNICODE
			delete[] port;
#endif

			if (mImpl->fp == INVALID_HANDLE_VALUE) return; //Not present on this port

			COMMTIMEOUTS ctmoNew = { 0 };

			GetCommTimeouts(mImpl->fp, &ctmoNew);
			ctmoNew.ReadIntervalTimeout = 50;
			ctmoNew.ReadTotalTimeoutConstant = 20;
			ctmoNew.ReadTotalTimeoutMultiplier = 250;
			ctmoNew.WriteTotalTimeoutMultiplier = 20;
			ctmoNew.WriteTotalTimeoutConstant = 250;
			if (!SetCommTimeouts(mImpl->fp, &ctmoNew))
			{
				// We need timeouts.
				CloseHandle(mImpl->fp);
				return;
			}

			//Get DCB and set appropriate values, store old DCB for when program exits
			DCB stDeviceControl = { 0 };
			stDeviceControl.DCBlength = sizeof(stDeviceControl);

			if (GetCommState(mImpl->fp, &stDeviceControl))
			{
				stDeviceControl.BaudRate = CBR_115200;
				stDeviceControl.Parity = NOPARITY;
				stDeviceControl.ByteSize = 8;
				stDeviceControl.StopBits = ONESTOPBIT;
			}
			else {
				CloseHandle(mImpl->fp);
				return;
			}

			if (!SetCommState(mImpl->fp, &stDeviceControl))
			{
				// We cannot set the serial port parameters, so close it
				CloseHandle(mImpl->fp);
				return;
			}

			DeviceIsOpen = true;

			// Clear Message Lists
			m_list.clear();
			while (!m_queue.empty()) m_queue.pop();
			while (!m_rxCharQueue.empty()) m_rxCharQueue.pop();

			// Start Report Sending Thread
			senderThread = std::thread(&CM_RS422::MessageSender, this);

			// Start Response Receiver Thread
			receiverThread = std::thread(&CM_RS422::ResponseReceiver, this);

			// Start Response Parser Thread
			parserThread = std::thread(&CM_RS422::MessageListManager, this);

			// Start Memory Transferer Thread
			mImpl->memoryTransferThread = std::thread(&CM_RS422::MemoryTransfer, this);

			// Start Interrupt receiving thread
			mImpl->interruptThread = std::thread(&CM_RS422::InterruptReceiver, this);
		}
	}

	void CM_RS422::Disconnect()
	{
		if (DeviceIsOpen)
		{
			if (mImpl->fp == NULL) {
				DeviceIsOpen = false;
				return;
			}

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
						(msg->getStatus() == Message::Status::RX_PARTIAL))
					{
						msg_waiting = true;
					}
				}
				list_lck.unlock();
				if (msg_waiting)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(250));
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
			mImpl->memoryTransferThread.join();  // TODO: need to abort in-flight transfers
			mImpl->interruptThread.join();

			if (mImpl->fp != NULL)
			{
				CloseHandle(mImpl->fp);
				mImpl->fp = NULL;
			}
		}

	}

	void CM_RS422::SetTimeouts(int send_timeout_ms, int rx_timeout_ms, int free_timeout_ms, int discover_timeout_ms)
	{
		(void)discover_timeout_ms;
		sendTimeout = std::chrono::milliseconds(send_timeout_ms);
		rxTimeout = std::chrono::milliseconds(rx_timeout_ms);
		autoFreeTimeout = std::chrono::milliseconds(free_timeout_ms);
	}

	void CM_RS422::MessageSender()
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

				// Get HostReport bytes and send to device
				DWORD numBytesWritten = 0, totalBytesWritten = 0;
				DWORD dwRes;
				std::vector<uint8_t> b = m->SerialStream();
				int status = 0;

				std::chrono::time_point<std::chrono::high_resolution_clock> tm_start = std::chrono::high_resolution_clock::now();
				do
				{
					if ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tm_start)) > sendTimeout)
					{
						// Do something with timeout
						m->setStatus(Message::Status::TIMEOUT_ON_SEND);
						mMsgEvent.Trigger<int>(this, MessageEvents::TIMED_OUT_ON_SEND, m->getMessageHandle());  // Notify listeners

						break;
					}

					// Timeout allows thread to cancel message
					std::unique_lock<std::timed_mutex> rdwrlck(mImpl->m_rdwrmutex, std::chrono::milliseconds(10));
					if (!rdwrlck.owns_lock()) {
						status = 1;
						continue;
					}

					// Send Data
					OVERLAPPED osWrite = { 0 };

					// Create this writes OVERLAPPED structure hEvent.
					osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
					if (osWrite.hEvent != NULL) {
						// Issue write.
						if (!WriteFile(mImpl->fp, &b[totalBytesWritten], (b.size() - totalBytesWritten), &numBytesWritten, &osWrite)) {
							if (GetLastError() != ERROR_IO_PENDING) {
								// WriteFile failed, but it isn't delayed. Report error and abort.
								status = 0;
							}
							else {
								dwRes = WaitForSingleObject(osWrite.hEvent, INFINITE);
								switch (dwRes)
								{
									// Event occurred.
								case WAIT_OBJECT_0: {
									if (!GetOverlappedResult(mImpl->fp, &osWrite, &numBytesWritten, FALSE)) {
										status = 1;
									}
									else {
										// Write operation completed successfully.
										status = 1;
										totalBytesWritten += numBytesWritten;
										//std::cout << "W ";
									}
									break;
								}
								case WAIT_TIMEOUT: {	
									status = 1;
									break;
								}
								case WAIT_FAILED: {
								default:
									status = 0;
									break;
								}
								}
							}
						}
						else {
							// WriteFile completed immediately.
							status = 1;
							totalBytesWritten += numBytesWritten;
						}

						CloseHandle(osWrite.hEvent);
					}

					rdwrlck.unlock();

					if (!status || !numBytesWritten)
					{
						// Do something with failure
						m->setStatus(Message::Status::SEND_ERROR);
						mMsgEvent.Trigger<int>(this, MessageEvents::SEND_ERROR, m->getMessageHandle());
						break;
					}

				} while (totalBytesWritten < b.size() && status);

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

				m_queue.pop();  // delete from queue
			}
			lck.unlock();
		}
	}

	void CM_RS422::ResponseReceiver()
	{
		DWORD dwRead;
		DWORD dwRes;
		unsigned char  chRead[Impl::RxBufferSize];
		BOOL fWaitingOnRead = FALSE;
		BOOL fHandleError = FALSE;
		OVERLAPPED osReader = { 0 };
		int retries = 0;

		//if (!SetCommMask(mImpl->fp, EV_RXCHAR)) {
			// Error setting communications event mask
		//}
		
		while ((DeviceIsOpen == true) && (retries++<10))
		{
			osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			if (osReader.hEvent == NULL)
				// error creating event; abort
				return;

			while (!fHandleError && (DeviceIsOpen == true))
			{
				// Timeout allows thread to terminate
				std::unique_lock<std::timed_mutex> rdwrlck(mImpl->m_rdwrmutex, std::chrono::milliseconds(10));
				if (!rdwrlck.owns_lock()) continue;

				if (!fWaitingOnRead) {
					
					if (!ReadFile(mImpl->fp, chRead, Impl::RxBufferSize, &dwRead, &osReader)) {
						if (GetLastError() == ERROR_IO_PENDING)
							fWaitingOnRead = TRUE;
						else {
							// error in WaitCommEvent; abort
							fHandleError = TRUE;
							break;
						}
					}
					else {
						// ReadFile returned immediately.
						std::unique_lock<std::mutex> rxlck{ m_rxmutex };
						//std::cout << "A: ";
						for (DWORD i = 0; i < dwRead; i++) {
							m_rxCharQueue.push(chRead[i]);
							//std::cout << std::hex;
							//std::cout << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(chRead[i]) << " ";
						}
						rxlck.unlock();
						m_rxcond.notify_one();
					}
//					rdwrlck.unlock();
				}

				// Check on overlapped operation.
				if (fWaitingOnRead) {
					// Wait a little while for an event to occur.
					dwRes = WaitForSingleObject(osReader.hEvent, 5);
					//std::unique_lock<std::mutex> rdwrlck{ mImpl->m_rdwrmutex };
					switch (dwRes)
					{
						// Event occurred.
					case WAIT_OBJECT_0:
						if (!GetOverlappedResult(mImpl->fp, &osReader, &dwRead, FALSE)) {
							// An error occurred in the overlapped operation;
							// call GetLastError to find out what it was
							// and abort if it is fatal.
							if (GetLastError() == ERROR_OPERATION_ABORTED) {
								fHandleError = TRUE;
								break;
							}
							fHandleError = TRUE;
							break;
						}
						else {
							std::unique_lock<std::mutex> rxlck{ m_rxmutex };
							//std::cout << "B(" << dwRead << "): ";
							for (DWORD i = 0; i < dwRead; i++) {
								m_rxCharQueue.push(chRead[i]);
								//std::cout << std::hex;
								//std::cout << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(chRead[i]) << " ";
							}
							rxlck.unlock();
							m_rxcond.notify_one();
						}

						// Set fWaitingOnRead flag to indicate that a new
						// ReadFile is to be issued.
						fWaitingOnRead = FALSE;
						break;

					case WAIT_TIMEOUT:
						// Operation isn't complete yet. fWaitingOnStatusHandle flag 
						// isn't changed since I'll loop back around and I don't want
						// to issue another WaitCommEvent until the first one finishes.
						break;

					default:
						// Error in the WaitForSingleObject; abort
						// This indicates a problem with the OVERLAPPED structure's
						// event handle.
						//CloseHandle(osStatus.hEvent);
						fHandleError = TRUE;
						break;
						//return;
					}
				}
				rdwrlck.unlock();

			}
			if (fHandleError) {
				fHandleError = FALSE;
			}
			CloseHandle(osReader.hEvent);
		}
	}

	bool CM_RS422::MemoryDownload(boost::container::deque<uint8_t>& arr, uint32_t start_addr, int image_index, const std::array<uint8_t, 16>& uuid)
	{
		(void)uuid;

		// Only proceed if idle
		if (mImpl->FastTransferStatus.load() != _FastTransferStatus::IDLE) {
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
			std::unique_lock<std::mutex> tfr_lck{ mImpl->m_tfrmutex };
			mImpl->fti = new FastTransfer(arr, start_addr, length, image_index);
		}

		// Signal thread to do the grunt work
		mImpl->FastTransferStatus.store(_FastTransferStatus::DOWNLOADING);
		mImpl->m_tfrcond.notify_one();

		return true;
	}

	bool CM_RS422::MemoryUpload(boost::container::deque<uint8_t>& arr, uint32_t start_addr, int len, int image_index, const std::array<uint8_t, 16>& uuid)
	{
		(void)uuid;

		// Only proceed if idle
		if (mImpl->FastTransferStatus.load() != _FastTransferStatus::IDLE) {
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
			std::unique_lock<std::mutex> tfr_lck{ mImpl->m_tfrmutex };
			mImpl->fti = new FastTransfer(arr, start_addr, len, image_index);
		}

		// Signal thread to do the grunt work
		mImpl->FastTransferStatus.store(_FastTransferStatus::UPLOADING);
		mImpl->m_tfrcond.notify_one();

		return true;
	}

	void CM_RS422::MemoryTransfer()
	{
		while (DeviceIsOpen == true)
		{
			{
				std::unique_lock<std::mutex> lck{ mImpl->m_tfrmutex };
				while (!mImpl->m_tfrcond.wait_for(lck, std::chrono::milliseconds(100), [&] {return mImpl->fti != nullptr; }))
				{
					if (mImpl->FastTransferStatus.load() != _FastTransferStatus::IDLE)
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

				if (mImpl->FastTransferStatus.load() == _FastTransferStatus::UPLOADING) mImpl->fti->m_data.clear();

#if defined(DMA_PERFORMANCE_MEASUREMENT_MODE)
				boost::chrono::steady_clock::time_point start = boost::chrono::steady_clock::now();
				boost::chrono::duration<double, boost::milli> copy_time(0.0);
				boost::chrono::duration<double, boost::milli> xfer_time(0.0);
#endif

				for (unsigned int i = 0; i < mImpl->fti->m_transCount; i++) {
					// Prime DMA Transfer
					HostReport *iorpt;
					//uint32_t len = static_cast<uint32_t>(mImpl->fti->m_transBytesRemaining);
					//uint32_t addr = mImpl->fti->m_addr + i * FastTransfer::TRANSFER_GRANULARITY;
					if (mImpl->FastTransferStatus.load() == _FastTransferStatus::DOWNLOADING) {
						iorpt = new HostReport(HostReport::Actions::CTRLR_IMAGE, HostReport::Dir::WRITE, ((mImpl->fti->m_currentTrans-1) & 0xFFFF));
						if (mImpl->fti->m_currentTrans > 0x10000) {
							ReportFields f = iorpt->Fields();
							f.context = static_cast<std::uint8_t>((mImpl->fti->m_currentTrans-1) >> 16);
							iorpt->Fields(f);
						}
						LONG len = FastTransfer::DL_TRANSFER_SIZE;
						if (mImpl->fti->m_transBytesRemaining < FastTransfer::DL_TRANSFER_SIZE) len = mImpl->fti->m_transBytesRemaining;
						auto buf_start = mImpl->fti->m_data_it;
						auto buf_end = mImpl->fti->m_data_it + len;
						std::vector<uint8_t> dataBuffer(buf_start, buf_end);
//						std::copy(buf_start, buf_end, dataBuffer);
						mImpl->fti->m_data_it += len;
						mImpl->fti->m_transBytesRemaining -= len;
						iorpt->Payload<std::vector<uint8_t>>(dataBuffer);
						DeviceReport resp = this->SendMsgBlocking(*iorpt);
						delete iorpt;
						if (!resp.Done()) {
							break;
						}
						bytesTransferred += len;
					}
					else if (mImpl->FastTransferStatus.load() == _FastTransferStatus::UPLOADING) {
						uint16_t len = FastTransfer::UL_TRANSFER_SIZE;
						if (mImpl->fti->m_transBytesRemaining < FastTransfer::UL_TRANSFER_SIZE) len = static_cast<std::uint16_t>(mImpl->fti->m_transBytesRemaining);
						iorpt = new HostReport(HostReport::Actions::CTRLR_IMAGE, HostReport::Dir::READ, ((mImpl->fti->m_currentTrans - 1) & 0xFFFF));
						ReportFields f = iorpt->Fields();
						if (mImpl->fti->m_currentTrans > 0x10000) {
							f.context = static_cast<std::uint8_t>((mImpl->fti->m_currentTrans - 1) >> 16);
						}
						f.len = len;
						iorpt->Fields(f);
						DeviceReport resp = this->SendMsgBlocking(*iorpt);
						if (resp.Done()) {
//							std::copy(resp.Payload<std::vector<uint8_t>>().cbegin(), resp.Payload<std::vector<uint8_t>>().cend(), dataBuffer);
							std::vector<uint8_t> vfy_data = resp.Payload<std::vector<uint8_t>>();
							mImpl->fti->m_data.insert(mImpl->fti->m_data.end(), vfy_data.begin(), vfy_data.end());
//							mImpl->fti->m_data_it += len;
							mImpl->fti->m_transBytesRemaining -= len;
							bytesTransferred += len;
						}
						else {
							break;
						}
						delete iorpt;
					}
					// Set up index data for next DMA Transaction
					mImpl->fti->startNextTransaction();
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
				delete mImpl->fti;
				mImpl->fti = nullptr;

				mImpl->FastTransferStatus.store(_FastTransferStatus::IDLE);
				mMsgEvent.Trigger<int>(this, MessageEvents::MEMORY_TRANSFER_COMPLETE, bytesTransferred);
			}
		}

	}

	void CM_RS422::InterruptReceiver()
	{

	}

}

#endif
