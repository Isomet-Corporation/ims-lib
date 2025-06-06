/*-----------------------------------------------------------------------------
/ Title      : Acousto-Optics Parameters CPP
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/AcoustoOptics/src/AcoustoOptics.cpp $
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
/ Date        Version  Author  Description
/ 2015-04-09  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

#include "AcoustoOptics.h"
#include "PrivateUtil.h"
#include "spline.h"
#include <sstream>
#include <iostream>
#include <list>
#include <map>

// For PI
#define _USE_MATH_DEFINES
#include <math.h>

namespace iMS
{
	struct CrystalData {
		CrystalData() {}

		std::string Description;
		double Wavelength;
		double RefractiveIndex;
		double AcousticVelocity;
	};

	static int xtalparam_callback(void *data, int argc, char **argv, char **azColName) {
		std::list<CrystalData> *xtal_list = (std::list<CrystalData>*)data;
		CrystalData xtal_data;
		for (int i = 0; i < argc; i++)
		{
//			std::cout << azColName[i] << " = " << (argv[i] ? argv[i] : "NULL") << std::endl;
			if (argv[i] != nullptr)
			{
				// Get String arg
				std::string s(argv[i]);

				// and to double
				double dparam = 1;
				bool dparam_ok = true;
				try
				{
					dparam = std::stod(s);
				}
				catch (std::invalid_argument&)
				{
					dparam_ok = false;
				}
				catch (std::out_of_range&)
				{
					dparam_ok = false;
				}

				if (std::string(azColName[i]) == "Name") {
					xtal_data.Description = s;
				}
				else if ((std::string(azColName[i]) == "Wavelength") && dparam_ok) {
					xtal_data.Wavelength = dparam;
				}
				else if ((std::string(azColName[i]) == "RefractiveIndex") && dparam_ok) {
					xtal_data.RefractiveIndex = dparam;
				}
				else if ((std::string(azColName[i]) == "AcousticVelocity") && dparam_ok) {
					xtal_data.AcousticVelocity = dparam;
				}

			}
		}
		xtal_list->push_back(xtal_data);
		return 0;
	}

	std::ostream& operator<< (std::ostream& os, const Crystal::Material& material)
	{
		switch (material)
		{
		case Crystal::Material::PbMoO4:   return os << "PbMoO4";
		case Crystal::Material::TeO2:     return os << "TeO2";
		case Crystal::Material::TeO2S:    return os << "TeO2(S)";
		case Crystal::Material::aQuartz:  return os << "a-Quartz";
		case Crystal::Material::fSilica:  return os << "FusedSilica";
		case Crystal::Material::fSilicaS: return os << "FusedSilica(S)";
		case Crystal::Material::Ge:       return os << "Ge";
			// omit default case to trigger compiler warning for missing cases
		};
		return os << static_cast<std::uint16_t>(material);
	}

	Crystal::Material GetMaterialFromString(std::string s)
	{
		if (s == "PbMoO4") return Crystal::Material::PbMoO4;
		else if (s == "TeO2") return Crystal::Material::TeO2;
		else if (s == "TeO2(S)") return Crystal::Material::TeO2S;
		else if (s == "a-Quartz") return Crystal::Material::aQuartz;
		else if (s == "FusedSilica") return Crystal::Material::fSilica;
		else if (s == "FusedSilica(S)") return Crystal::Material::fSilicaS;
		else if (s == "Ge") return Crystal::Material::Ge;
		else return Crystal::Material::TeO2;
	}

	class Crystal::Impl
	{
	public:
		Impl(Crystal* parent, Crystal::Material material);
		~Impl() {}

		void updateFromDB();
		friend std::ostream& operator<< (std::ostream& stream, const Crystal::Material&);

		Crystal* m_parent;
		Material mat;
		bool isDirty;
		double acousticVelocity;
		std::string description;
		tk::spline refrIndexSpline;
	};

	Crystal::Impl::Impl(Crystal* parent, Crystal::Material material)
		: m_parent(parent), mat(material)
	{
		this->updateFromDB();
	}

	void Crystal::Impl::updateFromDB() {
		sqlite3 *xtalparam = nullptr;
		int rc;

		xtalparam = get_db();
		if (xtalparam != nullptr)
		{
			std::stringstream getMat;
			getMat << "SELECT * FROM xtalparam WHERE Type = \'";
			getMat << this->mat << "\'" << std::endl;
			std::list<CrystalData> data;
			rc = sqlite3_exec(xtalparam, getMat.str().c_str(), xtalparam_callback, (void *)&data, nullptr);
			if (rc != SQLITE_OK) {
				sqlite3_close(xtalparam);
			}
			else {
				std::map<double, double> wavelength_map;
				for (std::list<CrystalData>::const_iterator it = data.begin(); it != data.end(); ++it) {
					wavelength_map.emplace(it->Wavelength, it->RefractiveIndex);
					acousticVelocity = it->AcousticVelocity;
					description = it->Description;
				}

				// Values in map will be natively sorted into ascending wavelengths
				std::vector<double> wavelength(wavelength_map.size()), refrIndex(wavelength_map.size());

				if (wavelength_map.size() >= 1) {
					int i = 0;
					for (auto& f : wavelength_map) {
						wavelength[i] = f.first;
						refrIndex[i++] = f.second;
					}

					refrIndexSpline = tk::spline();
					//refrIndexSpline.set_boundary(tk::spline::first_deriv, 0.0, tk::spline::first_deriv, 0.0, false);
					refrIndexSpline.set_points(wavelength, refrIndex);
				}
			}

			sqlite3_close(xtalparam);
		}
		isDirty = false;
	}

	Crystal::Crystal(Crystal::Material material) : p_Impl(new Impl(this, material)) {
	}

	Crystal::Crystal(const Crystal &rhs) : p_Impl(new Impl(this, Crystal::Material::TeO2))
	{
		p_Impl->mat = rhs.p_Impl->mat;
		p_Impl->isDirty = true;
	}

	const Crystal &Crystal::operator =(const Crystal &rhs)
	{
		if (this == &rhs) return *this;
		p_Impl->mat = rhs.p_Impl->mat;
		p_Impl->isDirty = true;
		return *this;
	}

	Crystal::~Crystal() {
		delete p_Impl;
		p_Impl = nullptr;
	}

	Crystal::Material& Crystal::Type()
	{
		p_Impl->isDirty = true;
		return p_Impl->mat;
	}

	const Crystal::Material& Crystal::Type() const
	{
		return p_Impl->mat;
	}

	const std::string& Crystal::Description() const
	{
		if (p_Impl->isDirty) {
			p_Impl->updateFromDB();
		}
		return p_Impl->description;
	}

	const double Crystal::AcousticVelocity() const
	{
		if (p_Impl->isDirty) {
			p_Impl->updateFromDB();
		}
		return p_Impl->acousticVelocity;
	}

	double Crystal::RefractiveIndex(micrometre wavelength)
	{
		if (p_Impl->isDirty) {
			p_Impl->updateFromDB();
		}
		return p_Impl->refrIndexSpline(wavelength.operator double());
	}

	Degrees Crystal::BraggAngle(micrometre wavelength, MHz frequency)
	{
		if (p_Impl->isDirty) {
			p_Impl->updateFromDB();
		}

		// NOT 360 / 2 * PI !!!
		return Degrees(360.0 * (wavelength.operator double() * frequency.operator double()) /
			(2.0 * RefractiveIndex(wavelength) * AcousticVelocity() * 1000.0));
	}

	struct AODeviceData
	{
		AODeviceData() {}

		std::string model;
		Crystal xtal;
		MHz centre;
		MHz bandwidth;
		double geom;
		micrometre wavelength;
	};

	static int aodev_callback(void *data, int argc, char **argv, char **azColName) {
		std::list<AODeviceData>* aodevlist = (std::list<AODeviceData>*)data;
		AODeviceData aodev_data;
		for (int i = 0; i < argc; i++)
		{
			//			std::cout << azColName[i] << " = " << (argv[i] ? argv[i] : "NULL") << std::endl;
			if (argv[i] != nullptr)
			{
				// Get String arg
				std::string s(argv[i]);

				// and to double
				double dparam = 1;
				bool dparam_ok = true;
				try
				{
					dparam = std::stod(s);
				}
				catch (std::invalid_argument&)
				{
					dparam_ok = false;
				}
				catch (std::out_of_range&)
				{
					dparam_ok = false;
				}

				if (std::string(azColName[i]) == "Model") {
					aodev_data.model = s;
				}
				else if (std::string(azColName[i]) == "Crystal") {
					aodev_data.xtal = Crystal(GetMaterialFromString(s));
				}
				else if ((std::string(azColName[i]) == "CentreFrequency") && dparam_ok) {
					aodev_data.centre = dparam;
				}
				else if ((std::string(azColName[i]) == "GeomConstant") && dparam_ok) {
					aodev_data.geom = dparam;
				}
				else if ((std::string(azColName[i]) == "SweepBW") && dparam_ok) {
					aodev_data.bandwidth = dparam;
				}
				else if ((std::string(azColName[i]) == "Wavelength") && dparam_ok) {
					aodev_data.wavelength = dparam;
				}
			}
		}
		aodevlist->push_back(aodev_data);
		return 0;
	}

	class AODevice::Impl {
	public:
		Impl()
			: m_geomConstant(0.1),
			m_centre(100.0),
			m_bandwidth(60.0),
			m_wavelength(1.0),
			m_model("Custom") {}

		Impl(Crystal& xtal, double GeomConstant = 0.1, MHz Centre = 100.0, MHz Bandwidth = 60.0)
			: m_xtal(xtal),
			m_geomConstant(GeomConstant),
			m_centre(Centre),
			m_bandwidth(Bandwidth),
			m_wavelength(1.0),
			m_model("Custom") {}
		~Impl() {}

		Crystal m_xtal;
		double m_geomConstant;
		MHz m_centre;
		MHz m_bandwidth;
		micrometre m_wavelength;
		std::string m_model;

		static const int nPts;
		static const double margin;
	};

	const int AODevice::Impl::nPts = 21;
	const double AODevice::Impl::margin = 1.5;

	AODevice::AODevice(Crystal& xtal, double GeomConstant, MHz Centre, MHz Bandwidth)
		: p_Impl(new Impl(xtal, GeomConstant, Centre, Bandwidth))
	{}

	AODevice::AODevice(const std::string& Model)
		: p_Impl(new Impl())
	{
		sqlite3 *aodev = nullptr;
		int rc;

		aodev = get_db();
		if (aodev != nullptr)
		{
			std::stringstream getDev;
			getDev << "SELECT * FROM aodevice WHERE Model = \'";
			getDev << Model << "\'" << std::endl;
			std::list<AODeviceData> data;
			rc = sqlite3_exec(aodev, getDev.str().c_str(), aodev_callback, (void *)&data, nullptr);
			if (rc != SQLITE_OK) {
				sqlite3_close(aodev);
			}
			else {
				p_Impl->m_xtal = data.front().xtal;
				p_Impl->m_centre = data.front().centre;
				p_Impl->m_bandwidth = data.front().bandwidth;
				p_Impl->m_geomConstant = data.front().geom;
				p_Impl->m_wavelength = data.front().wavelength;
				p_Impl->m_model = data.front().model;
			}

			sqlite3_close(aodev);
		}
	}

	AODevice::AODevice(const AODevice &rhs) : p_Impl(new Impl())
	{
		p_Impl->m_bandwidth = rhs.p_Impl->m_bandwidth;
		p_Impl->m_centre = rhs.p_Impl->m_centre;
		p_Impl->m_geomConstant = rhs.p_Impl->m_geomConstant;
		p_Impl->m_xtal = rhs.p_Impl->m_xtal;
		p_Impl->m_wavelength = rhs.p_Impl->m_wavelength;
		p_Impl->m_model = rhs.p_Impl->m_model;
	}

	const AODevice &AODevice::operator =(const AODevice &rhs)
	{
		if (this == &rhs) return *this;
		p_Impl->m_bandwidth = rhs.p_Impl->m_bandwidth;
		p_Impl->m_centre = rhs.p_Impl->m_centre;
		p_Impl->m_geomConstant = rhs.p_Impl->m_geomConstant;
		p_Impl->m_xtal = rhs.p_Impl->m_xtal;
		p_Impl->m_wavelength = rhs.p_Impl->m_wavelength;
		p_Impl->m_model = rhs.p_Impl->m_model;
		return *this;
	}

	AODevice::~AODevice()
	{
		delete p_Impl;
		p_Impl = nullptr;
	}

	const std::string& AODevice::Model() const
	{
		return p_Impl->m_model;
	}

	const Crystal& AODevice::Material() const
	{
		return p_Impl->m_xtal;
	}

	const MHz& AODevice::CentreFrequency() const
	{
		return p_Impl->m_centre;
	}

	const MHz& AODevice::SweepBW() const
	{
		return p_Impl->m_bandwidth;
	}

	const micrometre& AODevice::OperatingWavelength() const
	{
		return p_Impl->m_wavelength;
	}

	const double& AODevice::GeomConstant() const
	{
		return p_Impl->m_geomConstant;
	}

	Degrees AODevice::ExternalBragg()
	{
		return this->ExternalBragg(this->OperatingWavelength());
	}

	CompensationFunction AODevice::GetCompensationFunction()
	{
		return this->GetCompensationFunction(this->OperatingWavelength());
	}

	Degrees AODevice::ExternalBragg(micrometre wavelength)
	{
		Degrees fc = ((360.0 / (2 * M_PI)) * (wavelength.operator double() * p_Impl->m_centre.operator double()) /
			(2.0 * 1.0 * p_Impl->m_xtal.AcousticVelocity() * 1000.0)); // External angle use 1.0 (refractive index of air)
		return fc;
	}

	CompensationFunction AODevice::GetCompensationFunction(micrometre wavelength)
	{
		CompensationFunction func;
		func.SetStyle(CompensationFeature::PHASE, CompensationFunction::InterpolationStyle::LINEXTEND);

		double sweep = p_Impl->m_bandwidth * AODevice::Impl::margin;

		MHz lower = p_Impl->m_centre - (sweep / 2.0);
		MHz upper = p_Impl->m_centre + (sweep / 2.0);

		MHz f = lower;
		double step_size = (upper - lower) / (AODevice::Impl::nPts - 1);

		Degrees fc = p_Impl->m_xtal.BraggAngle(wavelength, p_Impl->m_centre);

		for (int i=0; i< AODevice::Impl::nPts; i++) {
			Degrees ft = p_Impl->m_xtal.BraggAngle(wavelength, f);

			func.push_back(
				CompensationPointSpecification(
					CompensationPoint(
						Percent(50.0),
						Degrees((fc - ft) * p_Impl->m_geomConstant * f)
					),
					f
				)
			);
			f = f + step_size;
		}

		// Add lower bookend: half centre frequency with phase matched to lowest freq value
		Degrees fl = p_Impl->m_xtal.BraggAngle(wavelength, lower);
		func.push_front(
			CompensationPointSpecification(
				CompensationPoint(
					Percent(0.0),
					Degrees((fc - fl) * p_Impl->m_geomConstant * lower)
				),
				MHz(p_Impl->m_centre / 2.0)
			)
		);

		// Add upper bookend: twice centre frequency with phase matched to highest freq value
		Degrees fu = p_Impl->m_xtal.BraggAngle(wavelength, upper);
		func.push_back(
			CompensationPointSpecification(
				CompensationPoint(
					Percent(0.0),
					Degrees((fc - fu) * p_Impl->m_geomConstant * upper)
				),
				MHz(p_Impl->m_centre * 2.0)
			)
		);

		return func;
	}

	class AODeviceList::Impl
	{
	public:
		Impl() {}
		~Impl() {}

		ListBase<std::string> aodevlist;
	};

	const ListBase<std::string>& AODeviceList::getList()
	{
		static AODeviceList theList;
		return theList.p_Impl->aodevlist;
	}

	AODeviceList::AODeviceList()
		: p_Impl(new Impl())
	{
		sqlite3 *aodev = nullptr;
		int rc;

		aodev = get_db();
		if (aodev != nullptr)
		{
			std::stringstream getDev;
			getDev << "SELECT Model FROM aodevice";
			std::list<AODeviceData> data;
			rc = sqlite3_exec(aodev, getDev.str().c_str(), aodev_callback, (void *)&data, nullptr);
			if (rc != SQLITE_OK) {
				sqlite3_close(aodev);
			}
			else {
				for (std::list<AODeviceData>::iterator it = data.begin(); it != data.end(); ++it)
					p_Impl->aodevlist.push_back(it->model);
			}

			sqlite3_close(aodev);
		}
	}

	AODeviceList::~AODeviceList()
	{
		delete p_Impl;
		p_Impl = nullptr;
	}

}
