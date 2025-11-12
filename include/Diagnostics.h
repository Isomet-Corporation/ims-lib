/*-----------------------------------------------------------------------------
/ Title      : Diagnostics Functions Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Diagnostics/h/Diagnostics.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2019-07-10 12:10:35 +0100 (Wed, 10 Jul 2019) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 418 $
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
/// \file Diagnostics.h
///
/// \brief Access diagnostic reporting information about the connected iMS System
///
/// The iMS provides a range of diagnostic reporting measures to ensure the continued health and
/// safe function of the Synthesiser, power amplifier and attached acousto-optic devices.
///
/// Diagnostics data includes:
/// \li A record of hours recorded while the device was powered up
/// \li The current temperature reading
/// \li Forward current passing through each channel of the amplifier
/// \li Forward power for each amplifier channel
/// \li Reflected power for each amplifier channel
///
/// Some of this data may have been stored on the device's non-volatile memory by the factory so 
/// the user application can compare against current readings and has a record of how the device
/// performance has changed over time.
///
/// \author Dave Cowan
/// \date 2016-03-08
/// \since 1.1
/// \ingroup group_Diag
///

#ifndef IMS_DIAGNOSTICS_H__
#define IMS_DIAGNOSTICS_H__

#include "IMSSystem.h"
#include "IEventHandler.h"

#include <memory>
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
	/// \class DiagnosticsEvents Diagnostics.h include\Diagnostics.h
	/// \brief All the different types of events that can be triggered by the Diagnostics class.
	///
	/// Some events contain floating point parameter data which can be processed by the IEventHandler::EventAction
	/// derived method
	/// \author Dave Cowan
	/// \date 2016-03-08
	/// \since 1.1
	class LIBSPEC DiagnosticsEvents
	{
	public:
		/// \enum Events List of Events raised by the Diagnostics module
		enum Events {
			/// Received a temperature update from the Acousto-Optic device
			AOD_TEMP_UPDATE,
			/// Received a temperature update from the RF Power Amplifier
			RFA_TEMP_UPDATE,
			/// Returns the number of hours logged by the Synthesiser while powered up
			SYN_LOGGED_HOURS,
			/// Returns the number of hours logged by the Acousto-Optic Device while powered up
			AOD_LOGGED_HOURS,
			/// Returns the number of hours logged by the RF Power Amplifier while powered up
			RFA_LOGGED_HOURS,
			/// Indicates to the application that an update of diagnostics data is available to be read
			DIAGNOSTICS_UPDATE_AVAILABLE,
			/// Indicates that the update that was requested has failed to respond with updated results
			DIAG_READ_FAILED,
			/// Indicates a failure to read diagnostics data from a specific channel
			DIAG_CHANNEL_ERROR,
			Count
		};
	};

	///
	/// \class Diagnostics Diagnostics.h include\Diagnostics.h
	/// \brief Provides a mechanism for retrieving diagnostics data about the attached iMS System
	///
	/// \author Dave Cowan
	/// \date 2016-03-08
	/// \since 1.1
	class LIBSPEC Diagnostics
	{
	public:
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for Diagnostics Object
		///
		/// An IMSSystem object, representing the configuration of an iMS target must be passed by const
		/// reference to the Diagnostics constructor.
		///
		/// The IMSSystem object must exist before the Diagnostics object, and must remain valid (not
		/// destroyed) until the Diagnostics object itself is destroyed.
		///
		/// Once constructed, the object can neither be copied or assigned to another instance.
		///
		/// \param[in] ims A const reference to the iMS System
		/// \since 1.1
		Diagnostics(std::shared_ptr<IMSSystem> ims);
		///
		/// \brief Destructor for Diagnostics Object
		~Diagnostics();
		//@}

		/// \enum TARGET
		/// \brief Sets which iMS device to request diagnostics data for
		/// \since 1.1
		enum class TARGET
		{
			/// Access the Synthesiser Diagnostics (Hours only)
			SYNTH,
			/// Access the AO Device Diagnostics
			AO_DEVICE,
			/// Access the RF Amplifier Diagnostics
			RF_AMPLIFIER
		};

		/// \enum MEASURE
		/// \brief Selects which diagnostics measurement to access
		/// \since 1.1
		enum class MEASURE
		{
			/// Forward Measured Power for Channel 1
			FORWARD_POWER_CH1,
			/// Forward Measured Power for Channel 2
			FORWARD_POWER_CH2,
			/// Forward Measured Power for Channel 3
			FORWARD_POWER_CH3,
			/// Forward Measured Power for Channel 4
			FORWARD_POWER_CH4,
			/// Reflected Measured Power for Channel 1
			REFLECTED_POWER_CH1,
			/// Reflected Measured Power for Channel 2
			REFLECTED_POWER_CH2,
			/// Reflected Measured Power for Channel 3
			REFLECTED_POWER_CH3,
			/// Reflected Measured Power for Channel 4
			REFLECTED_POWER_CH4,
			/// Measured DC Current for Channel 1
			DC_CURRENT_CH1,
			/// Measured DC Current for Channel 2
			DC_CURRENT_CH2,
			/// Measured DC Current for Channel 3
			DC_CURRENT_CH3,
			/// Measured DC Current for Channel 4
			DC_CURRENT_CH4
		};

		/// \name Event Notifications
		//@{
		///
		/// \brief Subscribe a callback function handler to a given DiagnosticsEvents event
		///
		/// Diagnostics can callback user application code when an event occurs that affects the signal path.
		/// Supported events are listed under DiagnosticsEvents.  The
		/// callback function must inherit from the IEventHandler interface and override
		/// its EventAction() method.
		///
		/// Use this member function call to subscribe a callback function to a DiagnosticsEvents event.
		/// For the period that a callback is subscribed, each time an event in Diagnostics occurs
		/// that would trigger the subscribed DiagnosticsEvents event, the user function callback will be
		/// executed.
		/// \param[in] message Use the DiagnosticsEvents::Event enum to specify an event to subscribe to
		/// \param[in] handler A function pointer to the user callback function to execute on the event trigger.
		/// \since 1.1
		void DiagnosticsEventSubscribe(const int message, IEventHandler* handler);
		/// \brief Unsubscribe a callback function handler from a given DiagnosticsEvents event
		///
		/// Removes all links to a user callback function from the Event Trigger map so that any
		/// events that occur in the Diagnostics object following the Unsubscribe request
		/// will no longer execute that function
		/// \param[in] message Use the DiagnosticsEvents::Event enum to specify an event to unsubscribe from
		/// \param[in] handler A function pointer to the user callback function that will no longer execute on an event
		/// \since 1.1
		void DiagnosticsEventUnsubscribe(const int message, const IEventHandler* handler);
		//@}

		/// \name Read Temperatures
		//@{
		/// \brief Triggers a temperature reading from the target device
		///
		/// Calling this function will cause the Synthesiser to initiate a temperature conversion on 
		/// either the RF Power Amplifier or the Acousto-Optic Device.  There is no sensor built into the
		/// Synthesiser itself.
		///
		/// The function returns as soon as the conversion has been initiated and the result will become available
		/// in the background, causing a DiagnosticsEvents TempUpdate event to fire so ensure that the user code
		/// has subscribed to the appropriate event first.
		/// \param[in] tgt Which of the connected devices to read temperature data from
		/// \return true if the temperature conversion was initiated successfully.
		/// \since 1.1
		bool GetTemperature(const TARGET& tgt) const;
		//@}

		/// \name Read Hours
		//@{
		/// \brief Triggers a logged hours reading from the target device
		///
		/// Calling this function will read back the current logged hours count from the timing circuit built into
		/// the Synthesiser, RF Power Amplifier or Acoust-Optic Device.
		///
		/// The function returns as soon as the request has been sent and a DiagnosticsEvents LoggedHours event will
		/// fire as soon as the result returns so ensure that the user code has subscribed to the appropriate event first.
		/// \param[in] tgt Which of the connected devices to read logged hours data from
		/// \return true if the logged hours request was sent successfully.
		/// \since 1.1
		bool GetLoggedHours(const TARGET& tgt) const;
		//@}

		/// \name Get Diagnostics Information
		//@{
		/// \brief Triggers a Diagnostics Conversion to read measurement data from the RF Power Amplifier
		///
		/// Calling this function will result in a new analog-to-digital conversion sequence being triggered in the diagnostics
		/// circuit built into the RF Power Amplifier.  This will result in updated values being made available for
		/// Forward power, Reflected Power and DC Current across all 4 RF signal channels.
		///
		/// The function returns as soon as the request has been sent and a DiagnosticsEvents UpdateAvailable event will
		/// fire as soon as the result returns so ensure that the user code has subscribed to the appropriate event first.
		/// If for any reason the conversion was not able to be completed, a ReadFailed event will instead be returned
		/// \return true if the update was initiated successfully.
		/// \since 1.1
		bool UpdateDiagnostics();
		/// \brief Returns a reference to the map of diagnostics data values currently stored by the Diagnostics class.
		///
		/// The map contains a set of key-value pairs representing the diagnostics data, one value per entry in the MEASURE enum.
		/// Each value is represented as a percentage where 100% represents the full scale analog measured value.
		///
		/// Call UpdateDiagnostics() first to retrieve the latest measurements from the system.
		///
		/// The map of values will be updated after the UpdateDiagnostics() function call and before the 
		/// DIAGNOSTICS_UPDATE_AVAILABLE event is fired so design the application to avoid accessing the map between
		/// these two timings to prevent a potential race condition.
		/// \return a reference to the diagnostics measurement map
		/// \since 1.1
		const std::map<MEASURE, Percent>& GetDiagnosticsData() const;

		/// \brief A helper method that returns human readable strings mapped to the available diagnostics data
		///
		/// The map contains a set of key-value pairs representing the diagnostics data, one value per entry alongside a human readable string.
		/// Each value is represented as a percentage where 100% represents the full scale analog measured value.
		///
		/// Call UpdateDiagnostics() first to retrieve the latest measurements from the system.
		///
		/// The map of values will be updated after the UpdateDiagnostics() function call and before the 
		/// DIAGNOSTICS_UPDATE_AVAILABLE event is fired so design the application to avoid accessing the map between
		/// these two timings to prevent a potential race condition.
		/// \return a copy of the internal diagnostics map, indexed by human readable strings
		/// \since 2.0.1
        std::map<std::string, Percent> GetDiagnosticsDataStr() const;
		//@}

	private:
		// Make this object non-copyable
		Diagnostics(const Diagnostics &);
		const Diagnostics &operator =(const Diagnostics &);

		// Declare Implementation
		class Impl;
		Impl * p_Impl;
	};

}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
