/*-----------------------------------------------------------------------------
/ Title      : IMS System Definitions Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/h/IMSSystem.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2023-11-24 10:35:07 +0000 (Fri, 24 Nov 2023) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 594 $
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
/// \file IMSSystem.h
///
/// \brief Classes within this group are used to store information about an iMS System and to Connect / Disconnect from it.
///
/// When a host system is scanned to find attached iMS Systems using ConnectionList::scan(), an
/// IMSSystem object is created for each system that it finds.  The system is then probed to discover
/// any Controllers and Synthesisers that belong to it, along with any Option boards that are attached
/// to the Synthesiser (e.g. Frequency doubling).  If an AO Deflector or Modulator is connected to the
/// Synthesiser and/or an RF Amplifier, it will also attempt to find out any information it can about
/// those devices.
///
/// Once done, the IMSSystem object is returned to the User application.  The User can read all of the
/// data that has been created about the iMS System that was discovered, including system structure,
/// capabilities, descriptions, model numbers, serial numbers and firmware versions.
///
/// Much of the data that is stored about an iMS System and its components is retrieved from a hardware
/// database which is crossreferenced by identity information read back from the hardware.  The
/// hardware database stored as a resource within the library object.
///
/// Because the iMS concept is modular in approach, there are many different configurations of an iMS
/// which must all be compatible with the iMS library.  Therefore, IMSSystem is vital to many functions
/// within the library to allow them to carry out their objectives according to the capabilities of
/// the attached hardware.  As a result, you will see many class constructors which require a const
/// reference to an IMSSystem object so that they have the information about hardware targetting
/// available.
///
/// Two further features of IMSSystem are important to notice: the ability to Connect to and Disconnect
/// from a system.  No functions can be carried out on an iMS System until it has been identified by
/// the connection scan, and a connection established by calling IMSSystem::Connect().
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_IMSSystem
///

#ifndef IMS_IMSSYSTEM_H__
#define IMS_IMSSYSTEM_H__

#include "IConnectionSettings.h"
#include "IMSTypeDefs.h"
#include "Containers.h"

#include <ctime>
#include <string>
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
	// Forward declaration required to allow IMSSystem class to know of the existence of a connection to the iMS
	class IConnectionManager;

	// Forward declaration required to allow IMSSystem class to know of the existence of a Synthesiser File System
	class FileSystemTable;

	// Forward declaration required to allow IMSSystem class to know of the existence of a Controller Image Table
	class ImageTable;

	///
    /// \class IMSOption IMSSystem.h include/IMSSystem.h
    /// \brief An iMS Synthesiser can support one iMS Option, which adds an additional hardware function
    /// to the capabilities of the Synthesiser.
    ///
    /// One example of an iMS Option is a Frequency Doubler, the iMS-FX2, which doubles the available
    /// range of frequencies reproducible by the Synthesiser RF output.
    ///
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC IMSOption
	{
	public:
    /// \cond IMS_OPTION_CONSTRUCTOR
		IMSOption();
		IMSOption(std::string&);
		~IMSOption();

		IMSOption(const IMSOption &);
		IMSOption &operator =(const IMSOption &);
    ///	\endcond
		/// \brief returns a const reference to the named identity of the option card.
		const std::string& Name() const;
	private:
		class Impl;
		Impl* p_Impl;
	};

    ///
    /// \struct FWVersion IMSSystem.h include/IMSSystem.h
    /// \brief Stores the version number of firmware running on iMS hardware
    ///
    /// Firmware version is always defined as 'M.m.r' where:
    ///   M = Major Version
    ///   m = Minor Version
    ///   r = Revision
    ///
    /// Revision increments continuously for each build of firmware that is created.  Major and
    /// Minor tags are only updated to mark an important release.
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	struct LIBSPEC FWVersion
	{
    /// \brief returns the Major firmware version number (or -1 if uninitialised)
		int major{ -1 };
    /// \brief returns the Minor firmware version number
		int minor{ 0 };
    /// \brief returns the firmware revision number
		int revision{ 0 };
    /// \brief returns a struct indicating the date on which the firmware was created
		struct std::tm build_date;

    /// \cond
    /// Constructors used by library internal functions
		FWVersion();
		FWVersion(const std::vector<std::uint16_t>&);
    /// \endcond

    /// \brief Use this operator overload to output to a console the firmware version in human-readable format
    ///
    /// For example:
    /// \code
    /// std::cout << " FW Version: " << myiMS->Ctlr().GetVersion() << std::endl;
    /// \endcode
    /// might print:
    /// \code
    /// FW Version: 1.0.23 Wed 23 September 2015 11:08 GMT
    /// \endcode
		friend LIBSPEC std::ostream& operator<< (std::ostream& stream, const FWVersion&);
	};

    ///
    /// \class IMSController IMSSystem.h include/IMSSystem.h
    /// \brief Stores Capabilities, Description, Model & Version Number of an iMS Controller
    ///
    /// An IMSController class is a member of the IMSSystem class and contains valid information about
    /// an iMS Controller if the ConnectionList::scan() function was able to successfully identify it.
    ///
    /// The fields that can be read back to describe the controller can be used in Application code
    /// to select between Controllers, display information about them or determine capabilities.
    /// The information is also used by internal library functions to correctly format data and messages
    /// that are sent to the hardware.
    ///
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC IMSController
	{
	public:

    ///
    /// \struct Capabilities IMSSystem.h include/IMSSystem.h
    /// \brief Returns information about the capabilities of the Controller hardware
    ///
    /// This struct is initialised during the Connection Scan process
    ///
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
		struct LIBSPEC Capabilities
		{
      /// \brief A Controller can have multiple Synthesiser interfaces.  This field reports how many there are (NOT necessarily how many Synthesisers are connected)
			int nSynthInterfaces{ 1 };
      /// \brief Some Controllers support a mechanism for transferring bulk Image data much faster than through the standard protocol
			bool FastImageTransfer{ false };
      /// \brief The maximum number of points that can be stored in a single Image downloaded to the Controller
			int MaxImageSize{ 4096 };
      /// \brief Indicates whether the Controller supports Image downloading and Image playback simultaneously
			bool SimultaneousPlayback{ false };
      /// \brief The maximum clock rate supported during Image playback
			Frequency MaxImageRate{ 250.0 };
	  /// \brief Indicates whether the Controller supports Remote Firmware Upgrade
			bool RemoteUpgrade{ false };
		};
    /// \cond IMS_CONTROLLER_CONSTRUCTORS
    /// Constructors used by library internal functions
		IMSController();
		IMSController(const std::string&, const std::string&, const Capabilities&, const FWVersion&, const ImageTable&);
		~IMSController();

		// Copy & Assignment Constructors
		IMSController(const IMSController &);
		IMSController &operator =(const IMSController &);
    /// \endcond

    /// \brief Returns the Capabilities structure for the Controller
    /// \since 1.0
		const Capabilities GetCap() const;
    /// \brief Returns a descriptive string for the Controller
    /// \since 1.0
		const std::string& Description() const;
    /// \brief Returns the short model number for the Controller
    /// \since 1.0
		const std::string& Model() const;
    /// \brief Returns the firmware version for the Controller
    /// \since 1.0
		const FWVersion& GetVersion() const;
	/// \brief Returns the Image Index Table for the Controller
	/// \since 1.2
		const ImageTable& ImgTable() const;
		/// \brief Returns true if the system scan successfully identified the Controller and initialised this Class
    /// \return true if the class contains valid data representing an attached iMS Controller
    /// \since 1.0
		const bool IsValid() const;
	/// \brief Returns a list of interfaces present on the Controller whose settings may be changed
	/// \return list of strings representing interface names
	/// \since 1.5.0
		const ListBase<std::string>& Interfaces() const;
	private:
		class Impl;
		Impl * p_Impl;
	};

    ///
    /// \class IMSSynthesiser IMSSystem.h include/IMSSystem.h
    /// \brief Stores Capabilities, Description, Model & Version Number of an iMS Synthesiser
    ///
    /// An IMSSynthesiser class is a member of the IMSSystem class and contains valid information about
    /// an iMS Synthesiser if the ConnectionList::scan() function was able to successfully identify it.
    ///
    /// The fields that can be read back to describe the Synthesiser can be used in Application code
    /// to select between Synthesisers, display information about them or determine capabilities.
    /// The information is also used by internal library functions to correctly format data and messages
    /// that are sent to the hardware.
    ///
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC IMSSynthesiser
	{
	public:

    ///
    /// \struct Capabilities IMSSystem.h include/IMSSystem.h
    /// \brief Returns information about the capabilities of the Synthesiser hardware
    ///
    /// This struct is initialised during the Connection Scan process
    ///
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
		struct LIBSPEC Capabilities
		{
      /// \brief The maximum clock rate supported during Image playback
            Frequency MaxImageRate{ 250.0 };
      /// \brief the Lowest RF output frequency that can be reproduced by the Synthesiser
			MHz lowerFrequency{ 0.0 };
      /// \brief the Highest RF output frequency that can be reproduced by the Synthesiser
			MHz upperFrequency{ 250.0 };
      /// \brief the internal bit representation of RF frequency data
			int freqBits{ 16 };
      /// \brief the internal bit representation of RF amplitude data
			int amplBits{ 10 };
      /// \brief the internal bit representation of RF phase data
			int phaseBits{ 12 };
      /// \brief the power-of-2 length of Compensation Tables (number of frequency bits used to address the table)
			int LUTDepth{ 12 };
      /// \brief the field width of amplitude data stored in the Compensation Tables
			int LUTAmplBits{ 12 };
      /// \brief the field width of phase data stored in the Compensation Tables
			int LUTPhaseBits{ 14 };
      /// \brief the field width of analogue synchronous data stored in the Compensation Tables
			int LUTSyncABits{ 12 };
      /// \brief the field width of digital synchronous data stored in the Compensation Tables
			int LUTSyncDBits{ 12 };
	  /// \brief System Clock Frequency
			MHz sysClock{ 500.0 };
      /// \brief Sync Clock Frequency
			MHz syncClock{ 125.0 };
	  /// \brief Number of RF Channels
			int channels{ 4 };
      /// \brief Indicates whether the Synthesiser supports Remote Firmware Upgrade
			bool RemoteUpgrade{ false };
	  /// \brief Can Synthesiser support independent compensation tables for each channel
			bool ChannelComp{ false };
		};

    /// \cond IMS_SYNTHESISER_CONSTRUCTOR
		IMSSynthesiser();
		IMSSynthesiser(const std::string&, const std::string&, const Capabilities&, const FWVersion&, const FileSystemTable&, const std::shared_ptr<const IMSOption>&);
		~IMSSynthesiser();

		// Copy & Assignment Constructors
		IMSSynthesiser(const IMSSynthesiser &);
		IMSSynthesiser &operator =(const IMSSynthesiser &);
    /// \endcond

    /// If there are any Options attached to the Synthesiser, these are accessed here, else a null pointer is returned
		std::shared_ptr<const IMSOption> AddOn() const;
    /// \brief Returns the Capabilities structure for the Synthesiser
    /// \since 1.0
		const Capabilities GetCap() const;
    /// \brief Returns a descriptive string for the Synthesiser
    /// \since 1.0
		const std::string& Description() const;
    /// \brief Returns the short model number for the Synthesiser
    /// \since 1.0
		const std::string& Model() const;
    /// \brief Returns the Firmware version for the Synthesiser
    /// \since 1.0
		const FWVersion& GetVersion() const;
    /// \brief Returns true if the system scan successfully identified the Synthesiser and initialised this Class
    /// \return true if the class contains valid data representing an attached iMS Synthesiser
    /// \since 1.0
		const bool IsValid() const;
	/// \brief Returns the FileSystemTable for the Synthesiser
	/// \since 1.1
		const FileSystemTable& FST() const;

	private:
		class Impl;
		Impl * p_Impl;
	};

    ///
    /// \class IMSSystem IMSSystem.h include/IMSSystem.h
    /// \brief An object representing the overall configuration of an attached iMS System and permits applications to connect to it.
    ///
    ///
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC IMSSystem : public std::enable_shared_from_this<IMSSystem>
	{
	public:
    /// \cond IMS_SYSTEM_CONSTRUCTORS
        template<typename ... T>
        static std::shared_ptr<IMSSystem> Create(T&& ... t) {
            return std::shared_ptr<IMSSystem>(new IMSSystem(std::forward<T>(t)...));
        }    
		~IMSSystem();

		// Copy & Assignment Constructors
		IMSSystem(const IMSSystem &);
		IMSSystem &operator =(const IMSSystem &);

    /// Initialise() function used internally by ConnectionList::scan() to populate IMSSystem object and its children with data
		bool Initialise();
    /// \endcond

    ///
    /// \name Connect to / Disconnect from iMS Hardware
    //@{
    ///
    /// \brief Attempts to establish a Connection to an iMS System
    ///
    /// Apart from scanning to identify attached iMS Systems (see ConnectionList::scan()), no interaction
    /// can occur with an iMS System until a connection has been established to it.  This can be done by
    /// calling the Connect() function.  Once established, the connection will remain open until
    /// Disconnect() is called.
    /// \since 1.0
		void Connect();
    /// \brief Breaks a connection to an iMS System
    ///
    /// Any existing connection to an iMS System can be terminated by calling the Disconenct() function.
    /// Any messages that are pending but not yet sent will be completed before closing the connection,
    /// so the application can be sure that any immediately preceding commands will be run to completion
    /// before the connection is closed.
    /// \since 1.0
		void Disconnect();
    /// <summary>
    ///  Sets the timeouts associated with an iMS Connection
    /// </summary>
    /// <param name="send_timeout_ms">The timeout if the Connection Manager is unable to deliver a message to the physical connection</param>
    /// <param name="rx_timeout_ms">The timeout waiting for a response from the iMS system</param>
    /// <param name="free_timeout_ms">The delay between when a message is sent and when it is removed from memory</param>
    /// <param name="discover_timeout_ms">The round trip wait time for iMS systems to respond to with an announce packet to a discovery broadcast (Ethernet)</param>
    /// \since 1.8.10
        void SetTimeouts(int send_timeout_ms = 500, int rx_timeout_ms = 5000, int free_timeout_ms = 30000, int discover_timeout_ms = 2500);

	/// \brief Tests Connection Status
	///
	/// If an open connection exists to the iMS System, this function will return true
	/// \since 1.3
		bool Open() const;
    //@}

    /// \brief returns a pointer to an object which is the Connection through which all messages to the hardware go
    /// \warning This function may be removed in a future release.  Avoid using.
		const std::shared_ptr<IConnectionManager> Connection() const;

    /// \brief Add an iMS Controller to the System. Intended for internal library use.
    /// \since 1.0
		void Ctlr(const IMSController&);
    /// \brief Add an iMS Synthesiser to the System.  Intended for internal library use.
    /// \since 1.0
		void Synth(const IMSSynthesiser&);
    /// \brief Retrieve data about the iMS Controller
    /// \return a const reference to the IMSSystem's Controller class
    /// \since 1.0
		const IMSController& Ctlr() const;
    /// \brief Retrieve data about the iMS Synthesiser
    /// \return a const reference to the IMSSystem's Synthesiser class
    /// \since 1.0
		const IMSSynthesiser& Synth() const;

    /// \brief Returns a descriptive string representing the connection port on which the iMS System was discovered
    /// \since 1.0
		const std::string& ConnPort() const;

	/// \brief Tests for equality between two IMSSystem's
	/// \since 1.3
		bool operator==(IMSSystem const& rhs) const;

		///
		/// \name Modify iMS Device Connection Settings
		///
		/// All iMS Devices include some non-volatile storage used for various purposes.  This includes an area set
		/// aside for storing parameters used in setting up the connection interfaces, for example IP addresses to be
		/// used by the Ethernet network interface.
		///
		/// These functions may be used to modify connection settings on the device, writing them to onboard storage
		/// and also to obtain the current settings that are used by the device.
		//@{
		///
		/// \brief Write new connection settings to iMS non-volatile memory
		///
		/// To update iMS connection settings, create an object from a class derived from IConnectionSettings and
		/// initialise it with the new settings.  Several such classes exist in the SDK, one for each type of interface
		/// available in iMS devices.  Settings are applied one interface at a time, not all together.  Call this method
		/// with the settings object to write the settings to the device.  When the device is next rebooted or power cycled
		/// the new settings will take effect.
		///
		/// \param[in] settings the new settings to update the device with, using the appropriate derived class for the interface type
		/// \return true if settings were applied successfully, false on failure.
		bool ApplySettings(const IConnectionSettings& settings);
		///
		/// \brief Read existing connection settings from iMS
		///
		/// Call this method with the appropriate derived IConnectionSettings class for the desired interface type to
		/// read into it the existing settings that are present on the iMS device.  The object will be populated with
		/// the iMS device's current connection settings.
		///
		/// \param[out] settings provide a class object derived from IConnectionSettings appropriate to the interface desired
		/// \return true if settings were retrieved successfully, false on failure.
		bool RetrieveSettings(IConnectionSettings& settings);
	private:
		IMSSystem();
		IMSSystem(const std::shared_ptr<IConnectionManager>, const std::string&);

		class Impl;
		Impl * p_Impl;
	};

}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
