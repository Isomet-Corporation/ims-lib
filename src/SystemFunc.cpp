/*-----------------------------------------------------------------------------
/ Title      : System Functions Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/SystemFunc/src/SystemFunc.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2024-10-29 15:20:37 +0000 (Tue, 29 Oct 2024) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 614 $
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

#include "SystemFunc.h"
#include "Message.h"
#include "IConnectionManager.h"
#include "PrivateUtil.h"
#include "IEventTrigger.h"
#include "IMSConstants.h"
#include "IMSTypeDefs_p.h"
#include "PrivateUtil.h"

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace iMS
{
	class SystemFuncEventTrigger :
		public IEventTrigger
	{
	public:
		SystemFuncEventTrigger() { updateCount(SystemFuncEvents::Count); }
		~SystemFuncEventTrigger() {};
	};

	class SystemFunc::Impl
	{
	public:
		Impl(IMSSystem&);
		~Impl();

		ClockGenConfiguration ckgen;

		const std::chrono::milliseconds poll_interval = std::chrono::milliseconds(250);

		class ResponseReceiver : public IEventHandler
		{
		public:
			ResponseReceiver(SystemFunc::Impl* sf) : m_parent(sf) {};
			void EventAction(void* sender, const int message, const int param);
		private:
			SystemFunc::Impl* m_parent;
		};
		ResponseReceiver* Receiver;

		bool BackgroundThreadRunning;
		mutable std::mutex m_bkmutex;
		std::condition_variable m_bkcond;
		std::thread BackgroundThread;
		void BackgroundWorker();

		std::array<std::queue<MessageHandle>, 2> TemperatureHandle;
		std::array<int, 2> latestTemperature;
		int ChecksumErrors;
		int MasterClockStatus;
		int MasterClockFreq;
		int MasterClockMode;

		std::atomic<int> ChecksumMsg;
		std::atomic<int> MasterClockStatusMsg;
		std::atomic<int> MasterClockFreqMsg;
		std::atomic<int> MasterClockModeMsg;
		std::unique_ptr<SystemFuncEventTrigger> m_Event;
		IMSSystem& myiMS;

        std::thread heartbeatThread;
        std::atomic<bool> heartbeatRunning;
        int heartbeatIntervalMs;
        std::mutex hbMutex;
        std::condition_variable hbCv;        
	};

	SystemFunc::Impl::Impl(IMSSystem& iMS) :
		Receiver(new ResponseReceiver(this)),
		m_Event(new SystemFuncEventTrigger()),
		myiMS(iMS),
        heartbeatRunning(false),
        heartbeatIntervalMs(1000) 
	{
		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("SystemFunc::SystemFunc()");

		BackgroundThreadRunning = true;
		BackgroundThread = std::thread(&SystemFunc::Impl::BackgroundWorker, this);

		// Subscribe listener
		IConnectionManager * const myiMSConn = myiMS.Connection();
		myiMSConn->MessageEventSubscribe(MessageEvents::SEND_ERROR, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::TIMED_OUT_ON_SEND, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_RECEIVED, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_VALID, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_TIMED_OUT, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_CRC, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_INVALID, Receiver);

		ChecksumErrors = INT_MIN;
		MasterClockFreq = INT_MIN;
		MasterClockMode = INT_MIN;
		MasterClockStatus = INT_MIN;
		latestTemperature[0] = INT_MIN;
		latestTemperature[1] = INT_MIN;

		ChecksumMsg.store(NullMessage);
		MasterClockStatusMsg.store(NullMessage);
		MasterClockFreqMsg.store(NullMessage);
		MasterClockModeMsg.store(NullMessage);
	}

	SystemFunc::Impl::~Impl() {
		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("SystemFunc::~SystemFunc()");

		// Unblock worker thread
		BackgroundThreadRunning = false;
		m_bkcond.notify_one();

		BackgroundThread.join();

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

	SystemFunc::SystemFunc(IMSSystem& iMS) : p_Impl(new Impl(iMS)) {}

	SystemFunc::~SystemFunc() { StopHeartbeat(); delete p_Impl; p_Impl = nullptr; }

	void SystemFunc::Impl::ResponseReceiver::EventAction(void* sender, const int message, const int param)
	{
		for (int i = 0; i <= 1; i++) {
			while (!m_parent->TemperatureHandle[i].empty() &&
				((NullMessage == m_parent->TemperatureHandle[i].front()) || (param > (m_parent->TemperatureHandle[i].front())))) m_parent->TemperatureHandle[i].pop();
		}

		if ((m_parent->ChecksumMsg.load() != NullMessage) || 
			(m_parent->MasterClockFreqMsg.load() != NullMessage) ||
			(m_parent->MasterClockModeMsg.load() != NullMessage) ||
			(m_parent->MasterClockStatusMsg.load() != NullMessage) ||
			!m_parent->TemperatureHandle[0].empty() || !m_parent->TemperatureHandle[1].empty()) {
			switch (message)
			{
			case (MessageEvents::RESPONSE_RECEIVED) :
			case (MessageEvents::RESPONSE_ERROR_VALID) : {

				// Check for response and send to user code
				if (m_parent->ChecksumMsg.load() == param)
				{
					IConnectionManager * const myiMSConn = m_parent->myiMS.Connection();
					const DeviceReport& Resp = myiMSConn->Response(param);
					std::unique_lock<std::mutex> lck{ m_parent->m_bkmutex };
					m_parent->ChecksumErrors = static_cast<int>(Resp.Payload<std::uint16_t>());
					//m_parent->m_Event->Trigger<int>(m_parent, SystemFuncEvents::PIXEL_CHECKSUM_ERROR_COUNT, errors);
					m_parent->ChecksumMsg.store(NullMessage);
					m_parent->m_bkcond.notify_one();
					lck.unlock();  // Wait until thread is not consuming the mutex (might be in timeout loop) before notifying
				}
				if (m_parent->MasterClockStatusMsg.load() == param)
				{
					IConnectionManager * const myiMSConn = m_parent->myiMS.Connection();
					const DeviceReport& Resp = myiMSConn->Response(param);
					std::unique_lock<std::mutex> lck{ m_parent->m_bkmutex };
					int sts = static_cast<int>(Resp.Payload<std::uint8_t>());
					sts &= ~0x2; // Don't care about SPI active bit
					if (sts & 0x4) {
						// Internal mode; mask out reference valid bit
						sts &= ~0x8;
					}
					m_parent->MasterClockStatus = sts;
					//m_parent->m_Event->Trigger<int>(m_parent, SystemFuncEvents::PIXEL_CHECKSUM_ERROR_COUNT, errors);
					m_parent->MasterClockStatusMsg.store(NullMessage);
					m_parent->m_bkcond.notify_one();
					lck.unlock();  // Wait until thread is not consuming the mutex (might be in timeout loop) before notifying
				}
				if (m_parent->MasterClockFreqMsg.load() == param)
				{
					IConnectionManager * const myiMSConn = m_parent->myiMS.Connection();
					const DeviceReport& Resp = myiMSConn->Response(param);
					std::unique_lock<std::mutex> lck{ m_parent->m_bkmutex };
					m_parent->MasterClockFreq = static_cast<int>(Resp.Payload<std::uint16_t>());
					//m_parent->m_Event->Trigger<int>(m_parent, SystemFuncEvents::PIXEL_CHECKSUM_ERROR_COUNT, errors);
					m_parent->MasterClockFreqMsg.store(NullMessage);
					m_parent->m_bkcond.notify_one();
					lck.unlock();  // Wait until thread is not consuming the mutex (might be in timeout loop) before notifying
				}
				if (m_parent->MasterClockModeMsg.load() == param)
				{
					IConnectionManager * const myiMSConn = m_parent->myiMS.Connection();
					const DeviceReport& Resp = myiMSConn->Response(param);
					std::unique_lock<std::mutex> lck{ m_parent->m_bkmutex };
					m_parent->MasterClockMode = static_cast<int>(Resp.Payload<std::uint8_t>());
					//m_parent->m_Event->Trigger<int>(m_parent, SystemFuncEvents::PIXEL_CHECKSUM_ERROR_COUNT, errors);
					m_parent->MasterClockModeMsg.store(NullMessage);
					m_parent->m_bkcond.notify_one();
					lck.unlock();  // Wait until thread is not consuming the mutex (might be in timeout loop) before notifying
				}
				for (int i = 0; i <= 1; i++) {
					if ((!m_parent->TemperatureHandle[i].empty()) && (param == m_parent->TemperatureHandle[i].front()))
					{
						IConnectionManager* object = static_cast<IConnectionManager*>(sender);
						IOReport resp = object->Response(param);

						std::unique_lock<std::mutex> lck{ m_parent->m_bkmutex };
						m_parent->latestTemperature[i] = resp.Payload<int16_t>();
						m_parent->TemperatureHandle[i].pop();
						m_parent->m_bkcond.notify_one();
						lck.unlock();  // Wait until thread is not consuming the mutex (might be in timeout loop) before notifying
						//std::cout << "Recvd " << param << std::endl;
					}
				}
				break;
			}
			case (MessageEvents::TIMED_OUT_ON_SEND) :
			case (MessageEvents::SEND_ERROR) :
			case (MessageEvents::RESPONSE_TIMED_OUT) :
			case (MessageEvents::RESPONSE_ERROR_CRC) :
			case (MessageEvents::RESPONSE_ERROR_INVALID) : {

				// Check for response and send to user code
				if (m_parent->ChecksumMsg.load() == param)
				{
					m_parent->ChecksumMsg.store(NullMessage);
				}
				else if (m_parent->MasterClockFreqMsg.load() == param)
				{
					m_parent->MasterClockFreqMsg.store(NullMessage);
				}
				else if (m_parent->MasterClockStatusMsg.load() == param)
				{
					m_parent->MasterClockStatusMsg.store(NullMessage);
				}
				else if (m_parent->MasterClockModeMsg.load() == param)
				{
					m_parent->MasterClockModeMsg.store(NullMessage);
				}
				for (int i = 0; i <= 1; i++) {
					if ((!m_parent->TemperatureHandle[i].empty()) && (param == m_parent->TemperatureHandle[i].front()))
					{
						std::unique_lock<std::mutex> lck{ m_parent->m_bkmutex };
						m_parent->TemperatureHandle[i].pop();
						lck.unlock();
					}
				}
				break;
			}
			}
		}
	}

	void SystemFunc::SystemFuncEventSubscribe(const int message, IEventHandler* handler)
	{
		p_Impl->m_Event->Subscribe(message, handler);
	}

	void SystemFunc::SystemFuncEventUnsubscribe(const int message, const IEventHandler* handler)
	{
		p_Impl->m_Event->Unsubscribe(message, handler);
	}

	void SystemFunc::Impl::BackgroundWorker()
	{
		std::unique_lock<std::mutex> lck{ m_bkmutex };
		while (BackgroundThreadRunning) {
			while (std::cv_status::timeout == m_bkcond.wait_for(lck, poll_interval))
			{
				if (!BackgroundThreadRunning || (latestTemperature[0] != INT_MIN) || (latestTemperature[1] != INT_MIN) || (ChecksumErrors != INT_MIN)
					|| (MasterClockFreq != INT_MIN) || (MasterClockMode != INT_MIN) || (MasterClockStatus != INT_MIN)) break;
			}

			if (!BackgroundThreadRunning) break;

			if (ChecksumErrors != INT_MIN) {
				lck.unlock();
				m_Event->Trigger<int>(this, SystemFuncEvents::PIXEL_CHECKSUM_ERROR_COUNT, ChecksumErrors);
				lck.lock();
				ChecksumErrors = INT_MIN;
			}

			if (MasterClockFreq != INT_MIN) {
				double ref_freq = static_cast<double>(MasterClockFreq) * 20000.0;
				lck.unlock();
				m_Event->Trigger<double>(this, SystemFuncEvents::MASTER_CLOCK_REF_FREQ, ref_freq);
				lck.lock();
				MasterClockFreq = INT_MIN;
			}

			if (MasterClockMode != INT_MIN) {
				lck.unlock();
				m_Event->Trigger<int>(this, SystemFuncEvents::MASTER_CLOCK_REF_MODE, MasterClockMode);
				lck.lock();
				MasterClockMode = INT_MIN;
			}

			if (MasterClockStatus != INT_MIN) {
				lck.unlock();
				m_Event->Trigger<int>(this, SystemFuncEvents::MASTER_CLOCK_REF_STATUS, MasterClockStatus);
				lck.lock();
				MasterClockStatus = INT_MIN;
			}

			if (latestTemperature[0] != INT_MIN) {
				double Temperature = static_cast<double>(latestTemperature[0]) / 8.0; // Resolution of temperature reading is 8 integer + 3 fractional bits
				lck.unlock();
				m_Event->Trigger<double>(this, SystemFuncEvents::SYNTH_TEMPERATURE_1, Temperature);
				lck.lock();
				latestTemperature[0] = INT_MIN;
			}

			if (latestTemperature[1] != INT_MIN) {
				double Temperature = static_cast<double>(latestTemperature[1]) / 8.0;
				lck.unlock();
				m_Event->Trigger<double>(this, SystemFuncEvents::SYNTH_TEMPERATURE_2, Temperature);
				lck.lock();
				latestTemperature[1] = INT_MIN;
			}
		}
	}

	bool SystemFunc::ClearNHF()
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		// Write a '1' to the Clear NHF Config Register to clear flag
		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Clear_NHF);
		iorpt->Payload<std::uint16_t>(1);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

    bool SystemFunc::SendHeartbeat()
    {
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		// Read the first register (effectively a no-op)
		HostReport *iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, 0);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;        
    }

        // Starts automatic heartbeat with specified interval in milliseconds
    void SystemFunc::StartHeartbeat(int intervalMs)
    {
        std::lock_guard<std::mutex> lock(p_Impl->hbMutex);

        p_Impl->heartbeatIntervalMs = intervalMs;

        if (!p_Impl->heartbeatRunning)
        {
            p_Impl->heartbeatRunning = true;
            p_Impl->heartbeatThread = std::thread([this]()
            {
                std::unique_lock<std::mutex> lock(p_Impl->hbMutex);
                while (p_Impl->heartbeatRunning)
                {
                    // Send heartbeat without holding the lock to avoid blocking Start/Stop
                    lock.unlock();
                    this->SendHeartbeat();
                    lock.lock();

                    // Wait for interval or stop signal
                    if (p_Impl->hbCv.wait_for(lock, std::chrono::milliseconds(p_Impl->heartbeatIntervalMs),
                                            [this]() { return !p_Impl->heartbeatRunning; }))
                    {
                        break; // stopped
                    }
                }
            });
        }
        else
        {
            // Thread already running, notify to apply new interval immediately
            p_Impl->hbCv.notify_one();
        }
    }

    // Stops automatic heartbeat
    void SystemFunc::StopHeartbeat()
    {
        // First, tell the thread to stop and notify it
        {
            std::lock_guard<std::mutex> lock(p_Impl->hbMutex);
            if (!p_Impl->heartbeatRunning)
                return;

            p_Impl->heartbeatRunning = false;
            p_Impl->hbCv.notify_one();
        }

        // Now join the thread outside the lock
        if (p_Impl->heartbeatThread.joinable())
            p_Impl->heartbeatThread.join();
    }

	bool SystemFunc::ConfigureNHF(bool Enabled, int milliSeconds, NHFLocalReset reset)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		// To disable, set milliseonds to zero
		if (!Enabled) {
			milliSeconds = 0;
		}
		else {
			// Expressed in FPGA in numbers of 10ms.
			milliSeconds = milliSeconds / 10;
		}
		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_NHF_Timeout);
		iorpt->Payload<std::uint16_t>(milliSeconds);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		// If iMS System NHF expires, it will trigger a local reset if configured to do so
		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_NHF_Action);
		iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(reset));

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		return true;
	}
	
	bool SystemFunc::EnableAmplifier(bool enable)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		// Asynchronous Control Register bottom bit enables RF Amplifier Gate
		// Use address field to apply a bitmask to bottom bit
		iorpt = new HostReport(HostReport::Actions::ASYNC_CONTROL, HostReport::Dir::WRITE, ACR_RFGate_bitmask);
		iorpt->Payload<std::uint16_t>((enable) ? ACR_RFGate_ON : ACR_RFGate_OFF);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SystemFunc::EnableExternal(bool enable)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		// Asynchronous Control Register second bit enables External Equipment Opto
		// Use address field to apply a bitmask to bottom bit
		iorpt = new HostReport(HostReport::Actions::ASYNC_CONTROL, HostReport::Dir::WRITE, ACR_EXTEn_bitmask);
		iorpt->Payload<std::uint16_t>((enable) ? ACR_EXTEn_ON : ACR_EXTEn_OFF);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SystemFunc::EnableRFChannels(bool chan1_2, bool chan3_4)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		// Asynchronous Control Register bottom bit enables RF Amplifier Gate
		// Use address field to apply a bitmask to bottom bit
		iorpt = new HostReport(HostReport::Actions::ASYNC_CONTROL, HostReport::Dir::WRITE, ACR_RFBias_bitmask);
		std::uint16_t enables = ( ( (chan1_2) ? ACR_RFBias12_ON : ACR_RFBias12_OFF ) |
			((chan3_4) ? ACR_RFBias34_ON : ACR_RFBias34_OFF) ) ;
		iorpt->Payload<std::uint16_t>(enables);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SystemFunc::GetChecksumErrorCount(bool Reset)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, SYNTH_REG_PDI_Checksum);
		
		MessageHandle h = myiMSConn->SendMsg(*iorpt);
		if (NullMessage == h)
		{
			delete iorpt;
			return false;
		}
		p_Impl->ChecksumMsg.store(h);
		delete iorpt;

		if (Reset) {
			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_PDI_Checksum);
			iorpt->Payload<std::uint16_t>(0);
			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
			delete iorpt;
		}
		return true;
	}

	// This feature doesn't work due to layout bug in rev A Q0910 Controller (FPI pin connected through PS GPIO can't accept clock)
	bool SystemFunc::SetDDSUpdateClockSource(UpdateClockSource src)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
		iorpt->Payload<std::uint16_t>(8);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Pix_Control);
		if (src == UpdateClockSource::EXTERNAL) {
			iorpt->Payload<std::uint16_t>(8);
		}
		else {
			iorpt->Payload<std::uint16_t>(0);
		}

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	static bool download_pacc_scripts(IMSSystem& ims)
	{
		FileSystemManager fsm(ims);
		FileSystemTableViewer fstv(ims);

		std::string pacc_clr("pacc_clr");
		std::string paccnclr("paccnclr");

		// Look for existence of pacc_clr/paccnclr and remove
		for (int i = 0; i < MAX_FST_ENTRIES; i++) {
			FileSystemTypes type = fstv[i].Type();
			std::string name = fstv[i].Name();
			if ((type == FileSystemTypes::DDS_SCRIPT))
			{
				name.resize(8);
				if ((name.compare(pacc_clr) == 0) || (name.compare(paccnclr) == 0))
				{
					fsm.Delete(i);
				}
			}
		}

		DDSScript scr_clr, scr_nclr;
		scr_clr.push_back(DDSScriptRegister(DDSScriptRegister::Name::FR2, { 0x20, 0 }));
		scr_nclr.push_back(DDSScriptRegister(DDSScriptRegister::Name::FR2, { 0, 0 }));

		DDSScriptDownload ddsdl_clr(ims, scr_clr);
		DDSScriptDownload ddsdl_nclr(ims, scr_nclr);

		FileSystemIndex idx_clr = ddsdl_clr.Program(pacc_clr, FileDefault::NON_DEFAULT);
		if (idx_clr < 0) return false;
		FileSystemIndex idx_nclr = ddsdl_nclr.Program(paccnclr, FileDefault::NON_DEFAULT);
		if (idx_nclr < 0) return false;

		return true;
	}

	bool SystemFunc::StoreStartupConfig(const StartupConfiguration& cfg)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();
		HostReport *iorpt;
		std::vector<std::uint16_t> cfg_data;

		// Get Magic Number (used as first field in stored configuration)
		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, 0);
		DeviceReport Resp = myiMSConn->SendMsgBlocking(*iorpt);
		delete iorpt;
		if (Resp.Done()) {
			cfg_data.push_back(Resp.Payload<std::uint16_t>());
		}

		// Add configuration struct data fields
		std::uint16_t cfg_word;

		double pwr_scaled = 255.0 - (cfg.DDSPower * 255.0) / 100.0;
		cfg_word = static_cast<std::uint16_t>(std::floor(pwr_scaled)) & 0xFF;
		double ampl_scaled = (cfg.RFAmplitudeWiper1 * 255.0) / 100.0;
		cfg_word |= (static_cast<std::uint16_t>(std::floor(ampl_scaled)) << 8) & 0xFF00 ;
		cfg_data.push_back(cfg_word);

		ampl_scaled = (cfg.RFAmplitudeWiper2 * 255.0) / 100.0;
		cfg_word = static_cast<std::uint16_t>(std::floor(ampl_scaled)) & 0xFF;
		cfg_data.push_back(cfg_word);

		cfg_word = static_cast<uint16_t>(cfg.ExtClockFrequency.operator double()) / 20;
		cfg_word &= 0x3FFF;
		cfg_word |= (((uint16_t)cfg.PLLMode & 0x3) << 14);
		cfg_data.push_back(cfg_word);

		if (cfg.PhaseAccClear) {
			cfg_data.push_back(1);
		}
		else {
			cfg_data.push_back(0);
		}

		cfg_word = (cfg.RFGate) ? ACR_RFGate_ON : ACR_RFGate_OFF;
		cfg_word |= (cfg.ExtEquipmentEnable) ? ACR_EXTEn_ON : ACR_EXTEn_OFF;
		switch (cfg.AmplitudeControlSource)
		{
			case SignalPath::AmplitudeControl::OFF: cfg_word |= ACR_AmplitudeControl_OFF; break;
			case SignalPath::AmplitudeControl::EXTERNAL: cfg_word |= ACR_AmplitudeControl_EXTERNAL; break;
			case SignalPath::AmplitudeControl::WIPER_1: cfg_word |= ACR_AmplitudeControl_WIPER_1; break;
			case SignalPath::AmplitudeControl::WIPER_2: cfg_word |= ACR_AmplitudeControl_WIPER_2; break;
		}
		cfg_word |= (cfg.RFBias12) ? ACR_RFBias12_ON : ACR_RFBias12_OFF;
		cfg_word |= (cfg.RFBias34) ? ACR_RFBias34_ON : ACR_RFBias34_OFF;
		cfg_word <<= 6;
		//cfg_word |= 0x3F; // set any of the lower 6 bits to '1' to preserve existing data
		cfg_data.push_back(cfg_word);

		cfg_word = static_cast<std::uint16_t>(cfg.LocalToneIndex);
		cfg_word |= ((cfg.LTBUseAmplitudeCompensation == SignalPath::Compensation::ACTIVE) ? 0x800 : 0);
		switch (cfg.LTBControlSource) {
			case SignalPath::ToneBufferControl::HOST: cfg_word |= 0x100; break;
			case SignalPath::ToneBufferControl::EXTERNAL: cfg_word |= 0x300; break;
			case SignalPath::ToneBufferControl::EXTERNAL_EXTENDED: cfg_word |= 0x500; break;
			case SignalPath::ToneBufferControl::OFF: break;
		}
		cfg_data.push_back(cfg_word);

		cfg_data.push_back(PhaseRenderer::RenderAsImagePoint(p_Impl->myiMS, cfg.PhaseTuneCh1));
		cfg_data.push_back(PhaseRenderer::RenderAsImagePoint(p_Impl->myiMS, cfg.PhaseTuneCh2));
		cfg_data.push_back(PhaseRenderer::RenderAsImagePoint(p_Impl->myiMS, cfg.PhaseTuneCh3));
		cfg_data.push_back(PhaseRenderer::RenderAsImagePoint(p_Impl->myiMS, cfg.PhaseTuneCh4));

		cfg_word = (cfg.ChannelReversal) ? 0x1 : 0;
		cfg_word |= (cfg.ImageUseAmplitudeCompensation == SignalPath::Compensation::ACTIVE) ? 0 : 0x2;
		cfg_word |= (cfg.ImageUsePhaseCompensation == SignalPath::Compensation::ACTIVE) ? 0 : 0x4;
		cfg_word |= (cfg.upd_clk == SystemFunc::UpdateClockSource::EXTERNAL) ? 0x8 : 0;
		cfg_word |= (cfg.XYCompEnable) ? 0x10 : 0;
		cfg_data.push_back(cfg_word);

		cfg_word = static_cast<std::uint16_t>(cfg.LEDGreen) & 0xF;
		cfg_word |= (static_cast<std::uint16_t>(cfg.LEDYellow) & 0xF) << 4;
		cfg_word |= (static_cast<std::uint16_t>(cfg.LEDRed) & 0xF) << 8;
		cfg_data.push_back(cfg_word);

		cfg_data.push_back(cfg.GPOutput);

		cfg_word = (!cfg.CommsHealthyCheckEnabled) ? 0 : (cfg.CommsHealthyCheckTimerMilliseconds / 10) & 0x3FF;
		cfg_word |= (cfg.ResetOnUnhealthy == NHFLocalReset::RESET_ON_COMMS_UNHEALTHY) ? 0x800 : 0;
		cfg_data.push_back(cfg_word);

		switch (cfg.SyncAnalogASource) {
			case SignalPath::SYNC_SRC::FREQUENCY_CH1: cfg_word = (0) << SYNTH_REG_IOSig_ANLG_A_Shift; break;
			case SignalPath::SYNC_SRC::FREQUENCY_CH2: cfg_word = (1) << SYNTH_REG_IOSig_ANLG_A_Shift; break;
			case SignalPath::SYNC_SRC::FREQUENCY_CH3: cfg_word = (2) << SYNTH_REG_IOSig_ANLG_A_Shift; break;
			case SignalPath::SYNC_SRC::FREQUENCY_CH4: cfg_word = (3) << SYNTH_REG_IOSig_ANLG_A_Shift; break;
			case SignalPath::SYNC_SRC::IMAGE_ANLG_A: cfg_word = (4) << SYNTH_REG_IOSig_ANLG_A_Shift; break;
			case SignalPath::SYNC_SRC::IMAGE_ANLG_B: cfg_word = (5) << SYNTH_REG_IOSig_ANLG_A_Shift; break;
			default: cfg_word = 0; break;
		}
		switch (cfg.SyncAnalogBSource) {
			case SignalPath::SYNC_SRC::AMPLITUDE_PRE_COMP_CH1: cfg_word |= (0) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::AMPLITUDE_PRE_COMP_CH2: cfg_word |= (1) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::AMPLITUDE_PRE_COMP_CH3: cfg_word |= (2) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::AMPLITUDE_PRE_COMP_CH4: cfg_word |= (3) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::AMPLITUDE_CH1: cfg_word |= (4) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::AMPLITUDE_CH2: cfg_word |= (5) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::AMPLITUDE_CH3: cfg_word |= (6) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::AMPLITUDE_CH4: cfg_word |= (7) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::PHASE_CH1: cfg_word |= (8) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::PHASE_CH2: cfg_word |= (9) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::PHASE_CH3: cfg_word |= (10) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::PHASE_CH4: cfg_word |= (11) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::LOOKUP_FIELD_CH1: cfg_word |= (12) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::LOOKUP_FIELD_CH2: cfg_word |= (13) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::LOOKUP_FIELD_CH3: cfg_word |= (14) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::LOOKUP_FIELD_CH4: cfg_word |= (15) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::IMAGE_ANLG_A: cfg_word |= (16) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			case SignalPath::SYNC_SRC::IMAGE_ANLG_B: cfg_word |= (17) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
			default: break;
		}
		switch (cfg.SyncDigitalSource) {
			case SignalPath::SYNC_SRC::IMAGE_DIG: cfg_word |= (0) << SYNTH_REG_IOSig_DIG_Shift; break;
			case SignalPath::SYNC_SRC::LOOKUP_FIELD_CH1: cfg_word |= (4) << SYNTH_REG_IOSig_DIG_Shift; break;
			case SignalPath::SYNC_SRC::LOOKUP_FIELD_CH2: cfg_word |= (5) << SYNTH_REG_IOSig_DIG_Shift; break;
			case SignalPath::SYNC_SRC::LOOKUP_FIELD_CH3: cfg_word |= (6) << SYNTH_REG_IOSig_DIG_Shift; break;
			case SignalPath::SYNC_SRC::LOOKUP_FIELD_CH4: cfg_word |= (7) << SYNTH_REG_IOSig_DIG_Shift; break;
			default: break;
		}
		cfg_data.push_back(cfg_word);

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, 0xC0);
		iorpt->Payload<std::vector<std::uint16_t>>(cfg_data);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		// Store Phase Accumulator Clear scripts
		auto model = p_Impl->myiMS.Synth().Model();
		if ((model == "iMS4") || (model == "iMS4b") || (model == "iMS4c")) {
			// Original iMS4 (rev A) cannot support DDS Scripts and therefore won't work with Enhanced Tone Mode
			// SDK v1.5.1: rev A v1.3.57 and later supports ETM
			if (model == "iMS4") {
				if (p_Impl->myiMS.Synth().GetVersion().revision < 57) return true;
			}
			else if ((model == "iMS4b") || (model == "iMS4c")) {
				// iMS4b/4c use DDS scripts, from version 68 onwards.  Earlier builds had issue with DDS scripts
				if (p_Impl->myiMS.Synth().GetVersion().revision < 68) return true;
			}

			if (!download_pacc_scripts(p_Impl->myiMS)) return false;
			FileSystemManager fsm(p_Impl->myiMS);
			if (cfg.PhaseAccClear) {
				fsm.SetDefault("pacc_clr");
				fsm.ClearDefault("paccnclr");
			}
			else {
				fsm.SetDefault("paccnclr");
				fsm.ClearDefault("pacc_clr");
			}
		}

		return true;
	}

	bool SystemFunc::ReadSystemTemperature(SystemFunc::TemperatureSensor sensor)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		if (sensor == SystemFunc::TemperatureSensor::TEMP_SENSOR_1) {
			iorpt = new HostReport(HostReport::Actions::FAN_CONTROL, HostReport::Dir::READ, (int)SystemFunc::TemperatureSensor::TEMP_SENSOR_1);

			std::unique_lock<std::mutex> lck{ p_Impl->m_bkmutex };
			p_Impl->TemperatureHandle[0].push(myiMSConn->SendMsg(*iorpt));
			delete iorpt;

			if (NullMessage == p_Impl->TemperatureHandle[0].back())
			{
				lck.unlock();
				return false;
			}
			lck.unlock();
			return true;
		}
		else if (sensor == SystemFunc::TemperatureSensor::TEMP_SENSOR_2) {
			iorpt = new HostReport(HostReport::Actions::FAN_CONTROL, HostReport::Dir::READ, (int)SystemFunc::TemperatureSensor::TEMP_SENSOR_2);

			std::unique_lock<std::mutex> lck{ p_Impl->m_bkmutex };
			p_Impl->TemperatureHandle[1].push(myiMSConn->SendMsg(*iorpt));
			delete iorpt;

			if (NullMessage == p_Impl->TemperatureHandle[1].back())
			{
				lck.unlock();
				return false;
			}
			lck.unlock();
			return true;
		}
		else return false;

	}

	bool SystemFunc::SetClockReferenceMode(SystemFunc::PLLLockReference mode, kHz ExternalFixedFreq)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		iorpt = new HostReport(HostReport::Actions::PLL_REF, HostReport::Dir::WRITE, (uint16_t)mode);
		iorpt->Payload<std::uint16_t>(static_cast<uint16_t>(ExternalFixedFreq.operator double()) / 20);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SystemFunc::GetClockReferenceStatus()
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;
		iorpt = new HostReport(HostReport::Actions::PLL_REF, HostReport::Dir::READ, 0);

		MessageHandle h = myiMSConn->SendMsg(*iorpt);
		if (NullMessage == h)
		{
			delete iorpt;
			return false;
		}
		p_Impl->MasterClockStatusMsg.store(h);
		delete iorpt;
		return true;
	}

	bool SystemFunc::GetClockReferenceFrequency()
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;
		iorpt = new HostReport(HostReport::Actions::PLL_REF, HostReport::Dir::READ, 1);

		MessageHandle h = myiMSConn->SendMsg(*iorpt);
		if (NullMessage == h)
		{
			delete iorpt;
			return false;
		}
		p_Impl->MasterClockFreqMsg.store(h);
		delete iorpt;
		return true;
	}

	bool SystemFunc::GetClockReferenceMode()
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;
		iorpt = new HostReport(HostReport::Actions::PLL_REF, HostReport::Dir::READ, 2);

		MessageHandle h = myiMSConn->SendMsg(*iorpt);
		if (NullMessage == h)
		{
			delete iorpt;
			return false;
		}
		p_Impl->MasterClockModeMsg.store(h);
		delete iorpt;
		return true;
	}

	bool SystemFunc::ConfigureClockGenerator(const ClockGenConfiguration& cfg)
	{
		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		p_Impl->ckgen = cfg;

		int clk_op, trg_op;
		clk_op = cfg.AlwaysOn ? 1 : 3;
		trg_op = cfg.GenerateTrigger ? 4 : 0;

		double int_clk = cfg.ClockFreq;

		HostReport* iorpt = new HostReport(HostReport::Actions::CTRLR_REG, HostReport::Dir::WRITE, CTRLR_REG_ClockOutput);
		iorpt->Payload<uint16_t>(clk_op | trg_op);
		MessageHandle h = myiMSConn->SendMsg(*iorpt);
		if (NullMessage == h)
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		// Configure Clock Frequency
		bool PrescalerDisable = true;
		if ((int_clk < 5.0) || !p_Impl->myiMS.Ctlr().GetCap().FastImageTransfer) {
			PrescalerDisable = false;
		}
		if (p_Impl->myiMS.Ctlr().GetCap().FastImageTransfer) {
			iorpt = new HostReport(HostReport::Actions::CTRLR_REG, HostReport::Dir::WRITE, CTRLR_REG_Img_Ctrl);
			if (PrescalerDisable) {
				iorpt->Payload<std::uint16_t>(8);
			}
			else {
				iorpt->Payload<std::uint16_t>(0);
			}
			h = myiMSConn->SendMsg(*iorpt);
			if (NullMessage == h)
			{
				delete iorpt;
				return false;
			}
			delete iorpt;
		}

		iorpt = new HostReport(HostReport::Actions::CTRLR_REG, HostReport::Dir::WRITE, CTRLR_REG_OscFreq);
		iorpt->Payload<std::uint16_t>(FrequencyRenderer::RenderAsPointRate(p_Impl->myiMS, cfg.ClockFreq, PrescalerDisable));

		h = myiMSConn->SendMsg(*iorpt);
		if (NullMessage == h)
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		// Configure Duty Cycle
		iorpt = new HostReport(HostReport::Actions::CTRLR_REG, HostReport::Dir::WRITE, CTRLR_REG_DutyCycle);
		iorpt->Payload<std::uint16_t>(FrequencyRenderer::RenderAsPointRate(p_Impl->myiMS, cfg.ClockFreq, PrescalerDisable) * (double)(cfg.DutyCycle) / 100.0);

		h = myiMSConn->SendMsg(*iorpt);
		if (NullMessage == h)
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		// Configure Phase
		iorpt = new HostReport(HostReport::Actions::CTRLR_REG, HostReport::Dir::WRITE, CTRLR_REG_OscPhase);
		iorpt->Payload<std::uint16_t>(FrequencyRenderer::RenderAsPointRate(p_Impl->myiMS, cfg.ClockFreq, PrescalerDisable) * (double)(cfg.OscPhase) / 360.0);

		h = myiMSConn->SendMsg(*iorpt);
		if (NullMessage == h)
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		// Set Signal Polarities
		iorpt = new HostReport(HostReport::Actions::CTRLR_REG, HostReport::Dir::WRITE, CTRLR_REG_ExtPolarity);

		// bit 0 = clock; bit 1 = trigger
		// <0> = rising edge; <1> = falling edge
		std::uint16_t d = 0;
		d |= (cfg.ClockPolarity == Polarity::INVERSE) ? 1 : 0;
		d |= (cfg.TrigPolarity == Polarity::INVERSE) ? 2 : 0;
		iorpt->Payload<std::uint16_t>(d);
		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		return true;
	}

	const ClockGenConfiguration& SystemFunc::GetClockGenConfiguration() const
	{
		return p_Impl->ckgen;
	}

	bool SystemFunc::DisableClockGenerator()
	{
		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		HostReport* iorpt = new HostReport(HostReport::Actions::CTRLR_REG, HostReport::Dir::WRITE, CTRLR_REG_ClockOutput);
		iorpt->Payload<uint16_t>(0);
		MessageHandle h = myiMSConn->SendMsg(*iorpt);
		if (NullMessage == h)
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}


}
