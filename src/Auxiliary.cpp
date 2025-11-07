/*-----------------------------------------------------------------------------
/ Title      : Auxiliary Functions CPP
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Auxiliary/src/Auxiliary.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2018-01-28 23:21:45 +0000 (Sun, 28 Jan 2018) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 315 $
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

#include "Auxiliary.h"
#include "IEventTrigger.h"
#include "FileSystem_p.h"
#include "HostReport.h"
#include "IConnectionManager.h"
#include "PrivateUtil.h"

#include <mutex>
#include <thread>
#include <condition_variable>

namespace iMS
{
	const std::uint16_t SYNTH_REG_IO_LED_Control = 17;
	const std::uint16_t SYNTH_REG_IO_GPInput = 28;
	const std::uint16_t SYNTH_REG_IO_GPOutput = 29;
	const std::uint16_t SYNTH_REG_IO_DDS_Profile = 30;
	const std::uint16_t SYNTH_REG_IO_Config_Mask = 31;

	const int ACR_extadc_convbusy_bit = 14;

	// RF ADC Channel Numbers
	const int EXTADC_ch1 = 0;
	const int EXTADC_ch2 = 2;

	class AuxiliaryEventTrigger :
		public IEventTrigger
	{
	public:
		AuxiliaryEventTrigger() { updateCount(AuxiliaryEvents::Count); }
		~AuxiliaryEventTrigger() {};
	};

	class Auxiliary::Impl
	{
	public:
		Impl(std::shared_ptr<IMSSystem>, const Auxiliary* aux);
		~Impl();

		std::weak_ptr<IMSSystem> m_ims;
		AuxiliaryEventTrigger m_Event;

		bool ADCActive;
		void ADCWorker();
		std::thread ADCThread;
		mutable std::mutex m_adcmutex;
		std::condition_variable m_adccond;

		std::map<EXT_ANLG_INPUT, Percent> m_measurements;
	private:
		const Auxiliary * const m_parent;
	};

	Auxiliary::Impl::Impl(std::shared_ptr<IMSSystem> ims, const Auxiliary* aux) : m_ims(ims), m_parent(aux)
	{
		auto conn = ims->Connection();

		// Configure ADC
		HostReport *iorpt;
		iorpt = new HostReport(HostReport::Actions::EXT_ADC, HostReport::Dir::WRITE, 0);
		iorpt->Payload<std::uint16_t>(0x5A);
		conn->SendMsg(*iorpt);
		delete iorpt;
	}

	Auxiliary::Impl::~Impl() 
	{}

	void Auxiliary::Impl::ADCWorker()
	{
		while (ADCActive)
		{
			std::unique_lock<std::mutex> lck{ m_adcmutex };
			m_adccond.wait(lck);
			if (!ADCActive) {
				lck.unlock();
				break;
			}

            auto ims = m_ims.lock();
            if (!ims) {
                lck.unlock();
				break;
            }
            auto conn = ims->Connection();

			HostReport *iorpt;
			DeviceReport Resp;

			std::uint16_t busy;
			do {
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				iorpt = new HostReport(HostReport::Actions::ASYNC_CONTROL, HostReport::Dir::READ, 0xFFFF);
				Resp = conn->SendMsgBlocking(*iorpt);
				if (!Resp.Done())
				{
					delete iorpt;
					m_Event.Trigger<int>((void *)m_parent, AuxiliaryEvents::EXT_ANLG_READ_FAILED, 0);
					break;
				}
				delete iorpt;
				busy = Resp.Payload<std::uint16_t>();
			} while ((busy & (1 << ACR_extadc_convbusy_bit)) != 0);
			if (!Resp.Done()) continue;

			iorpt = new HostReport(HostReport::Actions::EXT_ADC, HostReport::Dir::READ, 0);
			ReportFields f = iorpt->Fields();
			f.len = 16;
			iorpt->Fields(f);
			Resp = conn->SendMsgBlocking(*iorpt);
			if (!Resp.Done())
			{
				delete iorpt;
				m_Event.Trigger<int>((void *)m_parent, AuxiliaryEvents::EXT_ANLG_READ_FAILED, 0);
				continue;
			}
			delete iorpt;
			std::vector<std::uint16_t> data = Resp.Payload<std::vector<std::uint16_t>>();

			m_measurements.clear();

			int channel = 0;
			for (std::vector<std::uint16_t>::const_iterator iter = data.cbegin(); iter != data.cend(); ++iter)
			{
				//int channel = ((*iter) & 0xF000) >> 12;  // upper nibble not compatible with decoding without modifying channel constants
				double value = ((double)((*iter) & 0xFFF) * 100.0 / 4095.0);
				switch (channel++) {
				case EXTADC_ch1: m_measurements[EXT_ANLG_INPUT::A] = value; break;
				case EXTADC_ch2: m_measurements[EXT_ANLG_INPUT::A] = value; break;
				}
			}

			m_Event.Trigger<int>((void *)m_parent, AuxiliaryEvents::EXT_ANLG_UPDATE_AVAILABLE, 0);
			m_Event.Trigger<double>((void *)m_parent, AuxiliaryEvents::EXT_ANLG_UPDATE_AVAILABLE, 0.0);

			lck.unlock();
		}
	}

	Auxiliary::Auxiliary(std::shared_ptr<IMSSystem> iMS) : p_Impl(new Impl(iMS, this)) 
	{
		// Start ADC listening thread
		p_Impl->ADCActive = true;
		p_Impl->ADCThread = std::thread(&Impl::ADCWorker, p_Impl);
	}

	Auxiliary::~Auxiliary() 
	{ 
		// Stop thread
		p_Impl->ADCActive = false;
		p_Impl->m_adccond.notify_one();
		p_Impl->ADCThread.join();

		delete p_Impl;
		p_Impl = nullptr;
	}

	bool Auxiliary::UpdateAnalogIn()
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();
            std::uint16_t data = (1 << ACR_extadc_convbusy_bit);
            auto iorpt = new HostReport(HostReport::Actions::ASYNC_CONTROL, HostReport::Dir::WRITE, data);
            iorpt->Payload<std::uint16_t>(data);
            MessageHandle h = conn->SendMsg(*iorpt);
            if (NullMessage == h)
            {
                delete iorpt;
                return false;
            }
            else {
                p_Impl->m_adccond.notify_one();
            }
            delete iorpt;
            return true;
        }).value_or(false);
	}

	const std::map<Auxiliary::EXT_ANLG_INPUT, Percent>& Auxiliary::GetAnalogData() const
	{
		return p_Impl->m_measurements;
	}

	// Set External Analog Output
	bool Auxiliary::UpdateAnalogOut(Percent& pct) const
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();
            auto iorpt = new HostReport(HostReport::Actions::ASYNC_DAC, HostReport::Dir::WRITE, 0);
            std::uint16_t data = static_cast<std::uint16_t>(((double)pct * 4095.0) / 100.0);
            iorpt->Payload<std::uint16_t>(data);
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;
            return true;
        }).value_or(false);
	}

	void Auxiliary::AuxiliaryEventSubscribe(const int message, IEventHandler* handler)
	{
		p_Impl->m_Event.Subscribe(message, handler);
	}

	void Auxiliary::AuxiliaryEventUnsubscribe(const int message, const IEventHandler* handler)
	{
		p_Impl->m_Event.Unsubscribe(message, handler);
	}

	bool Auxiliary::AssignLED(const LED_SINK& sink, const LED_SOURCE& src) const
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {        
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();
            auto iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
            switch (sink)
            {
                case LED_SINK::GREEN:	iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(0xF)); break;
                case LED_SINK::YELLOW:	iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(0xF0)); break;
                case LED_SINK::RED:		iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(0xF00)); break;
            }
            
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;

            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_LED_Control);
            std::uint16_t data = static_cast<std::uint16_t>(src);
            switch (sink)
            {
                case LED_SINK::GREEN: 		iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(data)); break;
                case LED_SINK::YELLOW: 		iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(data << 4)); break;
                case LED_SINK::RED: 		iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(data << 8)); break;
            }

            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;

            return true;
        }).value_or(false);
	}

	bool Auxiliary::SetDDSProfile(const DDS_PROFILE& prfl) const
	{
		return (SetDDSProfile(prfl, 0));
	}

	bool Auxiliary::SetDDSProfile(const DDS_PROFILE& prfl, const std::uint16_t& select) const
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {   
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();
            auto iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_DDS_Profile);
            //std::uint16_t data = select % 16;
            std::uint16_t data = static_cast<std::uint16_t>(prfl);
            iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(data));

            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;

            iorpt = new HostReport(HostReport::Actions::ASYNC_CONTROL, HostReport::Dir::WRITE, 0xF00);  // address is data mask
            data = (select % 16) << 8;

            iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(data));

            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;
            return true;
        }).value_or(false);
	}

	class DDSScriptRegister::Impl
	{
	public:
		Impl() {};
		Impl(DDSScriptRegister::Name name);
		Impl(DDSScriptRegister::Name name, const std::initializer_list<std::uint8_t>& data);
		Impl(DDSScriptRegister::Name name, const std::deque<std::uint8_t>& data);
		std::pair < Name, std::deque<std::uint8_t> > m_register_data;
		static std::map<Name, int> create_register_map();
		static const std::map<Name, int> RegisterSize;
	};

	DDSScriptRegister::Impl::Impl(DDSScriptRegister::Name name) : m_register_data(name, std::deque<std::uint8_t>()) {};

	DDSScriptRegister::Impl::Impl(DDSScriptRegister::Name name, const std::initializer_list<std::uint8_t>& data) :
		m_register_data(name, data) {};

	DDSScriptRegister::Impl::Impl(DDSScriptRegister::Name name, const std::deque<std::uint8_t>& data) :
		m_register_data(name, data) {};

	DDSScriptRegister::DDSScriptRegister(DDSScriptRegister::Name name) : p_Impl(new Impl(name)) {};
	
	DDSScriptRegister::DDSScriptRegister(DDSScriptRegister::Name name, const std::initializer_list<std::uint8_t>& data) :
		p_Impl(new Impl(name, data)) {};

	DDSScriptRegister::DDSScriptRegister(DDSScriptRegister::Name name, const std::deque<std::uint8_t>& data) :
		p_Impl(new Impl(name, data)) {};

	DDSScriptRegister::DDSScriptRegister(const DDSScriptRegister &rhs) : p_Impl(new Impl())
	{
		p_Impl->m_register_data = rhs.p_Impl->m_register_data;
	}

	DDSScriptRegister &DDSScriptRegister::operator =(const DDSScriptRegister &rhs)
	{
		if (this == &rhs) return *this;
		p_Impl->m_register_data = rhs.p_Impl->m_register_data;
		return *this;
	}

	DDSScriptRegister::~DDSScriptRegister() { delete p_Impl; p_Impl = nullptr; }

	std::map<DDSScriptRegister::Name, int> DDSScriptRegister::Impl::create_register_map()
	{
		std::map<DDSScriptRegister::Name, int> m;
		m[DDSScriptRegister::Name::CSR] = 1;
		m[DDSScriptRegister::Name::FR1] = 3;
		m[DDSScriptRegister::Name::FR2] = 2;
		m[DDSScriptRegister::Name::CFR] = 3;
		m[DDSScriptRegister::Name::CFTW0] = 4;
		m[DDSScriptRegister::Name::CPOW0] = 2;
		m[DDSScriptRegister::Name::ACR] = 3;
		m[DDSScriptRegister::Name::LSRR] = 2;
		m[DDSScriptRegister::Name::RDW] = 4;
		m[DDSScriptRegister::Name::FDW] = 4;
		m[DDSScriptRegister::Name::CW1] = 4;
		m[DDSScriptRegister::Name::CW2] = 4;
		m[DDSScriptRegister::Name::CW3] = 4;
		m[DDSScriptRegister::Name::CW4] = 4;
		m[DDSScriptRegister::Name::CW5] = 4;
		m[DDSScriptRegister::Name::CW6] = 4;
		m[DDSScriptRegister::Name::CW7] = 4;
		m[DDSScriptRegister::Name::CW8] = 4;
		m[DDSScriptRegister::Name::CW9] = 4;
		m[DDSScriptRegister::Name::CW10] = 4;
		m[DDSScriptRegister::Name::CW11] = 4;
		m[DDSScriptRegister::Name::CW12] = 4;
		m[DDSScriptRegister::Name::CW13] = 4;
		m[DDSScriptRegister::Name::CW14] = 4;
		m[DDSScriptRegister::Name::CW15] = 4;
		m[DDSScriptRegister::Name::UPDATE] = 0;
		return m;
	}

	const std::map<DDSScriptRegister::Name, int> DDSScriptRegister::Impl::RegisterSize = DDSScriptRegister::Impl::create_register_map();

	int DDSScriptRegister::append(const std::uint8_t& c)
	{
		p_Impl->m_register_data.second.push_back(c);
		return (p_Impl->m_register_data.second.size());
	}

	std::vector<std::uint8_t> DDSScriptRegister::bytes() const
	{
		std::deque<std::uint8_t> d = p_Impl->m_register_data.second;
		d.resize(p_Impl->RegisterSize.find(p_Impl->m_register_data.first)->second);
		d.push_front(static_cast<std::uint8_t>(p_Impl->m_register_data.first));
		std::vector<std::uint8_t> v(d.cbegin(), d.cend());
		return v;
	}

	class DDSScriptDownload::Impl
	{
	public:
		Impl(std::shared_ptr<IMSSystem> ims, const DDSScript& dds);
		std::weak_ptr<IMSSystem> m_ims;
		const DDSScript& m_dds;
		FileSystemWriter* fsw;
	};

	DDSScriptDownload::Impl::Impl(std::shared_ptr<IMSSystem> ims, const DDSScript& dds) : m_ims(ims), m_dds(dds) {};

	DDSScriptDownload::DDSScriptDownload(std::shared_ptr<IMSSystem> ims, const DDSScript& dds) : p_Impl(new Impl(ims, dds)) {};

	DDSScriptDownload::~DDSScriptDownload() { delete p_Impl; p_Impl = nullptr; };

	const FileSystemIndex DDSScriptDownload::Program(const std::string& FileName, FileDefault def) const
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> FileSystemIndex
        {        
            FileSystemManager fsm(ims);
            std::uint32_t addr;

            std::vector<std::uint8_t> data;
            for (DDSScript::const_iterator it = p_Impl->m_dds.cbegin(); it != p_Impl->m_dds.cend(); ++it)
            {
                std::vector<std::uint8_t> dds_bytes = it->bytes();
                data.insert(data.end(), dds_bytes.cbegin(), dds_bytes.cend());
            }

            if (!fsm.FindSpace(addr, data)) return -1;
            FileSystemTableEntry fste(FileSystemTypes::DDS_SCRIPT, addr, data.size(), def, FileName);
            p_Impl->fsw = new FileSystemWriter(ims, fste, data);

            FileSystemIndex result = p_Impl->fsw->Program();
            delete p_Impl->fsw;
            return result;
        }).value_or(-1);
	}
}
