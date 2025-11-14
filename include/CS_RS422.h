/*-----------------------------------------------------------------------------
/ Title      : Connection Settings Header - RS422 Configuration
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
/// \file CS_RS422.h
///
/// \brief Modify Baud Rate of RS422 Connection
///
/// \author Dave Cowan
/// \date 2018-12-10
/// \since 1.5.0
/// \ingroup group_ConnectionSettings
///

#ifndef CS_RS422_H__
#define CS_RS422_H__

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
	/// \class CS_RS422 CS_RS422.h include/CS_RS422.h
	/// \brief Applies Serial Port Configuration settings to an iMS Device
	///
	/// \code
	///		CS_RS422 cs_rs422;
	///		if (std::find(myiMS->Ctlr().Interfaces().begin(), myiMS->Ctlr().Interfaces().end(), cs_rs422.Ident())
	///			!= myiMS->Ctlr().Interfaces().end()) {
	///
	///			myiMS->RetrieveSettings(cs_rs422);
	///
	///			if (cs_rs422.BaudRate() == 115200) {
	///				cs_rs422.BaudRate(57600);
	///			}
	///			else {
	///				cs_rs422.BaudRate(115200);
	///			}
	///
	///			myiMS->ApplySettings(cs_rs422);
	///			std::cout << "New settings: Baud Rate = " << cs_rs422.BaudRate() << std::endl;
	///		}
	/// \endcode
	///
	/// \author Dave Cowan
	/// \date 2018-07-25
	/// \since 1.5.0
	class LIBSPEC CS_RS422 : public IConnectionSettings
	{
	public:
        ///  
        /// \brief RS422 Parity settings
        enum class ParitySetting {
            NONE,
            ODD,
            EVEN
        };

        enum class DataBitsSetting {
            BITS_7,
            BITS_8
        };

        enum class StopBitsSetting {
            BITS_1,
            BITS_2
        };

		/// \brief full specification constructor
        CS_RS422(unsigned int baud_rate = 115200,
            DataBitsSetting data_bits = DataBitsSetting::BITS_8,
            ParitySetting parity = ParitySetting::NONE,
            StopBitsSetting stop_bits = StopBitsSetting::BITS_1);
		/// \brief constructor from data buffer
        CS_RS422(std::vector<std::uint8_t> process_data);
		~CS_RS422();

		CS_RS422(const CS_RS422 &);
		CS_RS422 &operator =(const CS_RS422 &);

        ///
        /// \name RS422 Settings Accessors
        //@{
        ///
        /// \brief Configure host or iMS device to use a specific baud rate for its serial port communications. 
        ///        
		/// \param[in] baud_data the data rate to use in bps (bits per second)
        void BaudRate(unsigned int baud_rate);
		/// \return the current serial port baud rate
        unsigned int BaudRate() const;
        ///
        /// \name Configure host or iMS Device to use a specific number of data bits for its serial port communications.
        ///        
		/// \param[in] data_bits the number of data bits to use
        void DataBits(DataBitsSetting data_bits);
		/// \return the current serial port number of data bits
        DataBitsSetting DataBits() const;        
        ///
        /// \name Configure host or iMS Device to use a specific parity type for its serial port communications.
        ///        
		/// \param[in] parity the type of parity to use
        void Parity(ParitySetting parity);
		/// \return the current serial port parity type
        ParitySetting Parity() const;   
        ///
        /// \name Configure host or iMS Device to use a specific number of stop bits for its serial port communications.
        ///        
		/// \param[in] stop_bits the number of stop bits to use
        void StopBits(StopBitsSetting stop_bits);
		/// \return the current serial port baud rate
        StopBitsSetting StopBits() const;           
        //@}
            
        ///
        /// \name Implement IConnectionSettings interface
        //@{
        ///
		/// \brief String identifier "CS_RS422" for thie connection settings class
		///
		/// \return a string that uniquely identifies this class
		const std::string& Ident() const;

		/// \brief Provide a raw buffer of settings byte data recovered from the device that should be converted to meaningful parameters
		/// \param[in] data a byte buffer of data from the device
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
