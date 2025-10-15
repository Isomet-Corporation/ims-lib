/*-----------------------------------------------------------------------------
/ Title      : Signal Path Functions Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/SignalPath/src/SignalPath.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2024-12-19 19:57:02 +0000 (Thu, 19 Dec 2024) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 652 $
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

#include <mutex>
#include <thread>
#include <condition_variable>
//#include <iostream>

#include "IMSTypeDefs_p.h"
#include "FileSystem.h"
#include "Auxiliary.h"
#include "PrivateUtil.h"

//
// Taken out from header file until h/w fixed
//
/// \brief Varies the input voltage range applied to the RF Modulation Inputs
///
/// The 4 RF outputs may be modulation by an input signal connected to the external input
/// connector.  To use this feature, the RF Amplitude Control Source must be set to ::EXTERNAL.
/// The signal applied to the mixer in the RF output stage must have an amplitude of no greater
/// than 1.0Vpk-pk.  To accommodate input signals with a different voltage range, there is an
/// input attenuation section with variable scaling that can accept input signals with a range
/// of amplitudes from 1.0V to 15.0Vpk-pk.  The required input range can be set using this
/// function in 256 non-linear steps from 1.0V to 15.0V.
///
/// \warning Be careful not to apply a signal with an amplitude greater than the range 
/// programmed by this function.
///
/// \param[in] ch Which of the four modulation inputs to apply scaling to.
/// \param[in] VoltageRange The maximum value for input modulation voltage scaling
/// \return true if the voltage range request was sent successfully
/// \since 1.1
/*bool UpdateModulationInputScaling(const InputScaling ch, const double VoltageRange) const;*/

/// \enum InputScaling
/// \brief 
/*enum class InputScaling
{
	CH1,
	CH2,
	CH3,
	CH4
};*/



#include "SignalPath.h"
#include "IConnectionManager.h"
#include "IEventTrigger.h"
#include "IMSConstants.h"

namespace iMS
{
	class SignalPathEventTrigger :
		public IEventTrigger
	{
	public:
		SignalPathEventTrigger() { updateCount(SignalPathEvents::Count); }
		~SignalPathEventTrigger() {};
	};

	class SignalPath::Impl
	{
	public:
		Impl(IMSSystem&);
		~Impl();
		IMSSystem& myiMS;

		const std::chrono::milliseconds poll_interval = std::chrono::milliseconds(250);

		class ResponseReceiver : public IEventHandler
		{
		public:
			ResponseReceiver(SignalPath::Impl* sf) : m_parent(sf) {};
			void EventAction(void* sender, const int message, const int param);
		private:
			SignalPath::Impl* m_parent;
		};
		ResponseReceiver* Receiver;

		bool BackgroundThreadRunning;
		mutable std::mutex m_bkmutex;
		std::condition_variable m_bkcond;
		std::thread BackgroundThread;
		void BackgroundWorker();

		std::array<std::queue<MessageHandle>, 2> VelocityHandle;
		std::array<int, 2> latestVelocity;

		std::unique_ptr<SignalPathEventTrigger> m_Event;
	};

	SignalPath::Impl::Impl(IMSSystem& iMS) :
		myiMS(iMS),
		Receiver(new ResponseReceiver(this)),
		m_Event(new SignalPathEventTrigger())
	{
		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("SignalPath::SignalPath()");

		BackgroundThreadRunning = true;
		BackgroundThread = std::thread(&SignalPath::Impl::BackgroundWorker, this);

		// Subscribe listener
		IConnectionManager * const myiMSConn = myiMS.Connection();
		myiMSConn->MessageEventSubscribe(MessageEvents::SEND_ERROR, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::TIMED_OUT_ON_SEND, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_RECEIVED, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_VALID, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_TIMED_OUT, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_CRC, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_INVALID, Receiver);

		latestVelocity[0] = INT_MIN;
		latestVelocity[1] = INT_MIN;
	}

