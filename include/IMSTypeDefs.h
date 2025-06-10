/*-----------------------------------------------------------------------------
/ Title      : iMS Useful Type Defines Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/h/IMSTypeDefs.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2024-11-07 15:36:38 +0000 (Thu, 07 Nov 2024) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 632 $
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
/// \file IMSTypeDefs.h
///
/// \brief Useful Type Definitions for working with iMS Systems
///
///
/// \author Dave Cowan
/// \date 2015-11-03
/// \since 1.0
/// \ingroup group_TypeDefs
///

#ifndef IMS_IMSTYPEDEFS_H__
#define IMS_IMSTYPEDEFS_H__

#include <cmath>
#include <stdexcept>
#include <cstdint>
#include <vector>
#include <chrono>

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

// This export doesn't work in VS2015
//EXPIMP_TEMPLATE template class LIBSPEC std::chrono::duration < double, std::ratio<1, 1> >;
// disable associated compiler warning
#if defined _WIN32
#pragma warning(push)
#pragma warning (disable:4251)
#endif

namespace iMS
{
	// Forward Declaration
	class IMSSystem;

    ///
    /// \class Frequency IMSTypeDefs.h include/IMSTypeDefs.h
    /// \brief Type Definition for all operations that require a frequency specification
    ///
    /// Internally, the Frequency value is stored as a double precision variable specified in Hertz
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC Frequency {
	protected:
		double value;

	public:
		/// \brief Construct a Frequency object from a double argument representing Hertz
		/// \param[in] arg Frequency in Hertz
		/// \since 1.0
		Frequency(double arg = 0.0);

		/// \brief Assignment of a double argument in Hertz to an existing Frequency object
		///
		/// \code
		/// Frequency f;
		/// f = 1000.0;
		/// // f contains 1000Hz
		/// \endcode
		/// \since 1.0
		Frequency& operator = (double arg);

		/// \brief Return a double representing the Frequency value in Hertz
		/// \code
		/// kHz f1(1.2);
		/// Frequency f2 = f1();
		/// std::cout << "f2's Frequency is: " << f2() << "Hz" << std::endl;
		/// \endcode
		/// prints:
		/// \code
		/// f2's Frequency is 1200.0Hz
		/// \endcode
		/// \since 1.0
		operator double() const;
	};

    ///
    /// \class kHz IMSTypeDefs.h include/IMSTypeDefs.h
    /// \brief Type Definition for all operations that require a frequency specification in kiloHertz
    ///
    /// kHz inherits from Frequency, which internally stores the value in Hertz.
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC kHz : public Frequency {
	public:
		/// \brief Construct a kHz object from a double argument representing kiloHertz
		/// \param[in] arg Frequency in kiloHertz
		/// \since 1.0
		kHz(double arg = 0.0);

		/// \brief Assignment of a double argument in kiloHertz to an existing Frequency object
		///
		/// \code
		/// kHz f;
		/// f = 1.0;
		/// // f contains 1000Hz
		/// \endcode
		/// \since 1.3
		kHz& operator = (double arg);

		/// \brief Return a double representing the Frequency value in kiloHertz
		/// \code
		/// Frequency f1(3750.0);
		/// kHz f2 = f1();
		/// std::cout << "f2's Frequency is: " << f2() << "kHz" << std::endl;
		/// \endcode
		/// prints:
		/// \code
		/// f2's Frequency is 3.75kHz
		/// \endcode
		/// \since 1.0
		operator double() const;
	};

    ///
    /// \class MHz IMSTypeDefs.h include/IMSTypeDefs.h
    /// \brief Type Definition for all operations that require a frequency specification in MegaHertz
    ///
    /// MHz inherits from Frequency, which internally stores the value in Hertz.
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC MHz : public Frequency {
	public:
		/// \brief Construct a MHz object from a double argument representing MegaHertz
		/// \param[in] arg Frequency in MegaHertz
		/// \since 1.0
		MHz(double arg = 0.0);

		/// \brief Assignment of a double argument in MegaHertz to an existing Frequency object
		///
		/// \code
		/// MHz f;
		/// f = 1.0;
		/// // f contains 1,000,000Hz
		/// \endcode
		/// \since 1.3
		MHz& operator = (double arg);

		/// \brief Return a double representing the Frequency value in MegaHertz
		/// \code
		/// Frequency f1(1234567.0);
		/// MHz f2 = f1();
		/// std::cout << "f2's Frequency is: " << f2() << "MHz" << std::endl;
		/// \endcode
		/// prints:
		/// \code
		/// f2's Frequency is 1.234567MHz
		/// \endcode
		/// \since 1.0
		operator double() const;
	};

    ///
    /// \class Percent IMSTypeDefs.h include/IMSTypeDefs.h
    /// \brief Type Definition for all operations that require a percentage specification
    ///
    /// Internally, the Percent value is stored as a double precision variable and is
    /// bounds-limited to 0.0 <= Percent <= 100.0.
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC Percent {
		double value;

	public:
		/// \brief Construct a Percent object from a double argument and check its value is within
		/// the range 0.0 <= arg <= 100.0.  If not, the object is still constructed, but the value is
		/// clipped to the upper or lower bound.
		/// \param[in] arg The percentage value
		/// \since 1.0
		Percent(double arg = 0.0);

		/// \brief Assignment of a double argument in percent to an existing Percent object.
		///
		/// The double argument of the assigner must be within the range 0.0 <= arg <= 100.0 else it
		/// will be limited to those bounds.
		/// \code
		/// // In a group of 7 children, 3 of them have dark hair
		/// Percent ChildrenWithDarkHair = (3.0 / 7.0) * 100.0;
		/// std::cout << ChildrenWithDarkHair << "% of the group have dark hair" << std::endl;
		/// \endcode
		/// prints:
		/// \code
		/// 42.8571% of the group have dark hair
		/// \endcode
		/// \since 1.0
		Percent& operator = (double arg);

		/// \brief Return a double representing the Percent object's value
		/// \since 1.0
		operator double() const;
	};

    ///
    /// \class Degrees IMSTypeDefs.h include/IMSTypeDefs.h
    /// \brief Type Definition for all operations that require an angle specification in degrees
    ///
    /// Internally, the Degrees value is stored as a double precision variable
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC Degrees {
		double value;

	public:
		/// \brief Construct a Degrees object from a double argument.
		/// \param[in] arg The degrees value
		/// \since 1.0
		Degrees(double arg = 0.0);

		/// \brief Assignment of a double argument in degrees to an existing Degrees object.
		///
		/// The double argument of the assigner must be within the range 0.0 <= arg < 360.0 else it
		/// will be wrapped around to fit within the range.
		/// \code
		/// // needed for PI
		/// #define _USE_MATH_DEFINES
		/// #include <iostream>
		/// #include <cmath>
		///
		/// Degrees phase = atan2(1.0, -1.0) * (360.0 / 2 * M_PI);
		/// std::cout << "The arctangent for [x=-1, y=1] is " << phase << " degrees" << std::endl;
		/// \endcode
		/// prints:
		/// \code
		/// The arctangent for [x=-1, y=1] is 135.000000 degrees
		/// \endcode
		/// \since 1.0
		Degrees& operator = (double arg);

		/// \brief Return a double representing the Degrees object's value
		/// \since 1.0
		operator double() const;
	};

    /// \struct FAP IMSTypeDefs.h include/IMSTypeDefs.h
    /// \brief FAP (Frequency/Amplitude/Phase) triad stores the instantaneous definition of a single RF output
    ///
    /// The FAP struct, also known as a triad, stores one frequency (in MHz), one amplitude (Percent) and one
    /// phase (Degrees) value which uniquely specifies the instantaneous output of any one RF channel output.
    ///
    /// 4 FAP's make up a single ImagePoint, one per RF channel, and sequences of ImagePoints then
    /// make up an Image.
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	struct LIBSPEC FAP
	{
    /// The RF Channel Output Frequency
		MHz freq;
    /// The RF Channel Output Amplitude
		Percent ampl;
    /// The RF Channel Output Phase
		Degrees phase;

	/// \brief Default construct a FAP object with zero data
		FAP();
    /// \brief Construct a FAP object from raw double precision input data
		FAP(double f, double a, double p);
    /// \brief Construct a FAP object from pre-existing MHz, Percent and Degrees objects
		FAP(MHz f, Percent a, Degrees p);

	/// \name Equality Operators
	//@{
	/// \brief Equality operators compare FAPs against each other
	/// \since 1.1.0
	///
		bool operator==(const FAP &other) const;
		bool operator!=(const FAP &other) const;
	};
	//@}

	/// \enum ENHANCED_TONE_MODE
	/// \brief Selects which type of linear sweep to perform in Enhanced Tone mode
	///
	///
	/// \since 1.4.3
	enum class ENHANCED_TONE_MODE
	{
		/// No sweep. Channel outputs a static tone.
		NO_SWEEP,
		/// Frequency Sweep with output dwell at final specification point.
		FREQUENCY_DWELL,
		/// Frequency Sweep with output returning to initial specification point at end of sweep.
		FREQUENCY_NO_DWELL,
		/// Two Point Frequency Fast Modulation
		FREQUENCY_FAST_MOD,
		/// Phase Sweep with output dwell at final specification point.
		PHASE_DWELL,
		/// Phase Sweep with output returning to initial specification point at end of sweep.
		PHASE_NO_DWELL,
		/// Two Point Phase Fast Modulation
		PHASE_FAST_MOD
	};
	///

	/// \enum DAC_CURRENT_REFERENCE
	/// \brief Sets the amount by which the current reference controlling the DAC amplitude is scaled down by in enhanced tone modes.
	///
	/// Note that normal amplitude control is not possible with linear sweeps, so the DAC current reference is provided
	/// as an alternative.
	///
	/// \since 1.4.3
	enum class DAC_CURRENT_REFERENCE
	{
		/// (Default) Output signal uses full current reference
		FULL_SCALE,
		/// Output signal reduced to half of current reference
		HALF_SCALE,
		/// Output signal reduced to 1/4 of current reference
		QUARTER_SCALE,
		/// Output signal reduced to 1/8 of current reference
		EIGHTH_SCALE
	};

	/// \class SweepTone IMSTypeDefs.h include/IMSTypeDefs.h
	///
	/// \brief Full specification for a single channel in Enhanced Tone Mode (ETM).
	///
	/// There are 5 different modes of operation per channel in ETM:
    ///
    /// (1) No Sweep. The output is programmed to a static tone using the Frequency, Amplitude and Phase point specification
    /// written in 'start'. 'end', 'up_ramp', 'down_ramp' and 'n_steps' are unused.
    ///
    /// (2) Frequency Sweep No Dwell. The output performs a linear sweep in frequency from the frequency in 'start' to the 
    /// frequency in 'end'.  Upon reaching 'end', the output returns instantly to the 'start' frequency until it is triggered
    /// again.
    ///
    /// (3) Frequency Sweep With Dwell. The output performs a linear sweep in frequency from the frequency in 'start' to the 
    /// frequency in 'end'.  Upon reaching 'end', the output remains at that frequency until the associated channel Profile
    /// input is deasserted and then a downward ramp is performed.
	///
    /// (4) Phase Sweep No Dwell. The output performs a linear sweep in phase from the phase in 'start' to the 
    /// phase in 'end'.  Upon reaching 'end', the output returns instantly to the 'start' phase until it is triggered
    /// again.
    ///
    /// (5) Phase Sweep With Dwell. The output performs a linear sweep in phase from the phase in 'start' to the 
    /// phase in 'end'.  Upon reaching 'end', the output remains at that phase until the associated channel Profile
    /// input is deasserted and then a downward ramp is performed.
	///
	/// The trigger for the linear sweep ramps is the 4 channel Profile input signals to the Synthesiser.  Alternatively,
	/// the signals may be controlled (overridden) from software, using the Auxiliary::SetDDSProfile method.
	///
	/// Rising and Falling ramp deltas are automatically computed by the API code, as are the Rising and Falling Ramp Rate
	/// (delta time) values from the desired up_ramp and down_ramp durations and the target number of interpolation steps.
	/// Note that is the target interpolation is too wide (> approx 2 microseconds per step) or too narrow (less than 8 nanoseconds),
	/// the code will limit the interpolation step to the minimum or maximum.
	///
	/// \date 2018-01-05
	/// \since 1.4.3
	struct LIBSPEC SweepTone 
	{
		/// Initial specification point for tone sweep
		FAP& start(); 
		/// Initial specification point for tone sweep
		const FAP& start() const;
		/// Final specification point for tone sweep
		FAP& end();
		/// Final specification point for tone sweep
		const FAP& end() const;
		/// The duration over which the rising sweep takes place
		std::chrono::duration<double>& up_ramp();
		/// The duration over which the rising sweep takes place
		const std::chrono::duration<double>& up_ramp() const;
		/// The duration over which the falling sweep takes place
		std::chrono::duration<double>& down_ramp();
		/// The duration over which the falling sweep takes place
		const std::chrono::duration<double>& down_ramp() const;
		/// the required step resolution of the tone sweep (number of steps from start to end - subject to hardware limitations)
		int& n_steps();
		/// the required step resolution of the tone sweep (number of steps from start to end - subject to hardware limitations)
		const int& n_steps() const;
		/// which type of sweep to perform
		ENHANCED_TONE_MODE& mode();
		/// which type of sweep to perform
		const ENHANCED_TONE_MODE& mode() const;
		/// Sets the output signal current reference scaling
		DAC_CURRENT_REFERENCE& scaling();
		/// Sets the output signal current reference scaling
		const DAC_CURRENT_REFERENCE& scaling() const;

	/// \brief Default Constructor
		SweepTone();
	/// \brief Parital specification Constructor programs a single tone only (no sweep)
		SweepTone(FAP tone);
	/// \brief Full specification Constructor
		SweepTone(FAP start, FAP end, std::chrono::duration<double>& up, std::chrono::duration<double>& down, int steps, ENHANCED_TONE_MODE mode, DAC_CURRENT_REFERENCE scaling);
	/// \brief Copy Constructor
		SweepTone(const SweepTone &);
	/// \brief Assignment Constructor
		SweepTone &operator =(const SweepTone &);

	private:
		class Impl;
		Impl* p_Impl;
	};

    /// \class RFChannel IMSTypeDefs.h include/IMSTypeDefs.h
    /// \brief Type that represents the integer values 1, 2, 3 and 4, one each for the RF Channels of an iMS Synthesiser
    ///
    /// The type is used to ensure that incorrect channel specifications cannot be passed to functions
    /// requiring an argument referencing an RF output channel.  Attempting to use an integer outside
    /// the range 1 <= arg <= 4 will result in RFChannel = 1 and an invalid_argument exception being thrown.
    /// \throws std::invalid_argument("Invalid RF Channel Number") Attempted to use an integer specification
    /// not tied to an RF Output Channel
    /// \author Dave Cowan
    /// \date 2015-11-03
    /// \since 1.0
    ///
	class LIBSPEC RFChannel {
	private:
		int value;
		
	public:
	/// \brief Default construct an RF Channel object initialised to all RF Channels
	/// \since 1.1
		RFChannel();
    /// \brief Construct an RF Channel object and check that it is being created with an integer value
    /// within the range 1 <= arg <= 4.  If not, the object is still constructed, but the RF Channel
    /// value is set to 1 and an invalid_argument exception is thrown
    /// \param[in] arg The channel specification
    /// \throws std::invalid_argument("Invalid RF Channel Number") Attempted to use an integer specification
    /// not tied to an RF Output Channel
    /// \since 1.0
		RFChannel(int arg);

    /// \brief Assignment of an integer argument to an existing RF Channel object
    ///
    /// Checks that it is being created with an integer value
    /// within the range 1 <= arg <= 4.  If not, the object is still constructed, but the RF Channel
    /// value is set to 1 and an invalid_argument exception is thrown
    /// \param[in] arg The channel specification
    /// \throws std::invalid_argument("Invalid RF Channel Number") Attempted to use an integer specification
    /// not tied to an RF Output Channel
    /// \since 1.0
		RFChannel& operator = (int arg);

	/// \name Increment & Decrement
	//@{
	/// \brief Prefix and Postfix operators for (dec)incrementing through channels
	/// \since 1.1
		RFChannel& operator++();
		RFChannel operator++(int);
		RFChannel& operator--();
		RFChannel operator--(int);
	//@}

	/// \name 'All' status
	//@{
	/// \brief Returns true if the object is set to the special state 'All Channels'
	/// \since 1.6
		bool IsAll() const;
	//@}

	//@{
	/// \brief Returns minimum and maximum allowed values
		static const int min = 1;
		static const int max = 4;
		static const int all;
		//@}

	/// \brief Return an integer representing the RF Channel that the object references
    /// \since 1.0
		operator int() const;

	};

	///
	/// \class distance  IMSTypeDefs.h include/IMSTypeDefs.h
    /// \brief A generic templated header-only class for handling related distance measurements 
	/// \author Dave Cowan
	/// \date 2019-04-03
	/// \since 1.7.0
	template <typename Ratio>
	class distance {
		double ticks;
	public:
		/// \brief Default Constructor
		distance(double ticks = 1.0) :
			ticks(ticks)
		{}

		/// \brief Copy Constructor
		/// Creates object from another distance object with ratio-defined value
		template <typename Ratio2>
		distance(distance<Ratio2> other) {
			using divided = std::ratio_divide<Ratio2, Ratio>;
			ticks = other.operator double() * divided::num / divided::den;
		}

		/// \brief Assignment Constructor
		distance& operator = (double arg) {
			ticks = arg; return *this;
		}

		/// \brief Get Distance value
		operator double() const {
			return ticks;
		}
	};

	/// \brief Returns true if two distance objects are identical, irrespective of distance-prefix
	template <typename Ratio1, typename Ratio2>
	bool operator==(distance<Ratio1> d1, distance<Ratio2> d2) {
		double normalized1 = d1.count() * Ratio1::num / Ratio1::den;
		double normalized2 = d2.count() * Ratio2::num / Ratio2::den;
		return normalized1 == normalized2;
	}

	/// \brief Metres
	using metre = distance<std::ratio<1>>;
	/// \brief Nanometres
	using nanometre = distance<std::nano>;
	/// \brief Micrometres
	using micrometre = distance<std::micro>;
	/// \brief Millimetres
	using millimetre = distance<std::milli>;
	/// \brief Centimetres
	using centimetre = distance<std::centi>;
	/// \brief Decimetres
	using decimetre = distance<std::deci>;

	// Alias the US spellings!
	/// \brief Meters
	using meter = metre;
	/// \brief Nanometers
	using nanometer = nanometre;
	/// \brief Micrometers
	using micrometer = micrometre;
	/// \brief Millimeters
	using millimeter = millimetre;
	/// \brief Centimeters
	using centimeter = centimetre;
	/// \brief Decimeters
	using decimeter = decimetre;

	/// <summary>
	///  Signal Polarity: rising or falling edge
	/// </summary>
	enum class Polarity {
		/// CLK / TRIG are active on the rising edge.  ENABLE is active high
		NORMAL,
		/// CLK / TRIG are active on the falling edge.  ENABLE is active low
		INVERSE
	};
}

#if defined _WIN32
#pragma warning(pop)
#endif

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
