/*-----------------------------------------------------------------------------
/ Title      : Image Operations Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ImageOps/h/ImageOps.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2025-01-08 21:36:05 +0000 (Wed, 08 Jan 2025) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 656 $
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
/// \file ImageOps.h
///
/// \brief Classes for downloading and playback of Image data
///
/// ImageOps or Image Operations is one of the core features of the iMS Library, providing
/// the user application with the ability to download and verify Images and ImageGroups
/// to an iMS Controller's memory along with the means to configure, start and stop the Controller
/// playback.
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_Image
///

#ifndef IMS_IMAGEOPS_H__
#define IMS_IMAGEOPS_H__

#include "IMSSystem.h"
#include "IEventHandler.h"
#include "IBulkTransfer.h"
#include "Image.h"

#include <memory>
#include <thread>

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

// MSVC: Exporting template specialization for use as Post Delay by users
// see https://jeffpar.github.io/kbarchive/kb/168/Q168958/ for explanation
EXPIMP_TEMPLATE template class LIBSPEC std::chrono::duration < std::uint16_t, std::ratio<1, 10000> >;


namespace iMS
{
  ///
  /// \class DownloadEvents ImageOps.h include\ImageOps.h
  /// \brief All the different types of events that can be triggered by the ImageDownload and SequenceDownload classes.
  ///
  /// Some events contain integer parameter data which can be processed by the IEventHandler::EventAction
  /// derived method
  /// \author Dave Cowan
  /// \date 2015-11-11
  /// \since 1.0
	class LIBSPEC DownloadEvents
	{
	public:
    /// \enum Events List of Events raised by the Image Downloader
		enum Events {
      /// Event raised when ImageDownload has confirmed that the iMS Controller received all of the Image data
			DOWNLOAD_FINISHED,
      /// Event raised each time the ImageDownload class registers an error in the download process
			DOWNLOAD_ERROR,
      /// Event raised on completion of a download verify, if the download was successfully verified
			VERIFY_SUCCESS,
      /// Event raised on completion of a download verify, if the download failed. \c param contains the number of failures recorded
			VERIFY_FAIL,
	  /// Event raise when unable to begin a fast transfer of image data to memory, e.g. Image memory is full
			DOWNLOAD_FAIL_MEMORY_FULL,
	  /// Event raise when unable to transfer any data through DMA mechanism
			DOWNLOAD_FAIL_TRANSFER_ABORT,
	  /// Event raised when a new download has been accepted prior to memory transfer commencing, reporting the new image index handle
			IMAGE_DOWNLOAD_NEW_HANDLE,
			Count
		};
	};

	/// \typedef ImageDownloadEvents 
	/// For Backwards compatibility. For newer software, use DownloadEvents
	using ImageDownloadEvents = DownloadEvents;
	/// \typedef SequenceDownloadEvents
	/// Alias for DownloadEvents class
	using SequenceDownloadEvents = DownloadEvents;

  ///
  /// \class ImageDownload ImageOps.h include\ImageOps.h
  /// \brief Provides a mechanism for downloading and verifying Images to a Controller's memory
  /// \author Dave Cowan
  /// \date 2015-11-11
  /// \since 1.0
	class LIBSPEC ImageDownload : public IBulkTransfer
	{
	public:
    ///
    /// \name Constructor & Destructor
    //@{
    ///
    /// \brief Constructor for ImageDownload Object
    ///
    /// The pre-requisites for an ImageDownload object to be created are:
    ///   (1) - an IMSSystem object, representing the configuration of an iMS target to which the Image
    /// is to be downloaded.
    ///   (2) - a complete Image object to download to the iMS target.
    ///
    /// ImageDownload stores const references to both.  This means that both must exist before the
    /// ImageDownload object, and both must remain valid (not destroyed) until the ImageDownload
    /// object itself is destroyed.  Because they are stored as references, the IMSSystem and Image
    /// objects themselves may be modified after the construction of the ImageDownload object.
    ///
    /// Once constructed, the object can neither be copied or assigned to another instance.
    ///
	/// From v1.7.0, the ImageDownload object checks the Controller Image Table to see if an identical
	/// image already exists on the Controller.  If it does, it will not repeat the Image Download.
	/// StartDownload() will return true and a DOWNLOAD_FINISHED message will be triggered immediately.
	///
    /// \param[in] ims A reference to the iMS System which is the target for downloading the Image
    /// \param[in] img A const reference to the Image which shall be downloaded to the target
    /// \since 1.0
    ImageDownload(std::shared_ptr<IMSSystem> ims, const Image& img);
    ///
    /// \brief Destructor for ImageDownload Object
		~ImageDownload();
    //@}

	/// \name Configure Image Format
	//@{
	///
	/// \brief Apply Format parameters to the downloaded Image
	/// 
	/// An iMS Image in software uses real values for Frequency, Amplitude, Phase and Sync Data that are
	/// abstract from the fixed point values and hardware capabilities of the connected iMS System.
	/// 
	/// During the ImageDownload process, the Image contents are rendered to the particular characteristics
	/// of the iMS system taking into account, the number of RF channels it supports, and the bit depths of
	/// the DDS device being used.
	/// 
	/// If the user does not specify how the Image should be formatted then a number of assumptions are made
	/// to ensure the Image can be played out with good precision and with a reasonably fast update rate.
	/// 
	/// However, it is possible to control the formatting to process to priorities either fixed point precision 
	/// or update rate, by extending or reducing the number of bytes per parameter.  It is also possible to
	/// reduce the number of channels being used to increase the update rate.
	/// 
	/// \param[in] fmt An optional Image Format description which can be used to modify the characteristics of 
	///  the Image stored in iMS system memory
	/// \since 1.8.12
		void SetFormat(const ImageFormat& fmt);
	//@}
    /// \name Bulk Transfer Initiation
    //@{
	///
	/// \brief Asynchronously begin the Image Download process
	///
	/// The ImageDownload class spawns an internal worker thread that performs the fast image download
	/// process in the background.  The thread will generate messages to the user application that can be
	/// registered for with the ImageDownloadEventSubscribe method.
	///
	/// To kick off the download process, call StartDownload once.  This will set up the necessary parameters
	/// on the iMS system to begin the Image download (such as address in memory) and pass the information on
	/// to the worker thread.  The thread will then be triggered to perform the download process.
	///
	/// Provided the conditions to start the download completed successfully, the function call will return
	/// as soon as the download procedure has begun.  It is then up to user software to monitor when the
	/// download has completed.
	/// 
	/// \return true if the download was initiated successfully
		bool StartDownload();
	///
	/// \brief Asynchronously begins the Image Verify process
	///
	/// The ImageDownload class supports a full readback and verify process for Images.  Image Data is returned
	/// from the iMS system in a background thread and compared with the expected data.  On completion, a
	/// success or failure event is returned to the user application.
	///
	/// Like the download process, the verify process is carried out in the background on a separate thread, so
	/// this function call will return once verify has started but before it has completed.  User software is
	/// responsible for monitoring the events that determine the verification result.
	/// \return true if the verify process was begun successfully.
		bool StartVerify();
    //@}

    ///
    /// \name Retrieve Error Information
	///
	/// \return Returns the ID of the iMS Message that resulted in a Verify Error
    //@{
		int GetVerifyError();
    //@}

    ///
    /// \name Event Notifications
    //@{
    ///
    /// \brief Subscribe a callback function handler to a given DownloadEvents entry
    ///
    /// ImageDownload can callback user application code when an event occurs in the
    /// download process.  Supported events are listed under DownloadEvents.  The
    /// callback function must inherit from the IEventHandler interface and override
    /// its EventAction() method.
    ///
    /// Use this member function call to subscribe a callback function to a DownloadEvents entry.
    /// For the period that a callback is subscribed, each time an event in ImageDownload occurs
    /// that would trigger the subscribed DownloadEvents entry, the user function callback will be
    /// executed.
    /// \param[in] message Use the DownloadEvents::Event enum to specify an event to subscribe to
    /// \param[in] handler A function pointer to the user callback function to execute on the event trigger.
    /// \since 1.0
		void ImageDownloadEventSubscribe(const int message, IEventHandler* handler);
    /// \brief Unsubscribe a callback function handler from a given DownloadEvent
    ///
    /// Removes all links to a user callback function from the Event Trigger map so that any
    /// events that occur in the ImageDownload object following the Unsubscribe request
    /// will no longer execute that function
    /// \param[in] message Use the DownloadEvent::Event enum to specify an event to unsubscribe from
    /// \param[in] handler A function pointer to the user callback function that will no longer execute on an event
    /// \since 1.0
		void ImageDownloadEventUnsubscribe(const int message, const IEventHandler* handler);
    //@}
	private:
    // Makes this object non-copyable
		ImageDownload(const ImageDownload &);
		const ImageDownload &operator =(const ImageDownload &);

		class Impl;
		Impl * p_Impl;
	};

	///
  /// \class ImagePlayerEvents ImageOps.h include\ImageOps.h
  /// \brief All the different types of events that can be triggered by the ImagePlayer class
  /// \author Dave Cowan
  /// \date 2015-11-11
  /// \since 1.0
  class LIBSPEC ImagePlayerEvents
	{
	public:
    /// \enum Events List of Events raised by the Image Player
		enum Events {
      /// Event raised in response to ImagePlayer::GetProgress(). Indicates the number of points into an Image playback.
			POINT_PROGRESS,
      /// Event raised when an Image in the Controller begins playback
			IMAGE_STARTED,
      /// Event raised when an Image in the Controller completes playback
			IMAGE_FINISHED,
			Count
		};
	};

	///
  /// \class ImagePlayer ImageOps.h include\ImageOps.h
  /// \brief Once an Image has been downloaded to Controller memory, ImagePlayer can be used to configure and begin playback
  ///
  /// ImagePlayer contains a Configuration Structure which holds all of the different attributes that
  /// may be used to modify the behaviour of the playback, including internal oscillator or external clock,
  /// next-image triggering and image repeating.  It does not define the internal oscillator clock
  /// rate for ImagePoint playback frequency when not using an external clock; this information is
  /// stored in the Image class.
  ///
  /// Once constructed, the ImagePlayer.Play() function will begin playback, ImagePlayer.Stop() will
  /// end playback (immediately or at the end of an image) and ImagePlayer.GetProgress() will raise an
  /// event to the user application indicating the current ImagePoint that has been reached in playback.
  /// \author Dave Cowan
  /// \date 2015-11-11
  /// \since 1.0
	class LIBSPEC ImagePlayer
	{
	public:
    /// \enum PointClock
    /// \brief Determines whether Image Progression is under the control of an internal or external clock
    /// \since 1.0
		enum class PointClock {
      /// ImagePoint progression through the Image at a rate determined by the programming of the internal NCO (Numerically Controlled Oscillator)
      INTERNAL,
      /// ImagePoint progression through the Image one point per edge detected on the CLK signal connected to the Controller
      EXTERNAL
    };
    /// \enum ImageTrigger
    /// \brief At the end of each Image, the next Image in the sequence (or the next Repeat of the same image) will begin after the ImageTrigger condition is satisfied
    /// \since 1.0
		enum class ImageTrigger {
      /// A programmable timer is started at the end of the image.  The next image is triggered after the timer times out.
      POST_DELAY,
      /// The next image is triggered when an edge is detected on the TRIG signal connected to the Controller
      EXTERNAL,
      /// The next image is triggered when application software sends a 'User Trigger' request
      HOST,
      /// The next image is triggered immediately
      CONTINUOUS
    };
    /// Repeats
    /// \brief Each Image can be repeated, either a programmable number of times, or indefinitely
    /// \since 1.0
		using Repeats = ImageRepeats;
    /// \brief The external signal connections can be configured to be active on the rising edge or the falling edge (CLK, TRIG), high or low (ENABLE)
    /// \since 1.0
		using Polarity = iMS::Polarity;
    /// \enum StopStyle
    /// \brief The ImagePlayer can end the Image Playback either at the end of the Image or Repeat, or immediately
    /// \since 1.0
		enum class StopStyle {
      /// The default method for stopping the Image is to action the Stop request at the end of the current Image, or Image Repeat.
      GRACEFULLY,
      /// Use this to end the Image Playback as soon as the command is processed by the Controller
      IMMEDIATELY
    };

	  ///
    /// \struct PlayConfiguration ImageOps.h include\ImageOps.h
    /// \brief This struct sets the attributes for the ImagePlayer to use when initiating an Image Playback
    /// \author Dave Cowan
    /// \date 2015-11-11
    /// \since 1.0
		struct LIBSPEC PlayConfiguration
		{
      /// \brief Use Internal NCO or External Clock signal
			PointClock int_ext{ PointClock::INTERNAL };
      /// \brief Trigger Next Image Immediately, after programmable delay, External Trigger signal or software Trigger
			ImageTrigger trig{ ImageTrigger::CONTINUOUS };
      /// \brief Run Image Once, Always until stopped, or a Programmable number of times
			Repeats rpts{ Repeats::NONE };
      /// \brief If Repeats set to Repeats::PROGRAM, this field sets the number of repeats to trigger (not including first pass, i.e. n_rpts = 3 => 4 playbacks in total)
			int n_rpts{ 0 };
      /// \brief Sets the active edge of the External Clock signal (Polarity::NORMAL = rising edge)
			Polarity clk_pol{ Polarity::NORMAL };
      /// \brief Sets the active edge of the External Trigger signal (Polarity::NORMAL = rising edge)
			Polarity trig_pol{ Polarity::NORMAL };

      /// \brief This type is used internally to define the correct scaling between std::chrono classes and the hardware delay counter.  Min Resolution is 0.1msec
			using post_delay = std::chrono::duration < std::uint16_t, std::ratio<1, 10000> > ;
      /// \brief When ImageTrigger is set to ImageTrigger::POST_DELAY, this field defines the length of time between the end of one image (or repeat) and the start of the next.
      /// Use SetPostDelay(std::chrono::milliseconds(...)) or an associated std::chrono class
			post_delay del{ 0 };

      /// \name Constructors
      ///
      //@{
      /// \brief Empty Constructor.
      /// All attributes take on their default values
			PlayConfiguration() {};
      /// \brief Constructor with Clock Initialisation.
      /// Use this to set the Clock to be supplied from an External signal.
			PlayConfiguration(PointClock c) : int_ext(c) {};
      /// \brief Constructor with Clock & Trigger Initialisation.
      /// Use this to set the Clock, Trigger or both to be supplied from External signals.
			PlayConfiguration(PointClock c, ImageTrigger t) : int_ext(c), trig(t) {};
      /// \brief Constructor with Clock Initialisation and Post-Delay.
      /// Use this for a configurable delay between images
			PlayConfiguration(PointClock c, std::chrono::duration<int> d) : int_ext(c), trig(ImageTrigger::POST_DELAY), del(std::chrono::duration_cast<post_delay>(d)) {};
      /// \brief Constructor with Clock Initialisation, Post-Delay and Image Repeats.
      /// Use this to configure the Clock source, Delay between Image repeats and the number of Repeats per Image
			PlayConfiguration(PointClock c, std::chrono::duration<int> d, Repeats r, int n_rpts) : int_ext(c), trig(ImageTrigger::POST_DELAY), rpts(r), n_rpts(n_rpts), del(std::chrono::duration_cast<post_delay>(d)) {};
      /// \brief Constructor with Indefinite Repeats.
      /// Use this to set the Image to Repeat Always until Stopped by User Command
			PlayConfiguration(Repeats r) : rpts(r) {};
      /// \brief Constructor with Programmable Repeats.
      /// Use this to set the Image to Repeat a programmable number of times
			PlayConfiguration(Repeats r, int n_rpts) : rpts(r), n_rpts(n_rpts) {};
      //@}
      ///
		} cfg; ///< Defines the configuration for Image Playback

    ///
    /// \name Constructor & Destructor
    //@{
    ///
    /// \brief Constructor for ImagePlayer Object
    ///
    /// An IMSSystem object, representing the configuration of an iMS target on which an Image has
    /// already been downloaded, must be passed by const reference to the ImagePlayer constructor.
    ///
    /// The IMSSystem object must exist before the ImagePlayer object, and must remain valid (not
    /// destroyed) until the ImagePlayer object itself is destroyed.
    ///
    /// The Image to be played back must also be passed by reference.  ImagePlayer will check the
    /// unique ID (UUID) of an Image against the value that is in memory on the hardware to ensure
    /// that it is playing the same Image that has been downloaded.
    ///
    /// Once constructed, the object can neither be copied or assigned to another instance.
    ///
    /// \param[in] ims A const reference to the iMS System which is the target on which to playback the Image
    /// \param[in] img A const reference to the Image that has been downloaded to the target
    /// \since 1.0
		ImagePlayer(std::shared_ptr<IMSSystem> ims, const Image& img);
    /// \brief Constructor for ImagePlayer Object with User Configuration
    ///
    /// As per the default constructor, but also receives a const reference to a PlayConfiguration
    /// struct which will have already been modified by the application to change the playback
    /// behaviour.  The attributes of the struct are copied to the internal configuration struct.
    ///
    /// An alternative is to use the default constructor and modify the configuration manually after
    /// construction.
    ///
    /// \param[in] ims A const reference to the iMS System which is the target on which to playback the Image
    /// \param[in] img A const reference to the Image that has been downloaded to the target
    /// \param[in] cfg A const reference to a PlayConfiguration playback configuration structure
    /// \since 1.0
		ImagePlayer(std::shared_ptr<IMSSystem> ims, const Image& img, const PlayConfiguration& cfg);
    /// \brief Constructor for ImagePlayer Object to play Image direct from Image Table using Internal Clock
    ///
    /// Sometimes it is necessary to play an Image on an iMS System which has already been downloaded
    /// but the Image object is not available to software (for example, it was downloaded by another
    /// application).  In this instance, use the ImageTableViewer class to identify the image in iMS
    /// memory, select an ImageTableEntry from the list and construct the ImagePlayer from this.
    /// 
    /// Construct the ImagePlayer with an Internal Clock specifier to control the image clock rate
    /// when using internal clock.
    ///
    /// For example:
    ///
    /// \code
    /// int i;
    /// ImageTableViewer itv(myiMS);
    /// std::cout << "Select an Image to Play: " << std::endl << itv;  // Display table to user
    /// std::cin >> i;
    /// if (i < itv.Entries()) {
    ///     ImagePlayer player(myiMS, itv[i], kHz(80.0));  // Play Image using Internal Clock
    ///     player.Play();   // Play Immediately
    /// }
    /// \endcode
    ///
    /// \param[in] ims A const reference to the iMS System which is the target on which to playback the Image
    /// \param[in] ite A const reference to the image residing on the iMS System which has been obtained from the ImageTableViewer
    /// \param[in] InternalClock the internal clock rate to use when playing the image
		ImagePlayer(std::shared_ptr<IMSSystem> ims, const ImageTableEntry& ite, const kHz InternalClock);
    /// \brief Constructor for ImagePlayer Object to play Image direct from Image Table using External Clock
    ///
    /// Sometimes it is necessary to play an Image on an iMS System which has already been downloaded
    /// but the Image object is not available to software (for example, it was downloaded by another
    /// application).  In this instance, use the ImageTableViewer class to identify the image in iMS
    /// memory, select an ImageTableEntry from the list and construct the ImagePlayer from this.
    /// 
    /// Construct the ImagePlayer with an External Clock Divide specifier to use an external clock
    /// with optional clock divider.  Leave as default for external clock without divide.
    ///
    /// For example:
    ///
    /// \code
    /// int i;
    /// ImageTableViewer itv(myiMS);
    /// std::cout << "Select an Image to Play: " << std::endl << itv;  // Display table to user
    /// std::cin >> i;
    /// if (i < itv.Entries()) {
    ///     ImagePlayer player(myiMS, itv[i]);  // Play Image using External Clock without divide
    ///     player.Play(ImagePlayer::ImageTrigger::EXTERNAL);   // Wait for External Trigger
    /// }
    /// \endcode
    ///
    /// \param[in] ims A const reference to the iMS System which is the target on which to playback the Image
    /// \param[in] ite A const reference to the image residing on the iMS System which has been obtained from the ImageTableViewer
    /// \param[in] ExtClockDivide Configures to skip external clock edges. Divides down clock rate by this divisor.
		ImagePlayer(std::shared_ptr<IMSSystem> ims, const ImageTableEntry& ite, const int ExtClockDivide = 1);
    /// \brief Constructor for ImagePlayer Object to play Image direct from Image Table using Internal Clock and User Configuration
    ///
    /// Sometimes it is necessary to play an Image on an iMS System which has already been downloaded
    /// but the Image object is not available to software (for example, it was downloaded by another
    /// application).  In this instance, use the ImageTableViewer class to identify the image in iMS
    /// memory, select an ImageTableEntry from the list and construct the ImagePlayer from this.
    /// 
    /// Construct the ImagePlayer with an Internal Clock specifier to control the image clock rate
    /// when using internal clock.  Use the PlayConfiguration struct to change the playback behaviour,
    /// for example the number of image repeats.
    ///
    /// For example:
    ///
    /// \code
    /// int i;
    /// ImageTableViewer itv(myiMS);
    /// std::cout << "Select an Image to Play: " << std::endl << itv;  // Display table to user
    /// std::cin >> i;
    /// if (i < itv.Entries()) {
    ///     ImagePlayer player(myiMS, itv[i], ImagePlayer::PlayConfiguration(ImageRepeats::FOREVER), kHz(80.0));  // Play Image using Internal Clock repeatedly
    ///     player.Play();   // Play Immediately
    /// }
    /// \endcode
    ///
    /// \param[in] ims A const reference to the iMS System which is the target on which to playback the Image
    /// \param[in] ite A const reference to the image residing on the iMS System which has been obtained from the ImageTableViewer
    /// \param[in] cfg A const reference to a PlayConfiguration playback configuration structure
    /// \param[in] InternalClock the internal clock rate to use when playing the image
		ImagePlayer(std::shared_ptr<IMSSystem> ims, const ImageTableEntry& ite, const PlayConfiguration& cfg, const kHz InternalClock);
    /// \brief Constructor for ImagePlayer Object to play Image direct from Image Table using External Clock and User Configuration
    ///
    /// Sometimes it is necessary to play an Image on an iMS System which has already been downloaded
    /// but the Image object is not available to software (for example, it was downloaded by another
    /// application).  In this instance, use the ImageTableViewer class to identify the image in iMS
    /// memory, select an ImageTableEntry from the list and construct the ImagePlayer from this.
    /// 
    /// Construct the ImagePlayer with an External Clock Divide specifier to use an external clock
    /// with optional clock divider.  Leave as default for external clock without divide.  Use the PlayConfiguration
    /// struct to change the playback behaviour, for example the number of image repeats.
    ///
    /// For example:
    ///
    /// \code
    /// int i;
    /// ImageTableViewer itv(myiMS);
    /// std::cout << "Select an Image to Play: " << std::endl << itv;  // Display table to user
    /// std::cin >> i;
    /// if (i < itv.Entries()) {
    ///     ImagePlayer player(myiMS, itv[i], ImagePlayer::PlayConfiguration(ImageRepeats::FOREVER), 4);  // Play Image repeatedly using External Clock divided down by 4
    ///     player.Play(ImagePlayer::ImageTrigger::EXTERNAL);   // Wait for External Trigger
    /// }
    /// \endcode
    ///
    /// \param[in] ims A const reference to the iMS System which is the target on which to playback the Image
    /// \param[in] ite A const reference to the image residing on the iMS System which has been obtained from the ImageTableViewer
    /// \param[in] cfg A const reference to a PlayConfiguration playback configuration structure
    /// \param[in] ExtClockDivide Configures to skip external clock edges. Divides down clock rate by this divisor.
		ImagePlayer(std::shared_ptr<IMSSystem> ims, const ImageTableEntry& ite, const PlayConfiguration& cfg, const int ExtClockDivide = 1);
    ///
    /// \brief Destructor for ImagePlayer Object
		~ImagePlayer();
    //@}

    ///
    /// \name Play Control Functions
    //@{
    /// \brief Starts Image Playback
    ///
    /// This function will begin the playback of an Image resident in Controller memory immediately
    /// on receipt of the message by the Controller.  If an Image is already playing, the function
    /// call will fail and return false.  Likewise, if an Image Download is in progress, the function
    /// call will fail and return false.  If no Image has been downloaded to the Controller, this
    /// function will run successfully, but nothing will happen on the Controller.
    ///
    /// Once the Controller has responded indicating that the Image Playback has started, an
    /// ImagePlayerEvent::IMAGE_STARTED event will be raised which the application can register to receive
    /// \return true if the Play Image request was sent to the Controller successfully.
    /// \since 1.0
		bool Play(ImageTrigger start_trig);
	/// \brief Starts Image Playback
	///
	/// Default Start function, using the same start trigger that is used by subsequent image repeats.
	/// Identical to \c Play(cfg.trig);
	/// \since 1.8.5
		inline bool Play() { return this->Play(this->cfg.trig); }
    /// \brief Requests current Point Progress
    ///
    /// This function call will request from the Controller the current ImagePoint position within the
    /// playback of the current Image.  If an Image playback is not in progress, this function call
    /// will return false.
    ///
    /// Once the Controller has responded with the ImagePoint position, an ImagePlayerEvent::POINT_PROGRESS
    /// event will be triggered containing the point position which the application can register to receive.
    /// \return true if the Progress request was successfully sent to the Controller
    /// \since 1.0
		bool GetProgress();
    /// \brief Halts the Image Playback
    ///
    /// This function call will end the playback of an Image that is currently taking place on the
    /// Controller.  There are two methods for stopping Image playback:
    ///   (1) StopStyle::GRACEFULLY : Ends the Image playback after the last point of the current
    /// Image.  If the Image is being repeated, either indefinitely or programmatically, playback
    /// will halt at the last point of the current Repeat, irrespective of whether there are more
    /// repeats programmed to happen.
    ///   (2) StopStyle::IMMEDIATELY : Ends the Image playback as soon as the message is received by
    /// the Controller.
    /// \param[in] stop Defines which Image Stop method to use
    /// \return true if the Stop message was successfully sent to the Controller.
    /// \since 1.0
		bool Stop(StopStyle stop);
    /// \brief Halts the Image Playback After Last Point in Image or Repeat
    ///
    /// Default Stop function.  Identical to \c Stop(StopStyle::GRACEFULLY);
    /// \since 1.0
		inline bool Stop() { return this->Stop(StopStyle::GRACEFULLY); };
    //@}

    ///
    /// \name Post Delay helper function
    //@{
    /// \brief Helper function that sets the Post Delay configuration attribute from any compatible
    /// std::chrono class (e.g. std::chrono::milliseconds(100.0))
    ///
    /// \param[in] dly Use one of the derived std::chrono classes to set an appropriate post-image delay
    /// \since 1.0
		void SetPostDelay(const std::chrono::duration<double>& dly);
    //@}

    ///
    /// \name Event Notifications
    //@{
    ///
    /// \brief Subscribe a callback function handler to a given ImagePlayerEvent
    ///
    /// ImagePlayer can callback user application code when an event occurs during
    /// playback.  Supported events are listed under ImagePlayerEvents.  The
    /// callback function must inherit from the IEventHandler interface and override
    /// its EventAction() method.
    ///
    /// Use this member function call to subscribe a callback function to an ImagePlayerEvent.
    /// For the period that a callback is subscribed, each time an event in ImagePlayer occurs
    /// that would trigger the subscribed ImagePlayerEvent, the user function callback will be
    /// executed.
    /// \param[in] message Use the ImagePlayerEvents::Event enum to specify an event to subscribe to
    /// \param[in] handler A function pointer to the user callback function to execute on the event trigger.
    /// \since 1.0
		void ImagePlayerEventSubscribe(const int message, IEventHandler* handler);
    /// \brief Unsubscribe a callback function handler from a given ImageDownloadEvent
    ///
    /// Removes all links to a user callback function from the Event Trigger map so that any
    /// events that occur in the ImageDownload object following the Unsubscribe request
    /// will no longer execute that function
    /// \param[in] message Use the ImageDownloadEvents::Event enum to specify an event to unsubscribe from
    /// \param[in] handler A function pointer to the user callback function that will no longer execute on an event
    /// \since 1.0
		void ImagePlayerEventUnsubscribe(const int message, const IEventHandler* handler);
    //@}

	private:
		// Make this object non-copyable
		ImagePlayer(const ImagePlayer &);
		const ImagePlayer &operator =(const ImagePlayer &);

		class Impl;
		Impl * p_Impl;
	};

	///
	/// \class ImageTableViewer ImageOps.h include\ImageOps.h
	/// \brief Provides a mechanism for viewing the ImageTable associated with an iMS System
	/// \author Dave Cowan
	/// \date 2016-01-21
	/// \since 1.1
	class LIBSPEC ImageTableViewer
	{
	public:
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for ImageTableViewer Object
		///
		/// The ImageTableViewer object requires an IMSSystem object, which will have had its ImageTable read back
		/// during initialisation.  It must therefore exist before the
		/// ImageTableViewer object, and must remain valid (not destroyed) until the ImageTableViewer
		/// object itself is destroyed.
		///
		/// Once constructed, the object can neither be copied or assigned to another instance.
		///
		/// \param[in] ims A reference to the iMS System whose ImageTable is to be viewed.
		/// \since 1.2
		ImageTableViewer(std::shared_ptr<IMSSystem> ims);
		~ImageTableViewer();
		//@}

		/// \name Image Table Information
		//@{
		/// \return The current number of entries stored in the ImageTable
		/// \since 1.2
		const int Entries() const;
		//@}
		/// \name Array operator for random access to ImageTableEntry s
		//@{
		/// \brief The ImageTable consists of a container of ImageTableEntry objects.  Each object may be
		/// accessed by calling the viewer object through an array subscript.
		///
		/// For example:
		/// \code
		/// ImageTableViewer itv(myiMS);
		///	int length = 0;
		///	for (int i=0; i<itv.Entries(); i++) {
		///		length += itv[i].Size();
		///	}
		///	std::cout << "Used space in Image Memory: " << length << " bytes" << std::endl;
		/// \endcode
		/// \since 1.1
		const ImageTableEntry operator[](const std::size_t idx) const;
		//@}

		/// \name Iterator Specification
		///
		/// Use these iterators when you want to iteratively read through or update the entries stored
		/// within the Image Table.
		///
		//@{
		/// \brief Const Iterator defined for list style access to image table contents
		typedef std::list<ImageTableEntry>::const_iterator const_iterator;

		/// \brief An iterator pointing to the first entry of the Image Table.
		/// \return An iterator pointing to the first entry of the Image Table.
		const_iterator begin();
		/// \brief Returns an iterator referring to the past-the-end element in the Image Table.
		/// \return Returns an iterator referring to the past-the-end element in the Image Table.
		const_iterator end();
		//@}

		/// \name Remove Images from iMS System Memory
		/// 
		/// Use these methods to remove an Image from iMS Controller system memory that has previously
		/// been downloaded to the Controller
		//@{
		/// \brief Random Access Erase
		/// 
		/// Removes an Image from memory by identifying its index within the Image Table.  For example, given this Image Table:
		///
		/// Image[0] id : 0 Addr : 0x01000000 Points : 1000 ByteLength : 26000 Format Code : 0 UUID : 3f06127a - 84f7 - 4ce7 - 9641 - e5eb7eec268d Name : Image 0
		///	Image[1] id : 5 Addr : 0x0101fbd0 Points : 1000 ByteLength : 26000 Format Code : 0 UUID : 3fe3a966 - 1178 - 4d3d - 8147 - 11fd057318f1 Name : Image 5
		///	Image[2] id : 14 Addr : 0x01072420 Points : 1000 ByteLength : 26624 Format Code : 0 UUID : 358bdd11 - 6cb5 - 4cfb - 83ff - 0f71a423a4d6 Name : Image 8
		///
		/// calling Erase(1) would remove the middle of the three images
		///
		/// \param[in] idx The index with respect to the Image Table contents to erase
		bool Erase(const std::size_t idx);
		/// \brief Alternative Random Access Erase
		/// e.g. itv.Erase(itv[1]) is equivalent to itv.Erase(1)
		///
		/// Tip: to erase all images:
		/// \code
		/// ImageTableViewer itv(myiMS);
		/// int images_total = itv.Entries();
		///	for (int i=0; i<images_total; i++) {
		///		itv.Erase(itv[0]);
		///	}
		/// \endcode
		/// \param[in] ite The individual table entry to remove from Controller Memory
		bool Erase(ImageTableEntry ite);
		/// \brief Iterator Erase
		/// Use this function when it is necessary to iterate through the Image Table to find a specific entry to remove, for example to remove 
		/// a particular image from memory called 'myimg'
		///
		/// \code
		/// Image myimg;
		///
		/// ... Create myimg, download it, play it, then when time to remove it, do this ...
		///
		/// ImageTableViewer itv(myiMS);
		///	for (const auto& ite : itv) {
		///		if (ite.Matches(myimg)) {
		///			std::cout << "Image Handle " << ite.Handle() << " Erased" << std::endl;
		///			itv.Erase(ite);
		///			break;
		///		}
		///	}
		/// \endcode
		/// \param[in] it An iterator pointing to the entry in the image table to remove
		bool Erase(const_iterator it);
		/// \brief Clears the Image Table erasing all images
		/// Using this function will cause all images in the Controller to be removed, resulting in an empty image memory
		///
		bool Clear();
		/// \brief Stream operator overload to simplify debugging
		/// 
		/// Example usage:
		/// \code
		/// ImageTableViewer itv(myiMS);
		/// if (itv.Entries() > 0) std::cout << itv;
		/// \endcode
		/// might produce the result:
		/// \code
		/// Image[0] id : 0 Addr : 0x00400000 Points : 10001 ByteLength : 440044 Format Code : 0 UUID : b31bdf48 - 0902 - 4277 - 86e1 - a6f0756a6acb
		///	Image[1] id : 1 Addr : 0x0046b6f0 Points : 08501 ByteLength : 374044 Format Code : 0 UUID : 5e03d558 - 46e8 - 49c4 - 80cf - d32fb51d8628
		/// Image[2] id : 2 Addr : 0x004c6c10 Points : 12461 ByteLength : 548284 Format Code : 0 UUID : 7358b86c - 0e90 - 4664 - 8b2b - ee0ba24542da
		/// \endcode
		friend LIBSPEC std::ostream& operator<< (std::ostream& stream, const ImageTableViewer&);
	private:
		// Make this object non-copyable
		ImageTableViewer(const ImageTableViewer& other);
		ImageTableViewer& operator= (const ImageTableViewer& other);

		class Impl;
		Impl* p_Impl;
	};


	///
	/// \class SequenceDownload ImageOps.h include\ImageOps.h
	/// \brief This class is a worker for transmitting an ImageSequence to an iMS Controller and joining it to the back of the sequence queue.
	/// \author Dave Cowan
	/// \date 2016-05-05
	/// \since 1.2.4
	class LIBSPEC SequenceDownload
	{
	public:
    ///
    /// \name Constructor & Destructor
    //@{
    ///
    /// \brief Constructor for SequenceDownload Object
    ///
    /// The pre-requisites for an SequenceDownload object to be created are:
    ///   (1) - an IMSSystem object, representing the iMS target to which the ImageSequence
    /// is to be downloaded.
    ///   (2) - a complete ImageSequence object to download to the iMS target.
    ///
    /// SequenceDownload stores references to both.  This means that both must exist before the
    /// SequenceDownload object, and both must remain valid (not destroyed) until the SequenceDownload
    /// object itself is destroyed.  Because they are stored as references, the IMSSystem and ImageSequence
    /// objects themselves may be modified after the construction of the SequenceDownload object.
    ///
    /// Once constructed, the object can neither be copied or assigned to another instance.
    ///
    /// \param[in] ims A reference to the iMS System which is the target for downloading the ImageSequence
    /// \param[in] seq A const reference to the ImageSequence which shall be downloaded to the target
    /// \since 1.2.4
		SequenceDownload(std::shared_ptr<IMSSystem> ims, const ImageSequence& seq);
    /// \brief Destructor
		~SequenceDownload();
    //@}

    /// \name Download Trigger
    //@{
    /// \brief Adds a new sequence to the end of the iMS Controller Sequence Queue.
    ///
    /// Calling this function will program the list of ImageSequenceEntry's and the termination action/value
    /// from the ImageSequence object reference into a new sequence added to the end of the Sequence Queue.
	///
	/// From v1.8.4, the download process supports two modes.  The original synchronous mode will download
	/// a sequence one entry at a time, and the Download() call will block until the full sequence has fully
	/// downloaded.  This is the default method for backwards compatibility with existing software.
	///
	/// The newer asynchronous method can be called by issuing a SequenceDownload::Download(true) command which
	/// will attempt to download the entire sequence in one go using the fastest mechanism available to the 
	/// library.  This can result in a speed improvement of > 100x.  The function call will first check to ensure
	/// that the connected iMS system supports the fast asynchronous sequence download procedure, then continue
	/// with this if it does.  User software can call LibVersion::HasFeature("FAST_SEQUENCE_DOWNLOAD") to check
	/// whether the linked iMSLibrary library supports fast sequence download.
	///
	/// If using asynchronous download, the user application must also subscribe to the DownloadEvents to determine
	/// when the download has completed or if there was an error during the download process.  Use
	/// SequenceDownloadEventSubscribe() for this purpose.
    /// \return True to indicate Sequence has been successfully added to the queue (asynchronous = false) or if
	/// the fast sequence download process has been initiated successfully (asynchronous = true)
		bool Download(bool asynchronous = false);

	/// \brief Initiates an asynchronous sequence download using similar syntax to the ImageDownload class.
	/// Identical to calling Download(true)
		inline bool StartDownload() { return Download(true); }
	//@}

	///
	/// \name Event Notifications
	//@{
	///
	/// \brief Subscribe a callback function handler to a given DownloadEvents entry
	///
	/// SequenceDownload can callback user application code when an event occurs in the
	/// download process.  Supported events are listed under DownloadEvents.  The
	/// callback function must inherit from the IEventHandler interface and override
	/// its EventAction() method.
	///
	/// Use this member function call to subscribe a callback function to a DownloadEvents entry.
	/// For the period that a callback is subscribed, each time an event in SequenceDownload occurs
	/// that would trigger the subscribed DownloadEvents entry, the user function callback will be
	/// executed.
	/// \param[in] message Use the DownloadEvents::Event enum to specify an event to subscribe to
	/// \param[in] handler A function pointer to the user callback function to execute on the event trigger.
	/// \since 1.8.4
		void SequenceDownloadEventSubscribe(const int message, IEventHandler* handler);
	/// \brief Unsubscribe a callback function handler from a given DownloadEvent
	///
	/// Removes all links to a user callback function from the Event Trigger map so that any
	/// events that occur in the ImageDownload object following the Unsubscribe request
	/// will no longer execute that function
	/// \param[in] message Use the DownloadEvent::Event enum to specify an event to unsubscribe from
	/// \param[in] handler A function pointer to the user callback function that will no longer execute on an event
	/// \since 1.8.4
		void SequenceDownloadEventUnsubscribe(const int message, const IEventHandler* handler);
	private:
		// Makes this object non-copyable
		SequenceDownload(const SequenceDownload &);
		const SequenceDownload &operator =(const SequenceDownload &);

		class Impl;
		Impl * p_Impl;
	};

	///
	/// \class SequenceEvents ImageOps.h include\ImageOps.h
	/// \brief All the different types of events that can be triggered by the SequenceManager class.
	///
	/// Some events contain integer parameter data which can be processed by the IEventHandler::EventAction
	/// derived method
	/// \author Dave Cowan
	/// \date 2016-05-04
	/// \since 1.2.4
	class LIBSPEC SequenceEvents
	{
	public:
		/// \enum Events List of Events raised by the Image Downloader
		enum Events {
			/// Event raised at the beginning of playback of each sequence
			SEQUENCE_START,
			/// Event raised after the final image of a sequence has completed and there are no more sequences in the queue, or the sequence was programmed to stop
			SEQUENCE_FINISHED,
			/// Event raised when an error occurs in processing the sequence queue (typically if the sequence queue was cleared during playback)
			SEQUENCE_ERROR,
			/// Event raised when sequence begins output of a tone buffer entry
			SEQUENCE_TONE,
			/// Event raised to report current position when requested by GetCurrentPosition()
			SEQUENCE_POSITION,
			Count
		};
	};


	///
	/// \class SequenceManager ImageOps.h include\ImageOps.h
	/// \brief The SequenceManager class allows application software to interact with the Sequence Queue running on the Controller.
	///
	/// The Sequence Queue permits application software to generate complicated chains of Image and RF Tone data.  This class allows
	/// the management of that sequence queue, enabling dynamic update of the queue according to system wide events.  The queue can
	/// be started, stopped, paused and restarted.  Individual sequences within the queue can be deleted, moved or updated.  The
	/// current status of the queue can be queried through this class allowing software to obtain a snapshot of the position of
	/// sequences within the queue.
	///
	/// \author Dave Cowan
	/// \date 2016-05-04
	/// \since 1.2.4
	class LIBSPEC SequenceManager
	{
	public:
    /// \name Constructors & Destructor
    //@{
    /// \brief Default Constructor
    ///
    /// Requires a reference to an iMS System in order to carry out communications with the Sequence Queue in the Controller
		SequenceManager(std::shared_ptr<IMSSystem>);
    /// \brief Destructor
		~SequenceManager();
    //@}

    /// \brief Defines Internal Oscillator / External Clock operation
		using PointClock = ImagePlayer::PointClock;
    /// \brief Defines Image Trigger function
		using ImageTrigger = ImagePlayer::ImageTrigger;
    /// \brief Defines polarity of external clock / trigger signals
		using Polarity = ImagePlayer::Polarity;

		/// \struct SeqConfiguration ImageOps.h include\ImageOps.h
		/// \brief This struct sets the attributes for the Sequence to use when initiating an Sequence Playback
		/// \author Dave Cowan
		/// \date 2016-05-05
		/// \since 1.2.4
		struct LIBSPEC SeqConfiguration
		{
			/// \brief Use Internal NCO or External Clock signal
			PointClock int_ext{PointClock::INTERNAL };
			/// \brief Trigger Next Image Immediately, after programmable delay, External Trigger signal or software Trigger
			ImageTrigger trig{ ImageTrigger::CONTINUOUS };
			/// \brief Sets the active edge of the External Clock signal (Polarity::NORMAL = rising edge)
			Polarity clk_pol{ Polarity::NORMAL };
			/// \brief Sets the active edge of the External Trigger signal (Polarity::NORMAL = rising edge)
			Polarity trig_pol{ Polarity::NORMAL };

			/// \name Constructors
			///
			//@{
			/// \brief Empty Constructor.
			/// All attributes take on their default values
			SeqConfiguration() {};
			/// \brief Constructor with Clock Initialisation.
			/// Use this to set the Clock to be supplied from an External signal.
			SeqConfiguration(PointClock c) : int_ext(c) {};
			/// \brief Constructor with Clock & Trigger Initialisation.
			/// Use this to set the Clock, Trigger or both to be supplied from External signals.
			SeqConfiguration(PointClock c, ImageTrigger t) : int_ext(c), trig(t) {};
			//@}
			///
		} cfg; ///< Defines the configuration for Sequence Playback

    /// \name Playback Operations
    //@{
    /// \brief Begins playback through the sequence queue
    ///
    /// The iMS Controller will start playing the ImageSequenceEntry that exists at the front of the Sequence Queue.
    /// If the queue is empty, the call will have no effect, but may still return true.  Use the QueueCount() function if it
    /// is necessary to check for queue contents prior to playback.  Image playback will continue through each SequenceEntry
    /// and ImageSequence in turn until it either encounters an ImageSequence with a 'STOP_*' termination action or
    /// the queue becomes empty, or Software requests the queue to Stop or Pause.
    ///
    /// The queue behaviour may be controlled by the SeqConfiguration struct, which specifies whether the Sequence is
    /// clocked by the internal NCO oscillator or an external clock and what method is used to propagate the start of the
    /// next SequenceEntry in the list.
    ///
    /// The start_trig parameter may be used to control how the Sequence playback begins.  If set to CONTINUOUS (or not specified)
    /// Sequence playback will start immediately.  If set to EXTERNAL, Sequence playback will start when an External trigger
    /// is detected.  If set to HOST, Sequence playback will begin when a software trigger is sent (Using SendHostTrigger()).
    /// \param[in] cfg The clocking and trigger configuration for propagating through images in the sequence
    /// \param[in] start_trig the type of trigger used to begin the sequence playing back
    /// \return true whe the Sequence playback has been initiated
		bool StartSequenceQueue(const SeqConfiguration& cfg = SeqConfiguration(), ImageTrigger start_trig = ImageTrigger::CONTINUOUS);
    /// \brief Software trigger for sequence Image propagation
    /// When either SeqConfiguration::trig or start_trig are set to ImageTrigger::HOST, the application software must
    /// send a signal to the hardware to begin playback of either the next Image in the sequence, or the first image in
    /// the sequence respectively.
		void SendHostTrigger();

	/// \brief Stops Sequence Queue playback
	///
	/// This function is used to rapidly end playback of a sequence.  Two styles of Sequence Stop are possible: GRACEFULLY
	/// allows the queue to continue playback until it reaches the end of the current SequenceEntry including any repeats, then stops.
	/// IMMEDIATELY terminates playback as soon as the command is processed.
	///
	/// If the Stop is processed in the last SequenceEntry of a ImageSequence and GRACEFULLY is used, then even though the 
	/// ImageSequence will have completed, its termination action will not run and the ImageSequence will remain at the head of the queue.
	///
	/// Once stopped, the Sequence Queue can be restarted using the StartSequenceQueue function and it will start from the beginning of
	/// the same ImageSequence, i.e. queue position is not retained.
	/// \param[in] style Choose GRACEFULLY (default) to stop at end of sequence entry and IMMEDIATELY 
	/// to stop immediately.
	/// \return true if the command was issued successfully
	/// \since 1.8.0
		bool Stop(ImagePlayer::StopStyle style = ImagePlayer::StopStyle::GRACEFULLY);
	/// \brief Stops Sequence Queue playback at end of current sequence
	///
	/// Call this function to bring to an end the Sequence Queue playback.  Unlike the Stop() function, this allows the current sequence
	/// to keep playing through all of its remaining SequenceEntry 's until there are no more entries left.  The termination action is
	/// also allowed to proceed.  It is equivalent to updating the termination action of the sequence at the head of the queue from	
	/// SequenceTermAction::RECYCLE to ::STOP_RECYCLE or from ::INSERT to ::STOP_INSERT.  It can also be used to stop playback of a
	/// a sequence queue that is in one of the ::REPEAT termination methods, since no more repeats will be carried out.
	///
	/// \return true if the command was issued successfully
	/// \since 1.8.0
		bool StopAtEndOfSequence();
	/// \brief Breaks ImageSequence playback without resetting the queue position
	///
	/// Pause will cause the Controller to stop updating the Synthesiser RF output but, unlike the Stop functions, the position in the
	/// Sequence queue is not lost, so play can be continued from exactly the same Image Point.  Like Stop() however, pause can happen
	/// either GRACEFULLY (at the end of the SequenceEntry) or IMMEDIATELY (as soon as processed).
	///
	/// While paused, sequences within the queue cannot be moved (just like they cannot be moved when the queue is running) and the
	/// current and next sequence cannot have their termination actions updated.
	/// \param[in] style Choose ::GRACEFULLY (default) to pause at end of sequence entry and ::IMMEDIATELY to pause immediately.
	/// \return true if the command was issued successfully
	/// \since 1.8.0
		bool Pause(ImagePlayer::StopStyle style = ImagePlayer::StopStyle::GRACEFULLY);
	/// \brief Restart a paused sequence queue
	///
	/// Call this function to restart the sequence playback that had previously been paused.  Calling this function if the queue is not
	/// paused will have no effect.
	/// \return true if the command was issued successfully
	/// \since 1.8.0
		bool Resume();
    //@}

    /// \name Queue Modification
    //@{
    /// \brief Number of Sequences programmed into the Queue
    /// \return the number of sequences that are currently in the Controller queue
		std::uint16_t QueueCount();
    /// \brief Returns the identity of a particular sequence via its index in the Controller sequence queue
    ///
    /// Every ImageSequence has a unique ID, which is downloaded to the Controller sequence queue so that,
    /// although it is not possible to readback the configuration of every sequence in the queue, it is possible
    /// to match the UUID of every sequence with the UUID of an ImageSequence object stored in the application.
    /// \param[in] index The offset from the front of the queue from which to retrieve the UUID (0 = Sequence currently playing or next to play if stopped)
    /// \param[in] uuid A reference to a 16-byte array in which to store the UUID
    /// \return True if the operation completed successfully
		bool GetSequenceUUID(int index, std::array<std::uint8_t, 16>& uuid);
    /// \brief Remove all sequences from the queue
    ///
    /// If the sequence queue is currently playing, this function call will fail and return false
    /// \return True if the queue was successfully cleared
		bool QueueClear();
    /// \brief Remove an individual sequence from the queue
    ///
    /// Removes an ImageSequence from anywhere within the sequence queue.
    /// If attempting to remove the sequence from the front of the queue, while it is playing, the operation will still
    /// succeed but subsequent behaviour may be undefined.
    /// If multiple identical sequences exist in the queue, the sequence most recently added (or most recently played)
    /// will be deleted.  The function must be called multiple times to remove multiple sequences
    /// \param[in] seq A reference to an ImageSequence object to find in the queue and remove
    /// \return True if the removal was carried out successfully
		bool RemoveSequence(const ImageSequence& seq);
    /// \brief Remove an individual sequence from the queue
    ///
    /// Removes an ImageSequence from anywhere within the sequence queue.
    /// If attempting to remove the sequence from the front of the queue, while it is playing, the operation will still
    /// succeed but subsequent behaviour may be undefined.
    /// If multiple identical sequences exist in the queue, the sequence most recently added (or most recently played)
    /// will be deleted.  The function must be called multiple times to remove multiple sequences
    /// \param[in] uuid An identifier that can be returned from GetSequenceUUID to mark a sequence for deletion
    /// \return True if the removal was carried out successfully
		bool RemoveSequence(const std::array<std::uint8_t, 16>& uuid);
    /// \brief Update the termination behaviour of a specific sequence
    ///
    /// This function is used to change the behaviour of a sequence after it has completed playback.  It can be used
    /// for example to modify a sequence set to RECYCLE mode so that next time it plays back, it is instead DISCARDed.
    /// If multiple identical sequences exist in the queue, the sequence most recently added (or most recently played)
    /// will be updated.
    /// \param[in] seq A reference to an ImageSequence object to find in the queue and update
    /// \param[in] action The new action value to program the ImageSequence with
    /// \param[in] val An optional Termination value to apply to the sequence
		/// \return True if the update was carried out successfully
		bool UpdateTermination(ImageSequence& seq, SequenceTermAction action, int val = 0);
	/// \brief Update the termination behaviour of a specific sequence
	///
	/// Use this function overload to update a sequence termination to INSERT or STOP_INSERT mode,
	/// specifying the other sequence to insert before in the queue
	/// \param[in] seq A reference to an ImageSequence object to find in the queue and update
	/// \param[in] action The new action value to program the ImageSequence with
	/// \param[in] term_seq A pointer to the ImageSequence object to to insert before in the queue
	/// \return True if the update was carried out successfully
		bool UpdateTermination(ImageSequence& seq, SequenceTermAction action, const ImageSequence* term_seq);
	/// \brief Update the termination behaviour of a specific sequence
    ///
    /// This function is used to change the behaviour of a sequence after it has completed playback.  It can be used
    /// for example to modify a sequence set to RECYCLE mode so that next time it plays back, it is instead DISCARDed.
    /// If multiple identical sequences exist in the queue, the sequence most recently added (or most recently played)
    /// will be updated.
    /// \param[in] uuid An identifier that can be returned from GetSequenceUUID to mark a sequence for update
    /// \param[in] action The new action value to program the ImageSequence with
    /// \param[in] val An optional Termination value to apply to the sequence
		/// \return True if the update was carried out successfully
		bool UpdateTermination(const std::array<std::uint8_t, 16>& uuid, SequenceTermAction action, int val = 0);
	/// \brief Update the termination behaviour of a specific sequence
	///
	/// Use this function overload to update a sequence termination to INSERT or STOP_INSERT mode,
	/// specifying the other sequence to insert before in the queue
	/// \param[in] uuid An identifier that can be returned from GetSequenceUUID to mark a sequence for update
	/// \param[in] term The new action value to program the ImageSequence with (should be INSERT of STOP_INSERT)
	/// \param[in] term_uuid An identifier that can be returned from GetSequenceUUID to reference an ImageSequence object that is to be inserted before in the queue
	/// \return True if the update was carried out successfully
		bool UpdateTermination(const std::array<std::uint8_t, 16>& uuid, SequenceTermAction term, const std::array<std::uint8_t, 16>& term_uuid);

	/// \brief Relocate a sequence that is already in the queue to a different position in the queue
	///
	/// Use this function to Remove a sequence from its position in the sequence queue and re-insert it before
	/// another sequence in the queue.  If the same sequence is specified for source and destination, the queue is
	/// not modified.  This function cannot be used if the Sequence Queue is currently playing or is paused. To 
	/// move a sequence to the end, use the MoveSequenceToEnd() function.
	/// \param[in] dest A reference to the ImageSequence which must exist within the queue on the Controller (i.e. previously downloaded).
	/// The src sequence is inserted before the dest sequence
	/// \param[in] src A reference to the ImageSequence which must exist within the queue on the Controller which is to be moved.
	/// \return true if the move command was sent successfully
	/// \since 1.8.0
		bool MoveSequence(const ImageSequence& dest, const ImageSequence& src);
	/// \brief Relocate a sequence that is already in the queue to the end of the queue
	///
	/// This function is used when it is necessary to move a sequence from its current position to the end of the queue.
	/// \param[in] src A reference to the ImageSequence which must exist within the queue on the Controller which is to be moved.
	/// \return true if the move command was sent successfully
	/// \since 1.8.0
		bool MoveSequenceToEnd(const ImageSequence& src);
	//@}

	/// \brief Retrieve the status of the head pointer of the Sequence queue - its UUID, and the index within the list of sequence entries
	///
	/// This function can be called while the queue is running, paused or stopped and returns information to application
	/// software about the current position of the head pointer of the sequence queue on the Controller. Information is
	/// not returned directly by the function but instead an event is raised (SequenceEvents::SEQUENCE_POSITION) which the
	/// application should subscribe to.  The event is triggered as soon as the data is returned by the Controller.  Create an
	/// IEventHandler subclass with the EventAction<int, std::vector<std::uint8_t>> property to obtain the data, for which the
	/// vector data (the UUID) can be matched to the UUID of a sequence in the application, and the int is the index of the
	/// SequenceEntry within the Sequence that is currently being output.
	/// \return true if the get position command was sent successfully
	/// \since 1.8.0
		bool GetCurrentPosition();

    /// \name Sequence Event Signalling
    ///
    /// If the user application requires that new sequences are created and downloaded to the Sequence Queue
    /// while playback is operational, it may be advantageous to configure the SequenceManager to inform
    /// the application when sequences complete or when the sequence queue has emptied.  To do that, create
    /// an EventHandler derived class in your code and subscribe it to the SequenceManager using these functions
    /// and one of the SequenceEvent messages.  The handler will be called at the relevant time which will allow
    /// the application to synchronously update the Controller sequence queue with new information.
    ///
    /// \warning Subscribing to sequence events turns on the interrupt sending mechanism in the Controller in
    /// order to guarantee minimum latency from the event occurrence.  Since this involves sending Controller
    /// initiated messages to the SDK, it is inadvisable to subscribe to the SEQUENCE_START event if
    /// sequences are expected to be started at a rate greater than approx once every 10msec, as buffer
    /// overruns can occur which will lead to the breakdown of communications between the SDK and the iMS System.
    //@{
    ///
    /// \brief Subscribe a callback function handler to a given SequenceManager event
    ///
    /// SequenceManager can callback user application code when an event occurs in the
    /// sequence playback process.  Supported events are listed under SequenceEvents.  The
    /// callback function must inherit from the IEventHandler interface and override
    /// its EventAction() method.
    ///
    /// Use this member function call to subscribe a callback function to an SequenceManager event.
    /// For the period that a callback is subscribed, each time an event in the Controller sequence playback occurs
    /// that would trigger the subscribed SequenceManager event, the user function callback will be
    /// executed.
    /// \param[in] message Use the SequenceEvents::Event enum to specify an event to subscribe to
    /// \param[in] handler A function pointer to the user callback function to execute on the event trigger.
		void SequenceEventSubscribe(const int message, IEventHandler* handler);
    /// \brief Unsubscribe a callback function handler from a given SequenceManager event
    ///
    /// Removes all links to a user callback function from the Event Trigger map so that any
    /// events that occur in the Controller sequence playback following the Unsubscribe request
    /// will no longer execute that function
    /// \param[in] message Use the SequenceEvents::Event enum to specify an event to unsubscribe from
    /// \param[in] handler A function pointer to the user callback function that will no longer execute on an event
		void SequenceEventUnsubscribe(const int message, const IEventHandler* handler);
	private:
		// Makes this object non-copyable
		SequenceManager(const SequenceManager &);
		const SequenceManager &operator =(const SequenceManager &);

		class Impl;
		Impl * p_Impl;
	};
}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