	SignalPath::Impl::~Impl() 
	{
		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("SignalPath::~SignalPath()");

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

	void SignalPath::Impl::ResponseReceiver::EventAction(void* sender, const int message, const int param)
	{
		for (int i = 0; i <= 1; i++) {
			while (!m_parent->VelocityHandle[i].empty() &&
				((NullMessage == m_parent->VelocityHandle[i].front()) || (param > (m_parent->VelocityHandle[i].front())))) m_parent->VelocityHandle[i].pop();
		}

		switch (message)
		{
		case (MessageEvents::RESPONSE_RECEIVED) :
		case (MessageEvents::RESPONSE_ERROR_VALID) : {

			{
				for (int i = 0; i <= 1; i++) {
					if ((!m_parent->VelocityHandle[i].empty()) && (param == m_parent->VelocityHandle[i].front()))
					{
						IConnectionManager* object = static_cast<IConnectionManager*>(sender);
						IOReport resp = object->Response(param);

						std::unique_lock<std::mutex> lck{ m_parent->m_bkmutex };
						m_parent->latestVelocity[i] = resp.Payload<int16_t>();
						m_parent->VelocityHandle[i].pop();
						m_parent->m_bkcond.notify_one();
						lck.unlock();  // Wait until thread is not consuming the mutex (might be in timeout loop) before notifying
						//std::cout << "Recvd " << param << std::endl;
					}
				}
			}
			break;
		}
		case (MessageEvents::TIMED_OUT_ON_SEND) :
		case (MessageEvents::SEND_ERROR) :
		case (MessageEvents::RESPONSE_TIMED_OUT) :
		case (MessageEvents::RESPONSE_ERROR_CRC) :
		case (MessageEvents::RESPONSE_ERROR_INVALID) : {

			{
				for (int i = 0; i <= 1; i++) {
					if ((!m_parent->VelocityHandle[i].empty()) && (param == m_parent->VelocityHandle[i].front()))
					{
						std::unique_lock<std::mutex> lck{ m_parent->m_bkmutex };
						m_parent->VelocityHandle[i].pop();
						lck.unlock();
					}
				}
				break;
			}
		}
		}
	}

	SignalPath::SignalPath(IMSSystem& iMS) : p_Impl(new Impl(iMS))
	{
	}

	SignalPath::~SignalPath()
	{
		delete p_Impl;
		p_Impl = nullptr;
	}

	void SignalPath::SignalPathEventSubscribe(const int message, IEventHandler* handler)
	{
		p_Impl->m_Event->Subscribe(message, handler);
	}

	void SignalPath::SignalPathEventUnsubscribe(const int message, const IEventHandler* handler)
	{
		p_Impl->m_Event->Unsubscribe(message, handler);
	}

	void SignalPath::Impl::BackgroundWorker()
	{
		std::unique_lock<std::mutex> lck{ m_bkmutex };
		while (BackgroundThreadRunning) {
			while (std::cv_status::timeout == m_bkcond.wait_for(lck, poll_interval))
			{
				if (!BackgroundThreadRunning || (latestVelocity[0] != INT_MIN) || (latestVelocity[1] != INT_MIN)) break;
			}

			if (!BackgroundThreadRunning) break;

			if (latestVelocity[0] != INT_MIN) {
				int velocity = latestVelocity[0] * 128; // Bottom 7 bits of velocity not returned
				lck.unlock();
				m_Event->Trigger<int>(this, SignalPathEvents::ENC_VEL_CH_X, velocity);
				lck.lock();
				latestVelocity[0] = INT_MIN;
			}

			if (latestVelocity[1] != INT_MIN) {
				int velocity = latestVelocity[1] * 128;
				lck.unlock();
				m_Event->Trigger<int>(this, SignalPathEvents::ENC_VEL_CH_Y, velocity);
				lck.lock();
				latestVelocity[1] = INT_MIN;
			}
		}
	}

	// The following 5 member functions update signal path parameters within the Synthesiser signal path
	bool SignalPath::UpdateDDSPowerLevel(const Percent& power)
	{
		// Make sure Synthesiser is present
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		// Power Level Attenuation: 00h = max power; FFh = min power
		double pwr_scaled = 255.0 - (power * 255.0) / 100.0;
		std::uint8_t pwr_int = static_cast<std::uint8_t>(std::floor(pwr_scaled));

		HostReport *iorpt;

		// DDS Power Level is on digital pot address zero
		iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 0);
		iorpt->Payload<std::uint8_t>(pwr_int);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SignalPath::UpdateRFAmplitude(const AmplitudeControl src, const Percent& ampl, const RFChannel& chan)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		// Power Level Attenuation: FFh = max power; 00h = min power
		double ampl_scaled = (ampl * 255.0) / 100.0;
		std::uint8_t ampl_int = static_cast<std::uint8_t>(std::floor(ampl_scaled));
		if (src == AmplitudeControl::OFF) {
			ampl_int = 0;
		}

		HostReport *iorpt;

		if (p_Impl->myiMS.Synth().Model() == "iMS4" || p_Impl->myiMS.Synth().Model() == "iMS4b") {

			// Wiper 1 on addr 1, Wiper 2 on addr 2, ignore anything else
			if (src == AmplitudeControl::WIPER_1) {
				iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 1);
			}
			else if (src == AmplitudeControl::WIPER_2) {
				iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 2);
			}
			else {
				return false;
			}
			iorpt->Payload<std::uint8_t>(ampl_int);
			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
			delete iorpt;

		}
		else {
			// rev C
			if ((src == AmplitudeControl::INDEPENDENT) ||
				(src == AmplitudeControl::WIPER_1) ||
				(src == AmplitudeControl::WIPER_2)) {
				if (chan.IsAll()) {
					for (RFChannel i = RFChannel::min; ; i++) {
						switch (i) {
						case 1: iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 1); break;
						case 2: iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 2); break;
						case 3: iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 8); break;
						case 4: iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 9); break;
						default: iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 1); break;
						}
						iorpt->Payload<std::uint8_t>(ampl_int);
						if (NullMessage == myiMSConn->SendMsg(*iorpt))
						{
							delete iorpt;
							return false;
						}
						delete iorpt;
						if ((i == RFChannel::max) || (i == p_Impl->myiMS.Synth().GetCap().channels)) break;
					}
				}
				else {
					switch (chan) {
					case 1: iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 1); break;
					case 2: iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 2); break;
					case 3: iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 8); break;
					case 4: iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 9); break;
					default: iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 1); break;
					}
					iorpt->Payload<std::uint8_t>(ampl_int);
					if (NullMessage == myiMSConn->SendMsg(*iorpt))
					{
						delete iorpt;
						return false;
					}
					delete iorpt;

				}
			}

		}

		return true;
	}

	bool SignalPath::SwitchRFAmplitudeControlSource(const AmplitudeControl src, const RFChannel& chan)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();
		HostReport *iorpt;

		if (p_Impl->myiMS.Synth().Model() == "iMS4" || p_Impl->myiMS.Synth().Model() == "iMS4b") {

			// Asynchronous Control Register has 2 bits representing amplitude control source
			// Use address field to apply a bitmask to those 2 bits to be updated
			iorpt = new HostReport(HostReport::Actions::ASYNC_CONTROL, HostReport::Dir::WRITE, ACR_AmplitudeControl_bitmask);
			switch (src)
			{
			case AmplitudeControl::OFF: iorpt->Payload<std::uint16_t>(ACR_AmplitudeControl_OFF); break;
			case AmplitudeControl::EXTERNAL: iorpt->Payload<std::uint16_t>(ACR_AmplitudeControl_EXTERNAL); break;
			case AmplitudeControl::WIPER_1: iorpt->Payload<std::uint16_t>(ACR_AmplitudeControl_WIPER_1); break;
			case AmplitudeControl::WIPER_2: iorpt->Payload<std::uint16_t>(ACR_AmplitudeControl_WIPER_2); break;
			default:  delete iorpt; return false; break;
			}

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
		}
		else {
			// rev C or other

			// Asynchronous Control Register has 4 bits representing amplitude control source
			// Use address field to apply a bitmask to those 4 bits to be updated
			if (chan.IsAll()) {
				iorpt = new HostReport(HostReport::Actions::ASYNC_CONTROL, HostReport::Dir::WRITE, (ACR_AmplitudeControl_bitmask |
					ACR_AmplitudeControlUpper_bitmask));
			}
			else {
				iorpt = new HostReport(HostReport::Actions::ASYNC_CONTROL, HostReport::Dir::WRITE, 0);
				ReportFields f = iorpt->Fields();
				switch ((int)chan) {
				case 1: f.addr = ACR_AmplitudeControlCh1_bitmask; break;
				case 2: f.addr = ACR_AmplitudeControlCh2_bitmask; break;
				case 3: f.addr = ACR_AmplitudeControlCh3_bitmask; break;
				case 4: f.addr = ACR_AmplitudeControlCh4_bitmask; break;
				}
				iorpt->Fields(f);
			}
			if (src == AmplitudeControl::EXTERNAL) {
				// Route all channels from external modulation source
				iorpt->Payload<std::uint16_t>(0xFFFF);
			}
			else {
				// Route all channels from respective per-channel digital pot wipers
				iorpt->Payload<std::uint16_t>(0);
			}
			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
			if (src == AmplitudeControl::OFF) {
				this->UpdateRFAmplitude(src, Percent(0.0), chan);
			}
		}
		delete iorpt;
		return true;
	}

	/*	bool SignalPath::UpdateModulationInputScaling(const InputScaling ch, const double VoltageRange) const
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		// Wiper 1 on addr 1, Wiper 2 on addr 2, ignore anything else
		if (ch == InputScaling::CH1) {
			iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 4);
		}
		else if (ch == InputScaling::CH2) {
			iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 5);
		}
		else if (ch == InputScaling::CH3) {
			iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 6);
		}
		else if (ch == InputScaling::CH4) {
			iorpt = new HostReport(HostReport::Actions::RF_POWER, HostReport::Dir::WRITE, 7);
		}
		else {
			return false;
		}

		double VR = VoltageRange;
		if (VR < 1.01) VR = 1.01;
		if (VR > 15.0) VR = 15.0;

		double N = ((271.0 / VR) + 932 - 1174 * sqrt(pow(1 / VR, 2) - (1.53 / VR) + 1.58)) / (2 * ((1 / VR) - 1));
		if (N < 0.0) N = 0.0;
		if (N > 255.0) N = 255.0;
		iorpt->Payload<std::uint8_t>(static_cast<std::uint8_t>(std::floor(256-(N+0.1))));

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}*/

	bool SignalPath::UpdatePhaseTuning(const RFChannel& channel, const Degrees& phase)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		if (channel.IsAll()) return false;

		// 14-bit phase field
		double phs_scaled = (phase * 16383.0) / 360.0;
		std::uint16_t phs_int = static_cast<std::uint8_t>(std::floor(phs_scaled));

		HostReport *iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Phase_Offset_Ch1+channel-1);
		iorpt->Payload<std::uint16_t>(phs_int);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SignalPath::SetChannelReversal(bool reversal)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
		iorpt->Payload<std::uint16_t>(1);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		// bottom bit indicates reversal
		std::uint16_t rvs_int = reversal ? 1 : 0;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Channel_Swap);
		iorpt->Payload<std::uint16_t>(rvs_int);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SignalPath::SetCalibrationTone(const FAP& fap)
	{
	  //	  BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("SetCalibrationTone");

		// Convert F/A/P data into integers relevant to synthesiser type
		unsigned int freq = FrequencyRenderer::RenderAsImagePoint(p_Impl->myiMS, fap.freq);
		std::uint16_t ampl = AmplitudeRenderer::RenderAsCalibrationTone(p_Impl->myiMS, fap.ampl);
		std::uint16_t phase = PhaseRenderer::RenderAsCalibrationTone(p_Impl->myiMS, fap.phase);

		int freqBits = p_Impl->myiMS.Synth().GetCap().freqBits;
		std::vector<std::uint8_t> fap_vec;
 
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();
		HostReport *iorpt;
		DeviceReport Resp;

		if (freqBits > 16) {
			fap_vec.clear();
			fap_vec.push_back(static_cast<std::uint8_t>((freq >> (freqBits - 32)) & 0xFF));
			fap_vec.push_back(static_cast<std::uint8_t>((freq >> (freqBits - 24)) & 0xFF));

			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_FreqLower);
			iorpt->Payload<std::vector<std::uint8_t>>(fap_vec);

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
			delete iorpt;
		}

		fap_vec.clear();
		fap_vec.push_back(static_cast<std::uint8_t>(phase & 0xFF));
		fap_vec.push_back(static_cast<std::uint8_t>((phase >> 8) & 0xFF));
		fap_vec.push_back(static_cast<std::uint8_t>(ampl & 0xFF));
		fap_vec.push_back(static_cast<std::uint8_t>((ampl >> 8) & 0xFF));
		fap_vec.push_back(static_cast<std::uint8_t>((freq >> (freqBits - 16)) & 0xFF));
		fap_vec.push_back(static_cast<std::uint8_t>((freq >> (freqBits - 8)) & 0xFF));

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_SingleTone_Phase);
		iorpt->Payload<std::vector<std::uint8_t>>(fap_vec);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		return true;
	}

	bool SignalPath::ClearTone()
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		FAP fap_clear;
		fap_clear.ampl = 0.0;

		SetCalibrationTone(fap_clear);

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		// Asynchronous Control Register bit 13 clears single tone mode
		// Use address field to apply a bitmask to bit
		iorpt = new HostReport(HostReport::Actions::ASYNC_CONTROL, HostReport::Dir::WRITE, ACR_ClearSTM_bitmask);
		iorpt->Payload<std::uint16_t>(ACR_ClearSTM);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		// Turn Off Auto Phase Clear
		//		this->AutoPhaseResync(false);

		return true;
	}

	bool SignalPath::SetCalibrationChannelLock(const RFChannel& chan)
	{
		std::uint16_t data = 0;
		bool lock_pairs_only = false;
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

        auto model = p_Impl->myiMS.Synth().Model();

        if (!model.compare(0, 3, "iMS")) {
            int rev = p_Impl->myiMS.Synth().GetVersion().revision;
            if (rev < 75) {
                BOOST_LOG_SEV(lg::get(), sev::error) << "Tried to set Calibration Tone channel lock but f/w version ." << rev << " doesn't support it. Need .75";
                return false;
            }
            else if (rev < 83) {
                lock_pairs_only = true;
            }
        }

		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		HostReport* iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
		if (chan.IsAll()) data = 0xF0;
		else if (chan == RFChannel(1)) data |= 0x10;
		else if (chan == RFChannel(2)) data |= 0x20;
		else if (chan == RFChannel(3)) data |= 0x40;
		else if (chan == RFChannel(4)) data |= 0x80;

		if (lock_pairs_only) {
			if (data & 0x30) {
				data |= 0x30;
			}
			if (data & 0xC0) {
				data |= 0xC0;
			}
		}

		iorpt->Payload<std::uint16_t>(data);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_STM_FuncHold);
		iorpt->Payload<std::uint16_t>(0xF0);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SignalPath::ClearCalibrationChannelLock(const RFChannel& chan)
	{
		std::uint16_t data = 0;
		bool lock_pairs_only = false;
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

        auto model = p_Impl->myiMS.Synth().Model();

        if (!model.compare(0, 3, "iMS")) {
            int rev = p_Impl->myiMS.Synth().GetVersion().revision;
            if (rev < 75) {
                BOOST_LOG_SEV(lg::get(), sev::error) << "Tried to set Calibration Tone channel lock but f/w version ." << rev << " doesn't support it. Need .75";
                return false;
            }
            else if (rev < 83) {
                lock_pairs_only = true;
            }
        }

		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		HostReport* iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
		if (chan.IsAll()) data = 0xF0;
		else if (chan == RFChannel(1)) data |= 0x10;
		else if (chan == RFChannel(2)) data |= 0x20;
		else if (chan == RFChannel(3)) data |= 0x40;
		else if (chan == RFChannel(4)) data |= 0x80;

		if (lock_pairs_only) {
			if (data & 0x30) {
				data |= 0x30;
			}
			if (data & 0xC0) {
				data |= 0xC0;
			}
		}

		iorpt->Payload<std::uint16_t>(data);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_STM_FuncHold);
		iorpt->Payload<std::uint16_t>(0);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SignalPath::GetCalibrationChannelLockState(const RFChannel& chan)
	{
		std::uint16_t data;
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

        auto model = p_Impl->myiMS.Synth().Model();

        if (!model.compare(0, 3, "iMS")) {
            int rev = p_Impl->myiMS.Synth().GetVersion().revision;
            if (rev < 75) {
                return false;
            }
        }        

		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		HostReport* iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, SYNTH_REG_STM_FuncHold);
		DeviceReport Resp = myiMSConn->SendMsgBlocking(*iorpt);
		delete iorpt;
		if (Resp.Done()) {
			data = Resp.Payload<std::uint16_t>();
		}
		else return false;

		bool locked = true;

		if (((chan == 1) || chan.IsAll()) && !(data & 0x10)) locked = false;
		if (((chan == 2) || chan.IsAll()) && !(data & 0x20)) locked = false;
		if (((chan == 3) || chan.IsAll()) && !(data & 0x40)) locked = false;
		if (((chan == 4) || chan.IsAll()) && !(data & 0x80)) locked = false;

		return locked;
	}

	static void add_sweep_to_script(IMSSystem& myiMS, DDSScript& scr, const SweepTone& tone, const RFChannel& chan)
	{
		/** CSR **/
		uint8_t csr_word = 0;
		if (myiMS.Synth().Model() == "iMS4") {
			switch (chan)
			{
			case 1: csr_word = 0x16; break;
			case 2: csr_word = 0x26; break;
			case 3: csr_word = 0x46; break;
			case 4: csr_word = 0x86; break;
			default: csr_word = 0xF6; break;
			}
		}
		else {
			// Reordered channel layout
			switch (chan)
			{
			case 1: csr_word = 0x26; break;
			case 2: csr_word = 0x16; break;
			case 3: csr_word = 0x86; break;
			case 4: csr_word = 0x46; break;
			default: csr_word = 0xF6; break;
			}
		}
		DDSScriptRegister reg_CSR(DDSScriptRegister::Name::CSR, { csr_word });

		/** CFR **/
		uint8_t ls_type = 0, ls_en = 0, ls_clr = 0x20;
		switch (tone.mode())
		{
			case ENHANCED_TONE_MODE::NO_SWEEP: ls_type = 0; ls_en = 0, ls_clr = 0x28;  break;
			case ENHANCED_TONE_MODE::FREQUENCY_DWELL: ls_type = 0x80; ls_en = 0x40; break;
			case ENHANCED_TONE_MODE::FREQUENCY_NO_DWELL: ls_type = 0x80; ls_en = 0xC0; break;
			case ENHANCED_TONE_MODE::PHASE_DWELL: ls_type = 0xC0; ls_en = 0x40; break;
			case ENHANCED_TONE_MODE::PHASE_NO_DWELL: ls_type = 0xC0; ls_en = 0xC0; break;
		}
		switch (tone.scaling()) {
			case DAC_CURRENT_REFERENCE::FULL_SCALE: ls_en |= 3; break;
			case DAC_CURRENT_REFERENCE::HALF_SCALE: ls_en |= 1; break;
			case DAC_CURRENT_REFERENCE::QUARTER_SCALE: ls_en |= 2; break;
			default: break;
		}
		DDSScriptRegister reg_CFR(DDSScriptRegister::Name::CFR, { ls_type, ls_en, ls_clr });

		/** CFTW **/
		std::vector<uint8_t> start_freq = VarToBytes<unsigned int>(FrequencyRenderer::RenderAsDDSValue(myiMS, tone.start().freq));
		std::reverse(start_freq.begin(), start_freq.end());
		DDSScriptRegister reg_CFTW(DDSScriptRegister::Name::CFTW0, std::deque<uint8_t>(start_freq.begin(), start_freq.end()));

		/** CPOW **/
		std::vector<uint8_t> start_phase = VarToBytes<unsigned int>(PhaseRenderer::RenderAsCalibrationTone(myiMS, tone.start().phase));
		std::reverse(start_phase.begin(), start_phase.end());
		DDSScriptRegister reg_CPOW(DDSScriptRegister::Name::CPOW0, std::deque<uint8_t>(start_phase.end() - 2, start_phase.end()));

		/** ACR **/
		
		/****************************************************************/
		/* WARNING: In Linear Sweep mode, AD9959 SDIO pins are reused   */
		/* for Amplitude Ramp Up/Down. This breaks SPI communication    */
		/* in any mode other than 1-bit SPI comm! We therefore turn off */
		/* the amplitude multiplier for sweep modes even though this    */
		/* results in zero control over output amplitude, and no        */
		/* amplitude sweeping                                           */
		/****************************************************************/
		unsigned int acr = AmplitudeRenderer::RenderAsCalibrationTone(myiMS, tone.start().ampl);
		if (tone.mode() == ENHANCED_TONE_MODE::NO_SWEEP) acr |= 0x001000;
		std::vector<uint8_t> start_ampl = VarToBytes<unsigned int>(acr);
		std::reverse(start_ampl.begin(), start_ampl.end());
		DDSScriptRegister reg_ACR(DDSScriptRegister::Name::ACR, std::deque<uint8_t>(start_ampl.end() - 3, start_ampl.end()));

		scr.push_back(reg_CSR);
		scr.push_back(reg_CFR);
		scr.push_back(reg_CFTW);
		scr.push_back(reg_CPOW);
		scr.push_back(reg_ACR);

		if (tone.mode() != ENHANCED_TONE_MODE::NO_SWEEP) {
			/** CW1 **/
			std::vector<uint8_t> end_val;
			switch (tone.mode())
			{
				case ENHANCED_TONE_MODE::NO_SWEEP: end_val = VarToBytes<unsigned int>(0); break;
				case ENHANCED_TONE_MODE::FREQUENCY_DWELL:
				case ENHANCED_TONE_MODE::FREQUENCY_NO_DWELL: end_val = VarToBytes<unsigned int>(FrequencyRenderer::RenderAsDDSValue(myiMS, tone.end().freq)); break;
				case ENHANCED_TONE_MODE::PHASE_DWELL:
				case ENHANCED_TONE_MODE::PHASE_NO_DWELL: end_val = VarToBytes<unsigned int>(PhaseRenderer::RenderAsChirp(myiMS, tone.end().phase)); break;
			}
			std::reverse(end_val.begin(), end_val.end());

			/** LSR, RDW, FDW **/
			unsigned int rsrr, fsrr;
			unsigned int rdw=0, fdw=0;
			// Calculate Rising and Falling Ramp Rate and Delta Words from number of steps and ramp period
			double sc = myiMS.Synth().GetCap().syncClock;
			rsrr = static_cast<unsigned int>(tone.up_ramp().count() * 1000000.0 * sc / (tone.n_steps() + 1));  // Number of target Sync Clock periods (= sys clock / 4) to reach sweep time period
			rsrr = rsrr > 255 ? 255 : rsrr < 1 ? 1 : rsrr;
			int actual_steps = static_cast<int>(tone.up_ramp().count() * 1000000.0 * sc / rsrr) + 1;  // Number of steps in linear sweep (plus 1 to ensure not zero)
			
			double delta_ampl = (tone.end().ampl - tone.start().ampl) / actual_steps;
			double delta_freq = (tone.end().freq - tone.start().freq) / actual_steps;
			double delta_phase = (tone.end().phase - tone.start().phase) / actual_steps;

			switch (tone.mode())
			{
				case ENHANCED_TONE_MODE::FREQUENCY_DWELL:
				case ENHANCED_TONE_MODE::FREQUENCY_NO_DWELL:	rdw = static_cast<unsigned int>(std::floor((delta_freq * std::pow(2, 32) / myiMS.Synth().GetCap().sysClock) - 0.5)); break;
				case ENHANCED_TONE_MODE::PHASE_DWELL:
				case ENHANCED_TONE_MODE::PHASE_NO_DWELL: rdw = static_cast<unsigned int>(std::floor((delta_phase * std::pow(2, 14) / 360.0) - 0.5)); break;
			}
			rdw = (rdw == 0) ? 1 : rdw;

			fsrr = static_cast<unsigned int>(tone.down_ramp().count() * 1000000.0 * sc / (tone.n_steps() + 1));  // Number of target Sync Clock periods (= sys clock / 4) to reach sweep time period
			fsrr = fsrr > 255 ? 255 : fsrr < 1 ? 1 : fsrr;
			actual_steps = static_cast<int>(tone.down_ramp().count() * 1000000.0 * sc / fsrr) + 1;  // Number of steps in linear sweep

			delta_ampl = (tone.end().ampl - tone.start().ampl) / actual_steps;
			delta_freq = (tone.end().freq - tone.start().freq) / actual_steps;
			delta_phase = (tone.end().phase - tone.start().phase) / actual_steps;

			switch (tone.mode())
			{
				case ENHANCED_TONE_MODE::FREQUENCY_DWELL:
				case ENHANCED_TONE_MODE::FREQUENCY_NO_DWELL: fdw = static_cast<unsigned int>(std::floor((delta_freq * std::pow(2, 32) / myiMS.Synth().GetCap().sysClock) - 0.5)); break;
				case ENHANCED_TONE_MODE::PHASE_DWELL:
				case ENHANCED_TONE_MODE::PHASE_NO_DWELL: fdw = static_cast<unsigned int>(std::floor((delta_phase * std::pow(2, 14) / 360.0) - 0.5)); break;
			}
			fdw = (fdw == 0) ? 1 : fdw;

			DDSScriptRegister reg_CW1(DDSScriptRegister::Name::CW1, std::deque<uint8_t>(end_val.begin(), end_val.end()));
			DDSScriptRegister reg_LSRR(DDSScriptRegister::Name::LSRR, { static_cast<uint8_t>(fsrr), static_cast<uint8_t>(rsrr) });
			std::vector<uint8_t> rdw_bytes = VarToBytes<unsigned int>(rdw);
			std::reverse(rdw_bytes.begin(), rdw_bytes.end());
			DDSScriptRegister reg_RDW(DDSScriptRegister::Name::RDW, std::deque<uint8_t>(rdw_bytes.begin(), rdw_bytes.end()));
			std::vector<uint8_t> fdw_bytes = VarToBytes<unsigned int>(fdw);
			std::reverse(fdw_bytes.begin(), fdw_bytes.end());
			DDSScriptRegister reg_FDW(DDSScriptRegister::Name::FDW, std::deque<uint8_t>(fdw_bytes.begin(), fdw_bytes.end()));
		
			scr.push_back(reg_LSRR);
			scr.push_back(reg_RDW);
			scr.push_back(reg_FDW);
			scr.push_back(reg_CW1);
		}

	}

	static std::vector < std::uint16_t > add_sweep_to_vector(IMSSystem& myiMS, const SweepTone& tone)
	{
		std::vector < std::uint16_t > etm_data;

		unsigned int start_freq = FrequencyRenderer::RenderAsDDSValue(myiMS, tone.start().freq);
		unsigned int start_ampl = AmplitudeRenderer::RenderAsCalibrationTone(myiMS, tone.start().ampl);
		unsigned int start_phs = PhaseRenderer::RenderAsCalibrationTone(myiMS, tone.start().phase);

		etm_data.push_back(static_cast<std::uint16_t>(start_freq & 0xFFFF));
		etm_data.push_back(static_cast<std::uint16_t>((start_freq >> 16) & 0xFFFF));
		etm_data.push_back(static_cast<std::uint16_t>(start_ampl & 0xFFFF));
		etm_data.push_back(static_cast<std::uint16_t>(start_phs & 0xFFFF));

		unsigned int end_freq = FrequencyRenderer::RenderAsDDSValue(myiMS, tone.end().freq);
		unsigned int end_ampl = AmplitudeRenderer::RenderAsCalibrationTone(myiMS, tone.end().ampl);
		unsigned int end_phs = PhaseRenderer::RenderAsCalibrationTone(myiMS, tone.end().phase);

		etm_data.push_back(static_cast<std::uint16_t>(end_freq & 0xFFFF));
		etm_data.push_back(static_cast<std::uint16_t>((end_freq >> 16) & 0xFFFF));
		etm_data.push_back(static_cast<std::uint16_t>(end_ampl & 0xFFFF));
		etm_data.push_back(static_cast<std::uint16_t>(end_phs & 0xFFFF));

		return etm_data;
	}

	static std::vector < std::uint16_t > add_rate_to_vector(IMSSystem& myiMS, const SweepTone& tone)
	{
		std::vector < std::uint16_t > etm_data;

		/** LSR, RDW, FDW **/
		unsigned int rsrr, fsrr;
		unsigned int rdw = 0, fdw = 0;
		// Calculate Rising and Falling Ramp Rate and Delta Words from number of steps and ramp period
		double sc = myiMS.Synth().GetCap().syncClock;
		rsrr = static_cast<unsigned int>(tone.up_ramp().count() * 1000000.0 * sc / (tone.n_steps() + 1));  // Number of target Sync Clock periods (= sys clock / 4) to reach sweep time period
		rsrr = rsrr > 255 ? 255 : rsrr < 1 ? 1 : rsrr;
		int actual_steps = static_cast<int>(tone.up_ramp().count() * 1000000.0 * sc / rsrr) + 1;  // Number of steps in linear sweep (plus 1 to ensure not zero)

		double delta_ampl = (tone.end().ampl - tone.start().ampl) / actual_steps;
		double delta_freq = (tone.end().freq - tone.start().freq) / actual_steps;
		double delta_phase = (tone.end().phase - tone.start().phase) / actual_steps;

		switch (tone.mode())
		{
		case ENHANCED_TONE_MODE::FREQUENCY_DWELL:
		case ENHANCED_TONE_MODE::FREQUENCY_NO_DWELL:	rdw = static_cast<unsigned int>(std::floor((delta_freq * std::pow(2, 32) / myiMS.Synth().GetCap().sysClock) - 0.5)); break;
		case ENHANCED_TONE_MODE::PHASE_DWELL:
		case ENHANCED_TONE_MODE::PHASE_NO_DWELL: rdw = static_cast<unsigned int>(std::floor((delta_phase * std::pow(2, 14) / 360.0) - 0.5)); break;
		}
		rdw = (rdw == 0) ? 1 : rdw;

		fsrr = static_cast<unsigned int>(tone.down_ramp().count() * 1000000.0 * sc / (tone.n_steps() + 1));  // Number of target Sync Clock periods (= sys clock / 4) to reach sweep time period
		fsrr = fsrr > 255 ? 255 : fsrr < 1 ? 1 : fsrr;
		actual_steps = static_cast<int>(tone.down_ramp().count() * 1000000.0 * sc / fsrr) + 1;  // Number of steps in linear sweep

		delta_ampl = (tone.end().ampl - tone.start().ampl) / actual_steps;
		delta_freq = (tone.end().freq - tone.start().freq) / actual_steps;
		delta_phase = (tone.end().phase - tone.start().phase) / actual_steps;

		switch (tone.mode())
		{
		case ENHANCED_TONE_MODE::FREQUENCY_DWELL:
		case ENHANCED_TONE_MODE::FREQUENCY_NO_DWELL: fdw = static_cast<unsigned int>(std::floor((delta_freq * std::pow(2, 32) / myiMS.Synth().GetCap().sysClock) - 0.5)); break;
		case ENHANCED_TONE_MODE::PHASE_DWELL:
		case ENHANCED_TONE_MODE::PHASE_NO_DWELL: fdw = static_cast<unsigned int>(std::floor((delta_phase * std::pow(2, 14) / 360.0) - 0.5)); break;
		}
		fdw = (fdw == 0) ? 1 : fdw;

		etm_data.push_back(static_cast<std::uint16_t>(fsrr));
		etm_data.push_back(static_cast<std::uint16_t>(rsrr));
		etm_data.push_back(static_cast<std::uint16_t>(fdw & 0xFFFF));
		etm_data.push_back(static_cast<std::uint16_t>((fdw >> 16) & 0xFFFF));
		etm_data.push_back(static_cast<std::uint16_t>(rdw & 0xFFFF));
		etm_data.push_back(static_cast<std::uint16_t>((rdw >> 16) & 0xFFFF));

		return etm_data;
	}

	static bool fst_match(const IMSSystem& myiMS, const std::string& check)
	{
		// Uses DDS Script
		FileSystemTableViewer fstv(myiMS);

		for (int i = 0; i < MAX_FST_ENTRIES; i++) {
			FileSystemTypes type = fstv[i].Type();
			std::string name = fstv[i].Name();
			if ((type == FileSystemTypes::DDS_SCRIPT))
			{
				name.resize(check.size());
				if (name.compare(check) == 0)
				{
					return true;
				}
			}
		}
		return false;
	}

	static FileSystemIndex fst_getindex(const IMSSystem& myiMS, const std::string& check)
	{
		// Uses DDS Script
		FileSystemTableViewer fstv(myiMS);

		for (int i = 0; i < MAX_FST_ENTRIES; i++) {
			FileSystemTypes type = fstv[i].Type();
			std::string name = fstv[i].Name();
			if ((type == FileSystemTypes::DDS_SCRIPT))
			{
				name.resize(check.size());
				if (name.compare(check) == 0)
				{
					return i;
				}
			}
		}
		return -1;
	}

	static bool check_linear_sweep(const IMSSystem& myiMS)
	{
		// Check whether we support linear sweep as part of STM, or need to use a DDS script
//		if (( (myiMS.Synth().Model() == "iMS4") || (myiMS.Synth().Model() == "iMS4b") ) && (myiMS.Synth().GetVersion().revision <= 68) )
		{
			// Uses DDS Script
			FileSystemTableViewer fstv(myiMS);
			return fst_match(myiMS, std::string("alwaowwr"));

		}
//		else {
			// Uses STM
//			return false;
//		}
//		return false;
	}

	static bool check_linear_sweep(const IMSSystem& myiMS, const RFChannel& chan)
	{
		// Check whether we support linear sweep as part of STM, or need to use a DDS script
//		if (((myiMS.Synth().Model() == "iMS4") || (myiMS.Synth().Model() == "iMS4b")) && (myiMS.Synth().GetVersion().revision <= 68))
		{
			bool ls_found = false;
			std::string check("alwaoww");
			check += std::to_string((int)chan);

			return fst_match(myiMS, check);
		}
//		else {
			// Uses STM
//			return false;
//		}
//		return false;
	}

	bool SignalPath::SetEnhancedToneMode(const SweepTone& tone_ch1, const SweepTone& tone_ch2, const SweepTone& tone_ch3, const SweepTone& tone_ch4)
	{
		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		HostReport* iorpt;

		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("Verifying support for ETM mode ") << p_Impl->myiMS.Synth().IsValid() << " " << p_Impl->myiMS.Synth().Model() << " " << p_Impl->myiMS.Synth().GetVersion().revision;

		bool use_dds_script = true;
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		// Original iMS4 (rev A) cannot support DDS Scripts and therefore won't work with Enhanced Tone Mode
		// SDK v1.5.1: rev A v1.3.57 and later supports ETM
		if (p_Impl->myiMS.Synth().Model() == "iMS4") {
			if (p_Impl->myiMS.Synth().GetVersion().revision < 57) return false;
		}
		else if ((p_Impl->myiMS.Synth().Model() == "iMS4b") || (p_Impl->myiMS.Synth().Model() == "iMS4c")) {
			// iMS4b/4c use DDS scripts, from version 68 onwards.  Earlier builds had issue with DDS scripts
			if (p_Impl->myiMS.Synth().GetVersion().revision < 68) return false;
		}
		else if (p_Impl->myiMS.Synth().Model() == "iMS4d") {
			// iMS4d use direct programming, from version 148 onwards.
			if (p_Impl->myiMS.Synth().GetVersion().revision < 148) return false;
			use_dds_script = false;
		}
		else {
			// Synth doesn't support scripts
			use_dds_script = false;
		}

		if (use_dds_script && check_linear_sweep(p_Impl->myiMS))
		{
			// Already playing out a linear sweep. Remove DDS script if present, and reprogram
			FileSystemTableViewer fstv(p_Impl->myiMS);
			FileSystemManager fsm(p_Impl->myiMS);
			for (int i = 0; i < MAX_FST_ENTRIES; i++) {
				FileSystemTypes type = fstv[i].Type();
				std::string name = fstv[i].Name();
				if ((type == FileSystemTypes::DDS_SCRIPT))
				{
					name.resize(8);
					if (name.compare(std::string("alwaowwr")) == 0)
					{
						fsm.Delete(i);
					}
				}
			}

		}

		// Check whether we support linear sweep as part of STM, or need to use a DDS script
		if (use_dds_script)
		{
			BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("Programming ETM Mode using DDS Script");

			// Use a DDS Script
			DDSScript scr;
			add_sweep_to_script(p_Impl->myiMS, scr, tone_ch1, 1);
			add_sweep_to_script(p_Impl->myiMS, scr, tone_ch2, 2);
			add_sweep_to_script(p_Impl->myiMS, scr, tone_ch3, 3);
			add_sweep_to_script(p_Impl->myiMS, scr, tone_ch4, 4);

			scr.push_back(DDSScriptRegister(DDSScriptRegister::Name::UPDATE));

			DDSScriptDownload ddsdl(p_Impl->myiMS, scr);
			FileSystemIndex idx = ddsdl.Program("alwaowwr", FileDefault::NON_DEFAULT);
			if (idx < 0) return false;

			FileSystemManager fsm(p_Impl->myiMS);
			fsm.Execute(idx);
		}
		else {
			BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("Programming ETM Mode using Direct DDS Configuration");

			std::vector<std::uint16_t> etm_data;
			const SweepTone* tone = &tone_ch1;

			for (int i = RFChannel::min; i <= RFChannel::max; i++)
			{
				switch (i)
				{
				case 1: tone = &tone_ch1; break;
				case 2: tone = &tone_ch2;  break;
				case 3: tone = &tone_ch3;  break;
				case 4: tone = &tone_ch4;  break;
				}
				etm_data = add_sweep_to_vector(p_Impl->myiMS, *tone);
				
				iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ETMStartFreqLo);
				iorpt->Payload<std::vector<std::uint16_t>>(etm_data);

				ReportFields f = iorpt->Fields();
				f.len = static_cast<std::uint16_t>(etm_data.size() * sizeof(std::uint16_t));
				iorpt->Fields(f);

				if (NullMessage == myiMSConn->SendMsg(*iorpt))
				{
					delete iorpt;
					return false;
				}
				delete iorpt;

				etm_data = add_rate_to_vector(p_Impl->myiMS, *tone);

				iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_SweepFSRR);
				iorpt->Payload<std::vector<std::uint16_t>>(etm_data);

				f = iorpt->Fields();
				f.len = static_cast<std::uint16_t>(etm_data.size() * sizeof(std::uint16_t));
				iorpt->Fields(f);

				if (NullMessage == myiMSConn->SendMsg(*iorpt))
				{
					delete iorpt;
					return false;
				}
				delete iorpt;

				// Program channel
				std::uint16_t etm_control = 0;

				etm_control |= SYNTH_REG_ETMControl_bits_Program_mask;  // Program Channel data
				if (i == RFChannel::max) {
					etm_control |= SYNTH_REG_ETMControl_bits_Trigger_mask;  // And trigger transfer to DDS
				}

				etm_control |= ((i - 1) & SYNTH_REG_ETMControl_bits_Channel_mask);
				switch (tone->mode())
				{
				case ENHANCED_TONE_MODE::NO_SWEEP: break;
				case ENHANCED_TONE_MODE::FREQUENCY_DWELL:    etm_control |= (0x9 << SYNTH_REG_ETMControl_bits_Function_shift); break;
				case ENHANCED_TONE_MODE::FREQUENCY_NO_DWELL: etm_control |= (0xB << SYNTH_REG_ETMControl_bits_Function_shift); break;
				case ENHANCED_TONE_MODE::FREQUENCY_FAST_MOD: etm_control |= (0x8 << SYNTH_REG_ETMControl_bits_Function_shift); break;
				case ENHANCED_TONE_MODE::PHASE_DWELL:        etm_control |= (0xD << SYNTH_REG_ETMControl_bits_Function_shift); break;
				case ENHANCED_TONE_MODE::PHASE_NO_DWELL:     etm_control |= (0xF << SYNTH_REG_ETMControl_bits_Function_shift); break;
				case ENHANCED_TONE_MODE::PHASE_FAST_MOD:     etm_control |= (0xC << SYNTH_REG_ETMControl_bits_Function_shift); break;
				}

				switch (tone->scaling())
				{
				case DAC_CURRENT_REFERENCE::FULL_SCALE: etm_control |= (3 << SYNTH_REG_ETMControl_bits_Scaling_shift); break;
				case DAC_CURRENT_REFERENCE::HALF_SCALE: etm_control |= (1 << SYNTH_REG_ETMControl_bits_Scaling_shift); break;
				case DAC_CURRENT_REFERENCE::QUARTER_SCALE: etm_control |= (2 << SYNTH_REG_ETMControl_bits_Scaling_shift); break;
				case DAC_CURRENT_REFERENCE::EIGHTH_SCALE: etm_control |= (0 << SYNTH_REG_ETMControl_bits_Scaling_shift); break;
				}

				iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ETMControl);
				iorpt->Payload<std::uint16_t>(etm_control);

				if (NullMessage == myiMSConn->SendMsg(*iorpt))
				{
					delete iorpt;
					return false;
				}
				delete iorpt;
			}
		}

		return true;
	}

	bool SignalPath::SetEnhancedToneChannel(const RFChannel& chan, const SweepTone& tone)
	{
		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		HostReport* iorpt;

		bool use_dds_script = true;
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		// Original iMS4 (rev A) cannot support DDS Scripts and therefore won't work with Enhanced Tone Mode
		// SDK v1.5.1: rev A v1.3.57 and later supports ETM
		if (p_Impl->myiMS.Synth().Model() == "iMS4") {
			if (p_Impl->myiMS.Synth().GetVersion().revision < 57) return false;
		}
		else if ((p_Impl->myiMS.Synth().Model() == "iMS4b") || (p_Impl->myiMS.Synth().Model() == "iMS4c")) {
			// iMS4b/4c use DDS scripts, from version 68 onwards.  Earlier builds had issue with DDS scripts
			if (p_Impl->myiMS.Synth().GetVersion().revision < 68) return false;
		}
		else if (p_Impl->myiMS.Synth().Model() == "iMS4d") {
			// iMS4d use direct programming, from version 148 onwards.
			if (p_Impl->myiMS.Synth().GetVersion().revision < 148) return false;
			use_dds_script = false;
		}
		else {
			// Synth doesn't support scripts
			use_dds_script = false;
		}

		if (chan.IsAll()) return SetEnhancedToneMode(tone, tone, tone, tone);

		if (use_dds_script && check_linear_sweep(p_Impl->myiMS, chan))
		{
			// Already playing out a linear sweep. Remove DDS script if present, and reprogram
			FileSystemTableViewer fstv(p_Impl->myiMS);
			FileSystemManager fsm(p_Impl->myiMS);
			std::string check("alwaoww");
			check += std::to_string((int)chan);

			for (int i = 0; i < MAX_FST_ENTRIES; i++) {
				FileSystemTypes type = fstv[i].Type();
				std::string name = fstv[i].Name();
				if ((type == FileSystemTypes::DDS_SCRIPT))
				{
					name.resize(8);
					if (name.compare(check) == 0)
					{
						fsm.Delete(i);
					}
				}
			}
		}

		// Check whether we support linear sweep as part of STM, or need to use a DDS script
		if (use_dds_script)
		{
			// Use a DDS Script
			DDSScript scr;
			add_sweep_to_script(p_Impl->myiMS, scr, tone, chan);

			scr.push_back(DDSScriptRegister(DDSScriptRegister::Name::UPDATE));

			DDSScriptDownload ddsdl(p_Impl->myiMS, scr);
			std::string name("alwaoww");
			name += std::to_string((int)chan);
			FileSystemIndex idx = ddsdl.Program(name, FileDefault::NON_DEFAULT);
			if (idx < 0) return false;

			FileSystemManager fsm(p_Impl->myiMS);
			fsm.Execute(idx);
		}
		else {
			std::vector<std::uint16_t> etm_data;

			etm_data = add_sweep_to_vector(p_Impl->myiMS, tone);

			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ETMStartFreqLo);
			iorpt->Payload<std::vector<std::uint16_t>>(etm_data);

			ReportFields f = iorpt->Fields();
			f.len = static_cast<std::uint16_t>(etm_data.size() * sizeof(std::uint16_t));
			iorpt->Fields(f);

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
			delete iorpt;

			etm_data = add_rate_to_vector(p_Impl->myiMS, tone);

			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_SweepFSRR);
			iorpt->Payload<std::vector<std::uint16_t>>(etm_data);

			f = iorpt->Fields();
			f.len = static_cast<std::uint16_t>(etm_data.size() * sizeof(std::uint16_t));
			iorpt->Fields(f);

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
			delete iorpt;

			// Program channel
			std::uint16_t etm_control = 0;

			etm_control |= SYNTH_REG_ETMControl_bits_Program_mask;  // Program Channel data
			etm_control |= SYNTH_REG_ETMControl_bits_Trigger_mask;  // And trigger transfer to DDS

			etm_control |= ((static_cast<int>(chan) - 1) & SYNTH_REG_ETMControl_bits_Channel_mask);
			switch (tone.mode())
			{
			case ENHANCED_TONE_MODE::NO_SWEEP: break;
			case ENHANCED_TONE_MODE::FREQUENCY_DWELL:    etm_control |= (0x9 << SYNTH_REG_ETMControl_bits_Function_shift); break;
			case ENHANCED_TONE_MODE::FREQUENCY_NO_DWELL: etm_control |= (0xB << SYNTH_REG_ETMControl_bits_Function_shift); break;
			case ENHANCED_TONE_MODE::FREQUENCY_FAST_MOD: etm_control |= (0x8 << SYNTH_REG_ETMControl_bits_Function_shift); break;
			case ENHANCED_TONE_MODE::PHASE_DWELL:        etm_control |= (0xD << SYNTH_REG_ETMControl_bits_Function_shift); break;
			case ENHANCED_TONE_MODE::PHASE_NO_DWELL:     etm_control |= (0xF << SYNTH_REG_ETMControl_bits_Function_shift); break;
			case ENHANCED_TONE_MODE::PHASE_FAST_MOD:     etm_control |= (0xC << SYNTH_REG_ETMControl_bits_Function_shift); break;
			}

			switch (tone.scaling())
			{
			case DAC_CURRENT_REFERENCE::FULL_SCALE: etm_control |= (3 << SYNTH_REG_ETMControl_bits_Scaling_shift); break;
			case DAC_CURRENT_REFERENCE::HALF_SCALE: etm_control |= (1 << SYNTH_REG_ETMControl_bits_Scaling_shift); break;
			case DAC_CURRENT_REFERENCE::QUARTER_SCALE: etm_control |= (2 << SYNTH_REG_ETMControl_bits_Scaling_shift); break;
			case DAC_CURRENT_REFERENCE::EIGHTH_SCALE: etm_control |= (0 << SYNTH_REG_ETMControl_bits_Scaling_shift); break;
			}

			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ETMControl);
			iorpt->Payload<std::uint16_t>(etm_control);

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
			delete iorpt;

		}
		return true;
	}

	bool SignalPath::ClearEnhancedToneChannel(const RFChannel& chan)
	{
		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		HostReport* iorpt;

		bool use_dds_script = true;
		// Original iMS4 (rev A) cannot support DDS Scripts and therefore won't work with Enhanced Tone Mode
		// SDK v1.5.1: rev A v1.3.57 and later supports ETM
		if (p_Impl->myiMS.Synth().Model() == "iMS4") {
			if (p_Impl->myiMS.Synth().GetVersion().revision < 57) return false;
		}
		else if ((p_Impl->myiMS.Synth().Model() == "iMS4b") || (p_Impl->myiMS.Synth().Model() == "iMS4c")) {
			// iMS4b/4c use DDS scripts, from version 68 onwards.  Earlier builds had issue with DDS scripts
			if (p_Impl->myiMS.Synth().GetVersion().revision < 68) return false;
		}
		else if (p_Impl->myiMS.Synth().Model() == "iMS4d") {
			// iMS4d use direct programming, from version 148 onwards.
			if (p_Impl->myiMS.Synth().GetVersion().revision < 148) return false;
			use_dds_script = false;
		}
		else {
			// Synth doesn't support scripts
			use_dds_script = false;
		}

		if (chan.IsAll()) return ClearEnhancedToneMode();

		if (!use_dds_script)
		{
			std::uint16_t etm_control = 0;

			etm_control |= SYNTH_REG_ETMControl_bits_Clear_mask;  // Clear ETM Mode

			// Do we support clearing ETM by channel???
			etm_control |= ((static_cast<int>(chan) - 1) & SYNTH_REG_ETMControl_bits_Channel_mask);
			
			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ETMControl);
			iorpt->Payload<std::uint16_t>(etm_control);

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
			delete iorpt;
		}
		else if (use_dds_script && check_linear_sweep(p_Impl->myiMS, chan))
		{
			// Uses DDS Script
			FileSystemTableViewer fstv(p_Impl->myiMS);

			// Reset Function Register
			DDSScript scr;

			SweepTone tone;
			add_sweep_to_script(p_Impl->myiMS, scr, tone, chan);

			scr.push_back(DDSScriptRegister(DDSScriptRegister::Name::UPDATE));

			DDSScriptDownload ddsdl(p_Impl->myiMS, scr);
			FileSystemIndex stop_idx = ddsdl.Program("stoplsm", FileDefault::NON_DEFAULT);
			if (stop_idx < 0) return false;

			FileSystemManager fsm(p_Impl->myiMS);
			fsm.Execute(stop_idx);
			fsm.Delete(stop_idx);

			std::string check("alwaoww");
			check += std::to_string((int)chan);

			for (int i = 0; i < MAX_FST_ENTRIES; i++) {
				FileSystemTypes type = fstv[i].Type();
				std::string name = fstv[i].Name();
				if ((type == FileSystemTypes::DDS_SCRIPT))
				{
					name.resize(8);
					if (name.compare(check) == 0)
					{
						fsm.Delete(i);
					}
				}
			}
		}
		else {
			return false;
		}

		return true;
	}

	bool SignalPath::ClearEnhancedToneMode()
	{
		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		HostReport* iorpt;

		bool use_dds_script = true;
		// Original iMS4 (rev A) cannot support DDS Scripts and therefore won't work with Enhanced Tone Mode
		// SDK v1.5.1: rev A v1.3.57 and later supports ETM
		if (p_Impl->myiMS.Synth().Model() == "iMS4") {
			if (p_Impl->myiMS.Synth().GetVersion().revision < 57) return false;
		}
		else if ((p_Impl->myiMS.Synth().Model() == "iMS4b") || (p_Impl->myiMS.Synth().Model() == "iMS4c")) {
			// iMS4b/4c use DDS scripts, from version 68 onwards.  Earlier builds had issue with DDS scripts
			if (p_Impl->myiMS.Synth().GetVersion().revision < 68) return false;
		}
		else if (p_Impl->myiMS.Synth().Model() == "iMS4d") {
			// iMS4d use direct programming, from version 148 onwards.
			if (p_Impl->myiMS.Synth().GetVersion().revision < 148) return false;
			use_dds_script = false;
		}
		else {
			// Synth doesn't support scripts 
			use_dds_script = false;
		}

		if (!use_dds_script)
		{
			std::uint16_t etm_control = 0;

			etm_control |= SYNTH_REG_ETMControl_bits_Clear_mask;  // Clear ETM Mode

			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ETMControl);
			iorpt->Payload<std::uint16_t>(etm_control);

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
			delete iorpt;
		}
		else if (use_dds_script && check_linear_sweep(p_Impl->myiMS))
		{
			// Check whether we support linear sweep as part of STM, or need to use a DDS script
//			if (((p_Impl->myiMS.Synth().Model() == "iMS4") || (p_Impl->myiMS.Synth().Model() == "iMS4b")) &&
//				(p_Impl->myiMS.Synth().GetVersion().revision <= 68))
			{
				// Uses DDS Script
				FileSystemTableViewer fstv(p_Impl->myiMS);

				// Reset Function Register
				DDSScript scr;

				SweepTone tone;
				add_sweep_to_script(p_Impl->myiMS, scr, tone, 1);
				add_sweep_to_script(p_Impl->myiMS, scr, tone, 2);
				add_sweep_to_script(p_Impl->myiMS, scr, tone, 3);
				add_sweep_to_script(p_Impl->myiMS, scr, tone, 4);
				
				scr.push_back(DDSScriptRegister(DDSScriptRegister::Name::UPDATE));

				DDSScriptDownload ddsdl(p_Impl->myiMS, scr);
				FileSystemIndex stop_idx = ddsdl.Program("stoplsm", FileDefault::NON_DEFAULT);
				if (stop_idx < 0) return false;

				FileSystemManager fsm(p_Impl->myiMS);
				fsm.Execute(stop_idx);

				for (int i = 0; i < MAX_FST_ENTRIES; i++) {
					FileSystemTypes type = fstv[i].Type();
					std::string name = fstv[i].Name();
					if ((type == FileSystemTypes::DDS_SCRIPT))
					{
						name.resize(7);
						if (name.compare(std::string("alwaoww")) == 0)
						{
							fsm.Delete(i);
						}
					}
				}
				fsm.Delete(stop_idx);
			}
//			else {
				// Uses STM
//				return false;
//			}

			return true;
		}
		return false;
	}

	bool SignalPath::AssignSynchronousOutput(const SYNC_SINK& sink, const SYNC_SRC& src) const
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
		switch (sink)
		{
			case SYNC_SINK::ANLG_A:	iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(SYNTH_REG_IOSig_ANLG_A_Mask)); break;
			case SYNC_SINK::ANLG_B:	iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(SYNTH_REG_IOSig_ANLG_B_Mask)); break;
			case SYNC_SINK::DIG:	iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(SYNTH_REG_IOSig_DIG_Mask)); break;
		}

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IOSig);
		std::uint16_t data;
		switch (sink)
		{
			case SYNC_SINK::ANLG_A: switch (src) {
				case SYNC_SRC::FREQUENCY_CH1: data = (0) << SYNTH_REG_IOSig_ANLG_A_Shift; break;
				case SYNC_SRC::FREQUENCY_CH2: data = (1) << SYNTH_REG_IOSig_ANLG_A_Shift; break;
				case SYNC_SRC::FREQUENCY_CH3: data = (2) << SYNTH_REG_IOSig_ANLG_A_Shift; break;
				case SYNC_SRC::FREQUENCY_CH4: data = (3) << SYNTH_REG_IOSig_ANLG_A_Shift; break;
				case SYNC_SRC::IMAGE_ANLG_A: data = (4) << SYNTH_REG_IOSig_ANLG_A_Shift; break;
				case SYNC_SRC::IMAGE_ANLG_B: data = (5) << SYNTH_REG_IOSig_ANLG_A_Shift; break;
				default: {
					delete iorpt;
					return false;
				}
			}
			break;
			case SYNC_SINK::ANLG_B: switch (src) {
				case SYNC_SRC::AMPLITUDE_PRE_COMP_CH1: data = (0) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::AMPLITUDE_PRE_COMP_CH2: data = (1) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::AMPLITUDE_PRE_COMP_CH3: data = (2) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::AMPLITUDE_PRE_COMP_CH4: data = (3) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::AMPLITUDE_CH1: data = (4) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::AMPLITUDE_CH2: data = (5) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::AMPLITUDE_CH3: data = (6) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::AMPLITUDE_CH4: data = (7) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::PHASE_CH1: data = (8) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::PHASE_CH2: data = (9) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::PHASE_CH3: data = (10) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::PHASE_CH4: data = (11) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::LOOKUP_FIELD_CH1: data = (12) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::LOOKUP_FIELD_CH2: data = (13) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::LOOKUP_FIELD_CH3: data = (14) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::LOOKUP_FIELD_CH4: data = (15) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::IMAGE_ANLG_A: data = (16) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				case SYNC_SRC::IMAGE_ANLG_B: data = (17) << SYNTH_REG_IOSig_ANLG_B_Shift; break;
				default: {
					delete iorpt;
					return false;
				}
			}
			break;
			case SYNC_SINK::DIG: switch (src) {
				case SYNC_SRC::IMAGE_DIG: data = (0) << SYNTH_REG_IOSig_DIG_Shift; break;
				case SYNC_SRC::PULSE_GATE: data = (1) << SYNTH_REG_IOSig_DIG_Shift; break;
				case SYNC_SRC::LOOKUP_FIELD_CH1: data = (4) << SYNTH_REG_IOSig_DIG_Shift; break;
				case SYNC_SRC::LOOKUP_FIELD_CH2: data = (5) << SYNTH_REG_IOSig_DIG_Shift; break;
				case SYNC_SRC::LOOKUP_FIELD_CH3: data = (6) << SYNTH_REG_IOSig_DIG_Shift; break;
				case SYNC_SRC::LOOKUP_FIELD_CH4: data = (7) << SYNTH_REG_IOSig_DIG_Shift; break;
				default: {
					delete iorpt;
					return false;
				}
			}
			break;
		}

		iorpt->Payload<std::uint16_t>(data);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		return true;

	}

	bool SignalPath::ConfigureSyncDigitalOutput(std::chrono::nanoseconds delay, std::chrono::nanoseconds pulse_length)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;
		std::vector<std::uint16_t> data;

		unsigned long long pulse = pulse_length.count() / 10;
		if (pulse > 65535) pulse = 65535;

		unsigned long long del = delay.count() / 10;
		int prescale = (del >> 12);
		del = del / (prescale + 1);
		if (prescale > 15) return false;

		data.push_back(static_cast<std::uint16_t>(pulse));
		data.push_back(static_cast<std::uint16_t>(del | (prescale << 12)));
