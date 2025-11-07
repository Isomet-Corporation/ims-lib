/*-----------------------------------------------------------------------------
/ Title      : Wave Shaping Functions Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/SignalPath/src/WaveShaping.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2020-11-02
/ Last update: $Date: 2020-06-05 07:45:07 +0100 (Fri, 05 Jun 2020) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 443 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2020 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2020-11-02  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

#include "WaveShaping.h"
#include "IMSConstants.h"
#include "IConnectionManager.h"
#include "PrivateUtil.h"

namespace iMS
{
	class WaveShaping::Impl
	{
	public:
		Impl(std::shared_ptr<IMSSystem>, RFChannel chan);
		~Impl();
		std::weak_ptr<IMSSystem> m_ims;
		RFChannel chan;
	};

	WaveShaping::Impl::Impl(std::shared_ptr<IMSSystem> iMS, RFChannel chan) :
		m_ims(iMS),
		chan(chan)
	{
		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("WaveShaping::WaveShaping()");
	}

	WaveShaping::Impl::~Impl()
	{
		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("WaveShaping::~WaveShaping()");
	}

	WaveShaping::WaveShaping(std::shared_ptr<IMSSystem> ims, RFChannel chan) : p_Impl(new Impl(ims, chan))
	{
	}

	WaveShaping::~WaveShaping()
	{
		delete p_Impl;
		p_Impl = nullptr;
	}

	bool WaveShaping::FreeRunningPulseGating(unsigned int gate_interval, unsigned int gate_width, StartingEdge edge)
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {         
            // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();

            HostReport* iorpt;
            ReportFields f;

            // Send Gate Width in Half cycles
            iorpt = new HostReport(HostReport::Actions::WAVE_SHAPING, HostReport::Dir::WRITE, 0);

            int i = 0;
            do
            {
                if (!p_Impl->chan.IsAll()) {
                    i = static_cast<int>(p_Impl->chan) - 1;
                }
                f = iorpt->Fields();
                f.addr = 0 + (i * 0x100);
                iorpt->Fields(f);

                iorpt->Payload<std::uint16_t>(gate_width);

                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }

                f.addr = 1 + (i * 0x100);
                iorpt->Fields(f);

                // Send free running Gate Interval
                iorpt->Payload<std::uint16_t>(gate_interval);

                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }

                f.addr = 2 + (i * 0x100);
                iorpt->Fields(f);
                iorpt->Payload<std::uint16_t>(static_cast<uint16_t>(edge) | 0x0002);

                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }

                /* Send prescale value */
                f.addr = 3 + (i * 0x100);
                iorpt->Fields(f);
                iorpt->Payload<std::uint16_t>(ims->Synth().GetCap().sysClock);

                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }
            } while (i++ < (ims->Synth().GetCap().channels) && p_Impl->chan.IsAll());

            delete iorpt;
            return true;
        }).value_or(false);
	}

	bool WaveShaping::OnTriggerPulseGating(unsigned int gate_width, StartingEdge edge)
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {         
            // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();

            HostReport* iorpt;
            ReportFields f;

            // Send Gate Width in Half cycles
            iorpt = new HostReport(HostReport::Actions::WAVE_SHAPING, HostReport::Dir::WRITE, 0);

            int i = 0;
            do
            {
                if (!p_Impl->chan.IsAll()) {
                    i = static_cast<int>(p_Impl->chan) - 1;
                }
                f = iorpt->Fields();
                f.addr = 0 + (i * 0x100);
                iorpt->Fields(f);

                iorpt->Payload<std::uint16_t>(gate_width);

                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }

                f.addr = 2 + (i * 0x100);
                iorpt->Fields(f);
                iorpt->Payload<std::uint16_t>(static_cast<uint16_t>(edge) & ~0x0002);

                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }

                /* Send prescale value */
                f.addr = 3 + (i * 0x100);
                iorpt->Fields(f);
                iorpt->Payload<std::uint16_t>(ims->Synth().GetCap().sysClock);

            } while (i++ < (ims->Synth().GetCap().channels) && p_Impl->chan.IsAll());

            delete iorpt;
            return true;
        }).value_or(false);

	}

}