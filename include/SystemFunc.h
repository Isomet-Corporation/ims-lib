/*-----------------------------------------------------------------------------
/ Title      : System Functions Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/SystemFunc/h/SystemFunc.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2025-01-17 19:40:37 +0000 (Fri, 17 Jan 2025) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 680 $
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
/// \file SystemFunc.h
///
/// \brief Classes for performing system functions not directly related to RF signal generation and output
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_SysFunc
///

#ifndef IMS_SYSTEMFUNC_H__
#define IMS_SYSTEMFUNC_H__

#include "IMSSystem.h"
#include "IEventHandler.h"
#include "SignalPath.h"
#include "Auxiliary.h"

#include <cstdint>
#include <list>
#include <memory>

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
  /// \class SystemFuncEvents SystemFunc.h include\SystemFunc.h
  /// \brief All the different types of events that can be triggered by the SystemFunc class.
  ///
  /// Some events contain integer parameter data which can be processed by the IEventHandler::EventAction
  /// derived method
  /// \author Dave Cowan
  /// \date 2015-11-11
  /// \since 1.0
	class LIBSPEC SystemFuncEvents
	{
	public:
		/// \enum Events List of Events raised by the Signal Path module
    enum Events {
		/// The number of Errors accumulated on the Pixel Interface
			PIXEL_CHECKSUM_ERROR_COUNT,
			MASTER_CLOCK_REF_FREQ,
			MASTER_CLOCK_REF_MODE,
			MASTER_CLOCK_REF_STATUS,
			SYNTH_TEMPERATURE_1,
			SYNTH_TEMPERATURE_2,
			Count
		};
	};

	// Forward declarations
	struct StartupConfiguration;
	struct ClockGenConfiguration;

  ///
  /// \class SystemFunc SystemFunc.h include\SystemFunc.h
  /// \brief Provides System Management functions not directly related to RF signal generation or signal path control
  ///
  /// \author Dave Cowan
  /// \date 2015-11-03
  /// \since 1.0
	class LIBSPEC SystemFunc
	{
	public:
    ///
    /// \name Constructor & Destructor
    //@{
    ///
    /// \brief Constructor for SystemFunc Object
    ///
    /// An IMSSystem object, representing the configuration of an iMS target must be passed by const
    /// reference to the SystemFunc constructor.
    ///
    /// The IMSSystem object must exist before the SystemFunc object, and must remain valid (not
    /// destroyed) until the SystemFunc object itself is destroyed.
    ///
    /// Once constructed, the object can neither be copied or assigned to another instance.
    ///
    /// \param[in] ims A const reference to the iMS System
    /// \since 1.0
		SystemFunc(std::shared_ptr<IMSSystem> ims);
    ///
    /// \brief Destructor for SystemFunc Object
		~SystemFunc();
    //@}

	/// \enum UpdateClockSource
	/// \brief Determines whether DDS Synthesiser IC should have its update signal driven by the Synthesiser
	/// internal circuitry or from an external source (for synchronising the device to a system clock)
	/// \since 1.1
		enum class UpdateClockSource
		{
			/// Drive Update Signal internally (default)
			INTERNAL,
			/// Drive Update Signal from external update source
			EXTERNAL
		};
		///
	/// \enum TemperatureSensor
	/// \brief There are two available temperature sensors in the Synthesiser System
	/// \since 1.4
		enum class TemperatureSensor
		{
			/// Sensor 1 is adjacent to the RF stage
			TEMP_SENSOR_1,
			/// Sensor 2 is adjacent to the DC power supplies
			TEMP_SENSOR_2
		};
	/// \enum PLLLockReference
	/// \brief Synthesiser Master Clock Reference Mode
	/// \since 1.4.1
		enum class PLLLockReference
		{
			/// Master Clock uses internal precision frequency reference
			INTERNAL,
			/// Master Clock attempts to phase lock to an externally supplied reference clock with manually configured frequency
			EXTERNAL_FIXED,
			/// Master Clock attempts to phase lock to an externally supplied reference clock whose frequency is automatically determined
			EXTERNAL_AUTO,
			/// Master Clock attempts to phase lock to an externally supplied reference clock whose frequency is automatically determined.
			/// If the reference frequency measurement goes invalid for > 400ms, switch over to the internal frequency source until
			/// the clock reference mode is reprogrammed.
			EXTERNAL_FAILOVER
		};
	/// \enum PLLLockStatus
	/// \brief Synthesiser Master Clock Status
	/// Cast the status value reported to application code from a GetMasterClockStatus() call to PLLLockStatus to
	/// determine the current status of the Synthesiser Master Clock
		enum class PLLLockStatus
		{
			/// No signal detected on external reference clock input. PLL Unlocked.
			EXTERNAL_NOSIGNAL = 0,
			/// Master Clock using internal clock reference but PLL Not Locked (this should only occur temporarily when switching from external to internal mode)
			INTERNAL_UNLOCKED = 4,
			/// Master Clock using internal clock reference and PLL is Locked.
			INTERNAL_LOCKED = 5,
			/// Reference signal detected on external reference clock input but PLL is Not Locked to it (usually due to a frequency mismatch or non-conformal external signal reference)
			EXTERNAL_VALID_UNLOCKED = 8,
			/// Reference signal detected and PLL is Locked.
			EXTERNAL_LOCKED = 9
		};
	/// \since 1.4.1
	///
    /// Communications with an iMS System can be monitored using a "Communications Not Healthy" mechanism.  The
    /// iMS Controller features a timer with a configurable timeout value which resets each time a message
    /// is received from the host.  If no message is received from the host within the timeout period,
    /// the host communications is considered to be in an "unhealthy" state, meaning that messages
    /// were expected but haven't arrived.  The iMS System will set the Communications Not Healthy flag
    /// in any subsequent message responses that do get sent to the host, in case the problem was a temporary
    /// one, to indicate the problem to the host.  The host can clear the not healthy flag and take any
    /// further action, as necessary.
    ///
    /// In addition, if configured to do so, the iMS System can cause a local system-wide reset to attempt
    /// to re-initialise the communications, once it registers a Not Healthy condition.  This will flush
    /// any communications buffers and may restart communications if the problem was a local one.  This
    /// behaviour is turned off by default.
    ///
    /// At the host end, two things must be done:
    ///
    /// (1) Host software must be sure to send messages to the iMS System regularly, and well within
    /// the timeout limit set by the NHF timer.  The message can be any simple request for status
    /// information or anything else as required.  All messages will reset the timer.
    ///
    /// (2) Host software must perform a similar type of check, looking for timed out responses to its
    /// requests to identify that communications have failed.  It should then take appropriate action,
    /// resetting its communications interfaces where possible.
    ///
    /// The mechanism is intended for high-reliability applications where uptime is important and
    /// service access is limited.  It can be disabled completely if required.
    ///
    /// \name Communications "Not Healthy Flag"
    ///
    //@{
    /// \enum NHFLocalReset
    /// \brief The action to perform at the iMS System when a Not Healthy condition is registered
    ///
		enum class NHFLocalReset
		{
      /// Do nothing other than set the NHF bit on future responses
			NO_ACTION = 0,
      /// Perform a system wide reset
			RESET_ON_COMMS_UNHEALTHY = 1
		};
    /// \brief Clear the Not Healthy Flag once normal service is resumed
    ///
		bool ClearNHF();
    /// \brief Sends a single No-op message to the iMS to reset the NHF timer
    ///
    /// \since 1.9
		bool SendHeartbeat();
    /// \brief Starts a timer that repeatedly sends keep-alive heartbeat messages to the iMS system on a specified millisecond interval.
    /// If the timer is already running, it is restarted and the interval updated
    ///
    /// \since 1.9
        void StartHeartbeat(int intervalMs);
    /// \brief Stops the automated keep-alive timer
    ///
    /// \since 1.9
        void StopHeartbeat();
    /// \brief Configure the Not Healthy Flag mechanism
    ///
    /// \param[in] Enabled Turns the mechanism on or off (default: on)
    /// \param[in] milliSeconds The timeout interval for the NHF timer (default: 500msec)
    /// \param[in] reset The behaviour to perform when a the Communications is determined to be "Not Healthy" (default: NO_ACTION)
    /// \return true if the configuration request was sent successfully
    /// \since 1.0
		bool ConfigureNHF(bool Enabled, int milliSeconds, NHFLocalReset reset);
    //@}

    ///
    /// These software switches drive signal lines in the Synthesiser which connect through to the
    /// RF Amplifier and turn on or off the high power RF Amplifier, and selectively enable
    /// pairs of RF Channels within it.
    ///
		/// \name RF Amplifier Master Switches
    ///
    //@{
    /// \brief Enables the RF Amplifier
    ///
    /// \param[in] en true turns on the RF Amplifier, false turns it off
    /// \return true if the enable request was sent successfully
    /// \since 1.0
		bool EnableAmplifier(bool en);
	/// \brief Enables the External Equipment Optoisolator
	///
	/// \param[in] enable true turns on the Optoisolator
	/// \return true if the enable request was sent successfully
	/// \since 1.1
		bool EnableExternal(bool enable);

    /// \brief Selectively enables channels 1&2 and channels 3&4
    ///
    /// \param[in] chan1_2 true turns on channels 1 and 2, false turns them off
    /// \param[in] chan3_4 true turns on channels 3 and 4, false turns them off
    /// \return true if the enable request was sent successfully
    /// \since 1.0
		bool EnableRFChannels(bool chan1_2, bool chan3_4);
    //@}

	///
	/// \name Pixel Interface Checksum Error Counter
	/// The Fast Pixel Interface between the iMS Controller and iMS Synthesiser is protected by
	/// a simple checksum.  Any errors that accumulate on the interface are recorded in a counter
	/// which can be read and reset from software.  If the counter is non-zero, an LED can be 
	/// configured to light on the Synthesiser - see function Auxiliary::AssignLED().
	/// \param[in] Reset clears the error count to zero, extinguishing the LED (default true)
	/// \return true if the checksum error count request was sent successfully.
	/// \since 1.1
		bool GetChecksumErrorCount(bool Reset = true);
    ///
	///
	/// \name DDS Advanced Configuration Control
	//@{
	///
	/// \brief  Configures DDS Update signal source
	/// The Direct Digital Synthesiser engine built into the Synthesiser requires an
	/// update signal to initiate the output of an RF signal that was previously programmed
	/// to the device from an ImagePoint, ToneBufferEntry or CalibrationTone.  Normally this is
	/// handled internally by the Synthesiser electronics, in which case this should be left
	/// to Internal.  In certain advanced usage scenarios (typically where the Synthesiser must
	/// be synchronised to a user supplied master clock), the update signal may be sourced externally
	/// in which case it is derived from the External Image Clock input.
	/// \param[in] src INTERNAL for most scenarios, set to EXTERNAL for external update signal applications
	/// \return true if the request to change update signal source was sent successfully
	/// \since 1.1
		bool SetDDSUpdateClockSource(UpdateClockSource src = UpdateClockSource::INTERNAL);
	//@}

    /// \name Startup Configuration Programming
	//@{
	///
	/// \brief Store Synthesiser Default Startup Configuration to Non-volatile Memory
	///
	/// After every power up and reset event, the Synthesiser will inspect the non-volatile
	/// memory to see if a startup configuration is present.  If it is, the configuration contents
	/// are parsed and assigned to their respective control registers.  Combining this process
	/// with Default Scripts stored in the Filesystem can result in a fully specified standalone 
	/// operational Synthesiser system with no software connection required.
	/// param[in] cfg A const reference to the required configuration behaviour structure.  Pre-define the behaviour
	/// by setting the config structure fields to requirements.
	/// \return true if the request to program the startup configuration was sent successfully
	/// \since 1.1
		bool StoreStartupConfig(const StartupConfiguration& cfg);
	//@}

	/// \name Temperature Sensing
	//@{
	///
	/// \brief Reads the current temperature of the iMS
	///
	/// Some iMS Synthesisers include onboard temperature sensors to monitor the temperature inside
	/// the iMS case (note this is different to the temperature readings available using the 
	/// Diagnostics class that perform temperature readings on the amplifier and AO Device).
	/// Call this function to initiate a temperature reading, specifying which sensor to read from.
	/// The temperature value will be reported back to the application code using the 
	/// SystemFuncEvents::SYNTH_TEMPERATURE_1 and SystemFuncEvents::SYNTH_TEMPERATURE_2 events.
	/// \return true if the requst to read the iMS temperature was sent successfully
	/// \since 1.4
		bool ReadSystemTemperature(SystemFunc::TemperatureSensor sensor);
	//@}

	/// \name Master Reference Clock
	///
	/// Some iMS Synthesisers feature a PLL (Phase Lock Loop) and high accuracy internal clock oscillator
	/// that can either be set to lock the Synthesiser master clock to a precision internal reference (<2ppm)
	/// or slave to ane externally supplied reference clock.
	///
	/// If set to slave to an external reference clock, there are three external modes:
	///
	///  1) External Manual: in which the frequency of the externally supplied clock source is programmed
	/// into the Synthesiser by application software.
	///  2) External Auto: in which the frequency of the externally supplied clock source is measured by the
	/// Synthesiser and the PLL continually updated to lock to that frequency
	///  3) External Failover: a modification of the "Auto" mode in which if the PLL is ever seen to lose its
	/// locked state, having previously been locked, it will switch over to the Internal precision crystal oscillator.
	///
	/// In all cases, the externally supplied clock may have any frequency that is a multiple of 10kHz with
	/// a minimum supported clock rate of 50kHz and a maximum of 10MHz.
	///
	//@{
	///
	/// \brief Sets the Master Reference Clock mode of the Synthesiser to either Internal or on of the External modes
	///
	/// Specify the desired reference clock mode.  If using EXTERNAL_FIXED, also specify the external frequency
	/// \param[in] mode The reference clock mode to set
	/// \param[in] ExternalFixedFreq the frequency of the external reference clock, if using Fixed mode
	/// \return true if the mode setting command was sent successfully
	/// \since 1.4.1
		bool SetClockReferenceMode(SystemFunc::PLLLockReference mode, kHz ExternalFixedFreq = kHz(1000.0));
	///
	/// \brief Returns the current status of the master reference clock function.
	///
	/// This command issues a status request from the Synthesiser's Master Reference Clock.  The status is returned as
	/// an integer event and the user should subscribe to the SystemFuncEvents::MASTER_CLOCK_REF_STATUS event to retrieve
	/// the result.  The returned integer can be cast to a SystemFunc::PLLLockStatus enum to interpret the status integer
	/// \return true if the status request was sent successfully
		bool GetClockReferenceStatus();
	///
	/// \brief Returns the measured frequency of the external reference clock port
	///
	/// The external reference clock port is continually monitored, even if the master clock is set to Internal.
	/// This function requests the current frequency of any signal connected to the reference clock port.  Note that
	/// the reference clock measurement function is limited to measure the input clock only as a multiple of 10kHz
	/// so this function will not return a value with any finer resolution than that.  The value is returned as a 
	/// real value (double) event and the user should subscribe to the SystemFuncEvents::MASTER_CLOCK_REF_FREQ event
	/// to retrieve the result.  The returned double represents the frequency of the external reference clock to the
	/// closest value of 10kHz.
	/// \return true if the frequency reference request was sent successfully
		bool GetClockReferenceFrequency();
	///
	/// \brief Returns the current mode of the reference clock function
	///
	/// This function requests from the Synthesiser the current mode in which the reference clock function is operating.
	/// The mode is returned as an integer event and the user should subscribe to the SystemFuncEvents::MASTER_CLOCK_REF_MODE
	/// event to retrieve the result.  The returned integer can be cast to a SystemFunc::PLLLockReference enum to interpret
	/// the event integer
	/// \return true if the mode request was sent successfully
		bool GetClockReferenceMode();
	//@}


	/// \name Image Clock Generator
	///
	/// The iMS System has an internal clock and trigger generator which can be used when an external clock or trigger
	/// is not provided to the system.  The generator operates in two modes:
	/// 
	/// (1) Internal mode (default).  In this mode, the clock is not visible externally.  The image clock frequency is
	/// determined by the frequency value associated with an image.
	/// 
	/// (2) Clock output mode.  The clock frequency is programmed by the user with configurable duty cycle and phase
	/// (relative to the position of the image point update).  The generated clock signal is output by the iMS system
	/// so that it can be used to drive other system components.  Clock frequencies associated with images are ignored.
	/// 
	/// The functions in this section are used to configure the iMS system in Clock Output mode.
	/// 
	/// \since 1.8.11
	/// 
	//@{
	///
	/// \brief Configure the internal clock generator for Clock Output mode
	/// 
		bool ConfigureClockGenerator(const ClockGenConfiguration& cfg);
	///
	/// \brief Return the current configuration of the clock generator
		const ClockGenConfiguration& GetClockGenConfiguration() const;
	///
	/// \brief Disables the clock generator, returning to Internal Clock only mode
	///
		bool DisableClockGenerator();
	///
	//@}

	/// \name Event Notifications
    //@{
    ///
    /// \brief Subscribe a callback function handler to a given SystemFuncEvents event
    ///
    /// SystemFunc can callback user application code when an event occurs that affects the signal path.
    /// Supported events are listed under SystemFuncEvents.  The
    /// callback function must inherit from the IEventHandler interface and override
    /// its EventAction() method.
    ///
    /// Use this member function call to subscribe a callback function to a SystemFuncEvents event.
    /// For the period that a callback is subscribed, each time an event in SystemFunc occurs
    /// that would trigger the subscribed SystemFuncEvents event, the user function callback will be
    /// executed.
    /// \param[in] message Use the SystemFuncEvents::Event enum to specify an event to subscribe to
    /// \param[in] handler A function pointer to the user callback function to execute on the event trigger.
    /// \since 1.0
		void SystemFuncEventSubscribe(const int message, IEventHandler* handler);
    /// \brief Unsubscribe a callback function handler from a given SystemFuncEvents event
    ///
    /// Removes all links to a user callback function from the Event Trigger map so that any
    /// events that occur in the SystemFunc object following the Unsubscribe request
    /// will no longer execute that function
    /// \param[in] message Use the SystemFuncEvents::Event enum to specify an event to unsubscribe from
    /// \param[in] handler A function pointer to the user callback function that will no longer execute on an event
    /// \since 1.0
		void SystemFuncEventUnsubscribe(const int message, const IEventHandler* handler);
    //@}
	private:
		// Makes this object non-copyable
		SystemFunc(const SystemFunc &);
		const SystemFunc &operator =(const SystemFunc &);

		class Impl;
		Impl * p_Impl;
	};

	/// \struct StartupConfiguration SystemFunc.h include\SystemFunc.h
	/// \brief The Synthesiser stores in its non-volatile memory a set of configuration
	/// values that are preloaded on startup.
	///
	/// Modify the values present in this struct and pass the struct by reference to the
	/// SystemFunc::StoreStartupConfig() function to overwrite the existing startup configuration parameters
	///
	/// e.g.
	/// \code
	/// SystemFunc sys(myiMS);
	/// StartupConfiguration cfg;
	/// cfg.DDSPower = 100.0;
	/// sys.StoreStartupConfig(cfg);
	/// \endcode
	/// \since 1.1
	struct LIBSPEC StartupConfiguration
	{
		/// Setting for RF Amplitude Control Wiper 1
		Percent RFAmplitudeWiper1{ 0.0 };
		/// Setting for RF Amplitude Control Wiper 2
		Percent RFAmplitudeWiper2{ 0.0 };
		/// Setting for DDS Powere Level
		Percent DDSPower{ 0.0 };
		/// Select which of the four control sources should be applied to the RF signal amplitude modulation
		SignalPath::AmplitudeControl AmplitudeControlSource{ SignalPath::AmplitudeControl::WIPER_1 };
		/// Switch the RF power amplifier gate signal on/off at startup
		bool RFGate{ false };
		/// Individually enable the bias power for channels 1 and 2 at startup
		bool RFBias12{ false };
		/// Individually enable the bias power for channels 3 and 4 at startup
		bool RFBias34{ false };
		/// Select whether to enable the external equipment optoswitch at startup
		bool ExtEquipmentEnable{ false };
		/// Sets whether the LTB should use amplitude compensation from the look-up table
		SignalPath::Compensation LTBUseAmplitudeCompensation{ SignalPath::Compensation::ACTIVE };
		/// Sets whether the LTB should use phase compensation from the look-up table
		SignalPath::Compensation LTBUsePhaseCompensation{ SignalPath::Compensation::BYPASS };
		/// Selects the control mode for the LTB: OFF (Image mode), host software control, external drive (16) or extended external (256) 
		SignalPath::ToneBufferControl LTBControlSource{ SignalPath::ToneBufferControl::OFF };
		/// In host mode, picks the initial setting for the Tone Buffer index
		std::uint8_t LocalToneIndex{ 0 };
		/// Apply any phase tuning offset coefficient to the RF output on channel 1
		Degrees PhaseTuneCh1{ 0.0 };
		/// Apply any phase tuning offset coefficient to the RF output on channel 2
		Degrees PhaseTuneCh2{ 0.0 };
		/// Apply any phase tuning offset coefficient to the RF output on channel 3
		Degrees PhaseTuneCh3{ 0.0 };
		/// Apply any phase tuning offset coefficient to the RF output on channel 4
		Degrees PhaseTuneCh4{ 0.0 };
		/// If true, the 4 RF signals will output in reverse channel order
		bool ChannelReversal{ false };
		/// Sets whether Image pixel data should use amplitude compensation from the look-up table
		SignalPath::Compensation ImageUseAmplitudeCompensation{ SignalPath::Compensation::ACTIVE };
		/// Sets whether Image pixel data should use phase compensation from the look-up table
		SignalPath::Compensation ImageUsePhaseCompensation{ SignalPath::Compensation::BYPASS };
		/// Configures the DDS update clock source to either be generated internally or be derived from the external Image clock input
		SystemFunc::UpdateClockSource upd_clk{ SystemFunc::UpdateClockSource::INTERNAL };
		/// Enables X/Y Deflector mode in which phase compensation is applied independently to each pair of channels
		bool XYCompEnable{ false };
		/// Configures the Green LED function
		Auxiliary::LED_SOURCE LEDGreen{ Auxiliary::LED_SOURCE::PULS };
		/// Configures the Yellow LED function
		Auxiliary::LED_SOURCE LEDYellow{ Auxiliary::LED_SOURCE::RF_GATE };
		/// Configures the Red LED function
		Auxiliary::LED_SOURCE LEDRed{ Auxiliary::LED_SOURCE::INTERLOCK };
		/// The default value to drive on the General Purpose output
		std::uint8_t GPOutput{ 0 };
		/// Sets what action to perform if the communications channel enters an "unhealthy" state
		SystemFunc::NHFLocalReset ResetOnUnhealthy{ SystemFunc::NHFLocalReset::NO_ACTION };
		/// Turns on/off the communications channel health state check
		bool CommsHealthyCheckEnabled{ false };
		/// Timeout between communications messages after which deemed unhealthy
		unsigned int CommsHealthyCheckTimerMilliseconds{ 500 };
		/// Sets the source of synchronous data applied to the digital outputs
		SignalPath::SYNC_SRC SyncDigitalSource { SignalPath::SYNC_SRC::IMAGE_DIG };
		/// Sets the source of synchronous data applied to the analog A output
		SignalPath::SYNC_SRC SyncAnalogASource { SignalPath::SYNC_SRC::IMAGE_ANLG_A };
		/// Sets the source of synchronous data applied to the analog B output
		SignalPath::SYNC_SRC SyncAnalogBSource { SignalPath::SYNC_SRC::IMAGE_ANLG_B };
		/// Sets the default system clock mode - internally generated or slave to an external reference clock input
		SystemFunc::PLLLockReference PLLMode{ SystemFunc::PLLLockReference::INTERNAL };
		/// Defines the external supplied reference clock frequency when manual external reference PLL mode is used
		kHz ExtClockFrequency{ 1000.0 };
		/// Defines whether Phase Accumulator clear on ImagePoint update is enabled
		bool PhaseAccClear{ false };
	};


	/// \struct ClockGenConfiguration SystemFunc.h include\SystemFunc.h
	/// \brief Set values in this struct to configure the clock generator for clock output mode
	///
	/// Modify the values present in this struct and pass the struct by reference to the
	/// SystemFunc::ConfigureClockGenerator() function to update the clock generator configuration
	///
	/// e.g.
	/// \code
	/// SystemFunc sys(myiMS);
	/// ClockGenConfiguration cfg;
	/// cfg.ClockFreq = kHz(700.0);
	/// cfg.AlwaysOn = true;
	/// sys.ConfigureClockGenerator(cfg);
	/// \endcode
	/// \since 1.8.11
	struct LIBSPEC ClockGenConfiguration
	{
		///  Set the Clock Generator Frequency (use Hz, kHz or MHz)
		Frequency  ClockFreq{ 1000000.0 };
		///  Set the Oscillator Phase (Output clock edge relative to RF image point update)
		Degrees    OscPhase{ 0.0 };
		///  Generated Clock Duty Cycle
		Percent    DutyCycle{ 50.0 };
		///  Set true to output clock continuously.  If false, clock is output only during image playback
		bool       AlwaysOn{ true };
		///  Also generate a trigger pulse during the first point of image playback on trigger connector
		bool       GenerateTrigger{ false };
		///  Clock Polarity: Rising Edge active (NORMAL) or Falling Edge active (INVERSE)
		Polarity   ClockPolarity{ Polarity::NORMAL };
		///  Trigger Polarity: Rising Edge active (NORMAL) or Falling Edge active (INVERSE)
		Polarity   TrigPolarity{ Polarity::NORMAL };
	};
}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