//		std::cout << "Del = " << del << " prescale = " << prescale << std::endl;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_SDOR_Pulse);
		iorpt->Payload < std::vector <std::uint16_t> >(data);

		ReportFields f = iorpt->Fields();
		f.len = static_cast<std::uint16_t>(data.size() * sizeof(std::uint16_t));
		iorpt->Fields(f);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}

		return true;
	}

	bool SignalPath::SyncDigitalOutputInvert(bool invert)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		HostReport* iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_SDORInvert);
		iorpt->Payload < std::uint16_t > (invert ? 1 : 0);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SignalPath::SyncDigitalOutputMode(SYNC_DIG_MODE mode, int index)
	{
		if ( ((index < 0) || (index > 11)) && (index != INT_MAX))
			return false;

		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		HostReport* iorpt;

		if (index != INT_MAX)
		{
			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
			iorpt->Payload<std::uint16_t>(1 << index);

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
			delete iorpt;
		}

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_SDORPulseSelect);
		iorpt->Payload<std::uint16_t>(mode == SYNC_DIG_MODE::LEVEL ? 0xFFFF : 0);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		return true;
	}

	bool SignalPath::SetXYChannelDelay(::std::chrono::nanoseconds delay)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		HostReport* iorpt;
		std::vector<std::uint16_t> data;

