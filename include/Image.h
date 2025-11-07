/*-----------------------------------------------------------------------------
/ Title      : Isomet Image Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ImageOps/h/Image.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2025-01-08 22:08:23 +0000 (Wed, 08 Jan 2025) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 657 $
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
/// \file Image.h
///
/// \brief Classes for storing sequences of synchronous multi-channel RF drive data
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_Image
///

#ifndef IMS_IMAGE_H__
#define IMS_IMAGE_H__

#include "IMSTypeDefs.h"
//#include "ToneBuffer.h"
#include "SignalPath.h"
#include "Containers.h"
#include <deque>
#include <list>
#include <array>
#include <chrono>
#include <ctime>

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
    /// \class ImagePoint Image.h include/Image.h
    /// \brief Stores 4 FAP Triads containing frequency, amplitude and phase data for 4 RF channels
    ///
    /// An ImagePoint uniquely defines the required output drive setting for each of the 4 RF
    /// Channels output by the iMS Synthesiser.  Each channel (from 1 to 4) is given its own
    /// FAP member variable, which is a combination of Frequency, Amplitude (Percent) and Phase (Degrees) data.
    ///
    /// At any instantaneous moment, the status of the 4 iMS RF driver outputs is representable
    /// by a single ImagePoint
    ///
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC ImagePoint
	{
	public:
    ///
    /// \brief Default Constructor
    ///
		ImagePoint() : ImagePoint(FAP(0.0, 0.0, 0.0)) {};
    ///
    /// \brief Constructor with Uniform Channel Data
    ///
    /// Construct ImagePoint with identical FAP for each channel
    /// \since 1.0
		ImagePoint(FAP fap);
    ///
    /// \brief Constructor with Independent Channel Data
    ///
    /// Construct ImagePoint with full specification of FAP for each channel
    /// \since 1.0
		ImagePoint(FAP ch1, FAP ch2, FAP ch3, FAP ch4);
	///
	/// \brief Constructor with Uniform Channel Data and Synchronous Data
	///
	/// Construct ImagePoint with identical FAP for each channel
	/// \since 1.2
		ImagePoint(FAP fap, float synca, unsigned int syncd);
	///
	/// \brief Constructor with Independent Channel Data and Synchronous Data
	///
	/// Construct ImagePoint with full specification of FAP for each channel
	/// \since 1.2
		ImagePoint(FAP ch1, FAP ch2, FAP ch3, FAP ch4, float synca_1, float synca_2, unsigned int syncd);

		~ImagePoint() {};

	/// \brief Equality Operator checks ImagePoint object for equivalence
	///
	/// \param[in] rhs An ImagePoint object to perform the comparison with
	/// \return True if the supplied ImagePoint is identical to this one.
	/// \since 1.1
		bool operator==(ImagePoint const& rhs) const;
	///
    /// \name Get/Set FAP data for the image point
    //@{
    ///
    /// \brief Retrieves Frequency, Amplitude (Percent) and Phase (Degrees) data for one RF channel
    /// \return FAP triad for the specified RF channel
    /// \since 1.0
		const FAP& GetFAP(const RFChannel) const;

    ///
    /// \brief Assigns Frequency, Amplitude (Percent) and Phase (Degrees) data for one RF channel
    /// \since 1.0
		void SetFAP(const RFChannel, const FAP&);

	///
	/// \brief Assigns Frequency, Amplitude (Percent) and Phase (Degrees) data for one RF channel
	/// \since 1.2
		FAP& SetFAP(const RFChannel);

	///
    /// \brief Assigns Frequency, Amplitude (Percent) and Phase (Degrees) data for all RF channels
    /// \since 1.0
		void SetAll(const FAP&);
	//@}
    ///

	///
	/// \name Get/Set Synchronous data for the image point
	//@{
	///
	/// \brief Retrieve Analogue Synchronous Data
	/// \param[in] index 0 or 1 references 2 independent synchronous data variables
	/// \return a floating point value between 0 and 1
	/// \since 1.2
		const float& GetSyncA(int index) const;

	/// \brief Assign Analogue Synchronous Data
	/// \param[in] index 0 or 1 references 2 independent synchronous data variables
	/// \param[in] value The floating point value to assign, will be clamped within the range 0 <= value <= 1
	/// \since 1.2
		void SetSyncA(int index, const float& value);

	///
	/// \brief Retrieve Digital Synchronous Data
	/// \return an unsigned integer value representing the synchronous data
	/// \since 1.2
		const unsigned int& GetSyncD() const;

	/// \brief Assign Digital Synchronous Data
	/// \param[in] value The unsigned integer value to assign
	/// \since 1.2
		void SetSyncD(const unsigned int& value);

	private:
		FAP m_fap[4];
		float m_synca[2];
		unsigned int m_syncd;
	};

	///
	/// \class ImageFormat Image.h include/Image.h
	/// \brief Configures formatting of software Image class to binary Image stored in iMS system memory
	///
	/// An Image created in software uses real world values for all parameters (Frequency, Amplitude, Phase
	/// and Sync Data).  Before downloading the Image to an iMS System, the user must specify what binary
	/// representation should be used by the iMS to interpret the image data that is sent to it.
	/// 
	/// Binary representation is a combination of the hardware capabilities of the attached Synthesiser 
	/// (made available in the IMSSynthesiser::Capabilities class) and a trade-off between Image
	/// speed (update rate) and precision (bit depth).
	/// 
	/// If not specified, then default values are assumed which provide a good compromise between update 
	/// rate and precision.
	///
	/// \author Dave Cowan
	/// \date 2025-01-08
	/// \since 1.8.12
	///
	class LIBSPEC ImageFormat
	{
	public:
		/// 
		/// \brief Default Constructor
		/// 
		/// 4 Independent Channels
		/// 2 Frequency Bytes
		/// 1 Amplitude Byte
		/// 2 Phase Bytes
		/// Amplitude and Phase Enabled
		/// Sync Digital Enabled
		/// 2 Sync Analog Channels Enabled
		/// 2 Sync Bytes
		/// 
		ImageFormat();
		/// 
		/// \brief iMS Specific Constructor
		/// 
		/// As default constructor but sets channel number according to iMS hardware channels,
		/// phase and amplitude depth according to iMS capability.  Frequency depth is set to 2 bytes
		/// regardless of iMS capability for backwards compatibility.
		/// 
		ImageFormat(std::shared_ptr<IMSSystem> ims);
		~ImageFormat() {}

		/// \brief Copy Constructor
		ImageFormat(const ImageFormat&);
		/// \brief Assignment Constructor
		ImageFormat& operator =(const ImageFormat&);

		/// \brief Set the number of RF channels to use from the Image data
		///
		/// Images allow up to 4 RF channels to be programmed although not all iMS systems support
		/// 4 channels.  This parameter instructs the library how many channels of the Image data
		/// should be stored in iMS system memory.  A smaller number than the channel count of the 
		/// iMS system can be used to increase update rate if not all channels are needed.
		//@{
		/// \brief Get Channel Count parameter
		int Channels() const;
		/// \brief Set Channel Count parameter
		void Channels(int value);
		//@}

		/// \brief Parameter Precision
		///
		/// The number of bytes store in iMS memory per parameter can be changed to optimise the
		/// system for increased precision where necessary or to reduce precision and increase
		/// speed where this is preferable.
		//@{
		/// \brief Get Frequency Parameter Precision (1 - 4 bytes)
		int FreqBytes() const;
		/// \brief Set Frequency Parameter Precision
		void FreqBytes(int value);

		/// \brief Get Amplitude Parameter Precision (1 - 3 bytes)
		int AmplBytes() const;
		/// \brief Set Amplitude Parameter Precision
		void AmplBytes(int value);

		/// \brief Get Phase Parameter Precision (1 - 3 bytes)
		int PhaseBytes() const;
		/// \brief Set Phase Parameter Precision
		void PhaseBytes(int value);

		/// \brief Get Sync Data Parameter Precision (1 - 4 bytes)
		int SyncBytes() const;
		/// \brief Set Sync Data Parameter Precision (1 - 4 bytes)
		void SyncBytes(int value);
		//@}

		/// \brief Return whether Image Amplitude Programming is enabled.
		bool EnableAmpl() const;
		/// \brief Enable Amplitude Programing as part of Image data (if disabled, compensation 
		/// programming can still be used to create frequency dependent amplitude profiles)
		void EnableAmpl(bool value);

		/// \brief Return whether Image Phase Programming is enabled.
		bool EnablePhase() const;
		/// \brief Enable Phase Programing as part of Image data (if disabled, compensation 
		/// programming can still be used to create frequency dependent phase profiles)
		void EnablePhase(bool value);

		/// \brief Return number of Analogue Sync Data Channels (1 - 2)
		int SyncAnlgChannels() const;
		/// \brief Set number of Analogue Sync Data Channels stored as part of image
		void SyncAnlgChannels(int value);

		/// \brief Return whether Digital Sync Data is enabled as part of Image
		bool EnableSyncDig() const;
		/// \brief Enable Digital Sync Data as part of Image in iMS memory
		void EnableSyncDig(bool value);

		/// \brief Return whether adjacent image channel pairs are combined
		bool CombineChannelPairs() const;
		/// \brief If true, the data for channel 1 in the Image is used for RF channels 1 + 2 and the data for channel 3 is used for RF 3 + 4
		void CombineChannelPairs(bool value);

		/// \brief Return whether all RF image channels are combined
		bool CombineAllChannels() const;
		/// \brief If true, the data for channel 1 in the Image is used for all RF channels of the synthesiser
		void CombineAllChannels(bool value);

		/// \brief Returns a unique code that describes the ImageFormat state to the hardware. Not required by user code.
		unsigned int GetFormatSpec() const;
	private:
		class Impl;
		Impl* p_Impl;
	};

    ///
    /// \class Image Image.h include/Image.h
    /// \brief A sequence of ImagePoints played out sequentially by the Controller and driven by the Synthesiser
    ///
    /// An Image contains a list of ImagePoints and a default Clock Rate. Its length is limited only by
    /// the available memory of the Controller onto which it is downloaded.
    ///
    /// It can be created, copied, modified, merged and more in software on the host running the SDK, stored to
    /// disk inside an ImageProject (from SDK rev1.3) and transferred to/from the iMS Controller
    /// using the ImageDownload mechanism.
    ///
    /// Once in memory on a Controller, the Image can be played back.  This can be triggered by software,
    /// or by an external trigger input signal applied to the Controller.  At the start of playback,
    /// the first ImagePoint is programmed by the Controller into the Synthesiser which updates the
    /// RF output of all 4 channels.
    ///
    /// The Controller then progresses through the Image sequence ImagePoint by ImagePoint, updating
    /// the Synthesiser's RF output as it goes.  The Image progression can either propagate using
    /// an internal clock or under the control of an external signal applied to the Controller.  If
    /// using the internal clock, the clock is programmed at the point of downloading the Image with
    /// the default value for Clock Rate which is stored alongside the PointList data in the Image object.
    ///
    /// If using an Image alongside other Images in an ImageGroup, for Controllers that support it,
    /// the internal ClockRate may be overriden by the value programmed into the SequenceTable that is
    /// a part of the ImageGroup object.
    ///
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC Image : public DequeBase<ImagePoint> {
    /// \name Iterator Specification
    ///
    /// Use these iterators when you want to work with ranges of points stored within images,
    /// insert or remove at specific locations or simply to iteratively read through a PointList
    /// within an image.  Iterators can be used to access elements at an arbitrary offset position
    /// relative to the element they point to
    ///
    /// Two types of iterators are supported; both are random access iterators.  Dereferencing const_iterator
    /// yields a reference to a constant element in the PointList (const ImagePoint&).
    ///
    /// For example:
    ///
    /// \code
    /// #include <iostream>
    /// Image img1();  // Empty Image
    ///
    /// // Create 30 points in the image increasing linearly from 70MHz to 99MHz
    /// for (int i=70, i<100; i++)
    /// {
    ///    img1.AddPoint(ImagePoint(FAP((double)i,100.0,0.0)));
    /// }
    ///
    /// for (Image::const_iterator it = img1.cbegin(); it != img1.cend(); ++it)
    /// {
    ///    std::cout << (*it).GetFAP(1).Frequency << "MHz" << std::endl;
    /// }
    /// \endcode
    ///
    /// will yield the output:
    ///
    /// \code
    /// 70MHz
    /// 71MHz
    /// 72MHz
    ///  <...>
    /// 99MHz
    /// \endcode
    ///
	public:
    ///
    /// \name Constructors & Destructors
    //@{
	/// \brief Empty Constructor
    ///
	/// \param[in] name The optional descriptive name to apply to the image
		Image(const std::string& name = "");
    ///
	/// \brief Fill Constructor
    ///
    /// Use this constructor to generate an Image with \c nPts number of points, each one initialised
    /// to the value of \c pt
    ///
    /// \param[in] nPts The size of the Image PointList after construction
    /// \param[in] pt The ImagePoint that will fill each of the new elements of the PointList
	/// \param[in] name The optional descriptive name to apply to the image
    /// \since 1.0
		Image(size_t nPts, const ImagePoint& pt, const std::string& name = "");
    ///
	/// \brief Fill Constructor with Internal Clock Initialisation
    ///
    /// Use this constructor to generate an Image with \c nPts number of points, each one initialised
    /// to the value of \c pt and with the Internal default Clock Rate initialised to \c f
    ///
    /// \param[in] nPts The size of the Image PointList after construction
    /// \param[in] pt The ImagePoint that will fill each of the new elements of the PointList
    /// \param[in] f The default Clock Rate that the Image will playback when using the Internal Clock mode
	/// \param[in] name The optional descriptive name to apply to the image
	/// \since 1.0
		Image(size_t nPts, const ImagePoint& pt, const Frequency& f, const std::string& name = "");
	///
	/// \brief Fill Constructor with External Clock Divider Initialisation
	///
	/// Use this constructor to generate an Image with \c nPts number of points, each one initialised
	/// to the value of \c pt and with the External default Clock Divider Ratio initialised to \c div
	///
	/// \param[in] nPts The size of the Image PointList after construction
	/// \param[in] pt The ImagePoint that will fill each of the new elements of the PointList
	/// \param[in] div The default Clock Divider Ratio that the Image will apply to the External Clock when using the External Clock mode
	/// \param[in] name The optional descriptive name to apply to the image
	/// \since 1.0
		Image(size_t nPts, const ImagePoint& pt, const int div, const std::string& name = "");
	///
    /// \brief Range Constructor
    ///
    /// Use this constructor to copy a range of ImagePoints from another Image
    /// For example,
    /// \code
    /// // Create an image with 1,024 points initialized to 70MHz, 100%
    /// Image img1 (1024, ImagePoint(FAP(70.0,100.0,0.0)));
    /// // Copy the first 500 points into a second image
    /// Image img2 (img1.begin(), img1.begin()+500);
    /// \endcode
    ///
    /// \param[in] first An iterator that points to the first ImagePoint of a range to construct the new Image from
    /// \param[in] last An iterator that points to the element after the last ImagePoint of a range to construct the new Image from
	/// \param[in] name The optional descriptive name to apply to the image
	/// \since 1.0
    ///
		Image(const_iterator first, const_iterator last, const std::string& name = "");
    ///
    /// \brief Range Constructor with Internal Clock Initialisation
    ///
    /// Use this constructor to copy a range of ImagePoints from another Image and set the internal
    /// default clock frequency
    ///
    /// \param[in] first An iterator that points to the first ImagePoint of a range to construct the new Image from
    /// \param[in] last An iterator that points to the element after the last ImagePoint of a range to construct the new Image from
    /// \param[in] f The default Clock Rate that the Image will playback when using the Internal Clock mode
	/// \param[in] name The optional descriptive name to apply to the image
	/// \since 1.0
    ///
		Image(const_iterator first, const_iterator last, const Frequency& f, const std::string& name = "");
	///
	/// \brief Range Constructor with External Clock Initialisation
	///
	/// Use this constructor to copy a range of ImagePoints from another Image and set the external
	/// default clock divider ratio
	///
	/// \param[in] first An iterator that points to the first ImagePoint of a range to construct the new Image from
	/// \param[in] last An iterator that points to the element after the last ImagePoint of a range to construct the new Image from
	/// \param[in] div The default Clock Divider Ratio that the Image will apply to the external clock signal when using the External Clock mode
	/// \param[in] name The optional descriptive name to apply to the image
	/// \since 1.0
	///
		Image(const_iterator first, const_iterator last, const int div, const std::string& name = "");

		///
    /// \brief Copy Constructor
    ///
		Image(const Image &);
    ///
    /// \brief Assignment Constructor
    ///
		Image &operator =(const Image &);

    ///
    /// \brief Destructor
    ///
		~Image();
    ///
    //@}
    ///

	///
    /// \name Insert/Add ImagePoints
    //@{
    ///
    /// \brief Add a single new ImagePoint at the end of the Image
    ///
    /// Extends the length of the Image by one ImagePoint and copies to it the data supplied in
    /// the const reference \c pt
    ///
    /// Equivalent to
    /// \code
    /// img.InsertPoint(img.end(), 1, pt);
    /// \endcode
    /// \param[in] pt The ImagePoint to append to the end of the Image
    /// \since 1.0
		void AddPoint(const ImagePoint& pt);
    ///
    ///  \brief Inserts a single new element into the PointList
    ///
    ///  The ImagePoint pt is inserted before the element pointed to by the iterator it.
    /// \param[in] it An ImagePoint will be inserted before the element pointed to by this iterator.
    /// \param[in] pt The ImagePoint to insert into the Image
    /// \since 1.0
		iterator InsertPoint(iterator it, const ImagePoint& pt);
    ///
    /// \brief Inserts multiple copies of an element into the PointList
    ///
    /// \c nPts copies of the ImagePoint \c pt are inserted into the PointList before the element pointed
    /// to be iterator \c it
    /// \param[in] it Multiple ImagePoints will be inserted before the element pointed to by this iterator.
    /// \param[in] nPts The number of copies of \c pt to insert
    /// \param[in] pt The ImagePoint to insert multiple copies of into the Image
    /// \since 1.0
		void InsertPoint(iterator it, size_t nPts, const ImagePoint& pt);
    ///
    /// \brief Inserts a range of ImagePoints into the PointList
    ///
    /// All of the ImagePoints located between first and last are copied in order into the PointList
    /// starting before the element pointed to by iterator it.
    ///
    /// For example,
    /// \code
    /// // Create an image with 4 points initialised to 70MHz
    /// Image img1( 4, ImagePoint(FAP(70.0,100.0,0.0)));
    /// // Create an image with 3 points initialised to 100MHz
    /// Image img2( 3, ImagePoint(FAP(100.0,100.0,0.0)));
    /// // Insert all of img2 in the middle of img1
    /// img.InsertPoint(img1.begin()+2, img2.begin(), img2.end());
    /// \endcode
    ///
    /// img2 contains [70, 70, 100, 100, 100, 70, 70]
    /// \param[in] it A range of ImagePoints will be inserted before the element pointed to by this iterator.
    /// \param[in] first An iterator pointing to the first in a range of ImagePoints to be inserted
    /// \param[in] last An iterator pointing to the ImagePoint after the last ImagePoint to be inserted
    /// \since 1.0
		void InsertPoint(iterator it, const_iterator first, const_iterator last);
    //@}

    ///
    /// \name Remove/Clear ImagePoints
    //@{
		///
    /// \brief Removes a single ImagePoint from the PointList
    ///
    /// Erases a single ImagePoint, reducing the size of the Image by one.  The removed ImagePoint
    /// is destroyed.
    /// \param[in] it Iterator pointing to a single ImagePoint to be removed from the PointList
    /// \return An iterator pointing to the new location of the ImagePoint that followed the
    /// element erased by the function call.  If the operation erased the last ImagePoint in the
    /// PointList, this will be equal to \c Image::end().
    /// \since 1.0
		iterator RemovePoint(iterator it);
    ///
    /// \brief Removes a range of ImagePoints from the PointList
    ///
    /// Erases a range of ImagePoints from the Image, reducing the size of the Image by the number
    /// of ImagePoints removed, which are destroyed.
    /// \param[in] first An iterator pointing to the first in a range of ImagePoints to be removed
    /// \param[in] last An iterator pointing to the ImagePoint after the last ImagePoint to be removed
    /// \return An iterator pointing to the new location of the ImagePoint that followed the last
    /// element erased by the function call.  If the operation erased the last ImagePoint in the
    /// PointList, this will be equal to \c Image::end().
    /// \since 1.0
		iterator RemovePoint(iterator first, iterator last);
    ///
    /// \brief Remove all ImagePoints from the Image
    ///
    /// The PointList is cleared, all ImagePoints are removed from it and destroyed.  The new size
    /// of the Image will be zero and \c Image::begin() \c == \c Image::end()
    /// \since 1.0
		void Clear();
    //@}

    ///
    /// \name Image Size
    //@{
    ///
    /// \brief Returns the number of ImagePoints in the PointList
    ///
    /// \return The number of ImagePoints in the PointList
    /// \since 1.0
		int Size() const;
    //@}
    ///

    ///
    /// \name Default Internal Clock Rate
    //@{
    ///
    /// \brief Sets the Internal Clock Rate that shall be the default playback frequency for the Image
    ///
    /// An Image shall have associated with it a default Clock Rate.  This is the frequency at which
    /// the iMS Controller playback will propagate from one ImagePoint to the next when it is operated
    /// in Internal Clock Mode (see ImagePlayer::PointClock).
    ///
    /// If the Controller supports multiple images and playback from ImageGroup's, the ImageGroup
    /// will contain a sequence table and in that case, the Clock Rate for playing back the Image as
    /// part of a sequence may be overriden by the Clock Rate field specified in the Sequence Table.
    ///
    /// \param[in] f A Frequency variable to set the Image default internal Clock Rate from.
    /// \since 1.0
 		void ClockRate(const Frequency& f);
    ///
    /// \brief Returns the default Internal Clock Rate associated with the Image
    ///
    /// The Image contains a default Clock Rate which shall be used as the frequency for playing
    /// out an Image when the Controller is configured for Internal Clock mode and is not overriden
    /// by the Clock Rate specified in a sequence table.
    ///
    /// \return A Frequency value that is the default Internal Clock Rate associated with an Image
    /// \since 1.0
		const Frequency& ClockRate() const;
    //@}

	///
	/// \name Default External Clock Divider
	//@{
	///
	/// \brief Sets the External Clock Divider ratio
	///
	/// An Image shall have associated with it a default external Clock Divider Ratio.  This is the ratio
	/// of the externally supplied clock signal to the Image playback rate.  For example, set this to 100
	/// and a 1MHz external clock signal will result in a 10kHz playback rate.
	///
	/// If the Controller supports multiple images and playback from ImageGroup's, the ImageGroup
	/// will contain a sequence table and in that case, the Clock Divider Ratio for playing back the Image as
	/// part of a sequence may be overriden by the Clock Divider Ratio field specified in the Sequence Table.
	///
	/// \param[in] div An integer variable to set the Image default external Clock Divider ratio from.
	/// \since 1.0.1
	void ExtClockDivide(const int div);
	///
	/// \brief Returns the default External Clock Divider Ratio associated with the Image
	///
	/// The Image contains a default External Clock Divider Ratio which shall be used as the frequency ratio 
	/// between the external clock signal and the Image playback frequency when the Controller is configured
	/// for External Clock mode and is not overriden by the Clock Divider Ratio specified in a sequence table.
	///
	/// \return An integer value representing the default External Clock Divier Ratio associated with an Image
	/// \since 1.0.1
	const int ExtClockDivide() const;
	//@}

	///
	/// \name Image Description
	//@{
	///
	/// \brief A string stored with the Image to aid human users in identifying the purpose of an image
	///
	/// A descriptive string can be set alongside the Image to allow users to identify and differentiate
	/// between images without having to browse through the point data.  The description is optional, and if,
	/// not used, the description will simply default to "image".
	///
	/// Updating the Image Description does not cause the Image UUID to change.
	std::string& Description();
	const std::string& Description() const;

	private:
		class Impl;
		Impl * p_Impl;
	};

  /// \brief Each ImageIndex is an offset into the Image Index Table that uniquely refers to an Image stored in Controller Memory
	using ImageIndex = int;

    ///
    /// \struct ImageTableEntry Image.h include/Image.h
    /// \brief An ImageTableEntry is created by the SDK on connecting to an iMS System, one for each Image that is stored in
    /// Controller memory and allocated in the Image Index Table.  Further ImageTableEntries are added to the table each time
    /// an Image is downloaded to the Controller.
    ///
    /// An ImageTableEntry should not be created by user software since it cannot be used to download Images
    /// to an iMS Controller and will not bear any relation to an existing Image on the Controller.  Instead, an ImageDownload
    /// operation should be performed on an Image object to send the Image data to Controller memory which will automatically
    /// create the index data in the Image Index Table.
    ///
    /// An ImageTableEntry can then be returned from an ImageTableViewer::operator [] function call into the IMSSystem object.
    ///
    /// This will result in being able to access relevant information about Images currently on the Controller, including
    /// Image Memory Size, number of Image points, address in memory, Name etc.
    ///
    /// The returned ImageTableEntry object may also be passed to either an ImagePlayer or ImageSequenceEntry object
    /// to permit playback of Images on the Controller.
    /// \author Dave Cowan
    /// \date 2016-04-03
    /// \since 1.2.1
    ///
	struct LIBSPEC ImageTableEntry
	{
		///
		/// \name Constructors & Destructor
    ///
    /// It should not be necessary to construct an ImageTableEntry object since this will be done automatically
    /// by the SDK on connection to an iMS System or after ImageDownload completes.  Entries may then be referenced through the ImageTableViewer class
		//@{
		///
    /// \brief Default Constructor
		ImageTableEntry();
    /// \brief Full Specification Constructor
		ImageTableEntry(ImageIndex handle, std::uint32_t address, int n_pts, int size, std::uint32_t fmt, std::array<std::uint8_t, 16> uuid, std::string name);
    /// \brief Construct object from byte array in binary format specific to Controller communications.  Used internally to build ImageTableEntries.
		ImageTableEntry(ImageIndex handle, const std::vector<std::uint8_t>&);
    /// \brief Destructor
		~ImageTableEntry();
    /// \brief Copy Constructor
		ImageTableEntry(const ImageTableEntry &);
    /// \brief Assignment Constructor
		ImageTableEntry &operator =(const ImageTableEntry &);
    //@}

    ///
    /// \name Image Details
    //@{
    /// \brief Unique Image Handle within Index Table
    ///
    /// \return An Image handle referencing the location of the Image Entry within the Image Table
		const ImageIndex& Handle() const;
    /// \brief Byte Address of Start of Image data stored within the Controller's Memory
    ///
    /// This is usually for information only as the ImageDownload class in conjunction with Controller
    /// firmware will select a memory location with sufficient free capacity.  User software is never
    /// responsible for memory management and does not require the address for Image operations.
    ///
    /// \return an unsigned integer representing the absolute address of the Image in the Controller
    /// memory address space
		const std::uint32_t& Address() const;
    /// \brief the number of points in the Image
    /// \return the number of points in the Image
		const int& NPts() const;
    /// \brief the size of the Image in bytes
    /// \return the size of the Image in bytes
		const int& Size() const;
    /// \brief A Format Specifier relates the byte structure of the Image in Controller Memory to Image Physical Data
    ///
    /// An Image as created in application software consists of physical information such as "frequency of channel
    /// 1 at Image Point 1000".  This must be translated into a byte format that is understood by the hardware
    /// to create the RF signal.  There are a number of optimisations that can be performed to trade off
    /// between flexibility and update speed, the mapping between real and physical Image data is described
    /// by the Format value.
    ///
    /// \return an unsigned integer representing the Image Format
		const std::uint32_t& Format() const;
    /// \brief Image Unique Identifier can be used to synchronise Image Entries with host software Image objects
    ///
    /// Each Image created in application software is automatically assigned a Unique ID (UUID) which is updated
    /// anytime the Image is modified.  The UUID is downloaded to the Image Table along with the Image and
    /// can be used to establish whether an Image resident in memory is identical to an Image present in
    /// application software, without having to upload the Image data.
    ///
    /// The UUID is also the mechanism that allows Sequences to be created from individual Images, either
    /// directly from the Image object, or from Images in Controller memory via the ImageTableEntry.
    ///
    /// \return a 16 byte array containing the Image UUID.
		const std::array<std::uint8_t, 16>& UUID() const;
	/// \brief Matches an ImageTableEntry object to an Image
	///
	/// Returns true if the image reference is identical to this entry in the Image Table
	/// \param[in] img A const reference to an Image which is to be checked for identity with the current entry
	/// \return true if match succeeds, false if the image is different
	/// \since 1.7.0
		bool Matches(const Image& img) const;
    /// \brief Descriptive Name assigned to an Image to aid User Recognition
    ///
    /// Each Image can be assigned a descriptive name to help identify its purpose.  The first 16 bytes
    /// are transferred to the Controller during Image Download.  The Name is optional and will return an
    /// empty string if not used.  Be aware that due to the 16 byte limitation, the Name returned from the
    /// ImageTableEntry may differ from the name assigned to the Image in application software (whose length
    /// is unlimited).
    ///
    /// \return a string object representing the description assigned to the Image
		const std::string& Name() const;
	private:
		class Impl;
		Impl *p_Impl;
	};

	/// \enum ImageRepeats
	/// \brief Each Image can be repeated, either a programmable number of times, or indefinitely
	/// \since 1.2.1
  ///
	enum class ImageRepeats {
    /// The Image is played back only once
		NONE,
    /// The Image is played back a programmable number of times according to the value set in the PlayConfiguration table
		PROGRAM,
    /// The Image is played back repeatedly until stopped by the application
		FOREVER
	};

	///
	/// \struct SequenceEntry Image.h include/Image.h
	/// \brief Abstract base class entry for creating ImageSequence 's.  Add derived classes to the ImageSequence.
	/// 
	/// The SequenceEntry struct is the base struct that is used to define an ImageSequence that can be downloaded and 
	/// played by the iMS System.  A SequenceEntry on its own is abstract and cannot be used directly as an object within the
	/// ImageSequence.  The ImageSequence must be made up of concrete objects which are inherited structs such as ImageSequenceEntry 
	/// and ToneSequenceEntry.  However, some parameters are common to all SequenceEntry types and are defined within this struct.
	///
	/// These include:
	///
	/// \li Number of times to repeat the sequence entry (once for each received trigger); 0 - 16777215.
	/// \li Amount of delay to apply to the Synchronous Digital Output signals
	/// \li Frequency Offsets: A positive or negative frequency deviation to apply to each Synthesiser channel for the duration of the SequenceEntry
	///
	/// The SequenceEntry must hold a unique reference to the underlying object in order to manage the sequence correctly.  Since
	/// you should never need to instantiate the base SequenceEntry class directly, you should not need to worry about this as it
	/// will be obtained from a reference to the Image, ImageTableEntry, ToneBuffer or whatever object that the derived class is
	/// constructed from. 
	///
	/// \author Dave Cowan
	/// \date 2020-06-08
	/// \since 1.8.0
	///
	struct LIBSPEC SequenceEntry
	{
	/// \cond SEQUENCE_ENTRY_CONSTRUCTORS
	/// \brief Default Constructor.  You should not need this.
		SequenceEntry();
	/// \brief Reference Constructor.  Constructs a SequenceEntry using the UUID of an object. You should not need this.
		SequenceEntry(const std::array<std::uint8_t, 16>& uuid, const int rpts = 0);
	/// \brief Virtual Destructor.  Always overridden
		virtual ~SequenceEntry() = 0;
		SequenceEntry(const SequenceEntry&);
		SequenceEntry& operator =(const SequenceEntry&);
	/// \endcond

	///
	/// \brief Equality Operator checks SequenceEntry object for equivalence
	///
	/// All derived classes must implement this function.  The derived type is checked and false is returned if
	/// the types are different.  If they are the same, the UUID and parameters are checked for equivalence.
	///
	/// \param[in] rhs A SequenceEntry object to perform the comparison with
	/// \return True if the supplied SequenceEntry is the same derived type and identical to this one.
		virtual bool operator==(SequenceEntry const& rhs) const = 0;

	///
	/// \name Synchronous Data Delay Setting
	//@{
	/// \brief Setter for Synchronous Digital Output signal Delay
	///
	/// The Synchronous Digital Output signals of the Synthesiser can be used to output data from either the FAP Synchronous digital field
	/// or from entries in the Compensation Look Up Table.  The data updates at the same time as the Image data updated the RF output.
	/// Using the SyncOutDelay field, the data can be shifted in time to compensate for latency in the system or to, for example, delay
	/// a trigger pulse to the middle of an RF Image Point.
	///
	/// The Delay time may be specified as any std::chrono value using duration_cast but hardware limitations restrict
	/// the real delay time to a minimum of 0.01us and a maximum of 655.35us.
	///
	/// \return a lvalue reference to the Synchronous Digital Output delay time
		std::chrono::duration<double>& SyncOutDelay();
	/// \brief Getter for Synchronous Digital Output signal delay
	/// \return a const reference (rvalue) for reading the Synchronous Digital Output signal delay time
		const std::chrono::duration<double>& SyncOutDelay() const;
	//@}

	///
	/// \name Frequency Offset Parameters
	//@{
	/// \brief Setter for channel Frequency Offset parameter
	///
	/// It is possible to apply a per-channel frequency offset to a SequenceEntry so that the the same object may be reused
	/// but positioned at a different angle within the scan range of the AOD.
	///
	/// \param[in] offset The new value of frequency to offset the channel by, can be positive or negative
	/// \param[in] chan Which RF Channel to apply the frequency offset to
	/// \since 1.8.0
		void SetFrequencyOffset(const MHz& offset, const RFChannel& chan = RFChannel::all);
	/// \param[in] chan Which RF Channel to apply the frequency offset to
	/// \return a const reference (rvalue) for reading the channel's Frequency Offset parameter
	/// \since 1.8.0
		const MHz& GetFrequencyOffset(const RFChannel& chan) const;
	//@}

	/// \name Sequence Entry Parameters
	///
	//@{
	/// \brief Unique Identifier can be used to synchronise Sequence Entries with host software objects
	///
	/// Each object (e.g. Image, ToneBuffer) created in application software is automatically assigned a Unique ID (UUID) which is updated
	/// anytime the object is modified.  Sequences are internally specified using the UUID of an Image or ToneBuffer to ensure
	/// absolute consistency with the object stored in Controller memory and referenced in the Index table.
	/// The User can check whether an  object matches the Image referenced in a SequenceEntry by
	/// comparing its UUID.
	///
	/// \return a 16 byte array containing the Image UUID.
		const std::array<std::uint8_t, 16>& UUID() const;
	//@}

	/// \brief returns the number of times to repeat an Image or Tone Buffer before moving to the next entry in the Sequence
	/// \return the number of times to repeat before moving to the next entry in the Sequence
		const int& NumRpts() const;
	private:
		class Impl;
		Impl* p_Impl;
	};

    ///
    /// \struct ImageSequenceEntry Image.h include/Image.h
    /// \brief Inserts an Image playback into an ImageSequence
	///
	/// An ImageSequenceEntry object can be created by application software to specify the parameters by which
    /// an Image is played back during an ImageSequence.  It is derived from the SequenceEntry base struct and specifies that
	/// the Sequence should play an Image from the ImageTable at this point within the Sequence.
    ///
    /// Additional parameters related to Image playback that can be specified include
    ///
    /// \li Internal Clock Frequency (implicitly defined when programmed from an Image object)
    /// \li External Clock Divider (implicitly defined when programmed from an Image object)
    /// \li Amount of delay to be added after the end of Image playback (if programmed for Post-Image Delay mode)
    /// 
	/// Note that from SDK v1.8.0, this struct was split into a base struct (SequenceEntry) and a derived class (ImageSequenceEntry)
	///
    /// \author Dave Cowan
    /// \date 2016-04-24
    /// \since 1.2.4
    ///
	struct LIBSPEC ImageSequenceEntry : SequenceEntry
	{
		///
		/// \name Constructors & Destructor
    ///
		//@{
		///
    /// \brief Default Constructor
		ImageSequenceEntry();
    /// \brief Construct ImageSequenceEntry object from Image object resident in application software.
    ///
    /// If using this construction method, the Internal Clock Rate or External Clock Divider, if required,
    /// should first be set using the Image::ClockRate() and Image::ExtClockDivide() functions.
    ///
    /// The user can optionally specify the number of times to repeat the Image before moving on to the
    /// next entry in the sequence.  The default is no repeats, and these parameters may then be ommitted.
    ///
    /// \param img A reference to the Image object which is to be played in the Sequence (must have been downloaded to Controller memory before playback)
    /// \param Rpt An optional parameter specifying whether repeats are required (ImageRepeats::PROGRAM) or not (ImageRepeats::NONE)
    /// \param rpts An optional integer specifying the number of repeats to perform (max 16777215).
		ImageSequenceEntry(const Image& img, const ImageRepeats& Rpt = ImageRepeats::NONE, const int rpts = 0);
    /// \brief Construct ImageSequenceEntry object from an Image resident in Controller memory referenced by its index table entry
    ///
    /// This is the preferred method for constructing an ImageSequenceEntry object when the Image has already have been downloaded to the
    /// Controller.  However, the Index Table in the Controller does not store default clock frequency or clock divider information, so this
    /// must be specified manually.
    ///
    /// The user can optionally specify the number of times to repeat the Image before moving on to the
    /// next entry in the sequence.  The default is no repeats, and these parameters may then be ommitted.
    ///
    /// \param ite A reference to the Image object from ite ImageTableEntry (can be retrieved from the IMSSystem object through an ImageTableViewer)
    /// \param InternalClock Specifies the clock rate with which to program the Internal NCO oscillator (optional, defaults to 1kHz)
    /// \param Rpt An optional parameter specifying whether repeats are required (ImageRepeats::PROGRAM) or not (ImageRepeats::NONE)
    /// \param rpts An optional integer specifying the number of repeats to perform (max 255).
		ImageSequenceEntry(const ImageTableEntry& ite, const kHz& InternalClock = kHz(1.0), const ImageRepeats& Rpt = ImageRepeats::NONE, const int rpts = 0);
    /// \brief Construct ImageSequenceEntry object from an Image resident in Controller memory referenced by its index table entry
    ///
    /// This is the preferred method for constructing an ImageSequenceEntry object when the Image has already have been downloaded to the
    /// Controller.  However, the Index Table in the Controller does not store default clock frequency or clock divider information, so this
    /// must be specified manually.
    ///
    /// The user can optionally specify the number of times to repeat the Image before moving on to the
    /// next entry in the sequence.  The default is no repeats, and these parameters may then be ommitted.
    ///
    /// \param ite A reference to the Image object from ite ImageTableEntry (can be retrieved from the IMSSystem object through an ImageTableViewer)
    /// \param ExtClockDivide divides down the externally supplied clock signal by an integer ratio, e.g. 3 => update every 3rd clock edge (optional, default to 1, i.e. off)
    /// \param Rpt An optional parameter specifying whether repeats are required (ImageRepeats::PROGRAM) or not (ImageRepeats::NONE)
    /// \param rpts An optional integer specifying the number of repeats to perform (max 255).
		ImageSequenceEntry(const ImageTableEntry& ite, const int ExtClockDivide = 1, const ImageRepeats& Rpt = ImageRepeats::NONE, const int rpts = 0);
    /// \brief Destructor
		~ImageSequenceEntry();
    /// \brief Copy Constructor from another ImageSequenceEntry
		ImageSequenceEntry(const ImageSequenceEntry &);
    /// \brief Assignment Constructor
		ImageSequenceEntry &operator =(const ImageSequenceEntry &);
	/// \brief Copy Constructor from another object derived from the base SequenceEntry class.  You should not need this.
		ImageSequenceEntry(const SequenceEntry& entry);
	//@}

	/// \brief Equality Operator checks ImageSequenceEntry object for equivalence
	///
	/// \param[in] rhs An SequenceEntry object to perform the comparison with
	/// \return True if the supplied SequenceEntry is also an ImageSequenceEntry, and is identical to this one.
		bool operator==(SequenceEntry const& rhs) const;

    ///
    /// \name Post Image Delay Setting
    //@{
    /// \brief Setter for post Image delay
    ///
    /// If the ImageSequence is configured to create a 'pause' at the end of playback for each Image in the sequence,
    /// the pause time can be programmed on a per entry basis using this function.  Set SequenceManager::SeqConfiguration::trig
    /// to ImageTrigger::POST_DELAY to use this feature.
    ///
    /// The Pause time may be specified as any std::chrono value using duration_cast but hardware restricts
    /// the real delay time to a resolution of 0.1ms and a maximum of 6.5535s.
    ///
    /// \return a lvalue reference to the Post Delay time
		std::chrono::duration<double>& PostImgDelay();
    /// \brief Getter for post Image delay
    /// \return a const reference (rvalue) for reading the Post Delay time
		const std::chrono::duration<double>& PostImgDelay() const;
	//@}

    /// \name Image Sequence Entry Parameters
    ///
    //@{
    /// \brief returns the programmed External Clock Divider ratio
    /// \return the programmed External Clock Divider ratio
		const int& ExtDiv() const;
    /// \brief returns the programmed Internal Oscillator Frequency
    /// \return the programmed Internal Oscillator Frequency
		const Frequency& IntOsc() const;
    /// \brief returns the configured Repeat style
    /// \return the configured Repeat style
		const ImageRepeats& RptType() const;
    //@}
	private:
		class Impl;
		Impl *p_Impl;
	};

	/// \enum SequenceTermAction
	/// \brief Operation to perform on the completion of the last repeat of the last entry in a Sequence
	/// \since 1.2.4
	enum class SequenceTermAction : std::uint8_t
	{
    /// Delete the ImageSequence from the Sequence Queue and move on to the next Sequence, if it exists, otherwise Stop
		DISCARD = 0,
    /// Move the ImageSequence to the end of Sequence Queue and move on to the next Sequence, if it exists, otherwise repeat this ImageSequence
		RECYCLE = 1,
    /// Delete the ImageSequence from the Sequence Queue and stop playback
		STOP_DISCARD = 2,
    /// Move the ImageSequence to the end of Sequence Queue and stop playback
		STOP_RECYCLE = 3,
    /// No effect on the Sequence Queue.  Repeat the current Sequence.
		REPEAT = 4,
    /// No effect on the Sequence Queue.  Repeat the current Sequence starting from the ImageSequenceEntry index specified in the Termination Value.
		REPEAT_FROM = 5,
	/// Move the ImageSequence to a location in the Sequence Queue, inserting it before another sequence
		INSERT = 7,
	/// Move the ImageSequence to a location in the Sequence Queue and stop playback
		STOP_INSERT = 8
	};

  ///
  /// \class ImageSequence Image.h include/Image.h
  /// \brief An ImageSequence object completely defines a sequence to be played back on an iMS Controller in terms by containing
  /// a list of ImageSequenceEntry 's and ToneSequenceEntry 's plus a terminating action and optional value.
  ///
  /// Each ImageSequenceEntry defines the Image to be played back at that point in the sequence, together with relevant
  /// parameters such as clock frequency, divider and number of repeats.  The ImageSequenceEntry 's are played back in
  /// the order in which they appear in the ImageSequence list.
  ///
  /// ToneSequenceEntry 's may also be added to the ImageSequence and when the Controller processes a ToneSequenceEntry, it enables
  /// RF output from the ToneBuffer that is resident in the Synthesiser.  Only one ToneBuffer can be present on the Synthesiser at
  /// any one time, so it should have been downloaded prior to the start of ImageSequence playback.  However, a ToneBuffer has up to
  /// 256 entries, so different ranges of entries can be used for different purposes.
  ///
  /// The ImageSequence is a container for the list of ImageSequenceEntry 's and ToneSequenceEntry 's.  User application code can create the entries
  /// and add them / remove them from the front or back of the list, insert them or erase them from anywhere in the list,
  /// or assign multiple copies of the entry to the list.
  ///
  /// As with Images, ImageSequences have a Unique ID (UUID) associated with them which are used to uniquely refer to
  /// sequences when communicating with the iMS Controller through the SequenceManager.
  /// \date 2016-04-24
  /// \since 1.2.4
  ///
	class LIBSPEC ImageSequence : public ListBase < std::shared_ptr < SequenceEntry > >
	{
	public:
    /// \name Constructors & Destructor
    //@{
    /// \brief Create a default empty Image Sequence
		ImageSequence();
    /// \brief Create a default empty Image Sequence with Termination Action specifier
    /// \param action The operation to perform once the Sequence has completed playback
    /// \param val Optional parameter to the Termination Action
		ImageSequence(SequenceTermAction action, int val = 0);
	/// \brief Create a default empty Image Sequence with Termination Action specifier
	/// \param action The operation to perform once the Sequence has completed playback (should be INSERT or STOP_INSERT)
	/// \param insert_before insert this sequence in front of any other sequence when it completes playback.
		ImageSequence(SequenceTermAction action, const ImageSequence* insert_before);
	/// \brief Destructor
		~ImageSequence();
    /// \brief Copy Constructor
		ImageSequence(const ImageSequence &);
    /// \brief Assignment Constructor
		ImageSequence &operator =(const ImageSequence &);
    //@}


    /// \name Control Sequence Terminating Actions
    //@{
    /// \brief Update Termination Action
    /// \param[in] act Assign an operation to perform when the Sequence completes
    /// \param[in] val Optional Parameter to use with some Termination Actions
		void OnTermination(SequenceTermAction act, int val = 0);
	/// \brief Update Termination Action
	/// \param[in] act Assign an operation to perform when the Sequence completes
	/// \param[in] term_seq Pointer to another sequence to insert before on completion
		void OnTermination(SequenceTermAction act, const ImageSequence* term_seq);
	/// \brief return a reference to the currently assign Termination Action
    /// \return a reference to the currently assign Termination Action
		const SequenceTermAction& TermAction() const;
    /// \brief return a reference to the currently assign Termination Action Parameter
    /// \return a reference to the currently assign Termination Action Parameter
		const int& TermValue() const;
	/// \brief return a pointer to the ImageSequence which this sequence will insert before when TermAction = INSERT or STOP_INSERT
	/// \return a pointer to another ImageSequence or nullptr
		const ImageSequence* TermInsertBefore() const;
    //@}


	private:
		class Impl;
		Impl *p_Impl;
	};

	///
	/// \class ImageGroup Image.h include/Image.h
	/// \brief An ImageGroup collects together multiple associated images and a single ImageSequence for controlling Image playback order
	///
	/// Individual Image's may be played back on an iMS System freely but to specify more complex behaviour, typically an ImageGroup is used.
	/// An ImageGroup can contain one or many images and always has exactly one sequence which may be used to define an order in which those
	/// Images are played back on the iMS Controller.
	///
	/// Additionally, user information may be supplied in the form of metadata (name, author, company, revision, description) to assist in 
	/// identifying the purpose of an ImageGroup.
	/// \date 2016-11-09
	/// \since 1.3
	///
	class LIBSPEC ImageGroup : public DequeBase<Image> {
	public:

		/// \name Constructors & Destructor
		//@{
		/// \brief Create a default empty ImageGroup
		///
		/// The ImageGroup is created with zero Images in its list and a default ImageSequence with zero entries.
		/// Call as \c ImageGroup() or \c ImageGroup("My \c Group").  do not use the create_time or modified_time parameters.
		/// \param name Optionally, the caller may specify a Name for the ImageGroup.  If not specified, it defaults to an empty string.
		/// \param create_time Specify the creation time for the ImageGroup.  This is intended for use only when loading ImageGroup's from an ImageProject disk file and should not be used.
		/// \param modified_time Specify the last modified time for the ImageGroup.  This is intended for use only when loading ImageGroup's from an ImageProject disk file and should not be used.
		ImageGroup(const std::string& name = "", const std::time_t& create_time = std::time(nullptr), const std::time_t& modified_time = std::time(nullptr));
		/// \brief Create an ImageGroup with n empty Images
		///
		/// The ImageGroup is created with 'n' empty Images in its list and a default ImageSequence with each Image added once.
		/// Call as \c ImageGroup(n) or \c ImageGroup(n, "My \c Group").  do not use the create_time or modified_time parameters.
		/// \param n The number of empty images to create in the group.
		/// \param name Optionally, the caller may specify a Name for the ImageGroup.  If not specified, it defaults to an empty string.
		/// \param create_time Specify the creation time for the ImageGroup.  This is intended for use only when loading ImageGroup's from an ImageProject disk file and should not be used.
		/// \param modified_time Specify the last modified time for the ImageGroup.  This is intended for use only when loading ImageGroup's from an ImageProject disk file and should not be used.
		ImageGroup(size_t n, const std::string& name = "", const std::time_t& create_time = std::time(nullptr), const std::time_t& modified_time = std::time(nullptr));
		///
		/// \brief Copy Constructor
		///
		ImageGroup(const ImageGroup &);
		///
		/// \brief Assignment Constructor
		///
		ImageGroup &operator =(const ImageGroup &);
		///
		/// \brief Destructor
		///
		~ImageGroup();
		///
		//@}

		/// \name ImageGroup collection modifiers
		//@{
		///
		/// \brief Adds a new Image to the back of the Image Queue
		/// \param img a const reference to the Image to be added
		void AddImage(const Image& img);
		/// \brief Inserts a new Image before the specified element in the Image Queue
		/// \param it The queue element to insert the image before
		/// \param img a const reference to the Image to be inserted
		/// \return an iterator to the newly inserted Image
		iterator InsertImage(iterator it, const Image& img);
		/// \brief Removes an Image at the specified element in the Image Queue
		/// \param it the queue element to remove
		/// \return an iterator to the element following the element removed from the Image Queue
		iterator RemoveImage(iterator it);
		/// \brief Removes a range of Image's from the specified range of elements in the Image Queue
		/// \param first the initial queue element in the range to remove
		/// \param last the final queue element in the range to remove
		/// \return an iterator to the element following the last element removed from the Image Queue
		iterator RemoveImage(iterator first, iterator last);

		/// \brief Clear ImageGroup
		///
		/// Remove all Images from the ImageGroup and all entries from the ImageSequence
		void Clear();
		/// \brief Returns the number of Images in the ImageGroup
		///
		/// Returns the number of Images in the ImageGroup
		int Size() const;
		//@}
		///

		///
		/// \name Timestamping
		//@{
		///
		/// \brief Returns Time at which the Container was created
		///
		/// At the time the ImageGroup is first created, the system time is recorded.
		/// If a ImageGroup is copied or assigned to another object, the new object inherits the Creation time of the parent so
		/// the timestamp always refers to the time at which an ImageGroup was initially created.
		/// \return a reference to a std::time_t representing the time at which the ImageGroup was created
		/// \since 1.3
		const std::time_t& CreatedTime() const;
		///
		/// \brief Returns Human-readable string for the time at which the ImageGroup was created
		/// \since 1.3
		std::string CreatedTimeFormat() const;
		//@}
		///

		///
		/// \name User MetaData
		//@{
		///
		/// \brief Author Set Accessor
		///
		/// Sets the Author's name for the ImageGroup
		/// \since 1.3
		std::string& Author();
		/// \brief Author Get Accessor
		///
		/// Gets the Author's name for the ImageGroup
		/// \since 1.3
		const std::string& Author() const;
		///
		/// \brief Company Set Accessor
		///
		/// Sets the Company name for the ImageGroup
		std::string& Company();
		/// \brief Company Get Accessor
		///
		/// Gets the Company name for the ImageGroup
		const std::string& Company() const;
		///
		/// \brief Revision Set Accessor
		///
		/// Sets the Revision number for the ImageGroup
		///
		/// Please note that this field is not handled internally by the ImageGroup class.  It is left to the user application
		/// to modify, update or increment the Revision number as well as specifying a numbering scheme as best fits the application.
		std::string& Revision();
		/// \brief Revision Get Accessor
		///
		/// Gets the Revision number for the ImageGroup
		const std::string& Revision() const;
		///
		/// \brief Description Set Accessor
		///
		/// Sets a Description field for the ImageGroup
		std::string& Description();
		/// \brief Description Get Accessor
		///
		/// Gets the Description field for the ImageGroup
		const std::string& Description() const;
		//@}
		///

		///
		/// \name ImageGroup Sequence
		//@{
		///
		/// \brief ImageSequence Set Accessor
		///
		/// Returns a reference to the ImageGroup sequence to allow user application code to modify the Sequence Table.
		ImageSequence& Sequence();
		/// \brief ImageSequence Get Accesor
		///
		/// Returns a const reference to the ImageGroup sequence to allow user application code to view the Sequence Table.
		const ImageSequence& Sequence() const;
		//@}
		///

	private:
		class Impl;
		Impl *p_Impl;
	};

	/// \brief For backwards compatibility with code written against SDK 1.2.6 or earlier.
	typedef ImageGroup ImageFile;
}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
