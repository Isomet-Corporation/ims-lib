/*-----------------------------------------------------------------------------
/ Title      : Event Handler Interface Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/EventManager/h/IEventHandler.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2017-09-11 23:55:34 +0100 (Mon, 11 Sep 2017) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 300 $
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
/// \file IEventHandler.h
///
/// \brief Interface Class for User Application code to receive and process events from the iMS library
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_EventHandler
///

#ifndef IMS_EVENT_HANDLER_H__
#define IMS_EVENT_HANDLER_H__

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

#include <vector>
#include <cstdint>

namespace iMS {
    ///
    /// \class IEventHandler IEventHandler.h include/IEventHandler.h
    /// \brief Interface Class for an Event Handler to be defined in User Code and subscribed to library events
    ///
    /// Note that it is not possible to subscribe a single derived class to multiple events from
    /// different source objects in the library (as in a system-wide message handler) because the message enum
    /// integer values overlap each other.  This is a conscious design choice to encourage encapsulation.
    /// A class may still be subscribed to multiple events as long as they are triggered from the same source object.
    ///
    /// Example:
    /// \code
    ///	class ImageVerifySupervisor : public IEventHandler
    ///	{
    ///	private:
    ///		bool m_verifying{ true };
    ///	public:
    ///		void EventAction(void* sender, const int message, const int param)
    ///		{
    ///			switch (message)
    ///			{
    ///			case (ImageDownloadEvents::VERIFY_SUCCESS) : std::cout << "Image Verify Successful!" << std::endl; m_verifying = false; break;
    ///			case (ImageDownloadEvents::VERIFY_FAIL) : std::cout << "Image Verify FAILED!" << std::endl; m_verifying = false; break;
    ///			}
    ///		}
    ///		bool Busy() const { return m_verifying; };
    ///	};
    /// \endcode
    ///
    /// The above code snippet defines a class "ImageVerifySupervisor" that inherits from the IEventHandler
    /// base class.  This is used during the download of an Image to the Controller to determine whether
    /// the verification of the download was successful or not.
    ///
    /// The class contains a private boolean variable which is initialised to true.  It overrides the
    /// EventAction interface class method to do something when the VERIFY_SUCCESS and VERIFY_FAIL
    /// events are raised.  User code can read the Busy() function to determine whether the downloader
    /// is still in the process of verifying the download or whether it has finished (which is assumed
    /// to be the case once either of the 2 events are received).
    ///
    /// To use the class, the application code creates an ImageVerifySupervisor object at the same time
    /// as starting a verify on an ImageDownload.  It then links the object to the ImageDownload by
    /// calling the Subscribe() method for both ImageDownloadEvents::VERIFY_SUCCESS and
    /// ImageDownloadEvents::VERIFY_FAIL, passing to the method the address of the ImageVerifySupervisor
    /// object as a function pointer.
    ///
    /// \code
    /// 	ImageDownload * dl = new ImageDownload(ims, img);
    /// 	ImageVerifySupervisor vs;
    /// 	dl->ImageDownloadEventSubscribe(ImageDownloadEvents::VERIFY_SUCCESS, &vs);
    /// 	dl->ImageDownloadEventSubscribe(ImageDownloadEvents::VERIFY_FAIL, &vs);
    ///
    /// 	dl->StartVerify();
    ///
    /// 	while (vs.Busy()) {
    /// 		std::this_thread::sleep_for(std::chrono::milliseconds(50));
    /// 	}
    ///
    /// 	dl->ImageDownloadEventUnsubscribe(ImageDownloadEvents::VERIFY_SUCCESS, &vs);
    /// 	dl->ImageDownloadEventUnsubscribe(ImageDownloadEvents::VERIFY_FAIL, &vs);
    ///   delete dl;
    /// \endcode
    ///
    /// When the verify completes, it will trigger either the VERIFY_SUCCESS or VERIFY_FAIL events which,
    /// through the library's event handling mechanism, will identify the subscribed function and call
    /// the EventAction method.
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
	class LIBSPEC IEventHandler
	{
    protected:
    /// \brief Default Constructor
        IEventHandler();
	public:
    /// \brief Virtual Destructor
		virtual ~IEventHandler() = default;
    /// \brief Used internally to identify Functions subscribed to Events.  Not intended for Application usage.
    /// \since 1.0
		bool operator == (const IEventHandler e);

    ///
    /// \name Overrideable User Action on Event
    //@{
	/// \brief This Method must be overriden by a User derived callback class.
	///
	/// When a user class derived from IEventHandler is subscribed to receive event notifications from
	/// the iMS Library, it is this function that is always called when the event is raised.  Therefore
	/// it is essential to override this method to process the event and to do something with it.
	///
	/// This overloaded callback function provides integer parameter data to user code.
	///
	/// \param[in] sender A pointer to the class that triggers the event callback.  Can be used to obtain additional information.
	/// \param[in] message an integer that maps to an enum in the Events class associated with the callback subscription
	/// \param[in] param an optional integer parameter that provides additional information on the callback event.
	/// \since 1.0
		virtual void EventAction(void* sender, const int message, const int param = 0) {}
	/// \brief This Method must be overriden by a User derived callback class.
  ///
  /// When a user class derived from IEventHandler is subscribed to receive event notifications from
  /// the iMS Library, it is this function that is always called when the event is raised.  Therefore
  /// it is essential to override this method to process the event and to do something with it.
  ///
	/// This overloaded callback function provides integer parameter data to user code.
	///
	/// \param[in] sender A pointer to the class that triggers the event callback.  Can be used to obtain additional information.
  /// \param[in] message an integer that maps to an enum in the Events class associated with the callback subscription
  /// \param[in] param an integer parameter that provides additional information on the callback event.
	/// \param[in] param2 an optional integer parameter that provides further additional information on the callback event.
	/// \since 1.2
		virtual void EventAction(void* sender, const int message, const int param, const int param2) {}
	/// \brief This Method must be overriden by a User derived callback class.
	///
	/// When a user class derived from IEventHandler is subscribed to receive event notifications from
	/// the iMS Library, it is this function that is always called when the event is raised.  Therefore
	/// it is essential to override this method to process the event and to do something with it.
	///
	/// This overloaded callback function provides floating point parameter data to user code.
	///
	/// \param[in] sender A pointer to the class that triggers the event callback.  Can be used to obtain additional information.
	/// \param[in] message an integer that maps to an enum in the Events class associated with the callback subscription
	/// \param[in] param an floating point parameter that provides additional information on the callback event.
	/// \since 1.1
		virtual void EventAction(void* sender, const int message, const double param) {}
	/// \brief This Method must be overriden by a User derived callback class.
	///
	/// When a user class derived from IEventHandler is subscribed to receive event notifications from
	/// the iMS Library, it is this function that is always called when the event is raised.  Therefore
	/// it is essential to override this method to process the event and to do something with it.
	///
	/// This overloaded callback function provides a vector of byte data to user code.
	///
	/// \param[in] sender A pointer to the class that triggers the event callback.  Can be used to obtain additional information.
	/// \param[in] message an integer that maps to an enum in the Events class associated with the callback subscription
	/// \param[in] param an integer parameter that provides information on the callback event.
	/// \param[in] data a byte vector parameter that provides additional information on the callback event.
	/// \since 1.2
		virtual void EventAction(void* sender, const int message, const int param, const std::vector<std::uint8_t> data) {}
	//@}
	private:
		int mID;
		static int mIDCount;
	};

}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