//		std::uint16_t data;

		long long del = delay.count() / 10;
		if ((del > 4092) || (del < -4092)) return false;

		if (del < 0) {
			del = -del;

			// undelayed channel experiences a 30ns bypass delay.  Compensate by adding 30ns to the delayed channel
			del += 3;

			data.push_back(static_cast<std::uint16_t>(0));
			data.push_back(static_cast<std::uint16_t>(del));
		}
		else {
			del += 3;
			data.push_back(static_cast<std::uint16_t>(del));
			data.push_back(static_cast<std::uint16_t>(0));
		}


		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ChannelDelay34);
		iorpt->Payload < std::vector < std::uint16_t > >(data);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		return true;
	}

	bool SignalPath::SetChannelDelay(::std::chrono::nanoseconds first, ::std::chrono::nanoseconds second)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

		HostReport* iorpt;
		std::vector<std::uint16_t> data;

		long long del1 = first.count() / 10;
		if ((del1 > 4092) || (del1 < 0)) return false;

		long long del2 = second.count() / 10;
		if ((del2 > 4092) || (del2 < 0)) return false;

		if (del1 < 3) del2 += (3 - del1);
		if (del2 < 3) del1 += (3 - del2);

		data.push_back(static_cast<std::uint16_t>(del2));
		data.push_back(static_cast<std::uint16_t>(del1));

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ChannelDelay34);
		iorpt->Payload < std::vector < std::uint16_t > >(data);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		return true;
	}

	bool SignalPath::UpdateLocalToneBuffer(const SignalPath::ToneBufferControl& tbc, const unsigned int index, 
		const SignalPath::Compensation AmplitudeComp, const SignalPath::Compensation PhaseComp)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_UseLocal);
		std::uint16_t data = std::min(index, (const unsigned int)255);
		data |= ((AmplitudeComp == Compensation::ACTIVE) ? 0x800 : 0);
		data |= ((PhaseComp == Compensation::ACTIVE) ? 0x1000 : 0);
		switch (tbc) {
			case SignalPath::ToneBufferControl::HOST: data |= 0x100; break;
			case SignalPath::ToneBufferControl::EXTERNAL: data |= 0x300; break;
			case SignalPath::ToneBufferControl::EXTERNAL_EXTENDED: data |= 0x500; break;
			case SignalPath::ToneBufferControl::OFF: break;
		}
		iorpt->Payload<std::uint16_t>(data);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SignalPath::UpdateLocalToneBuffer(const SignalPath::ToneBufferControl& tbc)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, SYNTH_REG_UseLocal);
		DeviceReport Resp = myiMSConn->SendMsgBlocking(*iorpt);
		delete iorpt;
		std::uint16_t data;
		if (Resp.Done()) {
			data = Resp.Payload<std::uint16_t>();
		}
		else return false;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_UseLocal);
		data &= ~0x700;
		switch (tbc) {
		case SignalPath::ToneBufferControl::HOST: data |= 0x100; break;
		case SignalPath::ToneBufferControl::EXTERNAL: data |= 0x300; break;
		case SignalPath::ToneBufferControl::EXTERNAL_EXTENDED: data |= 0x500; break;
		case SignalPath::ToneBufferControl::OFF: break;
		}
		iorpt->Payload<std::uint16_t>(data);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SignalPath::UpdateLocalToneBuffer(const SignalPath::Compensation AmplitudeComp, const SignalPath::Compensation PhaseComp)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, SYNTH_REG_UseLocal);
		DeviceReport Resp = myiMSConn->SendMsgBlocking(*iorpt);
		delete iorpt;
		std::uint16_t data;
		if (Resp.Done()) {
			data = Resp.Payload<std::uint16_t>();
		}
		else return false;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_UseLocal);
		data &= ~0x1800;
		data |= ((AmplitudeComp == Compensation::ACTIVE) ? 0x800 : 0);
		data |= ((PhaseComp == Compensation::ACTIVE) ? 0x1000 : 0);
		iorpt->Payload<std::uint16_t>(data);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SignalPath::UpdateLocalToneBuffer(const unsigned int index)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, SYNTH_REG_UseLocal);
		DeviceReport Resp = myiMSConn->SendMsgBlocking(*iorpt);
		delete iorpt;
		std::uint16_t data;
		if (Resp.Done()) {
			data = Resp.Payload<std::uint16_t>();
		}
		else return false;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_UseLocal);
		data &= ~0xFF;
		std::uint16_t index_lim = std::min(index, (const unsigned int)255);
		data |= index_lim;
		iorpt->Payload<std::uint16_t>(data);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SignalPath::EnableImagePathCompensation(SignalPath::Compensation amplComp, SignalPath::Compensation phaseComp)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
		iorpt->Payload<std::uint16_t>(0x6);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Pix_Control);
		std::uint16_t data = 0;
		if (amplComp == SignalPath::Compensation::BYPASS) {
			data |= 2;
		}
		if (phaseComp == SignalPath::Compensation::BYPASS) {
			data |= 4;
		}
		iorpt->Payload<std::uint16_t>(data);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	SignalPath::CompensationScope SignalPath::QueryCompensationScope()
	{
		CompensationScope scope = CompensationScope::GLOBAL;

		// Make sure Synthesiser is present
		if (!p_Impl->myiMS.Synth().IsValid()) return scope;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();
		HostReport *iorpt;
		std::uint16_t data;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, SYNTH_REG_Chan_Scope);
		DeviceReport Resp = myiMSConn->SendMsgBlocking(*iorpt);
		delete iorpt;
		if (Resp.Done()) {
			data = Resp.Payload<std::uint16_t>();
		}
		else return scope;

		if (!(data & 0x100)) {
			// no support for channel scoped compensation
			return scope;
		} else {
			if (data & 0xF) {
				scope = CompensationScope::CHANNEL;
			}
		}
		return scope;
	}

	bool SignalPath::EnableXYPhaseCompensation(bool XYCompEnable)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
		iorpt->Payload<std::uint16_t>(16);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Pix_Control);
		std::uint16_t data = 0;
		if (XYCompEnable) {
			data |= 16;
		}
		iorpt->Payload<std::uint16_t>(data);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	
	bool SignalPath::PhaseResync()
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		// Original iMS4 (rev A) cannot support DDS Scripts and therefore won't work with Phase Resync
		// SDK v1.5.1: rev A v1.3.57 and later supports ETM
		auto model = p_Impl->myiMS.Synth().Model();
		// Original iMS4 (rev A) cannot support DDS Scripts and therefore won't work with Enhanced Tone Mode
		// SDK v1.5.1: rev A v1.3.57 and later supports ETM
		if (model == "iMS4") {
			if (p_Impl->myiMS.Synth().GetVersion().revision < 57) return false;
		}
		else if ((model == "iMS4b") || (model == "iMS4c")) {
			// iMS4b/4c use DDS scripts, from version 68 onwards.  Earlier builds had issue with DDS scripts
			if (p_Impl->myiMS.Synth().GetVersion().revision < 68) return false;
		}

		if ((model == "iMS4") || (model == "iMS4b") || (model == "iMS4c")) {
		  FileSystemIndex idx;
		  bool need_to_download = !fst_match(p_Impl->myiMS, std::string("PHSCLR"));
		  if (need_to_download) {
		    DDSScript scr;
		    scr.push_back(DDSScriptRegister(DDSScriptRegister::Name::FR2, { 0x10, 0x00}));
		    scr.push_back(DDSScriptRegister(DDSScriptRegister::Name::UPDATE));
		    /*			scr.push_back(DDSScriptRegister(DDSScriptRegister::Name::FR2, { 0x00, 0x00 }));
					scr.push_back(DDSScriptRegister(DDSScriptRegister::Name::UPDATE));*/

		    DDSScriptDownload ddsdl(p_Impl->myiMS, scr);
		    idx = ddsdl.Program("PHSCLR", FileDefault::NON_DEFAULT);
		  }
		  else {
		    idx = fst_getindex(p_Impl->myiMS, "PHSCLR");
		  }
		  if (idx < 0) return false;

		  FileSystemManager fsm(p_Impl->myiMS);
		  if (!fsm.Execute(idx)) return false;

		  // Phase is asynchronously cleared. Now set Autoclear off to restore operation
		  return this->AutoPhaseResync(false);
		} else {
		  // Send Synth Reg command
		  HostReport *iorpt;

		  iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
		  iorpt->Payload<std::uint16_t>(0x2);

		  if (NullMessage == myiMSConn->SendMsg(*iorpt))
		    {
		      delete iorpt;
		      return false;
		    }
		  delete iorpt;

		  iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Phase_Resync);
		  iorpt->Payload<std::uint16_t>(0x2);

		  if (NullMessage == myiMSConn->SendMsg(*iorpt))
		    {
		      delete iorpt;
		      return false;
		    }
		  delete iorpt;
		}
		return true;
	}

	bool SignalPath::AutoPhaseResync(bool enable)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		// Original iMS4 (rev A) cannot support DDS Scripts and therefore won't work with Phase Resync
		// SDK v1.5.1: rev A v1.3.57 and later supports ETM
		auto model = p_Impl->myiMS.Synth().Model();
		// Original iMS4 (rev A) cannot support DDS Scripts and therefore won't work with Enhanced Tone Mode
		// SDK v1.5.1: rev A v1.3.57 and later supports ETM
		if (model == "iMS4") {
			if (p_Impl->myiMS.Synth().GetVersion().revision < 57) return false;
		}
		else if ((model == "iMS4b") || (model == "iMS4c")) {
			// iMS4b/4c use DDS scripts, from version 68 onwards.  Earlier builds had issue with DDS scripts
			if (p_Impl->myiMS.Synth().GetVersion().revision < 68) return false;
		}

		if ((model == "iMS4") || (model == "iMS4b") || (model == "iMS4c")) {
		  FileSystemIndex idx;
		  if (enable) {
		    bool need_to_download = !fst_match(p_Impl->myiMS, std::string("APHSCLRE"));
		    if (need_to_download) {
		      DDSScript scr;
		      scr.push_back(DDSScriptRegister(DDSScriptRegister::Name::FR2, { 0x20, 0x00 }));
		      scr.push_back(DDSScriptRegister(DDSScriptRegister::Name::UPDATE));

		      DDSScriptDownload ddsdl(p_Impl->myiMS, scr);
		      idx = ddsdl.Program("APHSCLRE", FileDefault::NON_DEFAULT);
		    }
		    else {
		      idx = fst_getindex(p_Impl->myiMS, "APHSCLRE");
		    }
		  }
		  else {
		    bool need_to_download = !fst_match(p_Impl->myiMS, std::string("APHSCLRD"));
		    if (need_to_download) {
		      DDSScript scr;
		      scr.push_back(DDSScriptRegister(DDSScriptRegister::Name::FR2, { 0x00, 0x00 }));
		      scr.push_back(DDSScriptRegister(DDSScriptRegister::Name::UPDATE));

		      DDSScriptDownload ddsdl(p_Impl->myiMS, scr);
		      idx = ddsdl.Program("APHSCLRD", FileDefault::NON_DEFAULT);
		    }
		    else {
		      idx = fst_getindex(p_Impl->myiMS, "APHSCLRD");
		    }
		  }
		  if (idx < 0) return false;

		  FileSystemManager fsm(p_Impl->myiMS);
		  return fsm.Execute(idx);
		} else {
		  // Send Synth Reg command
		  HostReport *iorpt;

		  iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
		  iorpt->Payload<std::uint16_t>(0x1);

		  if (NullMessage == myiMSConn->SendMsg(*iorpt))
		    {
		      delete iorpt;
		      return false;
		    }
		  delete iorpt;

		  iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Phase_Resync);
		  std::uint16_t data = 0;
		  if (enable) data = 1;
		  iorpt->Payload<std::uint16_t>(data);

		  if (NullMessage == myiMSConn->SendMsg(*iorpt))
		    {
		      delete iorpt;
		      return false;
		    }
		  delete iorpt;
		  return true;
		}
	}

	bool SignalPath::AddFrequencyOffset(MHz& offset, RFChannel chan)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();
		HostReport* iorpt;
		
		uint32_t offset_int = FrequencyRenderer::RenderAsStaticOffset(p_Impl->myiMS, offset, 0);

		if (chan.IsAll()) {
			std::vector<uint16_t> payload;
			for (RFChannel ch = RFChannel::min; ; ch++) {
				payload.push_back(static_cast<uint16_t>(offset_int));
				if (ch == RFChannel::max) break;
			}
			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Freq_Offset_Ch1);
			iorpt->Payload<std::vector<std::uint16_t>>(payload);
		}
		else {
			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Freq_Offset_Ch1 + ((int)chan - RFChannel::min));
			iorpt->Payload<std::uint16_t>(static_cast<uint16_t>(offset_int));
		}

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SignalPath::SubtractFrequencyOffset(MHz& offset, RFChannel chan)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();
		HostReport* iorpt;

		uint32_t offset_int = FrequencyRenderer::RenderAsStaticOffset(p_Impl->myiMS, offset, 1);

		if (chan.IsAll()) {
			std::vector<uint16_t> payload;
			for (RFChannel ch = RFChannel::min; ; ch++) {
				payload.push_back(static_cast<uint16_t>(offset_int));
				if (ch == RFChannel::max) break;
			}
			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Freq_Offset_Ch1);
			iorpt->Payload<std::vector<std::uint16_t>>(payload);
		}
		else {
			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Freq_Offset_Ch1 + ((int)chan - RFChannel::min));
			iorpt->Payload<std::uint16_t>(static_cast<uint16_t>(offset_int));
		}

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;
		return true;
	}

	bool SignalPath::UpdateEncoder(const VelocityConfiguration& velcomp)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;
		std::vector<std::uint16_t> data;

		// Set Mode
		iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ENC_Control);

		data.push_back(((velcomp.EncoderMode == ENCODER_MODE::QUADRATURE) ? ENC_Control_ENC_Mode_Quadrature : ENC_Control_ENC_Mode_Clk_Dir) |
			((velcomp.VelocityMode == VELOCITY_MODE::FAST) ? ENC_Control_Vel_Mode_Fast : ENC_Control_Vel_Mode_Slow));
		data.push_back(velcomp.TrackingLoopProportionCoeff);
		data.push_back(velcomp.TrackingLoopIntegrationCoeff);
		data.push_back(0);
		data.push_back(0);
		data.push_back(velcomp.VelocityGain[0]);
		data.push_back(velcomp.VelocityGain[1]);

		iorpt->Payload<std::vector<std::uint16_t>>(data);
		ReportFields f = iorpt->Fields();
		f.len = static_cast<std::uint16_t>(data.size() * sizeof(std::uint16_t));
		iorpt->Fields(f);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		return true;
	}

	bool SignalPath::DisableEncoder()
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		std::vector<std::uint16_t> data;

		HostReport *iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_ENC_VelGainX);
		data.push_back(0);
		data.push_back(0);

		iorpt->Payload<std::vector<std::uint16_t>>(data);
		ReportFields f = iorpt->Fields();
		f.len = static_cast<std::uint16_t>(data.size() * sizeof(std::uint16_t));
		iorpt->Fields(f);

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		return true;
	}

	bool SignalPath::ReportEncoderVelocity(ENCODER_CHANNEL chan)
	{
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

		HostReport *iorpt;
		
		if (chan == ENCODER_CHANNEL::CH_X) {
			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, SYNTH_REG_ENC_VelEstX);

			std::unique_lock<std::mutex> lck{ p_Impl->m_bkmutex };
			p_Impl->VelocityHandle[0].push(myiMSConn->SendMsg(*iorpt));
			delete iorpt;

			if (NullMessage == p_Impl->VelocityHandle[0].back())
			{
				lck.unlock();
				return false;
			}
			lck.unlock();
			return true;
		}
		else if (chan == ENCODER_CHANNEL::CH_Y) {
			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, SYNTH_REG_ENC_VelEstY);

			std::unique_lock<std::mutex> lck{ p_Impl->m_bkmutex };
			p_Impl->VelocityHandle[1].push(myiMSConn->SendMsg(*iorpt));
			delete iorpt;

			if (NullMessage == p_Impl->VelocityHandle[1].back())
			{
				lck.unlock();
				return false;
			}
			lck.unlock();
			return true;
		}
		else return false;
	}

	void VelocityConfiguration::SetVelGain(const IMSSystem& ims, SignalPath::ENCODER_CHANNEL chan, kHz EncoderFreq, MHz DesiredFreqDeviation, bool Reverse)
	{
		double freq_range = ims.Synth().GetCap().upperFrequency - ims.Synth().GetCap().lowerFrequency;
		int value = static_cast<int>( (32767 / (freq_range / (4 * 65.535) ) ) * DesiredFreqDeviation.operator double() / EncoderFreq.operator double() );
		if (Reverse) value = -value;
		if (value > INT16_MAX) value = INT16_MAX;
		if (value < INT16_MIN) value = INT16_MIN;
		if (chan == SignalPath::ENCODER_CHANNEL::CH_X) {
			this->VelocityGain[0] = static_cast<int16_t>(value);
		}
		else if (chan == SignalPath::ENCODER_CHANNEL::CH_Y) {
			this->VelocityGain[1] = static_cast<int16_t>(value);
		}
	}

}
