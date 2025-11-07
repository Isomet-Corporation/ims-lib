/*-----------------------------------------------------------------------------
/ Title      : RF Wave Shaping Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/SignalPath/h/WaveShaping.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2020-11-02
/ Last update: $Date: 2020-07-30 21:50:24 +0100 (Thu, 30 Jul 2020) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 465 $
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

///
/// \file WaveShaping.h
///
/// \brief Class for shaping and modulating the RF waveform after synthesis
///
/// Useful when very short bursts of RF are desired that can't be created through
/// image modes alone, e.g. RF pulse picking
///
/// \author Dave Cowan
/// \date 2020-11-02
/// \since 1.8.2
/// \ingroup group_SigPath
///

#ifndef IMS_WAVESHAPING_H__
#define IMS_WAVESHAPING_H__

#include "IMSSystem.h"
#include "IMSTypeDefs.h"

/// \cond LIB_CREATION
#if defined _WIN32 || defined __CYGWIN__
#ifdef __GNUC__
#define DLL_EXPORT __attribute__ ((dllexport))
#define DLL_IMPORT __attribute__ ((dllimport))
#else
#define DLL_EXPORT __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#define DLL_IMPORT __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#endif
#define DLL_LOCAL
#else
#if __GNUC__ >= 4
#define DLL_EXPORT __attribute__ ((visibility ("default")))
#define DLL_IMPORT __attribute__ ((visibility ("default")))
#define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#else
#define DLL_EXPORT
#define DLL_IMPORT
#define DLL_LOCAL
#endif
#endif

#if defined(_EXPORTING_IMS)
#define LIBSPEC DLL_EXPORT
#define LIBLOCAL DLL_LOCAL
#define EXPIMP_TEMPLATE
#elif defined(_STATIC_IMS)
#define LIBSPEC
#define LIBLOCAL
#define EXPIMP_TEMPLATE
#else
#define LIBSPEC DLL_IMPORT
#define LIBLOCAL DLL_LOCAL
#define EXPIMP_TEMPLATE extern
#endif
/// \endcond

namespace iMS
{
	///
	/// \class WaveShaping WaveShaping.h include\WaveShaping.h
	/// \brief Controls Signal Shaping and Modulation after Synthesis
	///
        /// Some iMS system synthesisers support a technology that permits complex modulation
        /// of the RF waveform after the base RF signal has been synthesised.  Such modulation
        /// can be used, for example, to deliver short gated bursts of a few cycles of RF to the
        /// connected AO device at programmable intervals or on demand.
        ///
        /// The technology is useful for pulse picking type applications.
        ///
        /// Not all synthesiser models support wave shaping.  Some that do are:
        ///
        /// * iDDS-3
        ///
        /// Contact Isomet to find out which models would be suitable for your application requirements.
        ///
	/// \author Dave Cowan
	/// \date 2020-11-02
	/// \since 1.8.2
	class LIBSPEC WaveShaping
	{
	public:
	  /// \enum StartingEdge Which edge the RF gate opens on
		enum class StartingEdge {
		  /// \brief Opens when RF crosses from positive to negative
			FALLING,
		  /// \brief Opens when RF crosses from negative to positive
			RISING
		};

		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for WaveShaping Object
		///
		/// An IMSSystem object, representing the configuration of an iMS target must be passed by const
		/// reference to the WaveShaping constructor.
		///
		/// The IMSSystem object must exist before the WaveShaping object, and must remain valid (not
		/// destroyed) until the WaveShaping object itself is destroyed.
		///
		/// Once constructed, the object can neither be copied or assigned to another instance.
		///
		/// \param[in] ims A const reference to the iMS System
		/// \param[in] chan to indicate which of the synthesised RF Channels should have wave shaping applied (or all)
		/// \since 1.8.2
		WaveShaping(std::shared_ptr<IMSSystem> ims, RFChannel chan);
		///
		/// \brief Destructor for WaveShaping Object
		~WaveShaping();
		//@}

		///
		/// \name Modulation Styles
		///@{
		///
		/// \brief Pulse Gate on fixed free running interval
		///
		/// A pulse gate has two states: either RF is passed through the gate, or it is muted/turned off.
		/// The free running pulse gate is set to open the gate at a fixed periodic interval and remains
		/// open for a given number of RF half cycles.  The gate always opens with the RF source at zero
		/// phase.
		///
		/// \param[in] gate_interval the duration in microseconds between each instance of the gate opening
		/// \param[in] gate_width the duration which the gate remains open, in RF half cycles
		/// \param[in] edge whether the gate opens on a rising or falling RF edge
		bool FreeRunningPulseGating(unsigned int gate_interval, unsigned int gate_width, StartingEdge edge);
		//@}

		///@{
		///
		/// \brief Pulse Gate activated by externally applied trigger signal
		///
		/// A pulse gate has two states: either RF is passed through the gate, or it is muted/turned off.
		/// The 'on trigger' pulse gate is set to open the gate at the rising edge of an external signal and remains
		/// open for a given number of RF half cycles.  The gate always opens with the RF source at zero
		/// phase.
		///
		/// \param[in] gate_width the duration which the gate remains open, in RF half cycles
		/// \param[in] edge whether the gate opens on a rising or falling RF edge
		bool OnTriggerPulseGating(unsigned int gate_width, StartingEdge edge);
		//@}

	private:
		// Makes this object non-copyable
		WaveShaping(const WaveShaping&);
		const WaveShaping& operator =(const WaveShaping&);

		class Impl;
		Impl* p_Impl;

	};


}

#endif

