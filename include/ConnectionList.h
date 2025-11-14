/*-----------------------------------------------------------------------------
/ Title      : Connection List Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/h/ConnectionList.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2021-08-20 22:35:21 +0100 (Fri, 20 Aug 2021) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 489 $
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
/// \file ConnectionList.h
/// 
/// \brief Creates iMS Connection Interfaces for Application Use and scans them to discover all available iMS Systems
///
/// ConnectionList.h is the starting point for all software interaction with an iMS System.
/// It maintains a list of all the available host to iMS connection types (USB, Ethernet, RS422, etc)
/// allows the application software to search all of them for iMS Systems with one function call,
/// populates the IMSSystem object with details about the attached system and provides it with the
/// internal library interface for communications to occur.
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_Connection
///

#ifndef IMS_CONNECTION_LIST_H__
#define IMS_CONNECTION_LIST_H__

#include "IMSSystem.h"
#include "Containers.h"

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
	// This forward declaration is required for the typedef
	class IConnectionManager;

    /// \class ConnectionList ConnectionList.h include/ConnectionList.h
    /// \brief Creates iMS Connection Interfaces and scans them to discover 
    /// available iMS Systems
    /// 
    /// For software to interact with an iMS system, it must first discover it
    /// on one of the supported connection types, then open a link to it.
    ///
    /// The ConnectionList maintains a private list of modules for all the known
    /// supported connection types (USB, Ethernet, etc.).  Each connection
    /// module is stored as a pointer to an object within this list and 
    /// implements a common interface so that other code within the library can 
    /// communicate with the iMS using the module and with no knowledge required 
    /// about what type of connection is used.
	///
	/// The list of supported module types enabled by the ConnectionList is
	/// dependent on which platform the host application is operating on. To
	/// see which module types are supported, browse the list of modules returned
	/// by the call to \c modules():
	///
	/// \code
	///
	///		auto& modules = connList->modules();
	///		for (auto&& mname : connList->modules())
	///		{
	/// 		std::cout << "Module: " << mname << std::endl;
	///		}
	///
	/// \endcode
    ///
	/// By default, all modules are enabled to scan for iMS systems and the 
	/// function call to \c scan() will attempt to open a connection on every
	/// available port to the module.  In this context, a "port" is a term used
	/// generically to refer to a unique point of access on which an iMS or
	/// multiple iMS's may be discovered.  A module may have multiple ports.
	/// For example, the CM_ETH connection module will have one port for each
	/// network interface on the system with each interface port being recognised
	/// by its host IP address.
	///
	/// Application software can choose to limit the range of the ConnectionList::scan mechanism
	/// by only enabling the modules on which the application is expecting to find
	/// iMS Systems.  Within a single module, the scan can be restricted further by
	/// adding a PortMask to the connection configuration.  If a PortMask is defined,
	/// a module will only scan the ports that are present within it.  Limiting the
	/// scope of the ConnectionList::scan in either of these ways can dramatically
	/// improve application startup time.
	///
    /// Once instantiated, ConnectionList can perform a \c scan, which starts a 
    /// discovery algorithm on each of the available enabled modules in turn.  When
    /// complete, it will return an array (std::vector) of IMSSystem objects,
    /// each fully populated with the iMS configuration, model, serial number etc.
    /// 
    /// \attention It is important to understand that all communications to/from iMS
    /// hardware happens through the connection module held in this list, 
    /// therefore the ConnectionList object once created must be maintained
    /// within scope until the software no longer needs to communicate with the iMS.
    /// Do not delete this object after the scan has completed, unless you don't
    /// intend on communicating with the iMS!
    ///
    /// \code
    /// #include "ConnectionList.h"
    /// #include "IMSSystem.h"
    ///
    /// using namespace iMS;
    ///
    /// int main(int argc, char* argv)
    /// {
    ///     // Create List of Connection Modules
    ///     ConnectionList* connList = new ConnectionList();
	///
	///		// Get Connection List of modules supported on this platform
	///		auto& modules = connList->modules();
	///
	///		// Disable scan on serial port connection module
	///		if (std::find(modules.begin(), modules.end(), "CM_SERIAL") != modules.end()) connList->config("CM_SERIAL").IncludeInScan = false;
	///		// Limit Ethernet Connection Module to only scan on host NIC with IP address 192.168.2.128
	///		if (std::find(modules.begin(), modules.end(), "CM_ETH") != modules.end()) connList->config("CM_ETH").PortMask.push_back("192.168.2.128");
    ///
	///     // Scan all enabled connection types for iMS systems and return an array of results
	///     std::vector<std::shared_ptr<IMSSystem>> iMSList = connList->scan();
	///
	///     // Our iMS object
	///     std::shared_ptr<IMSSystem> myiMS;
	///     for (std::vector<std::shared_ptr<IMSSystem>>::const_iterator iter = iMSList.begin();	iter != iMSList.end(); ++iter)
	///     {
    ///         myiMS = (*iter);
    ///         // Look for the first iMS system that contains an iMSL type Controller
    ///		    if (myiMS->Ctlr().IsValid() && (myiMS->Ctlr().Model == "iMSL"))
    ///         {
    ///             break;
    ///         }
    ///         // None found
    ///         if (iter == iMSList.end())
    ///         {
    ///             std::cout << "No iMS found." << std::endl;
    ///             // Don't forget to free the ConnectionList
    ///             delete connList;
    ///             return -1;
    ///         }
    ///	    }
    ///
    ///     // Open the connection
    ///     myiMS->Connect();
    ///		std::cout << "Connecting to IMS System on port: " << myiMS->ConnPort() << std::endl;
    ///
    ///     // .... Do something with the iMS
    ///
    ///     // All done.  Disconnect.
    ///     myiMS->Disconnect();
    ///
    ///     // Always free the ConnectionList memory after all iMS functions are complete and connections closed
    ///     delete connList;
    ///
    ///     return 0;
    /// }
    /// \endcode
    ///
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC ConnectionList
	{
	public:
		/// Constructor initialises the list with the connection types
        ///
        /// \param[in] max_discover_timeout_ms Limit the time spend waiting for responses to broadcast discovery messages per interface, in milliseconds
		ConnectionList(unsigned int max_discover_timeout_ms = 100);
		/// Destructor
		~ConnectionList();
		
		/// \struct ConnectionConfig ConnectionList.h include\ConnectionList.h
		/// \brief Controls the behaviour of a Connection Module during its discovery process
		///
		/// The ConnectionList class maintains an internal map of ConnectionConfig configuration structs,
		/// one per module included in the ConnectionList.
		///
		/// Each Connection Module has a discovery mechanism which is invoked when the
		/// ConnectionList performs a scan.  Before calling the discovery function, the
		/// ConnectionList first checks the ConnectionConfigMap for details about how
		/// the discovery function for that module should be configured.  Firstly, it checks
		/// to see if the module should be included in the scan, and only calls the discovery
		/// function if this is set to true.  Secondly, a user supplied list of strings is 
		/// passed to the discovery functions which, if non-empty, acts as a mask, only permitting
		/// discovery on interface ports that can be matched to an entry in the list.  If the 
		/// list is empty, all interface ports are queried.
		/// \since 1.4.2
		struct LIBSPEC ConnectionConfig
		{
			/// If true, the Connection Module associated with the ConnectionConfig is enabled for iMS discovery
			///
			bool IncludeInScan;
			/// A list of interfaces (ports) that may be queried. For example, an Ethernet Connection Module
			/// might include a reference to a host static IP address that is known to reside on a network 
			/// containing iMS devices (e.g. "192.168.1.100").  An application might know that it is 
			/// expecting to find an iMS connected to Windows serial port COM8 so it would add "COM8" to
			/// the PortMask.  If the PortMask is empty, the module will iterate through every interface
			/// port that is available to it.
			ListBase<std::string> PortMask;

			/// Constructors for ConnectionConfig
			///
			/// Default Constructor enables scan on all available interface ports
			ConnectionConfig();
			/// Constructor with inclusion boolean specifier
			ConnectionConfig(bool inc);
			/// Constructor with list of port masks to use (IncludeInScan defaults to true)
			ConnectionConfig(const ListBase<std::string>& mask);
		};
		///

		/// \name System Discovery
		//@{
        ///
		/// \brief Configure the Connection process to each supported Connection interface
		///
		/// Returns a reference to the internal configuration for a given module. When ConnectionList is 
		/// constructed, it loads a map with Keys, one for each Connection Module supported
		/// by the platform.  The Value associated with each key is a ConnectionConfig struct
		/// which by default enables iMS discovery on all available interface ports.
		///
		/// It is up to the user's application to restrict the scope of the scan by modifying
		/// the configuration for each module as desired.
		///
		/// \return Returns a reference to the internal Connection Configuration Map
		/// \since 1.4.2
		ConnectionConfig& Config(const std::string& module);

        [[deprecated("Use ConnectionList::Config(const std::string& module) instead")]]
        inline ConnectionConfig& config(const std::string& module) { return Config(module);}

		///
		/// \brief Returns a list of string identifiers for each of the Connection Modules
		///
		/// Each Connection Module has a unique string identifier.  The string identifier is
		/// used as the "Key" in the ConnectionConfigMap.  This function returns a list
		/// of all the Connection Modules supported by this platform.
		///
		/// \return Returns a const reference to a list of all supported Connection Modules
		/// \since 1.4.2
		const ListBase<std::string>& Modules() const;

        [[deprecated("Use ConnectionList::Modules() instead")]]
		inline const ListBase<std::string>& modules() const {return Modules();}

        ///
        /// \brief Apply connection settings to a given interface
        ///
        /// Used to configure the host connection for settings specific to the connection
        /// type.  E.g. sets the baud rate for serial style interfaces.
        ///
        /// \since 2.0.2
        void Settings(const std::string& module, const IConnectionSettings* settings);

		///
		/// \brief Probe each of the known connection types for attached iMS Systems
        ///
        /// The \c Scan() function iterates through the list of connection types, opening
        /// a port on each in an implementation defined manner.  On a successful open, it
        /// will send a sequence of query messages to identify if an iMS Controller 
        /// and/or an iMS Synthesiser(s) is present.  If any of the query messages
        /// results in a valid response without timing out, the function creates an
        /// IMSSystem object and begins populating the object with information it
        /// can find out about it(them) either by sending further query messages to
        /// the device or by cross-referencing a hardware database built into the library.
        ///
        /// Once all connection types have been probed and all iMS Systems discovered,
        /// the IMSSystem objects are loaded into a vector which is returned for
        /// application processing.
        ///
        /// \return Returns an array of discovered iMS Systems
        /// \since 1.0
		std::vector<std::shared_ptr<IMSSystem>> Scan();

        ///
        /// \brief Scan a specific interface for IMS systems.
        ///
        /// Probes a single hardware interface and optionally restricts
        /// the search to the provided address range(s).
        ///
        /// \param interfaceName Name of the interface to scan (e.g. "CM_USBSS").
        /// \param addressHints Optional list of address or range strings to
        ///                    narrow the search (defaults to an empty list).
        ///
        /// \return A shared pointer to the first IMSSystem found on the
        ///        given interface, or nullptr if none found.
        /// \since 2.0.1
        ///
        std::shared_ptr<IMSSystem> Scan(
            const std::string& interfaceName,
            const std::vector<std::string>& addressHints = {}
        );
                
        [[deprecated("Use ConnectionList::Scan() instead")]]
        inline std::vector<std::shared_ptr<IMSSystem>> scan() { return Scan();}

        ///
        /// \brief Find a specific IMS system by its ID.
        ///
        /// Searches the given interface for a system whose ID matches the
        /// specified value. The system ID corresponds to the string returned
        /// by IMSSystem::ConnPort().
        ///
        /// \param interfaceName Name of the interface to search (e.g. "CM_USBSS").
        /// \param systemId The ID of the target system to locate.
        /// \param addressHints Optional list of address or range strings to
        ///                    narrow the search.
        ///
        /// \return A shared pointer to the matching IMSSystem if found,
        ///        or nullptr if not found.
        /// \since 2.0.1
        ///        
        std::shared_ptr<IMSSystem> Find(
            const std::string& interfaceName,
            const std::string& systemId,
            const std::vector<std::string>& addressHints = {}
        );        
		//@}
	private:
		class Impl;
		Impl *pImpl;
	};

}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif // CONNECTION_MANAGER_H
