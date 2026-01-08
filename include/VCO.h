/*-----------------------------------------------------------------------------
/ Title      : Voltage Controlled Oscillator Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/SignalPath/h/VCO.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2025-12-19
/ Last update: $Date: 2020-07-30 21:50:24 +0100 (Thu, 30 Jul 2020) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 465 $
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

///
/// \file VCO.h
///
/// \brief Class for controlling Voltage Controlled Synthesisers
///
/// \author Dave Cowan
/// \date 2025-12-19
/// \since 2.0.5
/// \ingroup group_SigPath
///

#ifndef IMS_VCO_H__
#define IMS_VCO_H__

#include "IMSSystem.h"
#include "IEventHandler.h"
#include "IMSTypeDefs.h"

#include <map>

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
	/// \class VCOEvents VCO.h include\VCO.h
	/// \brief All the different types of events that can be triggered by the VCO class.
	///
	/// Some events contain floating point parameter data which can be processed by the IEventHandler::EventAction
	/// derived method
	/// \author Dave Cowan
	/// \date 2026-01-07
	/// \since 2.0.5
	class LIBSPEC VCOEvents
	{
	public:
		/// \enum Events List of Events raised by the VCO module
		enum Events {
			/// Indicates to the application that an update of diagnostics data is available to be read
			VCO_UPDATE_AVAILABLE,
			/// Indicates that the update that was requested has failed to respond with updated results
			VCO_READ_FAILED,
			Count
		};
	};

	///
	/// \class VCO VCO.h include\VCO.h
	/// \brief Configures Voltage Controlled Synthesisers
	///
	/// \author Dave Cowan
	/// \date 2025-12-19
	/// \since 2.0.4
	class LIBSPEC VCO
	{
	public:
        enum class VCOInput
        {
            A,
            B,
            Both
        };

        enum class VCOOutput
        {
            CH1_FREQUENCY,
            CH1_AMPLITUDE,
            CH2_FREQUENCY,
            CH2_AMPLITUDE
        };

        enum class VCOGain
        {
            X1 = 0,
            X2 = 1,
            X4 = 2,
            X8 = 3
        };

        enum class VCOTracking
        {
            TRACK,
            HOLD,
            PIN_CONTROLLED,
            CONSTANT
        };

        enum class VCOMute
        {
            UNMUTE,
            MUTE,
            PIN_CONTROLLED
        };

		/// \enum MEASURE
		/// \brief Selects which VCO Input measurement to access
		/// \since 2.0.5
		enum class MEASURE
		{
			/// Voltage at Device Input Ch A
			ANLG_INPUT_A_VOLTS,
			/// Voltage at Device Input Ch B
			ANLG_INPUT_B_VOLTS,
			/// Processed Value (Filtered + Digital Gain) Ch A
			ANLG_INPUT_A_PROCESSED,
			/// Processed Value (Filtered + Digital Gain) Ch B
			ANLG_INPUT_B_PROCESSED,
            Count,
		};
        
        ///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for VCO Object
		///
		/// An IMSSystem object, representing the configuration of an iMS target must be passed by const
		/// reference to the VCO constructor.
		///
		/// The IMSSystem object must exist before the VCO object, and must remain valid (not
		/// destroyed) until the VCO object itself is destroyed.
		///
		/// Once constructed, the object can neither be copied or assigned to another instance.
		///
		/// \param[in] ims A const reference to the iMS System
		/// \param[in] chan to indicate which of the synthesised RF Channels should have wave shaping applied (or all)
		/// \since 2.0.4
		VCO(std::shared_ptr<IMSSystem> ims);
		///
		/// \brief Destructor for VCO Object
		~VCO();
		//@}

	bool ConfigureCICFilter(bool enable, unsigned int filterLength = 6);
	bool ConfigureIIRFilter(bool enable, double freqCutoff = 10.0, unsigned int cascadeStages = 3);

    bool SetFrequencyRange(MHz& lowerFreq, MHz& upperFreq, RFChannel ch = RFChannel::all);
    bool SetAmplitudeRange(Percent& lowerAmpl, Percent& upperAmpl, RFChannel ch = RFChannel::all);
    bool ApplyDigitalGain(VCOGain gain);

    bool Route(VCOOutput output, VCOInput input);
    bool TrackingMode(VCOOutput output, VCOTracking func);
    bool RFMute(VCOMute = VCOMute::MUTE, RFChannel ch = RFChannel::all);

    bool SetConstantFrequency(MHz freq, RFChannel ch = RFChannel::all);
    bool SetConstantAmplitude(Percent ampl, RFChannel ch = RFChannel::all);

    bool SaveStartupState();

    bool ReadVoltageInput();
    const std::map<MEASURE, Percent>& GetVoltageInputData() const;
    std::map<std::string, Percent> GetVoltageInputDataStr() const;    

	void VCOEventSubscribe(const int message, IEventHandler* handler);
	void VCOEventUnsubscribe(const int message, const IEventHandler* handler);

	private:
		// Makes this object non-copyable
		VCO(const VCO&);
		const VCO& operator =(const VCO&);

		class Impl;
		Impl* p_Impl;

	};


}

#endif

