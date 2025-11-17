/*-----------------------------------------------------------------------------
/ Title      : Acousto Optics Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/AcoustoOptics/h/AcoustoOptics.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2019-04-02
/ Last update: $Date: 2022-02-23 14:10:12 +0000 (Wed, 23 Feb 2022) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 521 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2019 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Description
/ 2019-04-02  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

///
/// \file AcoustoOptics.h
///
/// \brief Classes for obtaining parameters associated with Acousto-Optic materials and devices
/// The iMS SDK maintains an internal database of Acousto-Optic materials and their properties
/// which can be explored using these classes.  This makes it easy to calculate physical parameters
/// of the material from known quantities, for example the Bragg Angle for a given optical wavelength
/// and acoustic Frequency.
///
/// The SDK also includes an internal database of Isomet AO Devices which allow straightforward 
/// selection of a device from its model number.  This can be used to simplify system design by
/// calculating the required Function for beam steered Phase Compensation for a provided optical
/// wavelength.
///
/// \author Dave Cowan
/// \date 2019-04-02
/// \since 1.7.0
/// \ingroup group_AO
///

#ifndef IMS_ACOUSTOOPTICS_H__
#define IMS_ACOUSTOOPTICS_H__

#include "IMSTypeDefs.h"
#include "Compensation.h"
#include <string>

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
	/// \class Crystal
	/// \brief "Shopfront" class to the internal database storing AO Crystal physical properties
	///
	/// Reports Acoustic Velocity for each material as well as calculating the refractive
	/// index and Bragg angle.
	/// \author Dave Cowan
	/// \date 2019-04-02
	/// \since 1.7.0
	///
	class LIBSPEC Crystal
    {
	public:
		/// \enum Material 
		/// \brief All of the different types of material stored in the database
		enum class Material : std::uint16_t
		{
			/// \brief Lead Molybdate
			PbMoO4,
			/// \brief Tellurium Dioxide
			TeO2,
			/// \brief Tellurium Dioxide (Shear Mode)
			TeO2S,
			/// \brief Alpha-Quartz
			aQuartz,
			/// \brief Fused Silica
			fSilica,
			/// \brief Fused Silica (Shear Mode)
			fSilicaS,
			/// \brief Germanium
			Ge
		};

		///
		/// \name Constructors & Destructor
		//@{
		/// \brief Compensation Point Constructor
		/// \param[in] material The type of AO material to retrieve from the database
		Crystal(Crystal::Material material = Crystal::Material::TeO2);
		/// \brief Copy Constructor
		Crystal(const Crystal &);
		/// \brief Assignment Constructor
		const Crystal &operator =(const Crystal &);
		~Crystal();
		//@}

		///
		/// \name Material Type
		//@{
		/// \brief Non-Const accessor allows user code to change Crystal type to a different material
		Material& Type();
		/// \brief Returns the current material type in use by the Crystal object
		const Material& Type() const;
		/// \brief a Human-readable description of the AO Crystal Material
		const std::string& Description() const;
		//@}

		///
		/// \name Material Properties
		//@{
		/// \brief Acoustic Velocity is a constant physical property of the AO material and is retrieved here, in mm / usec (millimetres per microsecond)
		const double AcousticVelocity() const;
		/// \brief The Refractive Index of the AO Material is dependent on the wavelength of the optical beam passing through it.  This function calculates the Refractive Index.
		double RefractiveIndex(micrometre wavelength);
		/// \brief The AO Bragg Angle is dependent on both the optical wavelength and the applied acoustic frequency.  This function calculates the Bragg Angle.
		Degrees BraggAngle(micrometre wavelength, MHz frequency);
		//@}

	private:
		class Impl;
		Impl* p_Impl;
    };

	///
	/// \class AODevice
	/// \brief "Shopfront" class to the internal database storing the physical parameters of Isomet AO Devices and Operating Parameter Calculation
	///
	/// Access device properties for Isomet Acousto-Optic Devices, work out the required bragg angle for a laser
	/// at either the AO design wavelength or any other wavelength and retrieve a compensation function that can
	/// be used for Look-Up Table based Phase Compensation in Beam-Steered applications.
	/// \author Dave Cowan
	/// \date 2019-04-02
	/// \since 1.7.0
	///
	class LIBSPEC AODevice
	{
	public:
		///
		/// \name Constructors & Destructor
		//@{
		/// \brief Explicit Constructor
		/// Create a hypothetical AO Device based on supplying all of the required parameters fpr the device
		/// \param[in] xtal The type of AO Crystal Material the device is made from
		/// \param[in] GeomConstant A constant based on the device geometry (electrode dimensions).  This is specified in Isomet datasheets.
		/// \param[in] Centre the designed centre frequency.
		/// \param[in] Bandwidth The sweep range of the device
		AODevice(Crystal& xtal, double GeomConstant = 4.0, MHz Centre = 100.0, MHz Bandwidth = 60.0);
		/// \brief Model Constructor
		/// Creates a AODevice based on a given Isomet model number passed as a string. Either enter model number directly or select from AODeviceList
		AODevice(const std::string& Model);
		/// Copy Constructor
		AODevice(const AODevice &);
		/// Assignment Constructor
		const AODevice &operator =(const AODevice &);
		~AODevice();
		//@}

		///
		/// \name AO Device Physical Properties
		//@{
		/// \brief The Model Number of the AO Device
		const std::string& Model() const;
		/// \brief The Crystal Material Type from which the AO Device is made
		const Crystal& Material() const;
		/// \brief The Device's intended centre frequency
		///
		/// The centre frequency should be the geometric centre of the RF matching network's filter band.  It is
		/// the acoustic frequency at which a correctly aligned AO device will produce maximum efficiency if all
		/// its channels are applied an RF signal with zero phase offset.
		const MHz& CentreFrequency() const;
		/// \brief the Bandwidth of the AO device
		///
		/// The bandwidth of the RF filter.  This can be used together with the Centre Frequency above to calculate
		/// minimum and maximum deflection angles.
		const MHz& SweepBW() const;
		/// \brief The intended optical wavelength at which the AO device is designed to operate.
		///
		/// The AO Device may be used with a laser wavelength that is marginally different from the intended
		/// operating wavelength.
		const micrometre& OperatingWavelength() const;
		/// \brief The geometric constant that is a dimensional construct of the Isomet AO Design
		const double& GeomConstant() const;
		//@}

		///
		/// \name Bragg Angle & Compensation Function Calculations
		///
		/// Functions that calculate operating parameters for the AO Device.  The External Bragg Angle
		/// defines the incidence angle for a laser beam entering the device crystal and the compensation
		/// function is used to steer the acoustic wavefront such that device efficiency is maximised
		/// across the operating bandwidth of the device.
		///
		/// To accurately compensation a device, obtain the Compensation Function from this class, create a 
		/// Look-up Table from it using the CompensationTable class, and send it to the hardware using the
		/// CompensationTableDownload class.
		///
		/// Two sets of functions are provided, one that calculates Bragg Angle and Compensation Function
		/// for the device at its intended operating optical wavelength, and one for an optical wavelength of
		/// the user's choice.
		//@{
		/// \brief Returns the External Bragg Angle for the device at its intended operating wavelength
		Degrees ExternalBragg();
		/// \brief Returns a Compensation Function for the device at its intended operating wavelength
		CompensationFunction GetCompensationFunction();

		/// \brief Returns the External Bragg Angle for the device at any operating wavelength
		/// \param[in] wavelength The desired optical wavelength 
		Degrees ExternalBragg(micrometre wavelength);
		/// \brief Returns a Compensation Function for the device at any operating wavelength
		/// \param[in] wavelength The desired optical wavelength 
		CompensationFunction GetCompensationFunction(micrometre wavelength);
		//@}
	private:
		class Impl;
		Impl* p_Impl;
	};

	///
	/// \struct AODeviceList
	/// \brief Utility for browsing all of the available AO Device Models which the internal database contains
	///
	/// Implemented as a singleton, so simply call the getList method to return a list of all the available models.
	///
	/// For example
	/// \code	
	///		std::cout << "AO Devices in Library:" << std::endl;
	///		for (auto it = iMS::AODeviceList::getList().begin(); it != iMS::AODeviceList::getList().end(); ++it) {
	///			std::cout << "Model: " << *it << std::endl;
	///			iMS::AODevice aod(*it);
	///			std::cout << "  Crystal Material: " << aod.Material().Description() << std::endl;
	///			std::cout << "  Fc(MHz): " << aod.CentreFrequency() << std::endl;
	///			std::cout << "  BW(MHz): " << aod.SweepBW() << std::endl;
	///			std::cout << "  Operating Wavelength(um): " << aod.OperatingWavelength() << std::endl;
	///			std::cout << "  External Bragg Angle at op wavelength: " << aod.ExternalBragg() << std::endl;
	///			std::cout << "  Acoustic Velocity: " << aod.Material().AcousticVelocity() << std::endl;
	///			std::cout << std::endl;
	///		}
	/// \endcode
	///
	/// \author Dave Cowan
	/// \date 2019-04-02
	/// \since 1.7.0
	///
	struct LIBSPEC AODeviceList
	{
	private:
		AODeviceList();
		AODeviceList(const AODeviceList&) = delete;
		AODeviceList& operator=(const AODeviceList&) = delete;
		~AODeviceList();
	public:
		/// \brief Constant Method that creates and returns a list of all the available device models in the internal database
		static const ListBase<std::string>& getList();
	private:
		class Impl;
		Impl* p_Impl;
	};
}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
