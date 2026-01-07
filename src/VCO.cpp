/*-----------------------------------------------------------------------------
/ Title      : Voltage Controlled Oscillator Functions Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/SignalPath/src/VCO.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2025-12-019
/ Last update: $Date: 2020-06-05 07:45:07 +0100 (Fri, 05 Jun 2020) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 443 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2025 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2025-12-19  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

#include "VCO.h"
#include "IMSConstants.h"
#include "IConnectionManager.h"
#include "PrivateUtil.h"
#include "IMSTypeDefs_p.h"

#define _USE_MATH_DEFINES
#include "math.h"

namespace iMS
{
	class VCO::Impl
	{
	public:
		Impl(std::shared_ptr<IMSSystem>);
		~Impl();
		std::weak_ptr<IMSSystem> m_ims;
	};

	VCO::Impl::Impl(std::shared_ptr<IMSSystem> iMS) :
		m_ims(iMS)
	{
		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("VCO::VCO()");
	}

	VCO::Impl::~Impl()
	{
		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("VCO::~VCO()");
	}

	VCO::VCO(std::shared_ptr<IMSSystem> ims) : p_Impl(new Impl(ims))
	{
	}

	VCO::~VCO()
	{
		delete p_Impl;
		p_Impl = nullptr;
	}

	bool VCO::ConfigureCICFilter(bool enable, unsigned int filterLength)
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {         
            // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();

            // Configure CIC Length
            filterLength = filterLength > 10 ? 10 : filterLength < 0 ? 0 : filterLength;
            auto iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_CICLength);

            iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(filterLength)); 
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;

            // Configure CIC Bypass
            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_CICBypass);

            iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(enable ? 0 : 1)); 
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;
            return true;
        }).value_or(false);
	}

	bool VCO::ConfigureIIRFilter(bool enable, double freqCutoff, unsigned int cascadeStages)
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {         
            // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();

            constexpr double Fs = 4062.5; // ADC sampling frequency in kHz
            const double omega = 2.0 * M_PI * freqCutoff / Fs;
            const double cos_omega = std::cos(omega);

            //double alpha_app = 1.0 - std::exp(-omega);  // approx formula
            const double alpha_cor =
                cos_omega - 1 +
                std::sqrt(cos_omega * cos_omega - 4 * cos_omega + 3); // exact formula 

            bool prescale_en = false;
            double alpha_scaled = alpha_cor * (1 << 16);

            if (alpha_scaled <= 255) {
                alpha_scaled *= 256.0;
                prescale_en = true;
            }

            auto alpha_int = static_cast<std::uint32_t>(alpha_scaled);
            alpha_int = std::clamp(alpha_int, 1u, 0xFFFFu);

            // Configure IIR Cutoff Frequency
            auto iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IIRAlpha);

            iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(alpha_int)); 
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;
            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IIRAlphaPre);

            iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(prescale_en?1:0)); 
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;

            // Configure CIC Bypass
            cascadeStages = (!enable)
                ? 0
                : std::clamp(cascadeStages, 0u, 8u);

            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IIRCount);

            iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(cascadeStages)); 
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;
            return true;
        }).value_or(false);
	}
    
    bool VCO::SetFrequencyRange(MHz& lowerFreq, MHz& upperFreq, RFChannel ch)
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {   
             // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();
            
            auto lf = ims->Synth().GetCap().lowerFrequency;
            auto uf = ims->Synth().GetCap().upperFrequency;
            double range = uf - lf;

            if ((lowerFreq < lf) || (upperFreq > uf)) return false;

            double scaling = (upperFreq - lowerFreq)/range;
            double shift = (lowerFreq - lf)/range;

            HostReport* iorpt;
            if ((ch == 1) || ch.IsAll()) {
                iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Ch1FScale);
                iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(scaling * 0xFFFF)); 
                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }
                delete iorpt;
                iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Ch1FShift);
                iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(shift * 0xFFFF)); 
                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }
                delete iorpt;
            } 
            if ((ch == 2) || ch.IsAll()) {
                iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Ch2FScale);
                iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(scaling * 0xFFFF)); 
                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }
                delete iorpt;
                iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Ch2FShift);
                iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(shift * 0xFFFF)); 
                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }
                delete iorpt;
            }
            return true;
        }).value_or(false);
	}
 
    bool VCO::SetAmplitudeRange(Percent& lowerAmpl, Percent& upperAmpl, RFChannel ch)
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {   
             // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();
            
            constexpr double range = 100.0;

            if ((lowerAmpl < 0.0) || (upperAmpl > 100.0)) return false;

            double scaling = (upperAmpl - lowerAmpl)/range;
            double shift = lowerAmpl/range;

            HostReport* iorpt;
            if ((ch == 1) || ch.IsAll()) {
                iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Ch1AScale);
                iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(scaling * 0xFFFF)); 
                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }
                delete iorpt;
                iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Ch1AShift);
                iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(shift * 0xFFFF)); 
                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }
                delete iorpt;
            } 
            if ((ch == 2) || ch.IsAll()) {
                iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Ch2AScale);
                iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(scaling * 0xFFFF)); 
                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }
                delete iorpt;
                iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Ch2AShift);
                iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(shift * 0xFFFF)); 
                if (NullMessage == conn->SendMsg(*iorpt))
                {
                    delete iorpt;
                    return false;
                }
                delete iorpt;
            }
            return true;
        }).value_or(false);
	}
 
    bool VCO::ApplyDigitalGain(VCOGain gain)
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {   
             // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();
            
            HostReport* iorpt;
            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_DigGain);
            iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(gain)); 
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;
            return true;
        }).value_or(false);
	}

    bool VCO::Route(VCOOutput output, VCOInput input)
    {
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {   
             // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();
            
            HostReport* iorpt;
            std::uint16_t mask = 0; 
            int shift = 0;
            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
            switch(output)
            {
                case VCOOutput::CH1_FREQUENCY: mask = 0x1; shift = 0; break;
                case VCOOutput::CH1_AMPLITUDE: mask = 0x4; shift = 2; break;
                case VCOOutput::CH2_FREQUENCY: mask = 0x10; shift = 4; break;
                case VCOOutput::CH2_AMPLITUDE: mask = 0x40; shift = 6; break;
                default: return false;
            }
            iorpt->Payload<std::uint16_t>(mask); 
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;

            std::uint16_t data;
            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_VCOMux);
            switch(input)
            {
                case VCOInput::A: data = 0; break;
                case VCOInput::B: data = (1 << shift); break;
                default: return false;
            }
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

    bool VCO::ControlFunction(VCOOutput output, VCOFunction func)
    {
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {   
             // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();
            
            // Set Constant bit
            HostReport* iorpt;
            std::uint16_t mask = 0; 
            int shift = 0;
            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
            switch(output)
            {
                case VCOOutput::CH1_FREQUENCY: mask = 0x2; shift = 0; break;
                case VCOOutput::CH1_AMPLITUDE: mask = 0x8; shift = 2; break;
                case VCOOutput::CH2_FREQUENCY: mask = 0x20; shift = 4; break;
                case VCOOutput::CH2_AMPLITUDE: mask = 0x80; shift = 6; break;
                default: return false;
            }
            iorpt->Payload<std::uint16_t>(mask); 
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;

            std::uint16_t data;
            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_VCOMux);
            switch(func)
            {
                case VCOFunction::CONSTANT: data = (2 << shift); break;
                default: data = 0;
            }
            iorpt->Payload<std::uint16_t>(data); 
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;

            // Set tracking and mute bits
            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
            switch(output)
            {
                case VCOOutput::CH1_FREQUENCY: mask = (func == VCOFunction::CONSTANT) || (func == VCOFunction::MUTE) ? 0x100 : 0x103; break;
                case VCOOutput::CH1_AMPLITUDE: mask = (func == VCOFunction::CONSTANT) || (func == VCOFunction::MUTE) ? 0x100 : 0x10C; break;
                case VCOOutput::CH2_FREQUENCY: mask = (func == VCOFunction::CONSTANT) || (func == VCOFunction::MUTE) ? 0x400 : 0x430; break;
                case VCOOutput::CH2_AMPLITUDE: mask = (func == VCOFunction::CONSTANT) || (func == VCOFunction::MUTE) ? 0x400 : 0x4C0; break;
                default: return false;
            }
            iorpt->Payload<std::uint16_t>(mask); 
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;

            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_VCOTrack);
            switch(func)
            {
                case VCOFunction::TRACK: data = (1 << shift); break;
                case VCOFunction::HOLD: data = 0; break;
                case VCOFunction::CONDITIONAL: data = (2 << shift); break;
                case VCOFunction::CONSTANT: data = 0; break;
                case VCOFunction::MUTE: data = mask; break;
                default: return false;
            }
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

    bool VCO::ExternalRFMute(bool enable, RFChannel ch)
    {
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {   
             // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();
            
            // Set Constant bit
            HostReport* iorpt;
            std::uint16_t mask = 0; 

            // Set tracking and mute bits
            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
            if (ch.IsAll()) {
                mask = 0xA00;
            }
            else if (ch == 1) {
                mask = 0x200;
            }
            else if (ch == 2) {
                mask = 0x800;
            }
            else {return false;}
            iorpt->Payload<std::uint16_t>(mask); 
            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;

            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_VCOTrack);
            std::uint16_t data = enable ? mask : 0;
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

    bool VCO::SetConstantFrequency(MHz freq, RFChannel ch)
    {
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {   
             // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();

            unsigned int freq_word = FrequencyRenderer::RenderAsImagePoint(ims, freq);

            std::vector<std::uint16_t> freq_data;
    		freq_data.push_back(static_cast<std::uint16_t>(freq_word & 0xFFFF));
		    freq_data.push_back(static_cast<std::uint16_t>((freq_word >> 16) & 0xFFFF));

            HostReport* iorpt;
            if ((ch == 1) || ch.IsAll()) {
                iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Ch1FConstHi);
                iorpt->Payload<std::vector<std::uint16_t>>(freq_data);

				ReportFields f = iorpt->Fields();
				f.len = static_cast<std::uint16_t>(freq_data.size() * sizeof(std::uint16_t));
				iorpt->Fields(f);

				if (NullMessage == conn->SendMsg(*iorpt))
				{
					delete iorpt;
					return false;
				}
				delete iorpt;

                this->ControlFunction(VCOOutput::CH1_FREQUENCY, VCOFunction::CONSTANT);
            }            
            if ((ch == 2) || ch.IsAll()) {
                iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Ch2FConstHi);
                iorpt->Payload<std::vector<std::uint16_t>>(freq_data);

				ReportFields f = iorpt->Fields();
				f.len = static_cast<std::uint16_t>(freq_data.size() * sizeof(std::uint16_t));
				iorpt->Fields(f);

				if (NullMessage == conn->SendMsg(*iorpt))
				{
					delete iorpt;
					return false;
				}
				delete iorpt;

                this->ControlFunction(VCOOutput::CH2_FREQUENCY, VCOFunction::CONSTANT);
            }
            return true;         
        }).value_or(false);                
    }

    bool VCO::SetConstantAmplitude(Percent ampl, RFChannel ch)
    {
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {   
             // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();

            unsigned int ampl_word = AmplitudeRenderer::RenderAsImagePoint(ims, ampl);

            HostReport* iorpt;
            if ((ch == 1) || ch.IsAll()) {
                iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Ch1AConst);
                iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(ampl_word));

				if (NullMessage == conn->SendMsg(*iorpt))
				{
					delete iorpt;
					return false;
				}
				delete iorpt;

                this->ControlFunction(VCOOutput::CH1_AMPLITUDE, VCOFunction::CONSTANT);
            }            
            if ((ch == 2) || ch.IsAll()) {
                iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Ch2AConst);
                iorpt->Payload<std::uint16_t>(static_cast<std::uint16_t>(ampl_word));

				if (NullMessage == conn->SendMsg(*iorpt))
				{
					delete iorpt;
					return false;
				}
				delete iorpt;

                this->ControlFunction(VCOOutput::CH2_AMPLITUDE, VCOFunction::CONSTANT);
            }
            return true;
        }).value_or(false);   
    }

    bool VCO::SaveStartupState()
    {
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {   
             // Make sure Synthesiser is present
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();

            HostReport* iorpt;
            iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_VCOSaveStartup);
            iorpt->Payload<std::uint16_t>(1);

            if (NullMessage == conn->SendMsg(*iorpt))
            {
                delete iorpt;
                return false;
            }
            delete iorpt;
            return true;
        }).value_or(false);         
    }

}