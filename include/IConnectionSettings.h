/*-----------------------------------------------------------------------------
/ Title      : Connection Settings Interface Header
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
/// \file IConnectionSettings.h
///
/// \brief Interface Class for User Application code to modify iMS Systems connection parameters
///
/// \author Dave Cowan
/// \date 2018-12-10
/// \since 1.5.0
/// \ingroup group_ConnectionSettings
///

#ifndef IMS_CONNECTION_SETTINGS_H__
#define IMS_CONNECTION_SETTINGS_H__

#include <vector>
#include <string>
#include <cstdint>
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
	/// \class IConnectionSettings IConnectionSettings.h include/IConnectionSettings.h
	/// \brief This is the interface that describes how the SDK updates
	/// iMS configuration settings used in the set up of a connection
	///
	/// IConnectionSettings is a pure virtual class that defines the interface methods that must be implemented
	/// by any concrete class derived from it.  Its purpose is to act as a placeholder in the ApplySettings/RetrieveSettings
	/// methods of the IMSSystem class which may be overridden by instances of classes that specialise the interface
	/// for a particular connection type, e.g. Ethernet or RS422.
	///
	/// A connection type may have specific settings on the iMS device which may be reconfigured for compatibility with
	/// the user's system.  For example, an iMS device that is attached to an Ethernet network may require configuring
	/// with a static IP address.  This interface class facilitates this process.
	///
	/// Typically, settings stored on the iMS device are non-volatile.  The iMS SDK knows for each device how many bytes of
	/// storage are required and at what address location for any given connection type represented by it's "Ident" string.
	/// It is up to the derived class to provide relevant access methods for the configuration parameters, and to 
	/// convert those parameters to or from a buffer of bytes that is presented using the ProcessData methods.  
	///
	/// It is not anticipated that a user should create their own derived classes from IConnectionSettings.  Use
	/// the provided derived classes starting CS_* to modify particular iMS configuration settings.
	///
	/// \author Dave Cowan
	/// \date 2018-07-18
	/// \since 1.5.0
	///
	class LIBSPEC IConnectionSettings
	{
	public:
		/// override default destructor
		virtual ~IConnectionSettings() {}

    ///
    /// \name Inheritable Connection Settings Methods
    //@{
		/// \brief Returns a string value uniquely identifying the IConnectionSettings derived class to the user and to the IMSSystem settings methods
		///
		///
		virtual const std::string& Ident() const = 0;

		/// \brief Provide a raw buffer of settings byte data recovered from the device that should be converted to meaningful parameters
		/// \param[in] data a byte buffer of data from the device
		virtual void ProcessData(const std::vector<std::uint8_t>& data) = 0;

		/// \brief Returns the raw buffer of byte data that represents the settings configuration on the device
		/// \return a byte buffer of configuration settings
		virtual const std::vector<std::uint8_t>& ProcessData() const = 0;

        /// \brief Implement this in each derived class to allow heap allocation to work in the IConnectionManager
        /// \return a heap allocated pointer to a copy of the object
        virtual std::shared_ptr<IConnectionSettings> Clone() const = 0;
    //@}
		
	};

}

#endif // CONNECTION_MANAGER_H
