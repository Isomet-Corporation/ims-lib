/*-----------------------------------------------------------------------------
/ Title      : Signal Path Operations Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/SignalPath/h/SignalPath.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2023-07-28 14:23:44 +0100 (Fri, 28 Jul 2023) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 576 $
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
/// \file SignalPath.h
///
/// \brief Classes for controlling the flow of data and RF signals through the Synthesiser
///
/// SignalPath is one of the core features of the iMS Library, providing
/// the user application with the ability to configure the routing of signal data (frequency, amplitude,
/// phase and synchronous output busses), switching in and out functions that affect the signal path,
/// and control RF signal flow, such as DDS output power and modulation control
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_SigPath
///

#ifndef IMS_SIGNALPATH_H__
#define IMS_SIGNALPATH_H__

#include "IMSSystem.h"
#include "IEventHandler.h"
#include "IMSTypeDefs.h"

#include <memory>
#include <array>
#include <chrono>
#include <climits>

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
  /// \class SignalPathEvents SignalPath.h include\SignalPath.h
  /// \brief All the different types of events that can be triggered by the SignalPath class.
  ///
  /// Some events contain integer parameter data which can be processed by the IEventHandler::EventAction
  /// derived method
  /// \author Dave Cowan
  /// \date 2015-11-11
  /// \since 1.0
	class LIBSPEC SignalPathEvents
	{
	public:
    /// \enum Events List of Events raised by the Signal Path module
		enum Events {
			/// \brief Returns DDS Power setting
			RX_DDS_POWER,
			/// \brief Returns current Encoder X Channel Velocity
			ENC_VEL_CH_X,
			/// \brief Returns current Encoder Y Channel Velocity
			ENC_VEL_CH_Y,
			Count
		};
	};

  /// Forward Declaration
	struct VelocityConfiguration;

  ///
  /// \class SignalPath SignalPath.h include\SignalPath.h
  /// \brief Controls Signal routing and other parameters related to the RF output signals
  ///
  /// The iMS Signal Path consists of Frequency, Amplitude, Phase and Synchronous Output data driven
  /// by the Controller, or generated internally by the Synthesiser, passing through the Compensation
  /// Tables, and driven by the DDS device to result in 4 RF signal outputs along with analogue and
  /// digital synchronous outputs.
  ///
  /// This class provides functions that control the routing options of that data and functions that
  /// control the attributes of the signals within the signal path.
  ///
  /// \author Dave Cowan
  /// \date 2015-11-03
  /// \since 1.0
	class LIBSPEC SignalPath
	{
	public:
    /// \enum AmplitudeControl
    /// \brief Selects Amplitude Control source for each of the 4 RF Channel outputs.
    ///
    /// The RF signal outputs from the Synthesiser feature channel bandwidth filtering and an RF mixer
    /// with a selectable control source.  The mixer input can be routed to one of two digital potentiometers
    /// on the Synthesiser, which act as amplitude controls, turned off, or to an external signal
    /// input (scalable range from 0 - 15V) for signal modulation.
		enum class AmplitudeControl
		{
      /// \brief Turn RF outputs off
			OFF,
      /// \brief Route RF mixer inputs to analogue modulation external signals
			EXTERNAL,
      /// \brief Connect RF mixer inputs to digital pot wiper 1
			WIPER_1,
      /// \brief Connect RF mixer inputs to digital pot wiper 2
			WIPER_2,
	  /// \brief RF mixer inputs are controlled by digital pot.  Use this on hardware that supports independent control per channel.
			INDEPENDENT
		};

	/// \enum ToneBufferControl
	/// \brief Selects Control Source for the Local Tone Buffer
	///
	/// The Local Tone Buffer (LTB) in the synthesiser contains 256 individually selectable TBEntry Entries.
	/// Each entry contains Frequency, Amplitude and Phase data for each of the 4 channels independently.
	/// The index into the LTB can be chosen from either software control, or one of two external control modes.
	/// In the standard external control mode, the 4 PROFILE input signals are used to index the first 16
	/// TBEntrys in the LTB.  In the extended external control mode, the 4 PROFILE input signals are used with
	/// the 4 GPI inputs GPI[4..1] to select the upper 4 bits of the tone buffer index
	///
	/// If none of these 3 modes is selected, the normal Image Path drives the Synthesiser outputs.
	/// \since 1.1
		enum class ToneBufferControl
		{
	  /// \brief The Local Tone Buffer is routed to the Synthesiser.  Index updates are controlled from host software
			HOST,
	  /// \brief The Local Tone Buffer is routed to the Synthesiser (first 16 entries only).  LTB is indexed from PROFILE pin inputs
			EXTERNAL,
	  /// \brief The Local Tone Buffer is routed to the Synthesiser (all 256 entries available).  Entry controlled from PROFILE pin inputs and GPI[4..1] inputs
			EXTERNAL_EXTENDED,
	  /// \brief Local Tone Buffer not used.  Synthesiser outputs from Image data
			OFF
		};

	/// \enum Compensation
	/// \brief Controls whether to use the Compensation Look-Up Table path for pixel data
	///
	/// The Synthesiser includes a Compensation system for correcting amplitude non-linearities in the RF
	/// signal path, generating inter-channel phase data for beam steered applications, and synchronous
	/// digital and analogue output signals all as a function of the current active pixel frequency.
	/// The Compensation table can be in circuit (active) or bypassed for both the normal pixel path
	/// from the Controller Image and also for the Local Tone Buffer used on the Synthesiser.
	///
	/// \since 1.1
		enum class Compensation : bool
		{
	  /// \brief Use the Compensation Look-up Path
			ACTIVE = true,
	  /// \brief Do not use the Compensation Look-up Path
			BYPASS = false
		};

	/// \enum CompensationScope
	/// \brief Chooses the applicable scope for a Compenation Table: globally or per channel
	///
	/// A CompensationTable downloaded to a Synthesiser can be applied to FAP data in one of two
	/// methods.  With Global scope, there is a single table applied universally to all channels.
	/// With Channel scope, multiple tables are downloaded to the Synthesiser, one for each RF
	/// Channel.
	///
	/// Note that the size of storage space on the Synthesiser remains constant in both scopes
	/// so the resolution of channel scope compared to global scope is the reciprocal of the 
	/// number of RF channels.
	///
	/// \since 1.6
		enum class CompensationScope
		{
		/// \brief Single CompensationTable applied universally to all channels
			GLOBAL,
		/// \brief Multiple CompensationTable's applied per channel
			CHANNEL
		};

	/// \enum SYNC_SRC
	/// \brief Selects a source of Synchronous Output Data
	/// \since 1.1
		enum class SYNC_SRC
		{
			FREQUENCY_CH1,
			FREQUENCY_CH2,
			FREQUENCY_CH3,
			FREQUENCY_CH4,
			AMPLITUDE_CH1,
			AMPLITUDE_CH2,
			AMPLITUDE_CH3,
			AMPLITUDE_CH4,
			AMPLITUDE_PRE_COMP_CH1,
			AMPLITUDE_PRE_COMP_CH2,
			AMPLITUDE_PRE_COMP_CH3,
			AMPLITUDE_PRE_COMP_CH4,
			PHASE_CH1,
			PHASE_CH2,
			PHASE_CH3,
			PHASE_CH4,
			LOOKUP_FIELD_CH1,
			LOOKUP_FIELD_CH2,
			LOOKUP_FIELD_CH3,
			LOOKUP_FIELD_CH4,
			IMAGE_ANLG_A,
			IMAGE_ANLG_B,
			IMAGE_DIG,
			PULSE_GATE
		};

	/// \enum SYNC_SINK
	/// \brief The Synchronous Output to which to assign Synchronous Data
	/// \since 1.1
		enum class SYNC_SINK
		{
			ANLG_A,
			ANLG_B,
			DIG
		};

	/// \enum SYNC_DIG_MODE
	/// \brief Synchronous Digital output mode setting: pulsed (configurable width) or level (image point duration)
	/// \since 1.8.7
		enum class SYNC_DIG_MODE
		{
			PULSE,
			LEVEL
		};

	/// \enum ENCODER_MODE
	/// \brief Selects the type of encoder connected to the Synthesiser
	///
	/// The preferred mode of operation is quadrature, in which the two encoder signals
	/// output a pulse train in which the second electrically leads or lags the other by 90
	/// degrees, depending on the direction of rotation.  This mode gives the best resolution.
	///
	/// The alternative mode: Count+Direction ouputs a single pulse train with the second signal
	/// indicating the direction of rotation ('1' = forward, '0' = reverse)
	/// \since 1.4
		enum class ENCODER_MODE
		{
			QUADRATURE,
			COUNT_DIRECTION
		};

	/// \enum VELOCITY_MODE
	/// \brief Selects the method of velocity calculation
	///
	/// The rotary encoder input is connected to a tracking loop filter which calculates the
	/// current angular velocity of the encoder shaft, in ticks / second.  The loop filter
	/// can generate two different estimates of the current velocity with different characteristics,
	/// without altering the behaviour of the filter response.
	///
	/// The first is the closest approximation to the filter state and has a fast response but a
	/// higher noise profile which may lead to low level frequency modulation on the DDS output signal.
	/// 
	/// The second has a much slower response (typically 1-2 orders of magnitude) but a cleaner spectrum.
	///
	/// \since 1.4
		enum class VELOCITY_MODE
		{
			FAST,
			SLOW
		};

	/// \enum ENCODER_CHANNEL
	/// \brief Selects which of two available encoder channels
	/// 
	/// The Rotary Encoder input has two channels, each comprising a pair of RS422 differential signals.
	/// The signal pairs are decoded by the rotary encoder block into a sequence of forward and reverse
	/// pulses which are processed to calculate a tick velocity, which can be converted to angular velocity
	/// through knowledge of the number of pulses per revolution (ppr) of the encoder.
	///
	/// Normally, only the first encoder is used and the velocity value used to compensate the frequency
	/// on all 4 channels of the synthesiser.  However, in the case where the Synthesiser is configured
	/// for X/Y deflection, the first encoder input affects Synthesiser channels 1 and 2 and the second
	/// encoder input affects Synthesiser channels 3 and 4.
	///
	/// \since 1.4
		enum class ENCODER_CHANNEL
		{
			CH_X,
			CH_Y
		};

	///
    /// \name Constructor & Destructor
    //@{
    ///
    /// \brief Constructor for SignalPath Object
    ///
    /// An IMSSystem object, representing the configuration of an iMS target must be passed by const
    /// reference to the SignalPath constructor.
    ///
    /// The IMSSystem object must exist before the SignalPath object, and must remain valid (not
    /// destroyed) until the SignalPath object itself is destroyed.
    ///
    /// Once constructed, the object can neither be copied or assigned to another instance.
    ///
    /// \param[in] ims A const reference to the iMS System
    /// \since 1.0
		SignalPath(std::shared_ptr<IMSSystem> ims);
    ///
    /// \brief Destructor for SignalPath Object
		~SignalPath();
    //@}

    ///
    /// \name RF Output Control
    //@{
    ///
    /// \brief Scales the DDS device (Direct Digital Synthesis RF signal generator) power up & down
    ///
    /// The RF signal generator device on the Synthesiser converts frequency, amplitude and phase
    /// data into the 4 RF signals that drive the output of the Synthesiser.  The device can be
    /// configured to scale the analogue output power up & down.  This function performs the power
    /// scaling between 0% (minimum power) and 100% (maximum power)
    ///
    /// \param[in] power the percentage of maximum power at which the DDS should drive RF signals into the output signal conditioning
    /// \return true if the power update request was sent successfully
    /// \since 1.0
		bool UpdateDDSPowerLevel(const Percent& power);
		//void GetDDSPowerLevel();
    /// \brief Scales the Digital Potentiometer mixer drive level up & down
    ///
    /// The 2 digital potentiometers on the Synthesiser can be selected to apply a DC drive level to the
    /// IF input of a wideband RF mixer in the output channel signal conditioning, thereby acting as
    /// an amplitude control voltage.
    ///
    /// This function sets the drive level of the 2 digital potentiometers.  The AmplitudeControl input
    /// determines which potentiometer is updated, if it is set to anything other than WIPER_1 or
    /// WIPER_2, the request is ignored and the function returns false.
    ///
    /// \param[in] src Which of the two digital potentiometers to update
    /// \param[in] ampl the percentage of maximum amplitude scaling to update the potentiometer to
	/// \param[in] chan On Synthesisers that support independent channel amplitude control, this can be used to select which channel to apply the change to
	/// \return true if the amplitude update request was sent successfully
    /// \since 1.0
		bool UpdateRFAmplitude(const AmplitudeControl src, const Percent& ampl, const RFChannel& chan = RFChannel::all);
    /// \brief Selects the amplitude control source for all 4 RF channels
    ///
    /// Selects the analogue control source to apply to the RF mixer in the output signal conditioning
    /// for all 4 RF Channels: digital pot 1, digital pot 2, external analogue modulation or turned off
    /// \param[in] src The Amplitude Control Source selection
	/// \param[in] chan On Synthesisers that support independent channel amplitude control, this can be used to select which channel to apply the change to
    /// \return true if the source select update request was sent successfully
    /// \since 1.0
		bool SwitchRFAmplitudeControlSource(const AmplitudeControl src, const RFChannel& chan = RFChannel::all);

    /// \brief Applies a constant Phase offset to one of the 4 RF Channels
    ///
    /// The 4 RF Channels can be 'tuned' to offset phase discrepancies in, for example, cable length
    /// differences by setting up a constant phase offset that will be added to the RF signal output
    /// of that channel
    /// \param[in] channel The RF Channel to apply the offset to
    /// \param[in] phase The amount of constant phase offset to apply, in degrees
    /// \return true if the phase offset update request was sent successfully
    /// \since 1.0
		bool UpdatePhaseTuning(const RFChannel& channel, const Degrees& phase);
    /// \brief Reverses the channel order of the 4 RF Outputs
    ///
    /// Sometimes, usually to simplify cable routing, it is desirable to order the 4 RF outputs in
    /// 4-3-2-1 configuration instead of 1-2-3-4.  This can be achieved by setting the channel reversal
    /// configuration bit, using this function.
    /// \param[in] reversal Set true to enable the channel reversal (Channel 1 outputs Channel 4 data and vice versa)
    /// \return true if the reversal update request was sent successfully
    /// \since 1.0
		bool SetChannelReversal(bool reversal);

	/// \brief Enables / Disables the programmed amplitude and phase Compensation Functions for Image Playback
	///
	/// Image Pixel data pass through a Compensation process in the Synthesiser which performs amplitude
	/// corrections and phase adjustment for beam steering applications as a function of the programmed
	/// frequency.  The Compensation tables must be programmed either from software, or from a look-up
	/// table stored in the Synthesiser FileSystem.  If no compensation table has been programmed, or the
	/// application does not wish to use Compensation, the process can be bypassed by calling this function
	/// with the appropriate settings for amplitude and phase.
	/// \param[in] amplComp Set to SignalPath::Compensation::BYPASS or SignalPath::Compensation::ACTIVE for amplitude compensation (frequency dependent correction)
	/// \param[in] phaseComp Set to SignalPath::Compensation::BYPASS or SignalPath::Compensation::ACTIVE for phase compensation (frequency dependent beam steering)
	/// \return true if the compensation request was sent successfully
	/// \since 1.3
		bool EnableImagePathCompensation(SignalPath::Compensation amplComp, SignalPath::Compensation phaseComp);

	/// \brief Indicates whether Synthesiser Compensation is enabled globally or per channel
	/// \since 1.6
		CompensationScope QueryCompensationScope();

	/// \brief Configures Beam Steering Phase Compensation for X/Y Deflector Mode
	///
	/// Normal phase beam steering configures the 4 RF Channel outputs for incremental phase adjustment so that
	/// channel 1 has zero phase, channel 2 has a frequency dependent phase offset with respect to channel 1,
	/// channel 3 has twice the phase offset and channel 4 has three times the phase offset.
	///
	/// In an X/Y deflector configuration, the first two channels are assigned to deflector X and the second two
	/// channels to deflector Y.  In this case, both channels 1 and 3 have zero phase, channels 2 and 4 have a single
	/// frequency dependent offset with respect to those channels.
	/// \param[in] XYCompEnable Set to true to enable X/Y style phase beam steering (split channels)
	/// \return true if the XY Phase Setting request was sent successfully.
	/// \since 1.3
		bool EnableXYPhaseCompensation(bool XYCompEnable);

	/// \brief Sets a configurable delay between output channel pairs
	/// 
	/// When using a pair of AO deflectors in X/Y mode, it can be useful to configure a small output delay
	/// between the RF channels used to drive the X AOD and the channels used to drive the Y AOD to 
	/// maximise the combined efficieny.
	/// 
	/// This function will delay the RF output of one pair of channels in a 4-channel synthesiser relative to 
	/// the other.  The delay is configurable in 10ns increments and ranges from -40.9 to +40.9 microseconds.
	/// If delay > 0, channels 3&4 (Y axis) are delayed relative to channels 1&2 (X axis).
	/// If delay < 0, channels 1&2 (X axis) are delayed relative to channels 3&4 (Y axis)
	/// \param[in] delay The amount of time to delay the second pair of channels relative to the first
	/// \return true if the delay command was issued successfully
	/// \since 1.8.7
		bool SetXYChannelDelay(::std::chrono::nanoseconds delay  = ::std::chrono::nanoseconds::zero());

	/// \brief Sets a configurable delay for both output channel pairs
	/// 
	/// This method configures both channel delays simultaneously.  Both delay values should be in the
	/// range 0 to +40.9 microseconds.  Setting both values the same allows the user to configure
	/// an overall RF delay for all RF outputs of the synthesiser.
	/// 
	/// \param[in] first The amount of time to delay the first pair of channels (channels 1 & 2)
	/// \param[in] second The amount of time to delay the second pair of channels (channels 3 & 4)
	/// \return true if the delay command was issued successfully
	/// \since 1.8.8
		bool SetChannelDelay(::std::chrono::nanoseconds first  = ::std::chrono::nanoseconds::zero(),
					 	     ::std::chrono::nanoseconds second = ::std::chrono::nanoseconds::zero());
		//@}

	/// \name Calibration Functions
    //@{
    ///
    /// \brief Bypasses Controller Data and Compensation Tables and plays a fixed tone for calibration purposes
    ///
    /// In order to calibrate the RF output signal path and the AO Device, it is sometimes useful to
    /// play a fixed calibration tone.  This can be achieved using this function, which disconnects the
    /// Controller from the signal path along with the Compensation Tables and immediately plays a pure tone on all
    /// 4 RF Channels simultaneously at the Frequency, Amplitude and Phase Offsets specified by the input
    /// argument.  The fixed tone will remain on the output until cleared.
	///
	/// \bug In v1.0 SDK calibration tone amplitude would be 25% of value provided in fap.  Corrected in 1.1.0.
	///
    /// \param[in] fap a FAP triad specifying the output tone to be played back.
    /// \return true if the calibration tone request was sent successfully
    /// \since 1.0
		bool SetCalibrationTone(const FAP& fap);
    /// \brief Stops the tone playback and restores the signal path configuration to the Controller and Compensation Tables
    ///
    /// \return true if the clear tone request was sent successfully
    /// \since 1.0
		bool ClearTone();
	///
	/// \brief Applies a holding function to one of the RF channels.
	/// 
	/// This allows user software to set the calibration tone independently for different channels.  This is
	/// particularly useful when calibrating X/Y deflectors as the user can set the calibration for the X channels
	/// then apply a lock to those channels while calibrating the Y channels separately.
	/// \param[in] chan Which of the RF Channels to set the hold state on.
	/// \return true if the lock command completed successfully
	/// \since 1.7.0
		bool SetCalibrationChannelLock(const RFChannel& chan);
	/// \brief Remove the tone holding state from the specified RF Channel
	///
	/// Call this function to allow the specified channel to track the Calibration Tone once more.  Use without
	/// a parameter to specify the default channel 'All' which re-enables all channels.
	/// \param[in] chan Which of the RF Channels to re-enable (or All)
	/// \return true if the unlock command completed successfully
	/// \since 1.7.0
		bool ClearCalibrationChannelLock(const RFChannel& chan = RFChannel::all);
	/// \brief Query the iMS System to determine current per-channel holding state
	///
	/// The current state of the Calibration Tone Channel Lock is held on the iMS System hardware so it
	/// can be queried by calling this function for a specific channel.  
	/// \param[in] chan Which of the channels to get current lock state for
	/// \return true if the specified channel is currently locked
	/// \since 1.7.0
		bool GetCalibrationChannelLockState(const RFChannel& chan);
    //@}

	/// \name Phase Resynchronisation
	///
	/// The DDS that generates the output RF signals maintains independent phase accumulators for each
	/// of the output channels.  In some applications, typically with beam-steered deflectors, it is imperative
	/// that the relative phase between each of the accumulators is precisely controlled.  For these applications,
	/// The AutoPhaseResync command ensures that on each new ImagePoint, the phase of each accumulator is zeroed
	/// prior to the application of the FAP data of the new point.  The downside of enableing AutoPhaseResync is
	/// that it creates a short discontinuity in the RF output that persists for the first RF cycle at the new
	/// frequency.  This may not be noticeable in most applications however we recommend the feature is turned off
	/// for fast updating images (Clock Rate > 100kHz).
	///
	/// The manual PhaseResync command provides a one-shot clear of all channel phase accumulators.  It is 
	/// recommended practice to issue this command when the Synthesiser is stopped prior to commencing playback.
	///
	//@{
	/// \brief Manual One-Shot Phase Resynchronisation
	/// Clears the phase accumulators of all RF Channels.
	/// \return true if the command was issued successfully.
	/// \since 1.7.0
		bool PhaseResync();
	/// \brief Automatic Phase Resynchronisation
	/// Clears the phase accumulators of all RF Channels for every new ImagePoint.  Causes a small discontinuity
	/// in the RF signal output
	/// \param[in] enable Set to true (or leave blank) to turn on the feature, false to turn off
	/// \return true if the command was issued successfully.
	/// \since 1.7.0
		bool AutoPhaseResync(bool enable = true);
	//@}
		
	/// \name Enhanced Tone Functions
    /// 	
    /// The Enhanced Tone Mode is an extended mechanism of tone output that can produce independent tones on each
	/// of the four output channels.  It also supports the generation of linear sweeps in either frequency or phase
	/// independently on each channel.  A linear sweep can be specified as start and end value, separate duration for
	/// upward and downward ramps and the number of target interpolation points per ramp.
	///
	/// Note that when using linear sweep, due to hardware limitations, it is not possible to have full control of the 
    /// amplitude of the output signal, however it is possible to modify the DAC current reference to one of four settings
    /// for a coarse amplitude variation.
    ///
	/// Please note that Enhanced Tone Mode requires f/w revision 1.3.57 or greater on older (rev A) iMS4 Synthesisers.
	///
	//@{
	///
	/// \brief Synthesiser Output Channel Tone with more enhanced functionality than the Calibration Tone applied to all
	/// 4 channels
	///
	/// \since 1.4.3
		bool SetEnhancedToneMode(const SweepTone& tone_ch1, const SweepTone& tone_ch2, const SweepTone& tone_ch3, const SweepTone& tone_ch4);
	///
	/// \brief Synthesiser Output Channel Tone with more enhanced functionality than the Calibration Tone applied to 1
	/// channel only
	///
	/// \since 1.4.3
		bool SetEnhancedToneChannel(const RFChannel& chan, const SweepTone& tone);
	///
	/// \brief Stops Enhanced Tone output. Returns to Image mode control.
	/// \since 1.4.3
		bool ClearEnhancedToneMode();
	///
	/// \brief Stops Enhanced Tone output on one channel.
	/// \since 1.4.3
		bool ClearEnhancedToneChannel(const RFChannel& chan);
	//@}

		/// \name Synchronous Output Control
	//@{
	///
	/// \brief Selects the source of data for the 2 Analog and 12 Digital output signals that operate synchronously with the Image Pixel Clock
	/// \since 1.1
		bool AssignSynchronousOutput(const SYNC_SINK& sink, const SYNC_SRC& src) const;
	///
	/// \brief Configures the Synchronous Digital Output data
	///
	/// Synchronous Digital output data is usually time aligned with the update of RF Channel data output and remains
	/// valid for the duration of the image pixel clock period.  There are two options to this:
	///
	/// (1) The assertion of Synchronous Digital output data can be delayed with respect to the RF signal by any
	/// number of nanoseconds that is less than 655360ns and has a minimum resolution of 10ns.
	/// (2) The synchronous digital output bits can be set to "pulse mode" - they return to inactive after a 
	/// defined time period.  The period may be any number of nanoseconds that is less than 655360ns and has
	/// a minimum resolution of 10ns. A setting of 0ns disables pulsed mode - all outputs are level out.
	/// \param[in] delay the number of nanoseconds to delay the onset of synchronous digital output data
	/// \param[in] pulse_length the width of the digital output data pulse (or zero to disable)
	/// \return true if the syncronous digital output data configuration request was sent successfully
	/// \since 1.4
		bool ConfigureSyncDigitalOutput(::std::chrono::nanoseconds delay = ::std::chrono::nanoseconds::zero(),
			::std::chrono::nanoseconds pulse_length = ::std::chrono::nanoseconds::zero());

	///
	/// \brief Sets the polarity of the Synchronous Digital outputs
	///
	/// The iMS digital outputs can be configured for active high or active low polarity.
	/// If output inversion is enabled (invert = true), the digital output is asserted low when the associated
	/// synchronous digital bit = 1 and asserted high when the sync digital bit = 0.  This is the default behaviour
	/// for compatibility with earlier iMS hardware with fixed inverting optocouled outputs.
	/// If the inversion is disabled (invert = false), the digital output follows the polarity of the sync
	/// digital bit, i.e. sync dig bit = 0, output = low (GND); sync dig bit = 1, output = high (VCC).
	/// \param[in] invert true = hardware output opposite sense to programmed bit.  false = hardware follows software bit
	/// \return true if the syncronous digital output inversion request was sent successfully
	/// \since 1.8.8
		bool SyncDigitalOutputInvert(bool invert = true);

	///
	/// \brief Sets the assertion mode of the Synchronous Digital outputs
	///
	/// The synchronous digital outputs can be set for pulsed or level mode.  In pulsed mode, if the software bit = 1,
	/// the output is asserted for the pulse length (see ConfigureSyncDigitalOutput for how to set the pulse length), then
	/// returns to deasserted.  In level mode, the output signal remains asserted or deasserted for the duration of the 
	/// image point.
	///
	/// With iMS rev D hardware fw ver 4.1.129+, individual synchronous output bits can be set independently.  Specify
	/// in 'index' which bit to change the mode for.  If index is not specified, all bits have their mode changed
	/// \param[in] mode Set to PULSED or LEVEL to configure the output mode
	/// \param[in] index Select a bit (0-11) to change the mode for, or INT_MAX for all bits
	/// \return true if the syncronous digital output mode request was sent successfully
	/// \since 1.8.8
		bool SyncDigitalOutputMode(SYNC_DIG_MODE mode, int index = INT_MAX);
	//@}

	///
	/// \name Local Tone Buffer Functions
	//@{
	///
	/// \brief Use these functions to output tones from the Local Tone Buffer, control their selection and compensation.
	///
	/// The Local Tone Buffer in the Synthesiser stores a set of 256 TBEntry's, each comprising of a FAP per each of the 4 output channels.
	/// The LTB can be inserted in the Synthesiser output signal path, replacing the Image data deriving from a connected Controller.
	/// Multiple ToneBuffers can be stored in Synthesiser non-volatile memory and any one of these can be recalled by host software
	/// and if one of them is marked with the filesystem 'default' flag, it will be loaded into the LTB at startup causing the Signal
	/// Path to be routed to the LTB.
	///
	/// In order to determine which method is used to provide the tone index for the LTB (Host Software, External 16-entry and
	/// External 256-entry), update the LTB buffer using one of the methods containing a ToneBufferControl parameter.
	///
	/// In order to change the currently selected LTB index (only in Host Software control mode), use one of the methods
	/// containing the index parameter.
	///
	/// The LTB outputs may be injected into the Synthesiser signal path either before or after the CompensationTable Look-Up Table.
	/// If before (true), amplitude compensation is applied to the signal amplitudes, if after (false), use the methods containing
	/// the AmplitudeCompensation parameter.
	/// \param[in] tbc Select LTB Control Source
	/// \param[in] AmplitudeComp indicates whether to apply LUT Compensation to Tone amplitude data
	/// \param[in] PhaseComp indicates whether to apply LUT Compensation to Tone phase data
	/// \param[in] index In Host Software control mode, select which LTB index to use
	/// \return true if update was successful
	/// \since 1.1
		bool UpdateLocalToneBuffer(const ToneBufferControl& tbc, const unsigned int index
			, const SignalPath::Compensation AmplitudeComp = SignalPath::Compensation::ACTIVE
			, const SignalPath::Compensation PhaseComp = SignalPath::Compensation::ACTIVE);
	/// \overload
		bool UpdateLocalToneBuffer(const ToneBufferControl& tbc);
	/// \overload
		bool UpdateLocalToneBuffer(const SignalPath::Compensation AmplitudeComp, const SignalPath::Compensation PhaseComp);
	/// \overload
		bool UpdateLocalToneBuffer(const unsigned int index);
	//@}

	///
	/// \name Velocity / Encoder Compensation Functions
	///
	/// Some iMS Synthesisers include dual optical encoder inputs and built in tracking filters that can be used to monitor
	/// the velocity of a moving object in two dimensions, compensate the RF frequency by a scaled amount to alter the 
	/// AOD deflection angle and hence remove distortion from the target feature.
	///
	/// Each of the 2 encoder inputs has a pair of RS422 receivers and can be configured to work with both quadrature
	/// (for best precision) and clock + direction style encoder signals.  The encoder inputs are passed through a 
	/// glitch filter to remove any excursions < 30ns before being decoded to extract a pulse train and to identify
	/// direction of travel.
	///
	/// This information is fed into a tracking loop filter that both attenuates noise from the signal and calculates
	/// an estimate for the encoder velocity (in encoder ticks per second).  The filter has a number of parameters
	/// that can be adjusted for optimum performance.  The transfer function of the filter is:
	///
	///   H(s) = ((kp / I.ki).s + 1) / ( (1 / I.ki).s^2 + (kp / I.ki).s + 1)
	///
	/// where:
	///  \li kp = the proportion gain coefficient
	///  \li ki = the integral gain coefficient
	///  \li I = a constant correction factor =  65535 / 687 = 95.393
	///  \li s = the Laplace operator
	///
	/// The resulting X and Y velocity estimates are applied to the pixel subsystem where they are scaled by a gain 
	/// coefficient and used to offset the RF channel output frequency from the value requested by Image data,
	/// Single Tone or Tone Buffer.  The offset is applied as follows:
	///
	/// \li (1) If X/Y Phase compensation is enabled (see EnableXYPhaseCompensation), offsets from Encoder input X are
	/// applied to RF Channels 1 and 2, offsets from Encoder input Y are applied to RF Channels 3 and 4.
	/// \li (2) If X/Y Phase compensation is not enabled, offsets from Encoder input X are applied to all RF Channels and
	/// Encoder input Y is ignored.
	///
	/// Note that negative gains are allowed which result in frequency offsets in the opposite direction.
	//@{
	/// \brief UpdateEncoder enables the Encoder velocity offset correction and updates the parameters
	///
	/// Calling this function will enable the velocity correction capability of the Synthesiser or update the
	/// parameters of the velocity correction according to the values in the VelocityConfiguration struct
	/// \param[in] velcomp Contains the values with which to configure the Velocity Correction process
	/// \return true if the Encoder Update request was sent successfully
	/// \since 1.4
		bool UpdateEncoder(const VelocityConfiguration& velcomp);
	/// \brief Turns off the Velocity Compensation process
	/// \since 1.4
		bool DisableEncoder();
	/// \brief Retrieves the current angular velocity of the requested encoder channel
	/// 
	/// Whilst enabled, the encoder inputs are continuously monitored for activity and any movement is converted
	/// by the tracking loop filter into an estimate of velocity in number of encoder ticks per second.  Note that
	/// for a quadrature encoder, a single tick is defined as an edge of either type (rising or falling) on either
	/// signal input, to guarantee maximum possible resolution, thus there are 4 ticks to a single pulse on one
	/// signal input.
	///
	/// This function allows application software to request the current velocity estimate of either encoder channel.
	/// The result is reported to the software in the SignalPathEvents::ENC_VEL_CH_X and ENC_VEL_CH_Y events.
	/// \param[in] chan which of the two encoder channels to request the velocity from (X or Y).
	/// \return true if the encoder velocity report request was sent successfully
	/// \since 1.4
		bool ReportEncoderVelocity(ENCODER_CHANNEL chan);
	//@}

	///
	/// \name Static Frequency Offsets
	//@{
	/// 
	/// \brief Set a positive frequency offset to an RF Channel
	///
	/// Sometimes it can be useful to apply a static frequency offset to a channel or range of channels in the Synthesiser
	/// output.  This allows the software to modify the region of the AOD scan which an Image will use without having to
	/// regenerate and download the image again.
	///
	/// \param[in] offset The amount of frequency offset to apply to the channel.  The offset increases the channel's frequency output
	/// \param[in] chan Which RF channel to apply the frequency offset to
	/// \return true if the command was sent successfully
	/// \since 1.8.0
		bool AddFrequencyOffset(MHz& offset, RFChannel chan = RFChannel::all);
	///
	/// \brief Set a negative frequency offset to an RF Channel
	///
	/// Sometimes it can be useful to apply a static frequency offset to a channel or range of channels in the Synthesiser
	/// output.  This allows the software to modify the region of the AOD scan which an Image will use without having to
	/// regenerate and download the image again.
	///
	/// \param[in] offset The amount of frequency offset to apply to the channel.  The offset reduces the channel's frequency output
	/// \param[in] chan Which RF channel to apply the frequency offset to
	/// \return true if the command was sent successfully
	/// \since 1.8.0
		bool SubtractFrequencyOffset(MHz& offset, RFChannel chan = RFChannel::all);
	//@}

    /// \name Event Notifications
    //@{
    ///
    /// \brief Subscribe a callback function handler to a given SignalPathEvents event
    ///
    /// SignalPath can callback user application code when an event occurs that affects the signal path.
    /// Supported events are listed under SignalPathEvents.  The
    /// callback function must inherit from the IEventHandler interface and override
    /// its EventAction() method.
    ///
    /// Use this member function call to subscribe a callback function to a SignalPathEvents event.
    /// For the period that a callback is subscribed, each time an event in SignalPath occurs
    /// that would trigger the subscribed SignalPathEvents event, the user function callback will be
    /// executed.
    /// \param[in] message Use the SignalPathEvents::Event enum to specify an event to subscribe to
    /// \param[in] handler A function pointer to the user callback function to execute on the event trigger.
    /// \since 1.0
		void SignalPathEventSubscribe(const int message, IEventHandler* handler);
    /// \brief Unsubscribe a callback function handler from a given SignalPathEvents event
    ///
    /// Removes all links to a user callback function from the Event Trigger map so that any
    /// events that occur in the SignalPath object following the Unsubscribe request
    /// will no longer execute that function
    /// \param[in] message Use the SignalPathEvents::Event enum to specify an event to unsubscribe from
    /// \param[in] handler A function pointer to the user callback function that will no longer execute on an event
    /// \since 1.0
		void SignalPathEventUnsubscribe(const int message, const IEventHandler* handler);
    //@}
	private:
		// Makes this object non-copyable
		SignalPath(const SignalPath &);
		const SignalPath &operator =(const SignalPath &);

		class Impl;
		Impl * p_Impl;
	};

	/// \struct VelocityConfiguration
	/// \brief Sets the parameters required to control the operation of the Encoder Input / Velocity Compensation function
	/// 
	/// Holds parameters for the Encoder type (Quadrature or Clk/Dir), Velocity Estimation method, tracking loop filter
	/// parameters and overall output gain - being the amount of deviation applied to the RF frequency generation for a
	/// given encoder velocity.  Also contains a method for calculating the value of the gain parameter for a desired
	/// frequency deviation at a given encoder velocity.
	/// \since 1.4
	struct LIBSPEC VelocityConfiguration
	{
		/// Sets the type of encoder signal connected to the Synthesiser inputs
		SignalPath::ENCODER_MODE EncoderMode { SignalPath::ENCODER_MODE::QUADRATURE };
		/// Sets the velocity calculation method used in the tracking filter for frequency compensation
		SignalPath::VELOCITY_MODE VelocityMode { SignalPath::VELOCITY_MODE::FAST };
		/// The Proportion Coefficient (0 - 65535) used in the Tracking Loop Filter
		std::uint16_t TrackingLoopProportionCoeff { 4000 };
		/// The Integration Coefficient (0 - 65535) used in the Tracking Loop Filter
		std::uint16_t TrackingLoopIntegrationCoeff { 10000 };
		/// Controls the extent to which a given value of velocity causes a deviation in synthesiser frequency. Do not set manually, use SetVelGain.
		std::array<std::int16_t, 2> VelocityGain;

		/// \brief Sets the amount of frequency deviation gain applied to velocity measurement
		///
		/// Use this function to set the encoder channel gain according to the amount of desired frequency offset (deviation) at
		/// a chosen spot encoder angular frequency.
		/// \param[in] ims a const reference to the IMSSystem in use
		/// \param[in] chan Which channel (X or Y) to set the encoder gain for
		/// \param[in] EncoderFreq The encoder tick frequency for which we shall define the gain
		/// \param[in] DesiredFreqDeviation The amount of change to the RF Frequency that shall be offset when the encoder is operating at the specified velocity
		/// \param[in] Reverse Causes the RF frequency deviation to effect in the opposite direction
		void SetVelGain(std::shared_ptr<IMSSystem> ims, SignalPath::ENCODER_CHANNEL chan, kHz EncoderFreq, MHz DesiredFreqDeviation, bool Reverse = false);

		VelocityConfiguration() : VelocityGain({ { 500, 500 } }) {}
	};
}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
