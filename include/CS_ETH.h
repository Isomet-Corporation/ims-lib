/*-----------------------------------------------------------------------------
/ Title      : Connection Settings Header - Ethernet Configuration
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg.qytek.lan/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/h/IConnectionSettings.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2018-07-18
/ Last update: $Date: 2018-03-23 18:32:16 +0000 (Fri, 23 Mar 2018) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 326 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2018 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2018-07-18  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

///
/// \file CS_ETH.h
///
/// \brief Modify IP Address, Net Mask and Gateway of Ethernet Connection
///
/// \author Dave Cowan
/// \date 2018-12-10
/// \since 1.5.0
///

#ifndef CS_ETH_H__
#define CS_ETH_H__

#if defined(_WIN32) || defined(__QNXNTO__) || defined(__linux__)

#include "IConnectionSettings.h"
#include <vector>

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
	/// \class CS_ETH CS_ETH.h include/CS_ETH.h
	/// \brief Applies Network Configuration settings to an iMS Device
	///
	/// \code
	///		CS_ETH cs_eth;
	/// 	if (std::find(myiMS->Ctlr().Interfaces().begin(), myiMS->Ctlr().Interfaces().end(), cs_eth.Ident())
	/// 		!= myiMS->Ctlr().Interfaces().end()) {
	/// 
	/// 		// Get existing settings from iMS Controller
	/// 		myiMS->RetrieveSettings(cs_eth);
	/// 
	/// 		// Toggle DHCP on/off
	/// 		if (cs_eth.UseDHCP()) {
	///				cs_eth.UseDHCP(false);
	///				cs_eth.Address("192.168.2.100");
	///				cs_eth.Netmask("255.255.255.0");
	///				cs_eth.Gateway("192.168.2.254");
	/// 		}
	/// 		else {
	///				cs_eth.UseDHCP(true);
	/// 		}
	/// 
	/// 		// Reapply to Controller. Power cycle to take effect
	/// 		myiMS->ApplySettings(cs_eth);
	/// 		std::cout << "New settings: DHCP=" << cs_eth.UseDHCP() << std::endl;
	/// 		if (!cs_eth.UseDHCP()) {
	///				std::cout << "IPv4 = " << cs_eth.Address() << std::endl;
	/// 			std::cout << "Mask = " << cs_eth.Netmask() << std::endl;
	/// 			std::cout << "GW   = " << cs_eth.Gateway() << std::endl;
	/// 		}
	/// 	}
	/// \endcode
	///
	/// \author Dave Cowan
	/// \date 2018-07-25
	/// \since 1.5.0
	///
	class LIBSPEC CS_ETH : public IConnectionSettings
	{
	public:
           
		/// \brief default constructor
        CS_ETH();
		/// \brief full specification constructor
        CS_ETH(bool use_dhcp,
                    std::string addr = std::string("192.168.1.10"),
                    std::string netmask = std::string("255.255.255.0"),
                    std::string gw = std::string("192.168.1.1"));
		/// \brief data buffer constructor 
        CS_ETH(std::vector<std::uint8_t> process_data);
        /// default destructor
		~CS_ETH();

        CS_ETH(const CS_ETH &);
		CS_ETH &operator =(const CS_ETH &);

        ///
        /// \name Ethernet Settings Accessors
        //@{
        ///
        /// \name Configure iMS device to acquire Ethernet IP Address settings from network, or use internally stored values
        ///
        /// \param[in] dhcp If true, acquire settings from network. If false, use values stored internally
        ///
        void UseDHCP(bool dhcp);
        /// \return true if device is configured to acquire settings from network and false if configured to use internal settings
        bool UseDHCP() const; 

        /// \name IPv4 Address Accessors
        ///
		/// \param[in] a string in decimal-dot notation reprenting an IPv4 address (e.g. "192.168.1.10")
		///
        void Address(const std::string& addr);
		/// \return a string representing the IPv4 address
        std::string Address() const;
        
        /// \name Netmask Get/Set
        ///
		/// \param[in] a string in decimal-dot notation reprenting an IPv4 netmask (e.g. "255.255.255.0")
		///
		void Netmask(const std::string& mask);
		/// \return a string representing the IPv4 netmask
		std::string Netmask() const;
        
        /// \name Gateway Get/Set
		///
		/// \param[in] a string in decimal-dot notation reprenting an IPv4 gateway (e.g. "192.168.1.1")
		///
		void Gateway(const std::string& gw);
		/// \return a string representing the IPv4 gateway
		std::string Gateway() const;
        //@}
            
        ///
        /// \name Implement IConnectionSettings interface
        //@{
        ///
        /// \brief String identifier "CS_ETH" for thie connection settings class
        ///
        /// \return a string that uniquely identifies this class
		const std::string& Ident() const;

		/// \brief Provide a raw buffer of settings byte data recovered from the device that should be converted to meaningful parameters
		/// \param[in] data a byte buffer of data from the device
		///
		void ProcessData(const std::vector<std::uint8_t>& data);

		/// \brief Returns the raw buffer of byte data that represents the settings configuration on the device
		/// \return a byte buffer of configuration settings
		const std::vector<std::uint8_t>& ProcessData() const;

        /// \brief Implement this in each derived class to allow heap allocation to work in the IConnectionManager
        /// \return a heap allocated pointer to a copy of the object
        std::shared_ptr<IConnectionSettings> Clone() const;
		//@}
	private:
        class Impl;
        Impl *pImpl;                
	};

}

#endif

#endif // CS_ETH_H__
