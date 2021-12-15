/*-----------------------------------------------------------------------------
/ Title      : Bulk Transfer (large binary data) Interface Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/h/IBulkTransfer.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2017-09-11 23:55:34 +0100 (Mon, 11 Sep 2017) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 300 $
/------------------------------------------------------------------------------
/ Description: Interface Specification class for sending large binary data objects to the iMS
/------------------------------------------------------------------------------
/ Copyright (c) 2015 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2015-04-09  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

///
/// \file IBulkTransfer.h
/// 
/// \brief Interface Specification class for sending large binary data objects to the iMS
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_IBulkTransfer
///

#ifndef IMS_BULKTRANSFER_H__
#define IMS_BULKTRANSFER_H__

#include "IMSSystem.h"

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

namespace iMS {
    /// 
    /// \class IBulkTransfer IBulkTransfer.h include/IBulkTransfer.h
    /// \brief Interface Specification class for sending large binary data objects to the iMS
    /// 
    /// There are several instances in which large binary data must be transferred either
    /// from the host to the iMS or in the other direction, e.g. download of image 
    /// data, compensation tables etc.  This is known as Bulk Transfer and it implements a
    /// background process that supervises the splitting up of large data objects into
    /// individual messages compatible with the communications module, queuing them
    /// for transfer, verifying the success or failure of the transfer and reporting to
    /// the application software when the transfer is complete.
    ///
    /// This interface class defines the methods which application software may use
    /// to control the Bulk Transfer process.  It is inherited by API classes that
    /// require the use of a Bulk Transfer, and which implement the Bulk Transfer 
    /// mechanism.
    ///
    /// Completion and success or failure of a Bulk Transfer are indicated by the
    /// IEventHandler mechanism, which must be implemented by the derivative class
    ///
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC IBulkTransfer
	{
	public:
		virtual ~IBulkTransfer() {}

    ///
    /// \brief Initiates a Bulk Transfer download
    ///
    /// If the user has subscribed to the relevant event notifications, the BulkTransfer
    /// derived object will issue a completion event at the end of the download process
    /// and will also warn the user anytime a download messaging error occurs.
    /// \return Boolean indicating whether Download has started successfully
    /// \since 1.0
		virtual bool StartDownload() = 0;
    
    ///
    /// \brief Initiates a Bulk Transfer verify
    ///
    /// If the user has subscribed to the relevant event notifications, the BulkTransfer
    /// derived object will raise an event to the application at the end of the verify process
    /// to indicate whether the verification was successful or not.
    /// \return Boolean indicating whether Verify has started successfully
    /// \since 1.0
		virtual bool StartVerify() = 0;

    ///
    /// \brief Returns the address of the next verify error or -1 if none
    ///
    /// After the application has been notified of a failed verify, it can probe
    /// the BulkTransfer derived object to obtain the approximate address at which
    /// the BulkTransfer failed.  The address is provided as a byte offset from the
    /// start of the BulkTransfer binary object.
    ///
    /// Due to the way in which the BulkTransfer mechanism splits the transfer
    /// into individual messages, there will be one error recorded for each message
    /// that results in a verify fail.  Therefore, the address will only be approximate,
    /// to the nearest message size boundary and if there are multiple byte fails
    /// within the scope of a single message, only one error will be recorded.
    ///
    /// Calling this function repeatedly will result in returning the next recorded
    /// verify error.  If there are no errors left, or the transfer was successful
    /// (i.e. there were no verify failures recorded) the function will return -1.
    ///
    /// \return byte address of transfer failure or -1 if none.
    /// \since 1.0  
		virtual int GetVerifyError() = 0;
	};

}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
