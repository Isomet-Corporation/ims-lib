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
		Impl(IMSSystem&, RFChannel chan);
		~Impl();
		IMSSystem& myiMS;
		RFChannel chan;
	};

	WaveShaping::Impl::Impl(IMSSystem& iMS, RFChannel chan) :
		myiMS(iMS),
		chan(chan)
	{
		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("WaveShaping::WaveShaping()");
	}

	WaveShaping::Impl::~Impl()
	{
		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("WaveShaping::~WaveShaping()");
	}

	WaveShaping::WaveShaping(IMSSystem& iMS, RFChannel chan) : p_Impl(new Impl(iMS, chan))
	{
	}

	WaveShaping::~WaveShaping()
	{
		delete p_Impl;
		p_Impl = nullptr;
	}

	bool WaveShaping::FreeRunningPulseGating(unsigned int gate_interval, unsigned int gate_width, StartingEdge edge)
	{
		// Make sure Synthesiser is present
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

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

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}

			f.addr = 1 + (i * 0x100);
			iorpt->Fields(f);

			// Send free running Gate Interval
			iorpt->Payload<std::uint16_t>(gate_interval);

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}

			f.addr = 2 + (i * 0x100);
			iorpt->Fields(f);
			iorpt->Payload<std::uint16_t>(static_cast<uint16_t>(edge) | 0x0002);

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}

			/* Send prescale value */
			f.addr = 3 + (i * 0x100);
			iorpt->Fields(f);
			iorpt->Payload<std::uint16_t>(p_Impl->myiMS.Synth().GetCap().sysClock);

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
		} while (i++ < (p_Impl->myiMS.Synth().GetCap().channels) && p_Impl->chan.IsAll());

		delete iorpt;
		return true;
	}

	bool WaveShaping::OnTriggerPulseGating(unsigned int gate_width, StartingEdge edge)
	{
		// Make sure Synthesiser is present
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager* const myiMSConn = p_Impl->myiMS.Connection();

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

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}

			f.addr = 2 + (i * 0x100);
			iorpt->Fields(f);
			iorpt->Payload<std::uint16_t>(static_cast<uint16_t>(edge) & ~0x0002);

			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}

			/* Send prescale value */
			f.addr = 3 + (i * 0x100);
			iorpt->Fields(f);
			iorpt->Payload<std::uint16_t>(p_Impl->myiMS.Synth().GetCap().sysClock);

		} while (i++ < (p_Impl->myiMS.Synth().GetCap().channels) && p_Impl->chan.IsAll());

		delete iorpt;
		return true;

	}

}