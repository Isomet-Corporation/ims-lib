/*-----------------------------------------------------------------------------
/ Title      : Auxiliary Functions Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Auxiliary/h/Auxiliary.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2018-01-28 23:21:45 +0000 (Sun, 28 Jan 2018) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 315 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2015 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Description
/ 2015-04-09  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

///
/// \file Auxiliary.h
///
/// \brief Classes for performing various auxiliary actions not directly related to driving Acousto-Optic devices
///
/// There are a number of additional functions provided by the Synthesiser which may be used to facilitate
/// integration of the iMS device into the overall system.  These features are not fundamental to the
/// operation of the iMS device which is why they are held in a separate 'Auxiliary' file.
///
/// Features include: 
/// \li assignment of LEDs to indicate specific events
/// \li Reading one of the two external analog inputs
/// \li Writing to the external analog output
/// \li Controlling the 4-bit Profile select signal driving the DDS Synthesiser IC (software control or externally provided)
/// \li Advanced manual control of register contents written to the DDS Synthesiser IC.
///
/// \author Dave Cowan
/// \date 2016-02-18
/// \since 1.1
/// \ingroup group_Aux
///

#ifndef IMS_AUXILIARY_H__
#define IMS_AUXILIARY_H__

#include "IEventHandler.h"
#include "IMSSystem.h"
#include "FileSystem.h"

#include <memory>
#include <map>
#include <initializer_list>
#include <deque>

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
	/// \class AuxiliaryEvents Auxiliary.h include\Auxiliary.h
	/// \brief All the different types of events that can be triggered by the Auxiliary class.
	///
	/// Some events contain floating point parameter data which can be processed by the IEventHandler::EventAction
	/// derived method
	/// \author Dave Cowan
	/// \date 2016-02-11
	/// \since 1.1
	class LIBSPEC AuxiliaryEvents
	{
	public:
		/// \enum Events List of Events raised by the Auxiliary module
		enum Events {
			/// Previous Analog Input Update request completed; data available to be read
			EXT_ANLG_UPDATE_AVAILABLE,
			/// Previous Analog Input Update request completed; request failed
			EXT_ANLG_READ_FAILED,
			Count
		};
	};

	///
	/// \class Auxiliary Auxiliary.h include\Auxiliary.h
	/// \brief Provides auxiliary additional functions not directly related to Synthesiser operation
	///
	/// \author Dave Cowan
	/// \date 2016-02-18
	/// \since 1.1
	class LIBSPEC Auxiliary
	{
	public:
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for Auxiliary Object
		///
		/// An IMSSystem object, representing the configuration of an iMS target must be passed by const
		/// reference to the Auxiliary constructor.
		///
		/// The IMSSystem object must exist before the Auxiliary object, and must remain valid (not
		/// destroyed) until the Auxiliary object itself is destroyed.
		///
		/// Once constructed, the object can neither be copied or assigned to another instance.
		///
		/// \param[in] ims A const reference to the iMS System
		/// \since 1.1
		Auxiliary(std::shared_ptr< IMSSystem > ims);
		///
		/// \brief Destructor for Auxiliary Object
		~Auxiliary();
		//@}

		/// \enum LED_SOURCE
		/// \brief Selects the function to be assigned to an LED
		/// \since 1.1
		enum class LED_SOURCE : std::uint16_t
		{
			/// \brief LED turned off
			OFF = 0,
			/// \brief LED turned on
			ON = 1,
			/// \brief LED slowly pulses
			PULS = 2,
			/// \brief LED slowly pulses with opposite phase to PULS
			NPULS = 3,
			/// \brief Illuminates whenever there is activity on the Pixel Interface between Controller and Synthesiser
			PIXEL_ACT = 4,
			/// \brief Illuminates whenever serial communications activity is detected
			CTRL_ACT = 5,
			/// \brief Illuminates when communications is in a normal condition
			COMMS_HEALTHY = 6,
			/// \brief Illuminates when Communications Healthy state has detected a timeout (no message received within healthy comms window)
			COMMS_UNHEALTHY = 7,
			/// \brief Illuminates when RF Gate to power amplifier is enabled and interlock is not set
			RF_GATE = 8,
			/// \brief Illuminates when interlock is active (overtemperature, user disabled or no connection to amplifier/acoust-optic device)
			INTERLOCK = 9,
			/// \brief Illuminates when external equipment is turned on by user
			LASER = 10,
			/// \brief Illuminates when a checksum error is detected on the pixel interface between Controller and Synthesiser (remains on until cleared in software)
			CHECKSUM = 11,
			/// \brief Illuminates when iMS system is overtemperature or a fan has failed
			OVERTEMP = 12,
			/// \brief Illuminates when master clock circuit PLL is locked (either to internal TCXO or externally supplied reference)
			PLL_LOCK = 13,
            /// \brief Illuminates when image playback is active
            ACTV = 14,
            /// \brief Illuminates when image playback is not active
            IDLE = 15
		};

		/// \enum LED_SINK
		/// \brief Which LED to assign function to
		/// \since 1.1
		enum class LED_SINK
		{
			/// Synthesiser Green LED
			GREEN,
			/// Synthesiser Yellow LED
			YELLOW,
			/// Synthesiser Red LED
			RED
		};

		/// \enum DDS_PROFILE
		/// \brief Control Source for Profile input to DDS Synthesiser IC
		/// \since 1.1
		enum class DDS_PROFILE : std::uint16_t
		{
			/// Profile Selection disabled (default)
			OFF = 0,
			/// Profile can be controlled from external signal pin inputs
			EXTERNAL = 16,
			/// Profile can be controlled from user application software
			HOST = 32
		};

		/// \enum EXT_ANLG_INPUT
		/// \brief Reference enum for addressing both analog inputs
		/// \since 1.1
		enum class EXT_ANLG_INPUT
		{
			/// Refer to analog input A
			A,
			/// Refer to analog input B
			B
		};

		/// \name LEDs
		//@{
		/// \brief Assignment function for LEDs
		///
		/// Provide two inputs indicating which LED to target and what function to assign to it.
		/// \param[in] sink Which LED to target
		/// \param[in] src the function that the LED should now perform
		/// \return true if the assignment request was sent successfully
		/// \since 1.1
		bool AssignLED(const LED_SINK& sink, const LED_SOURCE& src) const;
		//@}

		/// \name DDS Profile Control
		//@{
		/// \brief Control the DDS Profile feature
		///
		/// The DDS IC used at the heart of the Synthesiser has a 4-wide signal input that can be used for
		/// modulation (FSK, PSK, ASK), to start/stop the sweep accumulators or used to ramp up/ramp down the
		/// output amplitude.  By default, the feature is disabled but this function can be used to set the
		/// control source for the profile signal either to external for hardware selection or to host for
		/// software selection
		/// \param[in] prfl select the profile pin control source
		/// \return true if the profile control soruce request was sent successfully
		bool SetDDSProfile(const DDS_PROFILE& prfl) const;
		/// \overload 
		/// \param[in] select chooses the profile signal value to provide when driven from software
		/// \param[in] prfl select the profile pin control source
		bool SetDDSProfile(const DDS_PROFILE& prfl, const std::uint16_t& select) const;
		//@}

		/// \name External Analog I/O
		//@{
		/// \brief Instructs the synthesiser to capture the current value of both the external analog inputs
		///
		/// There are 2 external analog input sources which can provide an auxiliary measurement of external
		/// signal data to user software for example to monitor environmental data.
		///
		/// Call this function to initiate a measurement conversion.  Once completed, the results will be returned 
		/// to user code by a callback with Event EXT_ANLG_UPDATE_AVAILABLE.  The callback handler can then read
		/// the conversion results from the GetAnalogData() function.
		/// \return true if the conversion request was sent successfully.
		/// \since 1.1
		bool UpdateAnalogIn();
		/// \brief Returns the analog measurements read by the conversion triggered by a call to UpdateAnalogIn()
		///
		/// \return a std::map containing one entry for each analog input to the Synthesiser.  The value associated
		/// with each entry in the map is returned as a percentage object where 100% represents the full scale
		/// analog voltage (typically 10.0V)
		/// \since 1.1
		const std::map<EXT_ANLG_INPUT, Percent>& GetAnalogData() const;
		/// \brief Instructs the synthesiser to update the analog output value provided externally
		///
		/// There is a single channel of analog output data which may provide an auxiliary analog signal to the
		/// external signal for example to indicate some internal system parameter state.
		/// \param[in] pct The percentage value to output where 100% represents full scale analog voltage (typ. 10.0V)
		/// \return true if the update request was sent successfully
		bool UpdateAnalogOut(Percent& pct) const;
		//@}

		/// \name Event Notifications
		//@{
		///
		/// \brief Subscribe a callback function handler to a given AuxiliaryEvents event
		///
		/// Auxiliary can callback user application code when an event occurs that affects the signal path.
		/// Supported events are listed under AuxiliaryEvents.  The
		/// callback function must inherit from the IEventHandler interface and override
		/// its EventAction() method.
		///
		/// Use this member function call to subscribe a callback function to a AuxiliaryEvents event.
		/// For the period that a callback is subscribed, each time an event in Auxiliary occurs
		/// that would trigger the subscribed AuxiliaryEvents event, the user function callback will be
		/// executed.
		/// \param[in] message Use the AuxiliaryEvents::Event enum to specify an event to subscribe to
		/// \param[in] handler A function pointer to the user callback function to execute on the event trigger.
		/// \since 1.1
		void AuxiliaryEventSubscribe(const int message, IEventHandler* handler);
		/// \brief Unsubscribe a callback function handler from a given AuxiliaryEvents event
		///
		/// Removes all links to a user callback function from the Event Trigger map so that any
		/// events that occur in the Auxiliary object following the Unsubscribe request
		/// will no longer execute that function
		/// \param[in] message Use the AuxiliaryEvents::Event enum to specify an event to unsubscribe from
		/// \param[in] handler A function pointer to the user callback function that will no longer execute on an event
		/// \since 1.1
		void AuxiliaryEventUnsubscribe(const int message, const IEventHandler* handler);
		//@}
	private:
		// Make this object non-copyable
		Auxiliary(const Auxiliary &);
		const Auxiliary &operator =(const Auxiliary &);

		// Declare Implementation
		class Impl;
		Impl * p_Impl;
	};

	///
	/// \class DDSScriptRegister Auxiliary.h include\Auxiliary.h
	/// \brief Create a register write to send to the DDS IC
	/// 
	/// The DDS IC that generates RF signals on the Synthesiser can be manually programmed to access advanced
	/// features that wouldn't normally be available through the iMS API.  To do this requires a knowledge
	/// and understanding of the Analog Devices AD9959 Frequency Synthesiser IC and its register map.
	///
	/// If it is decided that it is necessary to manually program the AD9959, a sequence of register writes
	/// can be generated (called a DDS Script) and stored in the Synthesiser Filesystem.  The application
	/// software may then recall the script from the filesystem and execute it to commit the register
	/// writes to the AD9959.
	///
	/// Each individual register write is an invocation of the DDSScriptRegister class.  The class object consists
	/// of a key-value pair where the key is the name of the register to access (corresponding to the register
	/// abbreviation in the datasheet) and the value is a list of bytes to transfer to the AD9959 following the
	/// register command.  
	///
	/// There must be the exact number of data bytes sent after the register command as specified in the datasheet.
	/// The class knows internally what this number is and enforces it, so that any extra bytes are truncated and
	/// any missing are zero-filled.
	///
	/// Note that the bottom four bits of data sent to the CSR register cannot be overwritten since they define
	/// the hardware interface to the register access port.
	///
	/// Some of the register writes do not take effect until a signal line called Update Clock is asserted to
	/// the AD9959.  This can be triggered by creating a DDSScriptRegister object with the Name property set to
	/// UPDATE.  It takes no byte data as input.
	/// \author Dave Cowan
	/// \date 2016-03-01
	/// \since 1.1
	class LIBSPEC DDSScriptRegister
	{
	public:
		/// \enum Name
		/// \brief the abbreviated register name for each register accessible in the DDS IC
		/// \since 1.1
		enum class Name : std::uint8_t
		{
			/// Channel Select Register
			CSR = 0,
			/// Function Register 1
			FR1 = 1,
			/// Function Register 2
			FR2 = 2,
			/// Channel Function Register
			CFR = 3,
			/// Channel Frequency Tuning Word
			CFTW0 = 4,
			/// Channel Phase Offset Word
			CPOW0 = 5,
			/// Amplitude Control Register
			ACR = 6,
			/// Linear Sweep Ramp Rate
			LSRR = 7,
			/// LSR Rising Delta Word
			RDW = 8,
			/// LSR Falling Delta Word
			FDW = 9,
			/// Channel Word 1
			CW1 = 10,
			/// Channel Word 2
			CW2 = 11,
			/// Channel Word 3
			CW3 = 12,
			/// Channel Word 4
			CW4 = 13,
			/// Channel Word 5
			CW5 = 14,
			/// Channel Word 6
			CW6 = 15,
			/// Channel Word 7
			CW7 = 16,
			/// Channel Word 8
			CW8 = 17,
			/// Channel Word 9
			CW9 = 18,
			/// Channel Word 10
			CW10 = 19,
			/// Channel Word 11
			CW11 = 20,
			/// Channel Word 12
			CW12 = 21,
			/// Channel Word 13
			CW13 = 22,
			/// Channel Word 14
			CW14 = 23,
			/// Channel Word 15
			CW15 = 24,
			/// Issue Update Clock (pseudo register write)
			UPDATE = 64
		};

		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for DDSScriptRegister Object
		///
		/// Example:
		/// \code
		/// DDSScriptRegister reg5(DDSScriptRegister::Name::UPDATE);
		/// \endcode
		/// \param[in] name Create the register object accessing the specified Register
		/// \since 1.1
		DDSScriptRegister(Name name = Name::CSR);
		/// \overload
		/// Example:
		/// \code
		/// DDSScriptRegister reg2(DDSScriptRegister::Name::CFTW0, { 0x33, 0x33, 0x33, 0x33 });
		/// \endcode
		/// \param[in] name Create the register object accessing the specified Register
		/// \param[in] data initialise the data byte array from this input field
		DDSScriptRegister(Name name, const std::initializer_list<std::uint8_t>& data);
		/// \overload
		/// \param[in] name Create the register object accessing the specified Register
		/// \param[in] data initialise the data byte array from queue variable
		DDSScriptRegister(Name name, const std::deque<std::uint8_t>& data);
		/// \brief Copy Constructor
		DDSScriptRegister(const DDSScriptRegister &);
		/// \brief Assignment Constructor
		DDSScriptRegister &operator =(const DDSScriptRegister &);
		/// \brief Destructor
		~DDSScriptRegister();
		//@}

		/// \brief Add an additional byte to the end of the data array
		/// \return the new number of bytes in the array
		/// \since 1.1
		int append(const std::uint8_t&);

		/// \brief Get the full byte array for programming to the FileSystem
		/// Shouldn't be called in user code
		std::vector<std::uint8_t> bytes() const;

	private:
		// Declare Implementation
		class Impl;
		Impl* p_Impl;
	};

	/// \brief \c DDSScript stores the sequence of register writes to be loaded onto the Synthesiser. Can be manipulated
	/// using the normal container operations provided by std::vector
	using DDSScript = std::vector<DDSScriptRegister>;

	///
	/// \class DDSScriptDownload Auxiliary.h include\Auxiliary.h
	/// \brief Provides a mechanism for transferring DDS Scripts into Filesystem memory
	/// 
	/// Use this class to program newly created DDS Scripts to the Synthesiser.  The class will automatically
	/// find and allocate space in the filesystem and update the filesystem table with the newly created entry.
	///
	/// Setting the FileDefault flag to DEFAULT will cause the downloaded script to be executed at every
	/// subsequent powerup.
	///
	/// Use the FileSystemManager class for any additional actions as required, such as setting/clearing default
	/// flags, executing scripts and deleting unwanted scripts.
	/// \author Dave Cowan
	/// \date 2016-03-01
	/// \since 1.1
	class LIBSPEC DDSScriptDownload
	{
	public:
		/// \brief Construct the DDSScriptDownload object from a reference to the iMS device and a const reference to the DDS Script
		/// to download
		/// \param[in] ims the iMS target System
		/// \param[in] script the DDSScript to download
		DDSScriptDownload(std::shared_ptr<IMSSystem> ims, const DDSScript& script);
		/// \brief DDSScriptDownload destructur
		~DDSScriptDownload();

		/// \brief Causes the DDS Script object to be programmed into the filesystem
		///
		/// Calculates the amount of storage space required, finds a space large enough and transfers the script byte
		/// data to be stored at the selected location in Synthesiser non-volatile memory.  The new entry is logged
		/// in the Filesystem table (FST) along with the default flag, if set.
		/// \param[in] FileName a max 8 char string to use to refer to the DDS Script (stored in the FST)
		/// \param[in] def Optional parameter indicating whether to set the default flag for future startup execution
		/// \return the index of the script in the FST (or -1 if programming failed, e.g. insufficient space or no free FST entries)
		/// \since 1.1
		const FileSystemIndex Program(const std::string& FileName, FileDefault def = FileDefault::NON_DEFAULT) const;
	private:
		// Make this object non-copyable
		DDSScriptDownload(const DDSScriptDownload &);
		const DDSScriptDownload &operator =(const DDSScriptDownload &);

		// Declare Implementation
		class Impl;
		Impl* p_Impl;
	};
}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
