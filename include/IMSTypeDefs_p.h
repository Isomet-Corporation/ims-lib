/*-----------------------------------------------------------------------------
/ Title      : iMS Useful Type Defines Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg.qytek.lan/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/h/IMSTypeDefs.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2017-09-11 23:55:34 +0100 (Mon, 11 Sep 2017) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 300 $
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

///
/// \file IMSTypeDefs_p.h
///
/// \brief Useful Type Definitions for working with iMS Systems
///
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_TypeDefs
///

#ifndef IMS_IMSTYPEDEFS_P_H__
#define IMS_IMSTYPEDEFS_P_H__

#include "IMSTypeDefs.h"

#include <cmath>
#include <stdexcept>
#include <cstdint>
#include <vector>

namespace iMS
{
	class FrequencyRenderer final
	{
	public:
		/// \brief Used internally by the library to convert a Frequency object into an hardware-dependent
		/// integer representation used by the Image for Internal Oscillator frequency
		///
		/// Not intended for use in application code
		static unsigned int RenderAsPointRate(const IMSSystem&, const Frequency, const bool PrescalerDisable = false);
		/// \brief Used internally by the library to convert a Frequency object into a hardware-dependent
		/// integer representation used by the Image for RF Output frequency
		///
		/// Not intended for use in application code
		static unsigned int RenderAsImagePoint(const IMSSystem&, const MHz);
		static unsigned int RenderAsStaticOffset(const IMSSystem& system, const MHz freq, int add_sub);
		static unsigned int RenderAsDDSValue(const IMSSystem&, const MHz);
	private:
		FrequencyRenderer() {}
	};


	class AmplitudeRenderer final
	{
	public:
		/// \brief Used internally by the library to convert a Percent object into a hardware-dependent
		/// integer representation used by the Image for RF Output amplitude
		///
		/// Not intended for use in application code
		static unsigned int RenderAsImagePoint(const IMSSystem&, const Percent);

		/// \brief Used internally by the library to convert a Percent object into a hardware-dependent
		/// integer representation used by the Compensation Table for Compensation amplitude
		///
		/// Not intended for use in application code
		static unsigned int RenderAsCompensationPoint(const IMSSystem&, const Percent);

		/// \brief Used internally by the library to convert a Percent object into a hardware-dependent
		/// integer representation used by the Calibration Tone for Single Tone amplitude
		///
		/// Not intended for use in application code
		///
		/// \since 1.1.0
		static unsigned int RenderAsCalibrationTone(const IMSSystem&, const Percent);
		static unsigned int RenderAsChirp(const IMSSystem&, const Percent);
	private:
		AmplitudeRenderer() {}
		static const int CalibAmplBits = 10;
	};

 	class PhaseRenderer final
	{
	public:
		/// \brief Used internally by the library to convert a Degrees object into a hardware-dependent
		/// integer representation used by the Image for RF Output phase
		///
		/// Not intended for use in application code
		static unsigned int RenderAsImagePoint(const IMSSystem&, const Degrees);

		/// \brief Used internally by the library to convert a Degrees object into a hardware-dependent
		/// integer representation used by the Compensation Table for channel phase increment
		///
		/// Not intended for use in application code
		static unsigned int RenderAsCompensationPoint(const IMSSystem&, const Degrees);

		/// \brief Used internally by the library to convert a Degrees object into a hardware-dependent
		/// integer representation used by the Calibration Tone for channel phase increment
		///
		/// Not intended for use in application code
		///
		/// \since 1.1.0
		static unsigned int RenderAsCalibrationTone(const IMSSystem&, const Degrees);
		static unsigned int RenderAsChirp(const IMSSystem&, const Degrees);
	private:
		static const int CalibPhaseBits = 14;

		PhaseRenderer() {}
	};

}

#endif
