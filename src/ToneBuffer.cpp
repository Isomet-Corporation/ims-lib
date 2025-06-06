    /*-----------------------------------------------------------------------------
/ Title      : Isomet ToneBuffer Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ToneBuffer/src/ToneBuffer.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2016-02-24
/ Last update: $Date: 2024-12-18 16:58:21 +0000 (Wed, 18 Dec 2024) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 648 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2016 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2016-02-24  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

#include "ToneBuffer.h"
#include "IEventTrigger.h"
#include "Message.h"
#include "IConnectionManager.h"
#include "Image.h"
#include "FileSystem.h"
#include "FileSystem_p.h"
#include "IMSTypeDefs_p.h"
#include "IMSConstants.h"

#include <mutex>
#include <condition_variable>
#include <thread>
#include <list>
#include <iostream>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

namespace iMS {

	class ToneBufferEventTrigger :
		public IEventTrigger
	{
	public:
		ToneBufferEventTrigger() { updateCount(ToneBufferEvents::Count); }
		~ToneBufferEventTrigger() {};
	};


	class ToneBuffer::Impl
	{
	public:
		Impl(const std::string& name = "") : m_name(name), tag(boost::uuids::random_generator()()), tagDirty(false) {};
		Impl(const TBEntry& tbe, const std::string& name = "") : m_name(name), tag(boost::uuids::random_generator()()), tagDirty(false) { array.fill(tbe); };
		~Impl() {}

		// Update the UUID whenever the image has been modified
		void updateUUID() {
			//this->tag = boost::uuids::random_generator()();
			tagDirty = true;
		}
		void refreshTag() {
			if (tagDirty) {
				this->tag = boost::uuids::random_generator()();
				tagDirty = false;
			}
		}

		bool tagDirty;
		boost::uuids::uuid tag;

		TBArray array;
		std::string m_name;
	};

	ToneBuffer::ToneBuffer(const std::string& name) : p_Impl(new Impl(name)) {};
	// Fill Constructor
	ToneBuffer::ToneBuffer(const TBEntry& tbe, const std::string& name) : p_Impl(new Impl(tbe, name)) {};
	// Non-volatile Memory Constructor
	ToneBuffer::ToneBuffer(const int entry, const std::string& name) : p_Impl(new Impl(name)) {
		// Add filesystem read stuff here...
	};

	ToneBuffer::~ToneBuffer() { delete p_Impl; p_Impl = nullptr; };

	// Copy & Assignment Constructors
	ToneBuffer::ToneBuffer(const ToneBuffer &rhs) : p_Impl(new Impl())
	{
		p_Impl->tag = rhs.p_Impl->tag;
		p_Impl->array = rhs.p_Impl->array;
		p_Impl->m_name = rhs.p_Impl->m_name;
	};

	ToneBuffer& ToneBuffer::operator =(const ToneBuffer &rhs)
	{
		if (this == &rhs) return *this;
		p_Impl->tag = rhs.p_Impl->tag;
		p_Impl->array = rhs.p_Impl->array;
		p_Impl->m_name = rhs.p_Impl->m_name;
		return *this;
	};

	bool ToneBuffer::operator==(ToneBuffer const& rhs) const {
		return (p_Impl->array == rhs.p_Impl->array && p_Impl->m_name == rhs.p_Impl->m_name);
	}

	ToneBuffer::iterator ToneBuffer::begin() 	{ return p_Impl->array.begin(); }
	ToneBuffer::iterator ToneBuffer::end()   	{ return p_Impl->array.end(); }
	ToneBuffer::const_iterator ToneBuffer::begin() const 	{ return p_Impl->array.cbegin(); }
	ToneBuffer::const_iterator ToneBuffer::end() const  	{ return p_Impl->array.cend(); }
	ToneBuffer::const_iterator ToneBuffer::cbegin() const 	{ return begin(); }
	ToneBuffer::const_iterator ToneBuffer::cend() const  	{ return end(); }

	const std::array<std::uint8_t, 16> ToneBuffer::UUID() const {
		p_Impl->refreshTag();
		boost::uuids::uuid u = p_Impl->tag;
		std::array<std::uint8_t, 16> v;
		std::copy_n(u.begin(), 16, v.begin());
		return v;
	}

	// Random Access (read-only) to a TBEntry in the ToneBuffer
	const TBEntry& ToneBuffer::operator[](std::size_t idx) const
	{
		// out_of_range exception thrown if idx greater than ToneBuffer size
		return p_Impl->array.at(idx);
	}

	// Random Access to a TBEntry in the ToneBuffer
	TBEntry& ToneBuffer::operator[](std::size_t idx)
	{
		this->p_Impl->updateUUID();
		// out_of_range exception thrown if idx greater than ToneBuffer size
		return p_Impl->array.at(idx);
	}

	const std::size_t ToneBuffer::Size() const { return p_Impl->array.size(); };

	const std::string& ToneBuffer::Name() const
	{
		return p_Impl->m_name;
	}

	std::string& ToneBuffer::Name()
	{
		return p_Impl->m_name;
	}

	class ToneBufferDownload::Impl
	{
	public:
		Impl(IMSSystem&, const ToneBuffer& tb);
		~Impl();

		IMSSystem& myiMS;
		const ToneBuffer& m_tb;

		ToneBufferEventTrigger m_Event;

		void AddPointToVector(std::vector<std::uint8_t>&, const TBEntry&, bool toEEPROM);

		class ResponseReceiver : public IEventHandler
		{
		public:
			ResponseReceiver(ToneBufferDownload::Impl* dl) : m_parent(dl) {};
			void EventAction(void* sender, const int message, const int param);
		private:
			ToneBufferDownload::Impl* m_parent;
		};
		ResponseReceiver* Receiver;

		ToneBuffer::const_iterator first;
		ToneBuffer::const_iterator last;
		int offset;

		std::list <MessageHandle> dl_list;
		MessageHandle dl_final;
		mutable std::mutex dl_list_mutex;

		bool downloaderRunning{ false };
		std::thread downloadThread;
		mutable std::mutex m_dlmutex;
		std::condition_variable m_dlcond;
		void DownloadWorker();

		std::thread RxThread;
		mutable std::mutex m_rxmutex;
		std::condition_variable m_rxcond;
		std::deque<int> rxok_list;
		std::deque<int> rxerr_list;
		void RxWorker();

		FileSystemWriter* fsw;

		
	};

	ToneBufferDownload::Impl::Impl(IMSSystem& iMS, const ToneBuffer& tb) :
		myiMS(iMS), m_tb(tb), Receiver(new ResponseReceiver(this))
	{
		downloaderRunning = true;

		// Subscribe listeners
		IConnectionManager * const myiMSConn = myiMS.Connection();
		myiMSConn->MessageEventSubscribe(MessageEvents::SEND_ERROR, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::TIMED_OUT_ON_SEND, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_RECEIVED, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_VALID, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_TIMED_OUT, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_CRC, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_INVALID, Receiver);

		// Start a thread to run the download in the background
		downloadThread = std::thread(&ToneBufferDownload::Impl::DownloadWorker, this);

		// And a thread to receive the download responses
		RxThread = std::thread(&ToneBufferDownload::Impl::RxWorker, this);
	}

	ToneBufferDownload::Impl::~Impl() {
		// Unblock worker thread
		downloaderRunning = false;
		m_dlcond.notify_one();
		m_rxcond.notify_one();

		downloadThread.join();
		RxThread.join();

		// Unsubscribe listener
		IConnectionManager * const myiMSConn = myiMS.Connection();
		myiMSConn->MessageEventUnsubscribe(MessageEvents::SEND_ERROR, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::TIMED_OUT_ON_SEND, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_RECEIVED, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_ERROR_VALID, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_TIMED_OUT, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_ERROR_CRC, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_ERROR_INVALID, Receiver);

		delete Receiver;
	}

	void ToneBufferDownload::Impl::ResponseReceiver::EventAction(void* sender, const int message, const int param)
	{
		switch (message)
		{
		case (MessageEvents::RESPONSE_RECEIVED) :
		case (MessageEvents::RESPONSE_ERROR_VALID) : {

			// Add response to verify list for checking by rx processing thread
			{
				std::unique_lock<std::mutex> lck{ m_parent->m_rxmutex };
				m_parent->rxok_list.push_back(param);
				m_parent->m_rxcond.notify_one();
				lck.unlock();
			}
			break;
		}
		case (MessageEvents::TIMED_OUT_ON_SEND) :
		case (MessageEvents::SEND_ERROR) :
		case (MessageEvents::RESPONSE_TIMED_OUT) :
		case (MessageEvents::RESPONSE_ERROR_CRC) :
		case (MessageEvents::RESPONSE_ERROR_INVALID) : {

			// Add error to list and trigger processing thread if handle exists
			{
				std::unique_lock<std::mutex> lck{ m_parent->m_rxmutex };
				m_parent->rxerr_list.push_back(param);
				m_parent->m_rxcond.notify_one();
				lck.unlock();
			}
			break;
		}
		}
	}

	ToneBufferDownload::ToneBufferDownload(IMSSystem& iMS, const ToneBuffer& tb) : p_Impl(new Impl(iMS, tb))	{}

	ToneBufferDownload::~ToneBufferDownload() { delete p_Impl; p_Impl = nullptr; }

	bool ToneBufferDownload::StartDownload()
	{
		return this->StartDownload(p_Impl->m_tb.cbegin(), p_Impl->m_tb.cend());
	}

	bool ToneBufferDownload::StartDownload(ToneBuffer::const_iterator single)
	{
		return this->StartDownload(single, single+1);
	}

	bool ToneBufferDownload::StartDownload(ToneBuffer::const_iterator first, ToneBuffer::const_iterator last)
	{
		p_Impl->first = first;
		p_Impl->last = last;
		p_Impl->offset = static_cast<int>(std::distance(p_Impl->m_tb.cbegin(), first));

		// Make sure Synthesiser is present
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		std::unique_lock<std::mutex> lck{ p_Impl->m_dlmutex, std::try_to_lock };

		if (!lck.owns_lock()) {
			// Mutex lock failed, Downloader must be busy, try again later
			return false;
		}
		p_Impl->dl_list.clear();

		p_Impl->m_dlcond.notify_one();
		return true;
	}

	void ToneBufferDownload::ToneBufferDownloadEventSubscribe(const int message, IEventHandler* handler)
	{
		p_Impl->m_Event.Subscribe(message, handler);
	}

	void ToneBufferDownload::ToneBufferDownloadEventUnsubscribe(const int message, const IEventHandler* handler)
	{
		p_Impl->m_Event.Unsubscribe(message, handler);
	}

	void ToneBufferDownload::Impl::AddPointToVector(std::vector<std::uint8_t>& ltb_data, const TBEntry& tbe, bool toEEPROM)
	{
		int freqBits = myiMS.Synth().GetCap().freqBits;
		// Note that for-construct here enters infinite loop as can't increment past 4
		RFChannel chan(RFChannel::min);
		do {
			unsigned int freq = FrequencyRenderer::RenderAsImagePoint(myiMS, tbe.GetFAP(chan).freq);
			if (!toEEPROM) {
				if ((chan == RFChannel::min) || (freqBits <= 16))
				{
					// The first two entries were previously the lower frequency bits but became unused.  Now using them for sync data (not to EEPROM)
					unsigned int syncd = tbe.GetSyncD();
					std::uint16_t syncd_mod = syncd & ((1 << myiMS.Synth().GetCap().LUTSyncDBits) - 1);
					ltb_data.push_back(static_cast<std::uint8_t>(syncd_mod & 0xFF));
					ltb_data.push_back(static_cast<std::uint8_t>((syncd_mod >> 8) & 0xFF));
				}
				else {
					ltb_data.push_back(static_cast<std::uint8_t>((freq >> (freqBits - 32)) & 0xFF));
					ltb_data.push_back(static_cast<std::uint8_t>((freq >> (freqBits - 24)) & 0xFF));
				}
			}
			ltb_data.push_back(static_cast<std::uint8_t>((freq >> (freqBits - 16)) & 0xFF));
			ltb_data.push_back(static_cast<std::uint8_t>((freq >> (freqBits - 8)) & 0xFF));
			std::uint16_t ampl = AmplitudeRenderer::RenderAsImagePoint(myiMS, tbe.GetFAP(chan).ampl);
			ltb_data.push_back(static_cast<std::uint8_t>(ampl & 0xFF));
			ltb_data.push_back(static_cast<std::uint8_t>((ampl >> 8) & 0xFF));
			std::uint16_t phase = PhaseRenderer::RenderAsImagePoint(myiMS, tbe.GetFAP(chan).phase);
			ltb_data.push_back(static_cast<std::uint8_t>(phase & 0xFF));
			ltb_data.push_back(static_cast<std::uint8_t>((phase >> 8) & 0xFF));
		} while (chan++ < RFChannel::max);
	}

	// ToneBuffer Downloading Thread
	void ToneBufferDownload::Impl::DownloadWorker()
	{
		while (downloaderRunning) {
			std::unique_lock<std::mutex> lck{ m_dlmutex };
			m_dlcond.wait(lck);

			// Allow thread to terminate 
			if (!downloaderRunning) break;

			// Download loop
			IConnectionManager * const myiMSConn = myiMS.Connection();
			HostReport *iorpt;
			int index = offset;
			int length = static_cast<int>(std::distance(first, last));
			int buf_bytes = 1024;

			dl_final = NullMessage;

			std::vector<std::uint8_t> ltb_data;
			ToneBuffer::const_iterator it = first;
			MessageHandle h2;
			while (((index - offset) < length) && (it < last))
			{
				// Add one TBEntry to vector
				AddPointToVector(ltb_data, (*it), false);

				if (buf_bytes <= 0)
				{
      				std::this_thread::sleep_for(std::chrono::milliseconds(200));
					// Clear Buffer every kB to prevent Hardware overrun
					do {
						{
							std::unique_lock<std::mutex> dllck{ dl_list_mutex };
							if (dl_list.empty()) break;
							dllck.unlock();
						}
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
					} while (1);
					buf_bytes = 1024;
				}
				buf_bytes -= ltb_data.size();

				iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ProgSyncDig);
				iorpt->Payload<std::vector<std::uint8_t>>(ltb_data);
				MessageHandle h = myiMSConn->SendMsg(*iorpt);
				delete iorpt;

				// Add message handles to download list so we can check the responses
				{
    				std::unique_lock<std::mutex> dllck{ dl_list_mutex };
       				dl_list.push_back(h);
                }

				ltb_data.clear();

				if (myiMS.Synth().GetCap().freqBits > 16)
				{
					unsigned int freq = FrequencyRenderer::RenderAsImagePoint(myiMS, it->GetFAP(RFChannel::min).freq);
					ltb_data.push_back(static_cast<std::uint8_t>((freq >> (myiMS.Synth().GetCap().freqBits - 32)) & 0xFF));
					ltb_data.push_back(static_cast<std::uint8_t>((freq >> (myiMS.Synth().GetCap().freqBits - 24)) & 0xFF));
					iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ProgFreq0L);
					iorpt->Payload<std::vector<std::uint8_t>>(ltb_data);
					myiMSConn->SendMsg(*iorpt);
					delete iorpt;
				}

				ltb_data.clear();

				// Send message to program entry into an LTB index
				iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ProgLocal);
				iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(index++));
				h2 = myiMSConn->SendMsg(*iorpt);
				delete iorpt;

				++it;

				// Add message handles to download list so we can check the responses
				{
				    std::unique_lock<std::mutex> dllck{ dl_list_mutex };
        			if ((it == last) || ((index - offset) >= length)) dl_final = h2;
        			dl_list.push_back(h2);
                }

				// Not a great hack: adding a gap ensures packets aren't coalesced (even with TCP_NODELAY enabled)
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}

//			std::unique_lock<std::mutex> dllck{ dl_list_mutex };
			//dl_list.push_back(h2);
//			if (!dl_list.empty()) dl_final = dl_list.back();
//			dllck.unlock();

			// Release lock, wait for next download trigger
			lck.unlock();
		}
	}

	// ToneBuffer Readback Verify Data Processing Thread
	void ToneBufferDownload::Impl::RxWorker()
	{
		std::unique_lock<std::mutex> lck{ m_rxmutex };
		while (downloaderRunning) {
			// Release lock implicitly, wait for next download trigger
			m_rxcond.wait(lck);

			// Allow thread to terminate 
			if (!downloaderRunning) break;

			//IConnectionManager * const myiMSConn = myiMS.Connection();

			while (!rxok_list.empty())
			{
				int param = rxok_list.front();
				rxok_list.pop_front();

				std::unique_lock<std::mutex> dllck{ dl_list_mutex };

				for (std::list<MessageHandle>::iterator iter = dl_list.begin();
					iter != dl_list.end();)
				{
					MessageHandle handle = static_cast<MessageHandle>(*iter);
					if (handle != (param))
					{
						++iter;
						continue;
					}

					// Remove from list
					iter = dl_list.erase(iter);

					// Download Finished?
					if (handle == dl_final)
					{
						m_Event.Trigger<int>((void *)this, ToneBufferEvents::DOWNLOAD_FINISHED, 0);
					}
				}
				dllck.unlock();

			}

			while (!rxerr_list.empty())
			{
				int param = rxerr_list.front();
				rxerr_list.pop_front();

				std::unique_lock<std::mutex> dllck{ dl_list_mutex };

				for (std::list<MessageHandle>::iterator iter = dl_list.begin();
					iter != dl_list.end();)
				{
					MessageHandle handle = static_cast<MessageHandle>(*iter);
					if (handle != (param))
					{
						++iter;
						continue;
					}

					// Download Finished?
					if (handle == dl_final)
					{
						m_Event.Trigger<int>((void *)this, ToneBufferEvents::DOWNLOAD_FINISHED, 0);
					}

					// Remove from list
					iter = dl_list.erase(iter);
					m_Event.Trigger<int>((void *)this, ToneBufferEvents::DOWNLOAD_ERROR, handle);
				}
				dllck.unlock();

			}

		}
	}

	const FileSystemIndex ToneBufferDownload::Store(const std::string& FileName, FileDefault def) const
	{
		FileSystemManager fsm(p_Impl->myiMS);
		std::uint32_t addr;

		std::vector<std::uint8_t> data;
		for (ToneBuffer::const_iterator it = p_Impl->m_tb.cbegin(); it != p_Impl->m_tb.cend(); ++it)
		{
			p_Impl->AddPointToVector(data, (*it), true);
		}

		if (!fsm.FindSpace(addr, data)) return -1;
		FileSystemTableEntry fste(FileSystemTypes::TONE_BUFFER, addr, data.size(), def, FileName);
		p_Impl->fsw = new FileSystemWriter(p_Impl->myiMS, fste, data);

		FileSystemIndex result = p_Impl->fsw->Program();
		delete p_Impl->fsw;
		return result;
	}

	class ToneSequenceEntry::Impl {
	public:
		Impl(SignalPath::ToneBufferControl tbc = SignalPath::ToneBufferControl::HOST, int initial_index = 0) : 
			index(initial_index), tbc(tbc) {}

		SignalPath::ToneBufferControl tbc;
		int index;
	};

	ToneSequenceEntry::ToneSequenceEntry() : p_Impl(new Impl()) {}
	ToneSequenceEntry::ToneSequenceEntry(const ToneBuffer& tb, SignalPath::ToneBufferControl tbc, const unsigned int initial_index) :
		p_Impl(new Impl(tbc, initial_index)), SequenceEntry(tb.UUID()) {}
	ToneSequenceEntry::ToneSequenceEntry(const SequenceEntry& entry) : SequenceEntry(entry), p_Impl(new Impl()) 
	{
		try {
			const ToneSequenceEntry& rhs = dynamic_cast<const ToneSequenceEntry&>(entry);

			p_Impl->tbc = rhs.p_Impl->tbc;
			p_Impl->index = rhs.p_Impl->index;
		}
		catch (std::bad_cast ex) {
			return;
		}
	}

	ToneSequenceEntry::ToneSequenceEntry(const ToneSequenceEntry& rhs) : p_Impl(new Impl()), SequenceEntry(rhs.UUID(), rhs.NumRpts())
	{
		p_Impl->index = rhs.p_Impl->index;
		p_Impl->tbc = rhs.p_Impl->tbc;
		this->SyncOutDelay() == rhs.SyncOutDelay();
		for (RFChannel ch = RFChannel::min; ; ch++) {
			this->SetFrequencyOffset(rhs.GetFrequencyOffset(ch), ch);
			if (ch == RFChannel::max) break;
		}
	}

	ToneSequenceEntry& ToneSequenceEntry::operator =(const ToneSequenceEntry& rhs)
	{
		if (this == &rhs) return *this;
		p_Impl->index = rhs.p_Impl->index;
		p_Impl->tbc = rhs.p_Impl->tbc;
		return *this;
	}

	ToneSequenceEntry::~ToneSequenceEntry() {
		delete p_Impl;
		p_Impl = nullptr;
	}

	bool ToneSequenceEntry::operator==(SequenceEntry const& rhs) const {
		try {
			const ToneSequenceEntry& tse = dynamic_cast<const ToneSequenceEntry&>(rhs);

			for (RFChannel ch = RFChannel::min; ; ch++) {
				if (SequenceEntry::GetFrequencyOffset(ch) != rhs.GetFrequencyOffset(ch))
					return false;
				if (ch == RFChannel::max) break;
			}
			return ((p_Impl->index == tse.p_Impl->index) &&
				(p_Impl->tbc == tse.p_Impl->tbc) &&
				(SequenceEntry::SyncOutDelay() == rhs.SyncOutDelay()) &&
				(SequenceEntry::UUID() == rhs.UUID()));
		}
		catch (std::bad_cast ex) {
			return false;
		}
	}

	SignalPath::ToneBufferControl ToneSequenceEntry::ControlSource() const {
		return p_Impl->tbc;
	}

	int ToneSequenceEntry::InitialIndex() const {
		return p_Impl->index;
	}

}
