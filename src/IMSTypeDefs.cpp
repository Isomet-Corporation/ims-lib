/*-----------------------------------------------------------------------------
/ Title      : iMS Useful Types Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/src/IMSTypeDefs.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2024-12-19 18:57:04 +0000 (Thu, 19 Dec 2024) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 650 $
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

#include "IMSTypeDefs.h"
#include "IMSSystem.h"
#include "IMSTypeDefs_p.h"
#include "PrivateUtil.h"

#include <algorithm>

namespace iMS {

	Frequency::Frequency(double arg) : value(arg) {}

	Frequency& Frequency::operator = (double arg) {
		value = arg;
		return *this;
	}

	Frequency::operator double() const { return value; }

	kHz::kHz(double arg) : Frequency(arg * 1000.0) {}

	kHz& kHz::operator = (double arg) {
		value = 1000.0 * arg;
		return *this;
	}

	kHz::operator double() const { return (value / 1000.0); }

	MHz::MHz(double arg) : Frequency(arg * 1000000.0) {}

	MHz& MHz::operator = (double arg) {
		value = 1000000.0 * arg;
		return *this;
	}

	MHz::operator double() const { return (value / 1000000.0); }

	Percent::Percent(double arg) : value(arg) {
		if (arg < 0.0) value = 0.0;
		if (arg > 100.0) value = 100.0;
	}

	Percent& Percent::operator = (double arg) {
		if (arg < 0.0) value = 0.0;
		else if (arg > 100.0) value = 100.0;
		else value = arg;
		return *this;
	}

	Percent::operator double() const { return value; }

	Degrees::Degrees(double arg) {
//		value = std::fmod(arg, 360.0);
//		value = arg - (std::floor(arg / 360.0) * 360.0);
		value = arg;
	}

	Degrees& Degrees::operator = (double arg) {
//		value = std::fmod(arg, 360.0);
		value = arg;
		return *this;
	}

	Degrees::operator double() const {
		return value;
	}

	unsigned int FrequencyRenderer::RenderAsPointRate(const IMSSystem& system, const Frequency freq, const bool PrescalerDisable)
	{
		double d = freq, cmax = system.Ctlr().GetCap().MaxImageRate, smax = system.Synth().GetCap().MaxImageRate;
		d = 100000000.0 / std::min(d, std::min(cmax, smax));
		if (!system.Ctlr().GetCap().FastImageTransfer || !PrescalerDisable) {
			d /= 100.0;
		}

		return (static_cast<unsigned int>(d));
	}

	unsigned int FrequencyRenderer::RenderAsImagePoint(const IMSSystem& system, const MHz freq)
	{
		double d = freq;
		d = std::max(d, static_cast<double>(system.Synth().GetCap().lowerFrequency));
		d = std::min(d, static_cast<double>(system.Synth().GetCap().upperFrequency));

		double int_repr = std::floor(std::pow(2.0, (system.Synth().GetCap().freqBits)) - 0.5);
		d = (d - static_cast<double>(system.Synth().GetCap().lowerFrequency)) / (static_cast<double>(system.Synth().GetCap().upperFrequency) - static_cast<double>(system.Synth().GetCap().lowerFrequency));
		d = d * int_repr;

		return (static_cast<unsigned int>(d)& ((1ULL << system.Synth().GetCap().freqBits) - 1));
	}

	unsigned int FrequencyRenderer::RenderAsStaticOffset(const IMSSystem& system, const MHz freq, int add_sub)
	{
		double d = freq;
		double range = static_cast<double>(system.Synth().GetCap().upperFrequency) - static_cast<double>(system.Synth().GetCap().lowerFrequency);
		d = std::max(d, 0.0);
		d = std::min(d, 0.5*range);

		double int_repr = std::floor(std::pow(2.0, (system.Synth().GetCap().freqBits)) - 0.5);
		d /= range;
		d = d * int_repr;
		if (add_sub) d *= -1.0;

		return static_cast<unsigned int>(static_cast<int>(d)& ((1ULL << system.Synth().GetCap().freqBits) - 1));
	}

	unsigned int FrequencyRenderer::RenderAsDDSValue(const IMSSystem& system, const MHz freq)
	{
		const int DDSFreqBits = 32;

		double d = freq, fmax = static_cast<double>(system.Synth().GetCap().sysClock);
		d = std::min(d, fmax/2.0);

		double int_repr = std::floor(std::pow(2.0, DDSFreqBits) - 0.5);
		d /= static_cast<double>(system.Synth().GetCap().sysClock);
		d = d * int_repr;

//		return (static_cast<unsigned int>(d)& ((1 << DDSFreqBits) - 1));
		return (static_cast<unsigned int>(d));
	}

	unsigned int AmplitudeRenderer::RenderAsImagePoint(const IMSSystem& system, const Percent pct)
	{
		double d = pct;

		double int_repr = std::floor(std::pow(2.0, (system.Synth().GetCap().amplBits)) - 0.5);
		d = (d / 100.0) * int_repr;

		return (static_cast<unsigned int>(d)& ((1ULL << system.Synth().GetCap().amplBits) - 1));
	}

	unsigned int AmplitudeRenderer::RenderAsCompensationPoint(const IMSSystem& system, const Percent ampl)
	{
		double d = ampl;

		double int_repr = std::floor(std::pow(2.0, (system.Synth().GetCap().LUTAmplBits)) - 0.5);
		d = (d / 100.0) * int_repr;

		return (static_cast<unsigned int>(d)& ((1ULL << system.Synth().GetCap().LUTAmplBits) - 1));
	}

	unsigned int AmplitudeRenderer::RenderAsCalibrationTone(const IMSSystem& system, const Percent ampl)
	{
		double d = ampl;

		double int_repr = std::floor(std::pow(2.0, (CalibAmplBits)) - 0.5);
		d = (d / 100.0) * int_repr;

		return (static_cast<unsigned int>(d)& ((1ULL << CalibAmplBits) - 1));
	}

	unsigned int AmplitudeRenderer::RenderAsChirp(const IMSSystem& system, const Percent ampl)
	{
		unsigned int data = AmplitudeRenderer::RenderAsCalibrationTone(system, ampl);
		return (data << (32 - CalibAmplBits));
	}

	double ModulusPhase(double value) {
		double wrap = std::fmod(value, 360.0);
		return (wrap >= 0.0) ? wrap : 360.0 + wrap;
	}

	unsigned int PhaseRenderer::RenderAsImagePoint(const IMSSystem& system, const Degrees deg)
	{
		double d = ModulusPhase(deg);

		double int_repr = std::floor(std::pow(2.0, (system.Synth().GetCap().phaseBits)) - 0.5);
		d = (d / 360.0) * int_repr;

		return (static_cast<unsigned int>(d)& ((1ULL << system.Synth().GetCap().phaseBits) - 1));
	}

	unsigned int PhaseRenderer::RenderAsCompensationPoint(const IMSSystem& system, const Degrees deg)
	{
		double d = ModulusPhase(deg);

		double int_repr = std::floor(std::pow(2.0, (system.Synth().GetCap().LUTPhaseBits)) - 0.5);
		d = (d / 360.0) * int_repr;

		return (static_cast<unsigned int>(d) & ((1ULL << system.Synth().GetCap().LUTPhaseBits)-1));
	}

	unsigned int PhaseRenderer::RenderAsCalibrationTone(const IMSSystem& system, const Degrees deg)
	{
		double d = ModulusPhase(deg);

		double int_repr = std::floor(std::pow(2.0, (CalibPhaseBits)) - 0.5);
		d = (d / 360.0) * int_repr;

		return (static_cast<unsigned int>(d)& ((1ULL << CalibPhaseBits) - 1));
	}

	unsigned int PhaseRenderer::RenderAsChirp(const IMSSystem& system, const Degrees deg)
	{
		unsigned int data = PhaseRenderer::RenderAsCalibrationTone(system, deg);
		return (data << (32 - CalibPhaseBits));
	}

	FAP::FAP() : freq(0.0), ampl(0.0), phase(0.0) {}

	FAP::FAP(double f, double a, double p) : freq(f), ampl(a), phase(p) {}

	FAP::FAP(MHz f, Percent a, Degrees p) : freq(f), ampl(a), phase(p) {}

	bool FAP::operator==(const FAP &other) const
	{
		return (float_compare(freq, other.freq) &&
			float_compare(ampl, other.ampl) &&
			float_compare(phase, other.phase));
	}

	bool FAP::operator!=(const FAP &other) const
	{
		return (!(*this == other));
	}

	class SweepTone::Impl 
	{
	public:
		Impl() 
			: mode(ENHANCED_TONE_MODE::NO_SWEEP), scaling(DAC_CURRENT_REFERENCE::FULL_SCALE)
		{
		}

		Impl(FAP start, FAP end, std::chrono::duration<double>& up, std::chrono::duration<double>& down, int steps, ENHANCED_TONE_MODE mode, DAC_CURRENT_REFERENCE scaling)
			: start(start), end(end), up_ramp(up), down_ramp(down), n_steps(steps), mode(mode), scaling(scaling)
		{
		}

		FAP start;
		FAP end;
		std::chrono::duration<double> up_ramp;
		std::chrono::duration<double> down_ramp;
		int n_steps;
		ENHANCED_TONE_MODE mode;
		DAC_CURRENT_REFERENCE scaling;
	};

	SweepTone::SweepTone()
		: p_Impl(new Impl())
	{
	}

	SweepTone::SweepTone(FAP tone)
		: SweepTone() {
		p_Impl->start = tone;
	}

	FAP& SweepTone::start() { return p_Impl->start; }
	const FAP& SweepTone::start() const { return p_Impl->start; }

	FAP& SweepTone::end() { return p_Impl->end; }
	const FAP& SweepTone::end() const { return p_Impl->end; }

	std::chrono::duration<double>& SweepTone::up_ramp() { return p_Impl->up_ramp; }
	const std::chrono::duration<double>& SweepTone::up_ramp() const { return p_Impl->up_ramp; }

	std::chrono::duration<double>& SweepTone::down_ramp() { return p_Impl->down_ramp; }
	const std::chrono::duration<double>& SweepTone::down_ramp() const { return p_Impl->down_ramp; }

	int& SweepTone::n_steps() { return p_Impl->n_steps; }
	const int& SweepTone::n_steps() const { return p_Impl->n_steps; }

	ENHANCED_TONE_MODE& SweepTone::mode() { return p_Impl->mode; }
	const ENHANCED_TONE_MODE& SweepTone::mode() const { return p_Impl->mode; }

	DAC_CURRENT_REFERENCE& SweepTone::scaling() { return p_Impl->scaling; }
	const DAC_CURRENT_REFERENCE& SweepTone::scaling() const { return p_Impl->scaling; }

	SweepTone::SweepTone(FAP start, FAP end, std::chrono::duration<double>& up, std::chrono::duration<double>& down, int steps, ENHANCED_TONE_MODE mode, DAC_CURRENT_REFERENCE scaling)
		: p_Impl(new Impl(start, end, up, down, steps, mode, scaling)) {}

	SweepTone::SweepTone(const SweepTone &rhs) : p_Impl(new Impl())
	{
		p_Impl->start = rhs.p_Impl->start;
		p_Impl->end = rhs.p_Impl->end;
		p_Impl->up_ramp = rhs.p_Impl->up_ramp;
		p_Impl->down_ramp = rhs.p_Impl->down_ramp;
		p_Impl->n_steps = rhs.p_Impl->n_steps;
		p_Impl->mode = rhs.p_Impl->mode;
		p_Impl->scaling = rhs.p_Impl->scaling;
	}

	SweepTone &SweepTone::operator =(const SweepTone &rhs)
	{
		if (this == &rhs) return *this;
		p_Impl->start = rhs.p_Impl->start;
		p_Impl->end = rhs.p_Impl->end;
		p_Impl->up_ramp = rhs.p_Impl->up_ramp;
		p_Impl->down_ramp = rhs.p_Impl->down_ramp;
		p_Impl->n_steps = rhs.p_Impl->n_steps;
		p_Impl->mode = rhs.p_Impl->mode;
		p_Impl->scaling = rhs.p_Impl->scaling;
		return *this;
	}

	RFChannel::RFChannel() : value(RFChannel::all) {}

	RFChannel::RFChannel(int arg) : value(arg) {
		if ((arg < min || arg > max) && (arg != all)) {
			value = RFChannel::min;
			throw std::invalid_argument("Invalid RF Channel Number");
		}
	}

	RFChannel& RFChannel::operator = (int arg) {
		if ((arg < min || arg > max) && (arg != all)) {
			value = RFChannel::min;
			throw std::invalid_argument("Invalid RF Channel Number");
		}
		else value = arg;
		return *this;
	}

	RFChannel& RFChannel::operator++() {
		if (value<RFChannel::max) value++;
		return *this;
	}

	RFChannel RFChannel::operator++(int) {
		RFChannel temp = *this;
		++(*this);
		return temp;
	}

	RFChannel& RFChannel::operator--() {
		if ((value>RFChannel::min) && (value<=RFChannel::max)) value--;
		return *this;
	}

	RFChannel RFChannel::operator--(int) {
		RFChannel temp = *this;
		--(*this);
		return temp;
	}

	bool RFChannel::IsAll() const {
		return (value == all);
	}

	RFChannel::operator int() const { 
		if (value < RFChannel::min || value > RFChannel::max)
			return min;
		else
			return value;
	}

  //	const int RFChannel::min = 1;
  //	const int RFChannel::max = 4;
	const int RFChannel::all = 0x246A;  // 'magic' value to indicate all channels

}
