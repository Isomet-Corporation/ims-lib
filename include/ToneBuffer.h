/*-----------------------------------------------------------------------------
/ Title      : Isomet Tone Buffer Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ToneBuffer/h/ToneBuffer.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2016-02-24
/ Last update: $Date: 2020-07-30 21:50:24 +0100 (Thu, 30 Jul 2020) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 465 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2016 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2016-02-24  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

///
/// \file ToneBuffer.h
///
/// \brief Class for storing an array of Synthesiser tones
///
/// \author Dave Cowan
/// \date 2016-02-24
/// \since 1.0
/// \ingroup group_ToneBuffer
///

#ifndef IMS_TONEBUFFER_H__
#define IMS_TONEBUFFER_H__

#include "IMSSystem.h"
#include "IEventHandler.h"
#include "IMSTypeDefs.h"
#include "IBulkTransfer.h"
#include "Image.h"
#include "FileSystem.h"

#include <array>

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
	/// \brief TBEntry is synonymous with ImagePoint
	/// An entry in the Tone Buffer contains four FAPs, one per output channel and is therefore
	/// comparable to a single ImagePoint making up one entry in an Image.
	/// \since 1.1
	using TBEntry = ImagePoint;

	///
	/// \class ToneBufferEvents ToneBuffer.h include\ToneBuffer.h
	/// \brief All the different types of events that can be triggered by the ToneBuffer and ToneBufferDownload classes.
	///
	/// Some events contain integer parameter data which can be processed by the IEventHandler::EventAction
	/// derived method
	/// \author Dave Cowan
	/// \date 2016-02-24
	/// \since 1.1
	class LIBSPEC ToneBufferEvents
	{
	public:
		/// \enum Events List of Events raised by the ToneBuffer Class and ToneBuffer Table Downloader
		enum Events {
			/// Event raised when ToneBufferDownload has confirmed that the iMS Controller received all of the ToneBuffer data
			DOWNLOAD_FINISHED,
			/// Event raised each time the ToneBufferDownload class registers an error in the download process
			DOWNLOAD_ERROR,
			Count
		};
	};

	///
    /// \class ToneBuffer ToneBuffer.h include/ToneBuffer.h
    /// \brief An array of 4-channel FAP Tones stored in memory on the Synthesiser
    ///
    /// \author Dave Cowan
    /// \date 2016-02-24
    /// \since 1.1
    ///
	class LIBSPEC ToneBuffer {
	public:
	///
	/// \name Tone Buffer Array
	///
	/// \brief \c TBArray is the internal type definition used for storing a buffer of TBEntry 's in the Image
		using TBArray = std::array < TBEntry, 256>;
	///
	/// \name Iterator Specification
    ///
    /// Use these iterators when you want to work with ranges of Tone Buffer entries stored within a tone buffer.
    /// Iterators can be used to access elements at an arbitrary offset position
    /// relative to the element they point to
    ///
    /// Two types of iterators are supported; both are random access iterators.  Dereferencing const_iterator
    /// yields a reference to a constant element in the ToneBuffer (const TBEntry&).
    ///
    //@{
    /// \brief Iterator defined for user manipulation of internal TBArray
		typedef TBArray::iterator iterator;
    ///
    /// \brief Const Iterator defined for user readback of internal TBArray
		typedef TBArray::const_iterator const_iterator;
    //@}
    ///

    ///
    /// \name Constructors & Destructors
    //@{
		/// \brief Empty Constructor
    ///
		/// \param[in] name The optional descriptive name to apply to the Tone Buffer
		ToneBuffer(const std::string& name = "");
    ///
		/// \brief Fill Constructor
    ///
    /// Use this constructor to generate a Tone Buffer with each entry initialised
    /// to the value of \c tbe
    ///
    /// \param[in] tbe The TBEntry that will fill each of the elements of the TBArray
	/// \param[in] name The optional descriptive name to apply to the Tone Buffer
		/// \since 1.1
		ToneBuffer(const TBEntry& tbe, const std::string& name = "");
    ///
	/// \brief Non-volatile Memory Constructor
	///
	/// Use this constructor to preload the ToneBuffer with data recalled from an entry in the Synthesiser
	/// FileSystem.
	///
	/// \param[in] entry the entry in the FileSystem Table from which to recall a ToneBuffer
	/// \param[in] name The optional descriptive name to apply to the Tone Buffer
		/// \since 1.1
		ToneBuffer(const int entry, const std::string& name = "");

	///
    /// \brief Copy Constructor
    ///
		ToneBuffer(const ToneBuffer &);
    ///
    /// \brief Assignment Constructor
    ///
		ToneBuffer &operator =(const ToneBuffer &);

    ///
    /// \brief Destructor
    ///
		~ToneBuffer();
    ///
    //@}
    ///

    ///
    /// \name ToneBuffer Boundary Iterators
    //@{
    /// \brief Returns an iterator pointing to the first element in the TBArray container.
    ///
    /// \return An iterator to the beginning of the TBArray container.
    /// \since 1.1
		iterator begin();
    /// \brief Returns an iterator referring to the past-the-end element in the TBArray container.
    ///
    /// The past-the-end element is the theoretical element that would follow the last element
    /// in the TBArray container. It does not point to any element, and thus shall not be dereferenced.
    ///
    /// Because the ranges used by functions of the standard library do not include the element
    /// pointed by their closing iterator, this function can be used in combination with
    /// TBArray::begin to specify a range including all the elements in the container.
    ///
    /// \return An iterator to the element past the end of the TBArray.
    /// \since 1.1
		iterator end();
	/// \brief Returns a const_iterator pointing to the first element in the TBArray container.
	/// \return A const_iterator to the beginning of the TBArray container.
	/// \since 1.2.5
		const_iterator begin() const;
	/// \brief Returns a const_iterator referring to the past-the-end element in the TBArray container.
	/// \return A const_iterator to the element past the end of the buffer.
	/// \since 1.2.5
		const_iterator end() const;
	/// \brief Returns a const_iterator pointing to the first element in the TBArray container.
    /// \return A const_iterator to the beginning of the TBArray container.
    /// \since 1.1
		const_iterator cbegin() const;
    /// \brief Returns a const_iterator referring to the past-the-end element in the TBArray container.
    /// \return A const_iterator to the element past the end of the buffer.
    /// \since 1.1
		const_iterator cend() const;
    //@}

	///
	/// \name Tone Buffer Unique Identifier
	//@{
	///
	/// \brief Returns a vector representing the Unique Identifier assigned to the Tone Buffer object
	/// \return UUID as an array of uint8_t's
		const std::array<std::uint8_t, 16> UUID() const;
	//@}
	///

    ///
    /// \name TBArray Operators
    //@{
    ///
	/// \brief Random Access to a TBEntry in the TBArray
    ///
    /// The array subscript operator is defined to permit applications to
    /// access a TBEntry at any arbitrary position for readback.
    /// \param[in] idx Integer offset into the TBArray with respect to the first element in the array (ToneBuffer::cbegin())
    /// \return A const reference to a TBEntry.
    /// \since 1.1
		const TBEntry& operator[](std::size_t idx) const;
	/// \brief Random Write Access to a TBEntry in the TBArray
	///
	/// The array subscript operator is defined to permit applications to
	/// access a CompensationPoint at any arbitrary position for modification.
	/// \param[in] idx Integer offset into the TBArray with respect to the first element in the array (ToneBuffer::begin())
	/// \return A reference to a TBEntry.
	/// \since 1.1
		TBEntry& operator[](std::size_t idx);
	///
	/// \brief Equality Operator checks ToneBuffer contents for equivalence
	///
	/// \param[in] rhs A ToneBuffer object to perform the comparison with
	/// \return True if the supplied ToneBuffer is identical to this one.
	/// \since 1.1
		bool operator==(ToneBuffer const& rhs) const;
    //@}

    ///
    /// \name ToneBuffer Size
    //@{
    ///
    /// \brief Returns the number of elements in the ToneBuffer (non-modifiable)
    ///
    /// \return The number of elements in the ToneBuffer
    /// \since 1.1
		const std::size_t Size() const;
	//@}
    ///

	///
	/// \name Tone Buffer Description
	//@{
	///
	/// \brief A string stored with the Tone Buffer to aid human users in identifying the purpose of the buffer
	///
	/// A descriptive string can be set alongside the Tone Buffer to allow users to identify and differentiate
	/// between Tone Buffers without having to browse through the data.  The description is optional, and if,
	/// not used, the description will simply default to null.
	///
		const std::string& Name() const;
		std::string& Name();
	private:
		class Impl;
		Impl * p_Impl;
	};

	///
	/// \class ToneBufferDownload ToneBuffer.h include\ToneBuffer.h
	/// \brief Provides a mechanism for downloading ToneBuffer's to a Synthesiser's LTB memory
	/// \author Dave Cowan
	/// \date 2016-02-24
	/// \since 1.1
	class LIBSPEC ToneBufferDownload : public IBulkTransfer
	{
	public:
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for ToneBufferDownload Object
		///
		/// The pre-requisites for an ToneBufferDownload object to be created are:
		///   (1) - an IMSSystem object, representing the configuration of an iMS target to which the ToneBuffer
		/// is to be downloaded.
		///   (2) - a complete ToneBuffer object to download to the iMS target.
		///
		/// ToneBufferDownload stores const references to both.  This means that both must exist before the
		/// ToneBufferDownload object, and both must remain valid (not destroyed) until the ToneBufferDownload
		/// object itself is destroyed.  Because they are stored as references, the IMSSystem and ToneBuffer
		/// objects themselves may be modified after the construction of the ToneBufferDownload object.
		///
		/// Once constructed, the object can neither be copied or assigned to another instance.
		///
		/// \param[in] ims A const reference to the iMS System which is the target for downloading the Image
		/// \param[in] tb A const reference to the ToneBuffer which shall be downloaded to the target
		/// \since 1.1
		ToneBufferDownload(std::shared_ptr<IMSSystem> ims, const ToneBuffer& tb);
		///
		/// \brief Destructor for ToneBufferDownload Object
		~ToneBufferDownload();
		//@}

		/// \name Bulk Transfer Initiation
		//@{
		/// \brief Begins download of entire ToneBuffer to LTB memory on Synthesiser
		/// \since 1.1
		bool StartDownload();
		/// \brief Begins download of partial ToneBuffer to LTB memory on Synthesiser beginning at \c first
		/// TBEntry and continuing until \c last TBEntry (including first but not including last)
		/// \since 1.1
		bool StartDownload(ToneBuffer::const_iterator first, ToneBuffer::const_iterator last);
		/// \brief Begins download of partial ToneBuffer to LTB memory on Synthesiser beginning at \c index
		/// TBEntry and continuing for \c count entries
		/// \since 1.9
		bool StartDownload(std::size_t index, std::size_t count);
		/// \brief Downloads a single TBEntry to LTB memory on Synthesiser
		/// \since 1.1
		bool StartDownload(ToneBuffer::const_iterator single);
		/// \brief Downloads a single TBEntry to LTB memory on Synthesiser
		/// \since 1.9
		bool StartDownload(std::size_t index);
		/// \brief No Verify is possible. Always returns false
		/// \since 1.1
		bool StartVerify() { return false; };
		/// \brief No Verify is possible. Always return -1
		/// \since 1.1
		int GetVerifyError() { return -1; };
		///
		//@}

		///
		/// \name Event Notifications
		//@{
		///
		/// \brief Subscribe a callback function handler to a given ToneBufferEvents entry
		///
		/// ToneBufferDownload can callback user application code when an event occurs in the
		/// download process.  Supported events are listed under ToneBufferEvents.  The
		/// callback function must inherit from the IEventHandler interface and override
		/// its EventAction() method.
		///
		/// Use this member function call to subscribe a callback function to an ToneBufferEvents entry.
		/// For the period that a callback is subscribed, each time an event in ToneBufferDownload occurs
		/// that would trigger the subscribed ToneBufferEvents entry, the user function callback will be
		/// executed.
		/// \param[in] message Use the ToneBufferEvents::Event enum to specify an event to subscribe to
		/// \param[in] handler A function pointer to the user callback function to execute on the event trigger.
		/// \since 1.1
		void ToneBufferDownloadEventSubscribe(const int message, IEventHandler* handler);
		/// \brief Unsubscribe a callback function handler from a given ToneBufferEvents entry
		///
		/// Removes all links to a user callback function from the Event Trigger map so that any
		/// events that occur in the ToneBufferDownload object following the Unsubscribe request
		/// will no longer execute that function
		/// \param[in] message Use the ToneBufferEvents::Event enum to specify an event to unsubscribe from
		/// \param[in] handler A function pointer to the user callback function that will no longer execute on an event
		/// \since 1.1
		void ToneBufferDownloadEventUnsubscribe(const int message, const IEventHandler* handler);
		//@}

		/// \name Store in Synthesiser Non-Volatile Memory
		//@{
		/// \brief Store ToneBuffer contents to non-volatile memory on the synthesiser
		///
		/// The contents of this ToneBuffer can be stored to an area of non-volatile memory on the
		/// Synthesiser for retrieval at a future time, including after subsequent power cycles.  The
		/// data stored can be used to select between alternative ToneBuffers without needing to
		/// recalculate or download from Software.
		///
		/// The table can be flagged to be used as a default at startup in which case the Synthesiser will
		/// use the contents as a default ToneBuffer program allowing the Synthesiser to be used
		/// with no connection to a host system.
		///
		/// \param[in] def mark the entry as a default and the Synthesiser will attempt 
		/// to program the data to the Local Tone Buffer on power up.
		/// \param[in] FileName a string to tag the download with in the File System Table (limited to 8 chars)
		/// \returns the index in the File System Table where the data was stored or -1 if the operation failed
		/// \since 1.1
		const FileSystemIndex Store(const std::string& FileName, FileDefault def = FileDefault::NON_DEFAULT) const;
		//@}
	private:
		// Makes this object non-copyable
		ToneBufferDownload(const ToneBufferDownload &);
		const ToneBufferDownload &operator =(const ToneBufferDownload &);

		class Impl;
		Impl * p_Impl;
	};

	///
	/// \struct ToneSequenceEntry ToneBuffer.h include/ToneBuffer.h
	/// \brief Inserts a ToneBuffer playback into an ImageSequence
	///
	/// An ToneSequenceEntry object can be created by application software to specify the parameters by which
	/// a ToneBuffer is played back during an ImageSequence.  It is derived from the SequenceEntry base struct and specifies that
	/// the Sequence should play a Tone from the ToneBuffer at this point within the Sequence.
	///
	/// During a ToneSequenceEntry, any external clock input is ignored.  Likewise, the internal clock source is disabled.  The
	/// RF output only changes through selection of a different ToneBuffer index, which can be done either by software control or
	/// through a hardware signal input.
	///
	/// The ImageSequence advances from the ToneSequenceEntry to the next entry when a Trigger is received, just like the ImageSequenceEntry
	/// except that the ImageTrigger::CONTINUOUS and ImageTrigger::POST_DELAY triggers have no meaning because the ToneSequenceEntry has 
	/// no clock and therefore no context of timing.  If the ImageSequence is started with CONTINUOUS or POST_DELAY trigger propagation,
	/// the Controller implicitly converts these to ImageTrigger::HOST and the software must send a trigger manually using 
	/// SequenceManager::UserHostTrigger() to continue the sequence.
	///
	/// To facilitate the software knowing when a ToneSequenceEntry is playing and therefore when a manual trigger may be required,
	/// applications should subscribe to the SequenceEvents::SEQUENCE_TONE event using the SequenceManager.
	///
	/// If the ImageSequence is using external triggers (ImageTrigger::EXTERNAL), then this will continue to work as normal.
	///
	/// Additional parameters related to Tone playback that can be specified include
	///
	/// \li ControlSource: When the ToneSequenceEntry is active, it can be controlled for tone selection either from software (INTERNAL) 
	/// or by hardware using 4 I/O pins (EXTERNAL) or 8 I/O pins (EXTERNAL_EXTENDED).
	/// \li Initial Index: which of the 256 tones in the buffer to output from first when using INTERNAL control mode.
	/// 
	/// \author Dave Cowan
	/// \date 2020-06-08
	/// \since 1.8.0
	///
	struct LIBSPEC ToneSequenceEntry : SequenceEntry
	{
	///
	/// \name Constructors & Destructor
	///
	//@{
	///
	/// \brief Default Constructor
		ToneSequenceEntry();
	/// \brief Construct ToneSequenceEntry object from ToneBuffer object in application software.
	///
	/// The user can optionally specify the control source for selecting tone entries, and if using software control (ToneBufferControl::HOST),
	/// which should be the first index in the tone buffer to output
	///
	/// \param tb A reference to the ToneBuffer object which is to be output in the Sequence (must have been downloaded to Controller memory before playback)
	/// \param tbc An optional parameter specifying whether the tone buffer is controlled by software (SignalPath::ToneBufferControl::HOST),
	/// 4 hardware pins (SignalPath::ToneBufferControl::EXTERNAL) or 8 h/w pins (SignalPath::ToneBufferControl::EXTERNAL_EXTENDED).  
	/// SignalPath::ToneBufferControl::OFF is invalid and will result in software control, as SignalPath::ToneBufferControl::HOST
	/// \param initial_index  Which of the 256 tone buffer entries should be output first when the ToneSequenceEntry begins
		ToneSequenceEntry(const ToneBuffer& tb, SignalPath::ToneBufferControl tbc = SignalPath::ToneBufferControl::HOST, const unsigned int initial_index = 0);
	/// \brief Copy Constructor from another ToneSequenceEntry
		ToneSequenceEntry(const ToneSequenceEntry&);
	/// \brief Assignment Constructor
		ToneSequenceEntry& operator =(const ToneSequenceEntry&);
	/// \brief Copy Constructor from another object derived from the base SequenceEntry class.  You should not need this.
		ToneSequenceEntry(const SequenceEntry& entry);
	/// \brief Destructor
		~ToneSequenceEntry();
	//@}

	/// \brief Equality Operator checks ToneSequenceEntry object for equivalence
	///
	/// \param[in] rhs An SequenceEntry object to perform the comparison with
	/// \return True if the supplied SequenceEntry is also an ToneSequenceEntry, and is identical to this one.
		bool operator==(SequenceEntry const& rhs) const;

	/// \name Tone Sequence Entry Parameters
	///
	//@{
	/// \brief Returns the method of control for which this ToneSequenceEntry has been configured to use
	/// \return a ToneBufferControl object - SignalPath::ToneBufferControl::HOST, SignalPath::ToneBufferControl::EXTERNAL or SignalPath::ToneBufferControl::EXTERNAL_EXTENDED
		SignalPath::ToneBufferControl ControlSource() const;
	/// \brief Returns the first index of the tone buffer which thie ToneSequenceEntry has been configured to output at the beginning of the SequenceEntry.
	/// \brief the Tone Buffer initial index between 0 and 255.
		int InitialIndex() const;
	//@}
	private:
		class Impl;
		Impl* p_Impl;
	};

}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
