/*-----------------------------------------------------------------------------
/ Title      : Compensation Functions Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Compensation/h/Compensation.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2019-07-10 12:10:35 +0100 (Wed, 10 Jul 2019) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 418 $
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
/// \file Compensation.h
///
/// \brief Classes for creating and downloading data that is used in the Compensation tables of the Synthesiser
///
/// The Compensation Tables are a part of the signal chain in the Synthesiser.  There are 4 of them,
/// each serving a different purpose.  All 4 are indexed by the signal frequency, spanning the lowest
/// to the highest frequency supported by the Synthesiser, each table consisting of a sequence of
/// look-up entries (typically 2,048) spaced equidistantly in frequency.
///
/// The 4 tables are:
///
/// (1) Amplitude Equalisation: used to compensate for frequency-dependent inefficiency in the AO device, as well
///   as in the RF Amplifier and the Synthesiser.  The signal amplitude passing through the Synthesiser
///   is multiplied by the compensation output to result in a combined amplitude being passed to the
///   Synthesiser DDS device.
///
/// (2) Phase Steer: used in beam-steered AO applications where multiple acoustic columns present in the
///   crystal are offset in phase from each other in a way that is linearly dependent on the frequency
///   offset from a central Bragg Angle adjusted frequency.
///
/// (3) Analogue Sync: The output of this table can be routed to the Synchronous DAC output which gives
///   a handy analogue reference signal for either test purposes or for driving external custom
///   circuitry.  The advantage of driving this from the look-up table is that custom mappings can
///   be generated which allows great flexibility in configuring the analogue signal in relation to the
///   signal frequency that drives it.
///
/// (4) Digital Sync: As with the analogue sync, the output of this table is routed to external synchronous
///   outputs which can be used for test purposes or for driving external custom circuitry.  The digital
///   output bits could, for example, be used to tune signal conditioning circuitry as the RF signal
///   passes through certain frequency bands.
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_Comp
///

#ifndef IMS_COMPENSATION_H__
#define IMS_COMPENSATION_H__

#include "Containers.h"
#include "IMSSystem.h"
#include "IEventHandler.h"
#include "IMSTypeDefs.h"
#include "IBulkTransfer.h"
#include "FileSystem.h"

#include <memory>
#include <deque>

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
	///
	/// \class CompensationEvents Compensation.h include\Compensation.h
	/// \brief All the different types of events that can be triggered by the Compensation and CompensationTableDownload classes.
	///
	/// Some events contain integer parameter data which can be processed by the IEventHandler::EventAction
	/// derived method
	/// \author Dave Cowan
	/// \date 2015-11-11
	/// \since 1.0
	class LIBSPEC CompensationEvents
	{
	public:
		/// \enum Events List of Events raised by the Compensation Class and Compensation Table Downloader
		enum Events {
			/// Not used
			RX_DDS_POWER,
			/// Event raised when CompensationTableDownload has confirmed that the iMS Controller received all of the Compensation Table data
			DOWNLOAD_FINISHED,
			/// Event raised each time the CompensationTableDownload class registers an error in the download process
			DOWNLOAD_ERROR,
			/// Event raised on completion of a download verify, if the download was successfully verified
			VERIFY_SUCCESS,
			/// Event raised on completion of a download verify, if the download failed. \c param contains the number of failures recorded
			VERIFY_FAIL,
			Count
		};
	};

	///
	/// \enum CompensationFeature
	/// \brief The Four features that are present in the compensation table
	enum class CompensationFeature {
		/// \brief Amplitude compensation is used to correct for frequency-dependent amplitude non-linearities
		AMPLITUDE,
		/// \brief Phase compensation is used to correct for frequency-dependent phase effects
		PHASE,
		/// \brief Synchronous Digital compensation may be used to assert digital I/O synchronously with programmed frequency ranges
		SYNC_DIG,
		/// \brief Synchronous Analog compensation may be used to provide an analog control signal that is a function of RF Frequency
		SYNC_ANLG
	};

	///
	/// \enum CompensationModifier
	/// \brief Used when generating a CompensationTable from a CompensationFunction to indicate whether the new data should overwrite the old data, or update it
	enum class CompensationModifier {
		/// \brief Newly generated CompensationTable data overwrites the previous set of data in the CompensationTable
		REPLACE,
		/// \brief Newly generated CompensationTable data is multiplied by the existing data and updated in the CompensationTable
		MULTIPLY
	};

	///
	/// \class CompensationPoint Compensation.h include/Compensation.h
	/// \brief Stores 4 data fields containing amplitude, phase, sync analogue and sync digital compensation data
	///
	/// A CompensationPoint represents one entry in the CompensationTable and is defined for a fixed
	/// frequency that is linearly spaced within the frequency range reproducible by the Synthesiser.
	///
	/// Each point has 4 fields, one each for amplitude compensation, phase steering, synchronous analogue
	/// and digital data.
	///
	/// \author Dave Cowan
	/// \date 2015-11-03
	/// \since 1.0
	///
	class LIBSPEC CompensationPoint
	{
	public:
		///
		/// \name Constructors & Destructor
		//@{
		/// \brief Compensation Point Constructor
		/// \param[in] ampl The initial Amplitude Compensation value
		/// \param[in] phase The initial Phase Steering value
		/// \param[in] sync_dig The initial Synchronous Digital Data value
		/// \param[in] sync_anlg The initial Synchronous Analogue Data value
		/// \since 1.0
		CompensationPoint(Percent ampl = 0.0, Degrees phase = 0.0, unsigned int sync_dig = 0, double sync_anlg = 0.0);
		CompensationPoint(Degrees phase, unsigned int sync_dig = 0, double sync_anlg = 0.0);
		CompensationPoint(unsigned int sync_dig, double sync_anlg = 0.0);
		CompensationPoint(double sync_anlg);
		~CompensationPoint();
		//@}

		/// \brief Copy Constructor
		CompensationPoint(const CompensationPoint &);
		/// \brief Assignment Constructor
		CompensationPoint &operator =(const CompensationPoint &);

		/// \brief Equality Operator
		/// \since 1.3
		bool operator==(CompensationPoint const& rhs) const;

		///
		/// \name Get/Set field data for the CompensationPoint
		//@{
		/// \brief Setter for Amplitude field
		///
		/// Amplitude, specified as a percentage figure from 0 - 100%, is applied to the signal amplitude
		/// passing from the Controller to the Synthesiser, resulting in a combined amplitude signal
		/// that is compensated for any variation in frequency response of the RF signal chain.
		/// \param[in] ampl The Amplitude value to set the Compensation field to
		void Amplitude(const Percent& ampl);
		/// \brief Getter for Amplitude field
		/// \return the CompensationPoint's Amplitude value
		const Percent& Amplitude() const;
		/// \brief Setter for Phase field
		///
		/// Phase, specified in Degrees from 0 - 360, defines an additional phase offset applied to
		/// RF Channel 2 compared with RF Channel 1.  The same phase offset is added cumulatively to
		/// subsequent output channels so that RF Channel 4 has an offset of 3 times the table phase
		/// value when compared with RF Channel 1.
		/// \param[in] phase The Phase value to set the Compensation field to
		void Phase(const Degrees& phase);
		/// \brief Getter for Phase field
		/// \return the CompensationPoint's Phase value
		const Degrees& Phase() const;
		/// \brief Setter for Digital Sync Data field
		///
		/// Digital Sync data can be routed to the SDIO signals output externally from the Synthesiser.
		/// They can be used for triggering external hardware, for test purposes, or anything else that
		/// requires a frequency-dependent logic signal.  The number of bits available is dependent on the
		/// hardware and can be read from the IMSSynthesiser::Capabilities struct.  The least significant bit
		/// of the unsigned int always maps to SDIO[0]
		/// \param[in] sync The Digital Sync value to set the Compensation field to
		void SyncDig(const unsigned int& sync);
		/// \brief Getter for Digital Sync Data field
		/// \return the CompensationPoint's Digital Sync Data field
		const std::uint32_t& SyncDig() const;
		/// \brief Setter for Analogue Sync Data field
		///
		/// Analogue Sync data can be routed to the SDAC signals output externally from the Synthesiser.
		/// They can be used for custom-scaled analogue frequency signals or any other purpose that requires
		/// a frequency-dependent analogue signal.  The analogue value is specified in the range 0.0 to +1.0
		/// which is converted to an unsigned bit representation stored in the CompensationTable.  Any values
		/// outside the range will be clamped. The number of bits used is hardware dependent and can be
		/// read from the IMSSynthesiser::Capabilities struct.
		/// \param[in] sync The Analogue Sync value to set the Compensation field to
		void SyncAnlg(const double& sync);
		/// \brief Getter for Analogue Sync Data field
		/// \return the CompensationPoint's Analogue Sync Data field
		const double& SyncAnlg() const;
		//@}
	private:
		class Impl;
		Impl * p_Impl;
	};

	///
	/// \class CompensationPointSpecification Compensation.h include/Compensation.h
	/// \brief Completely specifies the desired compensation at a spot frequency
	///
	/// A CompensationPointSpecification object is the basic unit of a Compensation Function.  It is required to know the
	/// Frequency at which the specification is made and this frequency must fall within the frequency range of the Synthesiser
	/// on which the resulting CompensationTable will be programmed else the specification will be disregarded in the 
	/// CompensationFunction calculation.
	///
	/// The calling software can program any of the Compensation parameters (amplitude, phase, synchronous analog or digital)
	/// and the programmed value will be used to generate CompensationTable data by the CompensationFunction calculation.
	///
	/// \author Dave Cowan
	/// \date 2016-11-03
	/// \since 1.3
	///
	class LIBSPEC CompensationPointSpecification
	{
	public:
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for CompensationPointSpecification Object
		///
		/// \since 1.3
		CompensationPointSpecification(CompensationPoint pt = CompensationPoint(), MHz f = 50.0);
		///
		/// \brief Destructor for CompensationPointSpecification Object
		~CompensationPointSpecification();

		/// \brief Copy Constructor
		CompensationPointSpecification(const CompensationPointSpecification &);
		/// \brief Assignment Constructor
		CompensationPointSpecification &operator =(const CompensationPointSpecification &);

		/// \brief Equality Operator
		/// \since 1.3
		bool operator==(CompensationPointSpecification const& rhs) const;

		/// \brief Sets the frequency (in MHz) at which the CompensationPointSpecification is valid
		void Freq(const MHz& f);
		/// \brief Gets the CompensationPointSpecification frequency
		const MHz& Freq() const;

		/// \brief Sets the specification data for this CompensationPointSpecification frequency point
		void Spec(const CompensationPoint& pt);
		/// \brief Gets the specification data for this CompensationPointSpecification frequency point
		const CompensationPoint& Spec() const;
	private:
		class Impl;
		Impl * p_Impl;
	};

	///
	/// \class CompensationFunction Compensation.h include/Compensation.h
	/// \brief Class for performing Compensation related functions with the Synthesiser
	///
	/// The purpose of this class is to perform compensation tasks such as measuring the diffraction
	/// efficiency of an AO device across a range of frequencies.  Such data can then be used to build
	/// Compensation tables.
	///
	/// A CompensationFunction object defines a set of parameters for each feature of compensation (amplitude,
	/// phase, sync digital and sync analog) at given spot frequency points.  It also defines the style of
	/// interpolation to use when rendering a CompensationTable from the CompensationFunction object.
	///
	/// It is not used for storing Compensation Table data or for downloading Compensation Tables.
	/// See the CompensationTable and CompensationTableDownload classes for these requirements.
	///
	/// \author Dave Cowan
	/// \date 2016-11-03
	/// \since 1.3
	///
	class LIBSPEC CompensationFunction : public ListBase < CompensationPointSpecification >
	{
	public:
		/// \enum InterpolationStyle
		/// \brief Selects the style with which the point specifications in a function are applied when generating a CompensationTable
		enum class InterpolationStyle {
			/// \brief Spot frequency style is used when the point specification is desired at only one frequency entry in the table
			SPOT,
			/// \brief Stepped frequency style is used when the point specification is to apply at or above this frequency until the next entry becomes valid
			STEP,
			/// \brief Linear style applies a linear interpolation of the point specifications between two frequencies. Endpoint extrapolation is constant value
			LINEAR,
			/// \brief  Linear with Extension applies a linear interpolation with linear extrapolation at the end points
			LINEXTEND,
			/// \brief Cubic B-Spline interpolation applies a curve fit between all point specifications throughout the function
			BSPLINE
		};

		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for Compensation Object
		///
		/// \since 1.3
		CompensationFunction();
		///
		/// \brief Destructor for Compensation Object
		~CompensationFunction();
		/// \brief Copy Constructor
		CompensationFunction(const CompensationFunction &);
		/// \brief Assignment Constructor
		CompensationFunction &operator =(const CompensationFunction &);
		//@}

		///
		/// \name Interpolation Methods
		//@{
		///
		/// \brief Sets the interpolation style used by one of the features in the CompensationFunction
		///
		/// \param[in] feat Which of the four features to update the style for
		/// \param[in] style The new interpolation style that should be used by that feature
		void SetStyle(const CompensationFeature feat, const InterpolationStyle style);
		///
		/// \brief Gets the interpolation style used by one of the features in the CompensationFunction
		///
		/// \param[in] feat Which of the four features to return the style for
		/// \return The interpolation style used by the specified feature
		InterpolationStyle GetStyle(const CompensationFeature feat) const;
		//@}

	private:
		class Impl;
		Impl * p_Impl;
	};

	///
	/// \class CompensationTable Compensation.h include/Compensation.h
	/// \brief A table of CompensationPoints storing look-up data that can be transferred to memory in the Synthesiser
	///
	/// A CompensationTable always contains a list of CompensationPoints whose length is defined by
	/// the available memory depth in an iMS Synthesiser to which the CompensationTable is targetted.
	///
	/// For this reason, a valid IMSSystem object is required to be passed as a const reference to the
	/// Constructor because the table will be initialised to the length of the Synthesiser's look-up
	/// memory (read from IMSSynthesiser::Capabilities::LUTDepth).  Note that a dummy IMSSystem object
	/// could also be created with this field set to the LUT depth (in bits, i.e. 12 => 4096 deep LUT).
	/// Once the CompensationTable has been constructed, the IMSSystem object is no longer required and
	/// may be destroyed.
	///
	/// The length of the CompensationTable cannot be altered after construction.
	///
	/// The CompensationTable can be constructed with all entries initialised to zero, or to a default
	/// value.  Subsequently, random access is possible for both reading and modifying CompensationPoints,
	/// although a faster method for accessing contents is to use the iterators.
	///
	/// Each entry of a CompensationTable has a unique frequency associated with it.  Although not part
	/// of the table contents itself, it can be readily calculated from the upper and lower frequency
	/// bounds of the Synthesiser.  A helper function is available to do this calculation.
	///
	/// A CompensationTable may be saved to disk in a '.lut' file.  A Constructor also exists to
	/// read back from a previously saved .lut file, creating a CompensationTable from the contents of
	/// the file.
	///
	/// \author Dave Cowan
	/// \date 2015-11-03
	/// \since 1.0
	class LIBSPEC CompensationTable : public DequeBase < CompensationPoint >
	{
	public:
		///
		/// \name Constructors & Destructors
		//@{
		/// \brief Default Constructor
		///
		/// The default constructor should not normally be used by application code since the length of the table will
		/// be left undefined.  However it is a required constructor to complete the ImageProject class.
		/// If using, the new object should then be assigned from another CompensationTable to ensure that it does not
		/// contain dangling pointers
		/// \since 1.3
		CompensationTable();
		///
		/// \brief Empty Constructor
		///
		/// An IMSSystem object must be passed by const reference to the CompensationTable constructor in
		/// order to determine the correct depth of the LUT memory.
		/// \param[in] iMS the IMSSystem object representing the system the CompensationTable will be constructed for
		/// \since 1.0
		CompensationTable(std::shared_ptr<IMSSystem> iMS);
		/// This Explicit Empty Constructor makes it possible to create Compensation Tables without being physically connected
		/// to an iMS System.
		/// \param[in] LUTDepth the number of entries in the Compensation Look-Up Table
		/// \param[in] lower_freq the Lowest Frequency reproducible by the Synthesiser
		/// \param[in] upper_freq the Highest Frequency reproducible by the Synthesiser
		/// \since 1.3
		CompensationTable(int LUTDepth, const MHz& lower_freq, const MHz& upper_freq);
		///
		/// \brief Fill Constructor
		///
		/// Use this constructor to preload the CompensationTable with identical values of \c CompensationPoint
		///
		/// \param[in] iMS the IMSSystem object representing the system the CompensationTable will be constructed for
		/// \param[in] pt The CompensationPoint that will fill each of the new elements of the CompensationTable
		/// \since 1.0
		CompensationTable(std::shared_ptr<IMSSystem> iMS, const CompensationPoint& pt);
		/// This Explicit Fill Constructor makes it possible to create Compensation Tables without being physically connected
		/// to an iMS System.
		/// \param[in] LUTDepth the number of entries in the Compensation Look-Up Table
		/// \param[in] lower_freq the Lowest Frequency reproducible by the Synthesiser
		/// \param[in] upper_freq the Highest Frequency reproducible by the Synthesiser
		/// \param[in] pt The CompensationPoint that will fill each of the new elements of the CompensationTable
		/// \since 1.3
		CompensationTable(int LUTDepth, const MHz& lower_freq, const MHz& upper_freq, const CompensationPoint& pt);
		/// \brief File Read Constructor
		///
		/// Use this constructor to preload the CompensationTable with data read in from a file on disk
		///
		/// \param[in] iMS the IMSSystem object representing the system the CompensationTable will be constructed for
		/// \param[in] fileName A string pointing to a '*.lut' file on the filesystem containing preexisting CompensationTable data
		/// \param[in] chan For .lut files containing multiple tables, select which table to import
		/// \since 1.0
		CompensationTable(std::shared_ptr<IMSSystem> iMS, const std::string& fileName, const RFChannel& chan = RFChannel::all);
		/// This Explicit File Read Constructor makes it possible to create Compensation Tables without being physically connected
		/// to an iMS System.
		/// \param[in] LUTDepth the number of entries in the Compensation Look-Up Table
		/// \param[in] lower_freq the Lowest Frequency reproducible by the Synthesiser
		/// \param[in] upper_freq the Highest Frequency reproducible by the Synthesiser
		/// \param[in] fileName A string pointing to a '*.lut' file on the filesystem containing preexisting
		/// \param[in] chan For .lut files containing multiple tables, select which table to import
		/// \since 1.3
		CompensationTable(int LUTDepth, const MHz& lower_freq, const MHz& upper_freq, const std::string& fileName, const RFChannel& chan = RFChannel::all);
		/// \brief Non-volatile Memory Constructor
		///
		/// Use this constructor to preload the CompensationTable with data recalled from an entry in the Synthesiser
		/// FileSystem.
		///
		/// \param[in] iMS the IMSSystem object representing the system the CompensationTable will be constructed for
		/// \param[in] entry the entry in the FileSystem Table from which to recall a Compensation Table
		/// CompensationTable data
		/// \since 1.1
		CompensationTable(std::shared_ptr<IMSSystem> iMS, const int entry);
		/// \brief Copy Constructor with resizing
		///
		/// This Constructor will create the CompensationTable object from data supplied by another CompensationTable
		/// using the table specification (size of look-up table and upper and lower frequencies) given by the supplied
		/// reference to an iMS System.
		///
		/// Where the frequency of a look-up entry is not identical to any entry in the supplied table, the nearest spot
		/// frequency data will be used instead.
		///
		/// \param[in] iMS the IMSSystem object representing the system the CompensationTable will be constructed for
		/// \param[in] tbl the source of Compensation data that will be used to construct the new table
		/// \since 1.6
		CompensationTable(std::shared_ptr<IMSSystem> iMS, const CompensationTable& tbl);
		/// \brief Copy Constructor with resizing
		///
		/// This Constructor will create the CompensationTable object from data supplied by another CompensationTable
		/// using the table specification (size of look-up table and upper and lower frequencies) given explicitly.
		///
		/// Where the frequency of a look-up entry is not identical to any entry in the supplied table, the nearest spot
		/// frequency data will be used instead.
		///
		/// \param[in] LUTDepth the number of entries in the Compensation Look-Up Table
		/// \param[in] lower_freq the Lowest Frequency reproducible by the Synthesiser
		/// \param[in] upper_freq the Highest Frequency reproducible by the Synthesiser
		/// \param[in] tbl the source of Compensation data that will be used to construct the new table
		/// \since 1.6
		CompensationTable(int LUTDepth, const MHz& lower_freq, const MHz& upper_freq, const CompensationTable& tbl);
		/// \brief Destructor
		~CompensationTable();

		/// \brief Copy Constructor
		CompensationTable(const CompensationTable &);
		/// \brief Assignment Constructor
		CompensationTable &operator =(const CompensationTable &);
		//@}

		///
		/// \name Render the CompensationTable object from a CompensationFunction
		///
		/// Use these methods to take a CompensationFunction specifying compensation data over a range of spot frequencies 
		/// and an interpolation style and render it into the CompensationTable.  CompensationFunction data may be applied
		/// to one or all of the four features of the CompensationTable and the data may be overwritten, or modified in place
		///
		//@{
		///
		/// \brief Apply a CompensationFunction object to one feature of the CompensationTable
		///
		/// Pass in a CompensationFunction object suitably populated with point specification data at multiple spot frequencies.
		/// The CompensationFunction object only needs to have specification data for the feature that will be updated by
		/// this function.  This function will perform the interpolation process encoded into the CompensationFunction and render
		/// the results into every frequency entry of the CompensationTable.  If the modifier is set to Multiply, the existing
		/// data is multiplied by the newly interpolated data, otherwise the new data replaces the existing.
		/// 
		/// \param[in] func The CompensationFunction to render the new data from
		/// \param[in] feat Which of the four features in the CompensationFunction to render
		/// \param[in] modifier Whether to replace the existing data (default) or modify it
		/// return true if the function was applied successfully
		bool ApplyFunction(const CompensationFunction& func, const CompensationFeature feat, CompensationModifier modifier = CompensationModifier::REPLACE);
		///
		/// \brief Apply a CompensationFunction object to all features of the CompensationTable
		///
		/// Pass in a CompensationFunction object suitably populated with point specification data at multiple spot frequencies.
		/// The CompensationFunction object needs to have specification data for all four features that will be updated by
		/// this function.  This function will perform the interpolation process encoded into the CompensationFunction and render
		/// the results into every frequency entry of the CompensationTable.  If the modifier is set to Multiply, the existing
		/// data is multiplied by the newly interpolated data, otherwise the new data replaces the existing.
		/// 
		/// \param[in] func The CompensationFunction to render the new data from
		/// \param[in] modifier Whether to replace the existing data (default) or modify it
		/// return true if the function was applied successfully
		bool ApplyFunction(const CompensationFunction& func, CompensationModifier modifier = CompensationModifier::REPLACE);
		//@}

		/// \name Helper Functions
		//@{
		/// \brief Returns the Number of Entries in the CompensationTable
		/// \return std::size_t representing the number of CompensationTable entries (which is defined in the Constructor)
		/// \since 1.0
		const std::size_t Size() const;
		/// \brief Returns the frequency represented by a given entry in the CompensationTable
		///
		/// Each entry in the CompensationTable has an implied frequency at which it will become active.
		/// \param[in] index The CompensationTable entry to retrieve the associated Frequency for
		/// \return The Frequency value which the CompensationTable entry represents
		/// \since 1.0
		const MHz FrequencyAt(const unsigned int index) const;

		const MHz LowerFrequency() const;
		const MHz UpperFrequency() const;
		//@}

		/// \name Save To Disk
		//@}
		/// \brief Save Table contents to file using latest protocol version
		///
		/// The contents of this CompensationTable can be saved to disk for retrieval at a later time.
		/// Calling this function will write out the contents of the table to a file which is opened
		/// at the filesystem location given by the string fileName.
		///
		/// \warning If the file already exists, it is overwritten, without warning.
		///
		/// If the function cannot create the file, it will not save the table, and return false.
		///
		/// fileName can be any valid filesystem location and any name, but we recommend the use of the
		/// file extension '.lut'
		/// \param[in] fileName the name and location of the file to write CompensationTable data to
		/// \return true if the save operation completed successfully
		/// \deprecated From v1.6.0, this function does nothing and always returns false.  Use CompensationTableExporter instead.
		/// \since 1.0
		const bool Save(const std::string& fileName) const;
		//@}

	private:
		class Impl;
		Impl * p_Impl;
	};

	///
	/// \class CompensationTableExporter
	/// \brief Use this class to save one or more compensation tables to a file (.LUT file) on disk
	///
	/// It can be useful to save a CompensationTable or set of CompensationTable's to disk for later
	/// recall, for example if crafting a CompensationTable that equalises the efficiency of an AO device
	/// over its entire frequency range it might be useful to have a calibration program that results in
	/// a CompensationTable.  You would not want to run the calibration routine each time the AO device is
	/// powered up, so you would save the table to disk and re-import it on program start.
	///
	/// A .LUT file may be exported with just one CompensationTable (typical when using the Compensation
	/// feature in "Global" scope) or multiple CompensationTable's (typical when using the Compensation
	/// feature in "Channel" scope - use one CompensationTable per channel of the Synthesiser).
	/// 
	/// The CompensationTableExporter class contains a singular CompensationTable object representing
	/// a global scoped LUT and an array of CompensationTable objects representing a channel-scoped LUT, 
	/// one element of the array for each RF Channel.  To export a global-scoped LUT file, assign the global object
	/// with a CompensationTable using either the Global Scope constructor or the ProvideGlobalTable() function,
	/// then call ExportGlobalLUT().  To export a channel-scoped LUT file, assign each channel of the channel 
	/// objects with a CompensationTable using ProvideChannelTable(), then call ExportChannelLUT().  The export
	/// process will fail (return false) if not all of the channel objects in the array have been assigned.
	///
	/// \author Dave Cowan
	/// \date 2019-02-26
	/// \since 1.6.0
	class LIBSPEC CompensationTableExporter
	{
	public:
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for CompensationTableExporter Object
		///
		/// Constructs the CompensationTableExporter object using the IMSSystem reference to infer the number
		/// of channels that would be required if exporting a Channel-scoped compensation LUT.
		///
		/// \param[in] ims A const reference to an iMS System
		/// \since 1.6.0
		CompensationTableExporter(std::shared_ptr<IMSSystem> ims);
		///
		/// \brief Constructor for CompensationTableExporter Object
		///
		/// Constructs the CompensationTableExporter object with an explicit number of channels to be used
		/// if exporting a Channel-scoped compensation LUT.
		///
		/// \param[in] channels the number of channels to export in a Channel-scoped LUT file
		/// \since 1.6.0
		CompensationTableExporter(const int channels);
		///
		/// \brief Empty Constructor for CompensationTableExporter Object
		///
		/// Constructs the CompensationTableExporter object without specifying the number of channels that
		/// would be required for a Channel-scoped LUT.  The number of channels defaults to 1 which means
		/// the LUT file is generated identically for Global-scope and Channel-scope exports.
		///
		/// \since 1.6.0
		CompensationTableExporter();
		///
		/// \brief Global Scope Constructor for CompensationTableExporter Object
		///
		/// Constructs the CompensationTableExporter object and initialises its global table with a supplied
		/// reference to a CompensationTable.  Call ExportGlobalLUT() after using this constructor to complete
		/// the export process.
		///
		/// \since 1.6.0
		CompensationTableExporter(const CompensationTable& tbl);
		///
		/// \brief Destructor for CompensationTableExporter Object
		~CompensationTableExporter();
		//@}

		///
		/// \name Assign internal CompensationTable objects
		//@{
		/// 
		/// \brief Assign the global object with a CompensationTable
		///
		/// Use this function to provide a CompensationTable that will be assigned to the internal global object
		/// \param[in] tbl The CompensationTable that will be copied to the global object
		void ProvideGlobalTable(const CompensationTable& tbl);
		///
		/// \brief Assign one of the channel objects ith a CompensationTable
		///
		/// Use this function to provide a CompensationTable that will be assigned to one of the internal objects
		/// in the channel array.  Assign tables to all of the elements in the array before exporting.
		/// \param[in] chan Which channel in the array to copy the table to
		/// \param[in] tbl The CompensationTable that will be copied to the channel object array
		void ProvideChannelTable(const RFChannel& chan, const CompensationTable& tbl);
		//@}

		///
		/// \name File Export functions
		//@{
		///
		/// \brief Export Global Object to LUT File
		///
		/// Use this function to create the global-scope LUT file from previously assigned CompensationTable
		/// data in the internal global object.  If the global object has not been assigned, the function will
		/// fail.  Any previously existing file will be overwritten without warning.
		/// \param[in] fileName Name of the .LUT file to export to.
		/// \return true if the file export completed successfully, false otherwise
		bool ExportGlobalLUT(const std::string& fileName);
		///
		/// \brief Export Channel Object array to LUT file
		///
		/// Use this function to create the channel-scope LUT file from previously assigned CompensationTable
		/// data in the internal channel object array.  If the complete channel array has not been assigned
		/// on all channels, the function will fail.  Any previously existing file will be overwritten without warning.
		/// \param[in] fileName Name of the .LUT file to export to.
		/// \return true if the file export completed successfully, false otherwise
		bool ExportChannelLUT(const std::string& fileName);
		//@}
	private:
		// Makes this object non-copyable
		CompensationTableExporter(const CompensationTableExporter &);
		const CompensationTableExporter &operator =(const CompensationTableExporter &);

		class Impl;
		Impl * p_Impl;
	};

	///
	/// \class CompensationTableImporter
	/// \brief Use this class to retrieve one or more compensation tables from a file (.LUT file) on disk
	///
	/// Create an instance of this class around a file from disk and, provided it is in the correct format
	/// for a .LUT file, CompensationTable objects can be retrieved from it.
	///
	/// On constructing the class object, the file is opened and the header parsed.  If the file cannot be
	/// opened or the header is in a format that isn't recognised, no further action can be taken with this
	/// object and IsValid() will return false.
	///
	/// After construction, information about the file contents is available to user code by examining the
	/// IsGlobal(), Channels(), Size(), LowerFrequency() and UpperFrequency() function calls.  A CompensationTable
	/// can be extracted from the file by calling either RetrieveGlobalLUT() or RetrieveChannelLUT(), specifying
	/// which channel to read back from where necessary
	///
	/// \author Dave Cowan
	/// \date 2019-02-26
	/// \since 1.6.0
	class LIBSPEC CompensationTableImporter
	{
	public:
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for CompensationTableImporter Object
		///
		/// Constructs the CompensationTableImporter object wrapped around a file on disk that is specified
		/// as the string argument passed to the constructor.
		///
		/// \param[in] fileName String pointing to a file on disk from which to read back CompensationTable data
		/// \since 1.6.0
		CompensationTableImporter(const std::string& fileName);
		///
		/// \brief Destructor for CompensationTableImporter Object
		~CompensationTableImporter();
		//@}

		///
		/// \name CompensationTable parameters
		//@{
		///
		/// \brief Does the file contain valid CompensationTable contents
		/// \return true if the file can be opened and successfully parsed
		bool IsValid() const;
		///
		/// \brief Is the CompensationTable data in the file intended for global scoped use or channel scoped use
		/// \return true if the data is global-scoped
		bool IsGlobal() const;
		///
		/// \brief The number of CompensationTable data objects that can be retrieved from the file
		/// \return the number of channels in a channel-scoped LUT file, or 1 if global
		int Channels() const;
		///
		/// \brief The size (number of CompensationPoint entries) of each CompensationTable object in the file
		/// \return number of points in each table
		int Size() const;
		///
		/// \brief The lower frequency bound of the CompensationTable data
		/// \return a MHz frequency object representing the lower bound
		MHz LowerFrequency() const;
		///
		/// \brief The upper frequency bound of the CompensationTable data
		/// \return a MHz frequency object representing the upper bound
		MHz UpperFrequency() const;
		//@}

		///
		/// \name Obtain CompensationTable object
		//@{
		///
		/// \brief Returns a CompensationTable from a global-scoped LUT
		/// \return A CompensationTable object that is read from a LUT file that encodes a global-scoped LUT
		CompensationTable RetrieveGlobalLUT();
		///
		/// \brief Returns one CompensationTable from a channel-scope LUT
		///
		/// A channel-scope LUT file will encode multiple CompensationTable, one per channel.
		/// \param[in] chan Which channel from the channel array to access. If greater than the number of channels in the file, an empty CompensationTable object is returned
		/// \return A CompensationTable object that is read from a LUT file that encodes a channel-scoped LUT
		CompensationTable RetrieveChannelLUT(RFChannel& chan);
		//@}

	private:
		// Makes this object non-copyable
		CompensationTableImporter(const CompensationTableImporter &);
		const CompensationTableImporter &operator =(const CompensationTableImporter &);

		class Impl;
		Impl * p_Impl;
	};

	///
	/// \class CompensationTableDownload Compensation.h include\Compensation.h
	/// \brief Provides a mechanism for downloading and verifying Compensation Tables to a Synthesiser's Look-Up memory
	/// \author Dave Cowan
	/// \date 2015-11-11
	/// \since 1.0
	class LIBSPEC CompensationTableDownload : public IBulkTransfer
	{
	public:
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for CompensationTableDownload Object
		///
		/// The pre-requisites for an CompensationTableDownload object to be created are:
		///   (1) - an IMSSystem object, representing the configuration of an iMS target to which the CompensationTable
		/// is to be downloaded.
		///   (2) - a complete CompensationTable object to download to the iMS target.
		///
		/// CompensationTableDownload stores const references to both.  This means that both must exist before the
		/// CompensationTableDownload object, and both must remain valid (not destroyed) until the CompensationTableDownload
		/// object itself is destroyed.  Because they are stored as references, the IMSSystem and CompensationTable
		/// objects themselves may be modified after the construction of the CompensationTableDownload object.
		///
		/// Once constructed, the object can neither be copied or assigned to another instance.
		///
		/// CompensationTable's may either be downloaded to apply to all channels of the Synthesiser (Global scope)
		/// or to only one channel of the Synthesiser (Channel scope)
		///
		/// \param[in] ims A reference to the iMS System which is the target for downloading the Image
		/// \param[in] tbl A const reference to the CompensationTable which shall be downloaded to the target
		/// \param[in] chan An optional const reference to which RF Channel of the Synthesiser the compensation should be applied to
		/// \since 1.0
		CompensationTableDownload(std::shared_ptr<IMSSystem> ims, const CompensationTable& tbl, const RFChannel& chan = RFChannel::all);
		///
		/// \brief Destructor for CompensationTableDownload Object
		~CompensationTableDownload();
		//@}

		/// \name Bulk Transfer Initiation
		//@{
		bool StartDownload();
		bool StartVerify();
		//@}

		///
		/// \name Retrieve Error Information
		//@{
		int GetVerifyError();
		//@}

		///
		/// \name Event Notifications
		//@{
		///
		/// \brief Subscribe a callback function handler to a given CompensationEvents entry
		///
		/// CompensationTableDownload can callback user application code when an event occurs in the
		/// download process.  Supported events are listed under CompensationEvents.  The
		/// callback function must inherit from the IEventHandler interface and override
		/// its EventAction() method.
		///
		/// Use this member function call to subscribe a callback function to an CompensationEvents entry.
		/// For the period that a callback is subscribed, each time an event in CompensationTableDownload occurs
		/// that would trigger the subscribed CompensationEvents entry, the user function callback will be
		/// executed.
		/// \param[in] message Use the CompensationEvents::Event enum to specify an event to subscribe to
		/// \param[in] handler A function pointer to the user callback function to execute on the event trigger.
		/// \since 1.0
		void CompensationTableDownloadEventSubscribe(const int message, IEventHandler* handler);
		/// \brief Unsubscribe a callback function handler from a given CompensationEvents entry
		///
		/// Removes all links to a user callback function from the Event Trigger map so that any
		/// events that occur in the CompensationTableDownload object following the Unsubscribe request
		/// will no longer execute that function
		/// \param[in] message Use the CompensationEvents::Event enum to specify an event to unsubscribe from
		/// \param[in] handler A function pointer to the user callback function that will no longer execute on an event
		/// \since 1.0
		void CompensationTableDownloadEventUnsubscribe(const int message, const IEventHandler* handler);
		//@}

		/// \name Store in Synthesiser Non-Volatile Memory
		//@{
		/// \brief Store Table contents to non-volatile memory on the synthesiser
		///
		/// The contents of this CompensationTable can be stored to an area of non-volatile memory on the
		/// Synthesiser for retrieval at a future time, including after subsequent power cycles.  The
		/// data stored can be used to select between alternative CompensationTables (e.g.  for different
		/// AOD crystal materials) without needing to recalculate or download from Software.
		///
		/// The table can be flagged to be used as a default at startup in which case the Synthesiser will
		/// use the contents as a default LUT program allowing the Synthesiser to be used
		/// with no connection to a host system.
		///
		/// \param[in] def mark the entry as a default and the Synthesiser will attempt 
		/// to program the data to the Local Tone Buffer on power up.
		/// \param[in] FileName a string to tag the download with in the File System Table (limited to 8 chars)
		/// \return the index in the File System Table where the data was stored or -1 if the operation failed
		/// \since 1.1
		const FileSystemIndex Store(FileDefault def, const std::string& FileName) const;
		//@}

	private:
		// Makes this object non-copyable
		CompensationTableDownload(const CompensationTableDownload &);
		const CompensationTableDownload &operator =(const CompensationTableDownload &);

		class Impl;
		Impl * p_Impl;
	};

}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
