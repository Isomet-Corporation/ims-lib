/*-----------------------------------------------------------------------------
/ Title      : Firmware Upgrade Functions Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg.qytek.lan/svn/sw/trunk/09-Isomet/iMS_SDK/API/FirwmareUpgrade/h/FirwmareUpgrade.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2018-11-13
/ Last update: $Date: 2018-01-28 23:21:45 +0000 (Sun, 28 Jan 2018) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 315 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2018 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Description
/ 2015-04-09  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

///
/// \file FirmwareUpgrade.h
///
/// \brief Classes for upgrading the unit embedded firmware
///
/// \author Dave Cowan
/// \date 2018-11-13
/// \since 1.6.0
/// \ingroup group_Upgrade
///

#ifndef IMS_FWUPGRADE_H__
#define IMS_FWUPGRADE_H__

#include "IEventHandler.h"
#include "IMSSystem.h"

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
	/// \class FirmwareUpgradeEvents FirmwareUpgrade.h include\FirmwareUpgrade.h
	/// \brief All the different types of events that can be triggered by the FirmwareUpgrade class.
	///
	/// \author Dave Cowan
	/// \date 2018-11-13
	/// \since 1.6.0
	class LIBSPEC FirmwareUpgradeEvents
	{
	public:
		/// \enum Events List of Events raised by the FirmwareUpgrade module
		enum Events {
			/// \brief Triggered when the upgrade process completes
			FIRMWARE_UPGRADE_DONE,
			/// \brief Triggered if there is an error in the upgrade process
			FIRMWARE_UPGRADE_ERROR,
			/// \brief Triggered immediately after the upgrade has started
			FIRMWARE_UPGRADE_STARTED,
			/// \brief Triggered when the upgrade process has completed initialising
			FIRMWARE_UPGRADE_INITIALIZE_OK,
			/// \brief Triggered when the upgrade process has successfully identified the Flash ID Code
			FIRMWARE_UPGRADE_CHECKID_OK,
			/// \brief Triggered on Starting the Upgrade Process (any subsequent failure would result in the device booting from its golden image)
			FIRMWARE_UPGRADE_ENTER_UG_MODE,
			/// \brief Triggered when the Flash program area has been erased
			FIRMWARE_UPGRADE_ERASE_OK,
			/// \brief Triggered when the Flash has completed downloading the new firmware image
			FIRMWARE_UPGRADE_PROGRAM_OK,
			/// \brief Triggered when the Flash has verified the CRC of the downloaded image
			FIRMWARE_UPGRADE_VERIFY_OK,
			/// \brief Triggered on completing the upgrade process (the device will now boot from the freshly downloaded image)
			FIRMWARE_UPGRADE_LEAVE_UG_MODE,
			Count
		};
	};

	///
	/// \class FirmwareUpgradeProgress FirmwareUpgrade.h include\FirmwareUpgrade.h
	/// \brief 
	///
	/// The firmware upgrade process has a defined sequence of events.  This class reports to the user
	/// which of the steps have been completed.
	///
	/// \author Dave Cowan
	/// \date 2018-11-13
	/// \since 1.6.0
	class LIBSPEC FirmwareUpgradeProgress
	{
	public:
		/// \brief Constructor used internally by library code
		FirmwareUpgradeProgress(std::uint8_t);

		/// \brief Returns true when the Upgrade Process has started
		bool Started() const;
		/// \brief Returns true when the upgrade process has completed initialising
		bool InitializeOK() const;
		/// \brief Returns true when the upgrade process has successfully identified the Flash ID COde
		bool CheckIdOK() const;
		/// \brief Returns true after entering upgrade mode (any subsequent failure would result in the device booting from its golden image)
		bool EnterUpgradeModeOK() const;
		/// \brief Returns true when the Flash program area has been erased
		bool EraseOK() const;
		/// \brief Returns true when the Flash has completed downloading the new firmware image
		bool ProgramOK() const;
		/// \brief Returns true when the Flash has verified the CRC of the downloaded image
		bool VerifyOK() const;
		/// \brief Returns true on leaving upgrade mode (the device will now boot from the freshly downloaded image)
		bool LeaveUpgradeModeOK() const;
	private:
		std::uint8_t progress_code;
	};

	///
	/// \class FirmwareUpgradeError FirmwareUpgrade.h include\FirmwareUpgrade.h
	/// \brief 
	///
	/// If the firmware upgrade process fails, it can fail for a number of reasons.  These can be 
	/// reported back to the user and this class decodes the error status for user consumption.
	///
	/// \author Dave Cowan
	/// \date 2018-11-13
	/// \since 1.6.0
	class LIBSPEC FirmwareUpgradeError
	{
	public:
		/// \brief Constructor used internally by library code
		FirmwareUpgradeError(std::uint8_t);

		/// \brief Returns true if the Firmware upgrade process was unable to correctly parse upgrade data from the supplied upgrade stream
		bool StreamError() const;
		/// \brief Returns true if the firmware upgrade was unable to identify the Flash device
		bool IdCode() const;
		/// \brief Returns true if there was a failure during the Erase portion of the ugprade cycle
		bool Erase() const;
		/// \brief Returns true if there was a failure during the Programming portion of the upgrade cycle
		bool Program() const;
		/// \brief Returns true if there was a communications timeout between the device processor and its attached Flash device
		bool TimeOut() const;
		/// \brief Returns true if on verifying the new firmware downloaded to Flash, the CRC did not match the expected value
		bool Crc() const;
	private:
		std::uint8_t error_code;
	};

	///
	/// \class FirmwareUpgrade FirmwareUpgrade.h include\FirmwareUpgrade.h
	/// \brief Provides functions for checking and upgrading firmware
	///
	/// Not all iMS devices support remote firmware upgrade.  Those that do will have their Capabilities struct
	/// RemoteUpgrade object set to true.
	///
	/// If a device supports remote firmware upgrade, it will have on-board Flash storage space for two firmware
	/// images.  The new image is downloaded to Flash and the device will only use it if the download process
	/// completes successfully.  On failure, the device will revert to a "golden" image which will allow subsequent
	/// upgrade attempts to be initiated.
	///
	/// \author Dave Cowan
	/// \date 2018-11-13
	/// \since 1.6.0
	class LIBSPEC FirmwareUpgrade
	{
	public:
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for FirmwareUpgrade Object
		///
		/// \param[in] ims A const reference to the iMS System
		/// \param[in] strm An std::istream object from which to obtain the new firmware  data, usually a std::ifstream from an upgrade file.
		/// THe file format needs to be in MCS format and should be opened in Text mode (not Binary).
		/// \since 1.6.0
		FirmwareUpgrade(std::shared_ptr<IMSSystem> ims, std::istream& strm);
		///
		/// \brief Destructor for FirmwareUpgrade Object
		~FirmwareUpgrade();
		//@}

		/// \enum UpgradeTarget
		/// \brief Of the attached device, select whether to upgrade the firmware of the Synthesiser or the Controller
		enum class UpgradeTarget {
			/// \brief Upgrade the Synthesiser Firmware
			SYNTHESISER,
			/// \brief Upgrade the Controller Firmware
			CONTROLLER
		};

		///
		/// \name Initiate Firmware Upgrade Functions
        //@{
		///
		/// \brief Begin the Upgrade process on either Synthesiser or Controller
		///
		/// This function will start the upgrade process to the attached target.  The upgrade process will continue
		/// after this function returns - it is non-blocking.  Upgrade progress can be monitored by referring to
		/// the upgrade monitoring functions
		/// \param[in] target Whether to upgrade Synthesiser or Controller
		/// \return true if the upgrade process started successfully
        bool StartUpgrade(const UpgradeTarget target = UpgradeTarget::SYNTHESISER);
		///
		/// \brief Begin a Verify process on either Synthesiser or Controller
		///
		/// The firmware integrity is protected by a CRC32 value encoded into the Firmware storage area.
		/// This function begins an integrity check that reads back the firmware data and computes its
		/// CRC value to ensure the firmware has been downloaded correctly.
		/// \param[in] target Whether to verify Synthesiser or Controller firmware
		/// \return true if the verify process started successfully
        bool VerifyIntegrity(const UpgradeTarget target = UpgradeTarget::SYNTHESISER);
		//@}

		///
		/// \name Monitor Firmware Upgrade and Verify processes
		//@{
		///
		/// \brief Returns true if the upgrade process has completed (either success or failure)
		bool UpgradeDone() const;
		/// \brief Returns true if there was a failure during the upgrade process
		bool UpgradeError() const;
		/// \brief Returns the current position in the upgrade process flow
		FirmwareUpgradeProgress GetUpgradeProgress() const;
		/// \brief Returns an error code if the upgrade failed
		FirmwareUpgradeError GetUpgradeError() const;
		/// \brief Returns the number of bytes that have so far been transferred to the target
		uint32_t GetTransferLength() const;
		/// \brief Returns the total number of bytes that will be transferred to the target
		uint32_t GetTotalTransferLength() const;
		//@}
            
		/// \name Event Notifications
		//@{
		///
		/// \brief Subscribe a callback function handler to a given FirmwareUpgrade event
		///
		/// FirmwareUpgrade can callback user application code when an event occurs that affects the upgrade process.
		/// Supported events are listed under FirmwareUpgradeEvents.  The
		/// callback function must inherit from the IEventHandler interface and override
		/// its EventAction() method.
		///
		/// Use this member function call to subscribe a callback function to a FirmwareUpgradeEvents event.
		/// For the period that a callback is subscribed, each time an event in FirmwareUpgrade occurs
		/// that would trigger the subscribed FirmwareUpgradeEvents event, the user function callback will be
		/// executed.
		/// \param[in] message Use the FirmwareUpgradeEvents::Event enum to specify an event to subscribe to
		/// \param[in] handler A function pointer to the user callback function to execute on the event trigger.
		/// \since 1.1
		void FirmwareUpgradeEventSubscribe(const int message, IEventHandler* handler);
		/// \brief Unsubscribe a callback function handler from a given FirmwareUpgradeEvents event
		///
		/// Removes all links to a user callback function from the Event Trigger map so that any
		/// events that occur in the FirmwareUpgrade object following the Unsubscribe request
		/// will no longer execute that function
		/// \param[in] message Use the FirmwareUpgradeEvents::Event enum to specify an event to unsubscribe from
		/// \param[in] handler A function pointer to the user callback function that will no longer execute on an event
		/// \since 1.1
		void FirmwareUpgradeEventUnsubscribe(const int message, const IEventHandler* handler);
		//@}
	private:
		// Make this object non-copyable
		FirmwareUpgrade(const FirmwareUpgrade &);
		const FirmwareUpgrade &operator =(const FirmwareUpgrade &);

		// Declare Implementation
		class Impl;
		Impl * p_Impl;
	};

}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
