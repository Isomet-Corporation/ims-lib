/*-----------------------------------------------------------------------------
/ Title      : Compensation Functions Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Compensation/src/Compensation.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2023-05-30 10:50:42 +0100 (Tue, 30 May 2023) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 561 $
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

#include "Compensation.h"
#include "IConnectionManager.h"
#include "BulkVerifier.h"
#include "PrivateUtil.h"
#include "IEventTrigger.h"
#include "FileSystem.h"
#include "FileSystem_p.h"
#include "IMSTypeDefs_p.h"
#include "IMSConstants.h"
#include "spline.h"

#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <iostream>
//#include <iomanip>
#include <fstream>
#include "math.h"

namespace iMS
{
	static MHz CalculateFrequencyAtIndex(double Upper, double Lower, unsigned int index, unsigned int size)
	{
		MHz freq(0.0);
		if (index < size)
		{
			freq = Lower + ((double)index / ((double)size - 1.0)) * (Upper - Lower);
		}
		return freq;
	}

	static unsigned int CalculateIndexFromFrequency(double Upper, double Lower, MHz freq, unsigned int size)
	{
		if (freq < Lower) 
			return 0;
		else if (freq > Upper) 
			return (size - 1);
		else
		{
			return static_cast<unsigned int>((freq - Lower) / (Upper - Lower) * (size - 1));
		}
	}

	static double lin_interp(double x1, double y1, double x2, double y2, double x_new)
	{
		return (x2 == x1) ? 0.0 : (x_new - x1) * (y2 - y1) / (x2 - x1) + y1;
	}

	class CompensationEventTrigger :
		public IEventTrigger
	{
	public:
		CompensationEventTrigger() { updateCount(CompensationEvents::Count); }
		~CompensationEventTrigger() {};
	};

	class CompensationPoint::Impl
	{
	public:
		Impl(Percent ampl = 0.0, Degrees phase = 0.0, unsigned int sync_dig = 0, double sync_anlg = 0.0) :
			m_ampl(ampl), m_phase(phase), m_sync_D(sync_dig), m_sync_A(sync_anlg) {}
		~Impl() {};
		Percent m_ampl{ 0.0 };
		Degrees m_phase{ 0.0 };
		std::uint32_t m_sync_D{ 0 };
		double m_sync_A{ 0.0 };
	};

	CompensationPoint::CompensationPoint(Percent ampl, Degrees phase, unsigned int sync_dig, double sync_anlg) : 
		p_Impl (new Impl(ampl, phase, sync_dig, sync_anlg))
	{}

    CompensationPoint::CompensationPoint(Degrees phase, unsigned int sync_dig, double sync_anlg) : 
        CompensationPoint(0.0, phase, sync_dig, sync_anlg)
    {}

    CompensationPoint::CompensationPoint(unsigned int sync_dig, double sync_anlg) : 
        CompensationPoint(0.0, 0.0, sync_dig, sync_anlg)
    {}

    CompensationPoint::CompensationPoint(double sync_anlg) : 
        CompensationPoint(0.0, 0.0, 0, sync_anlg)
    {}

	CompensationPoint::~CompensationPoint()
	{
		delete p_Impl;
		p_Impl = nullptr;
	}

	// Copy Constructor
	CompensationPoint::CompensationPoint(const CompensationPoint &rhs) : p_Impl(new Impl())
	{
		this->p_Impl->m_ampl = rhs.p_Impl->m_ampl;
		this->p_Impl->m_phase = rhs.p_Impl->m_phase;
		this->p_Impl->m_sync_D = rhs.p_Impl->m_sync_D;
		this->p_Impl->m_sync_A = rhs.p_Impl->m_sync_A;
	}

	// Assignment Constructor
	CompensationPoint& CompensationPoint::operator =(const CompensationPoint &rhs)
	{
		if (this == &rhs) return *this;
		this->p_Impl->m_ampl = rhs.p_Impl->m_ampl;
		this->p_Impl->m_phase = rhs.p_Impl->m_phase;
		this->p_Impl->m_sync_D = rhs.p_Impl->m_sync_D;
		this->p_Impl->m_sync_A = rhs.p_Impl->m_sync_A;
		return *this;
	}

	bool CompensationPoint::operator==(CompensationPoint const& rhs) const
	{
		return ((p_Impl->m_ampl == rhs.p_Impl->m_ampl) &&
			(p_Impl->m_phase == rhs.p_Impl->m_phase) &&
			(p_Impl->m_sync_A == rhs.p_Impl->m_sync_A) &&
			(p_Impl->m_sync_D == rhs.p_Impl->m_sync_D));
	}

	void CompensationPoint::Amplitude(const Percent& ampl) { p_Impl->m_ampl = ampl; };
	const Percent& CompensationPoint::Amplitude() const { return p_Impl->m_ampl; };
	void CompensationPoint::Phase(const Degrees& phase) { p_Impl->m_phase = phase; };
	const Degrees& CompensationPoint::Phase() const { return p_Impl->m_phase; };
	void CompensationPoint::SyncDig(const unsigned int& sync) { p_Impl->m_sync_D = sync; };
	const std::uint32_t& CompensationPoint::SyncDig() const { return p_Impl->m_sync_D; };
	void CompensationPoint::SyncAnlg(const double& sync) { 
//		float f = fmaxf(fminf((float)sync, 1.0), 0.0);
		float f = ((float)sync > 1.0f) ? 1.0f : ((float)sync < 0.0f) ? 0.0f : (float)sync;
		p_Impl->m_sync_A = f;
	};
	const double& CompensationPoint::SyncAnlg() const { return p_Impl->m_sync_A; };

	class CompensationPointSpecification::Impl
	{
	public:
		Impl(CompensationPoint pt = CompensationPoint(), MHz f = 50.0);
		~Impl();
		std::pair<MHz, CompensationPoint> m_spec;
	};

	CompensationPointSpecification::Impl::Impl(CompensationPoint pt,  MHz f) : 
		m_spec(std::pair<MHz, CompensationPoint>(f, pt)) {};

	CompensationPointSpecification::Impl::~Impl() {};

	CompensationPointSpecification::CompensationPointSpecification(CompensationPoint pt, MHz f) : 
		p_Impl(new Impl(pt, f)) {}

	CompensationPointSpecification::~CompensationPointSpecification() { delete p_Impl; p_Impl = nullptr; }

	// Copy Constructor
	CompensationPointSpecification::CompensationPointSpecification(const CompensationPointSpecification &rhs) : p_Impl(new Impl())
	{
		this->p_Impl->m_spec = rhs.p_Impl->m_spec;
	}

	// Assignment Constructor
	CompensationPointSpecification& CompensationPointSpecification::operator =(const CompensationPointSpecification &rhs)
	{
		if (this == &rhs) return *this;
		this->p_Impl->m_spec = rhs.p_Impl->m_spec;
		return *this;
	}

	bool CompensationPointSpecification::operator==(CompensationPointSpecification const& rhs) const
	{
		return (this->p_Impl->m_spec == rhs.p_Impl->m_spec);
	}

	void CompensationPointSpecification::Freq(const MHz& f) { p_Impl->m_spec.first = f; }
	const MHz& CompensationPointSpecification::Freq() const { return p_Impl->m_spec.first; }

	void CompensationPointSpecification::Spec(const CompensationPoint& pt) { p_Impl->m_spec.second = pt; }
	const CompensationPoint& CompensationPointSpecification::Spec() const { return p_Impl->m_spec.second; }

	class CompensationFunction::Impl
	{
	public:
		Impl()
			{
			m_style[CompensationFeature::AMPLITUDE] = CompensationFunction::InterpolationStyle::LINEAR;
			m_style[CompensationFeature::PHASE]     = CompensationFunction::InterpolationStyle::LINEXTEND;
			m_style[CompensationFeature::SYNC_ANLG] = CompensationFunction::InterpolationStyle::LINEAR;
			m_style[CompensationFeature::SYNC_DIG]  = CompensationFunction::InterpolationStyle::SPOT;
		}
		std::map<CompensationFeature, CompensationFunction::InterpolationStyle> m_style;
	};

	CompensationFunction::CompensationFunction()
		: p_Impl(new Impl()) {}

	CompensationFunction::~CompensationFunction() { delete p_Impl; p_Impl = nullptr; }

	// Copy Constructor
	CompensationFunction::CompensationFunction(const CompensationFunction &rhs)
		: ListBase < CompensationPointSpecification >(rhs), p_Impl(new Impl())
	{
		p_Impl->m_style[CompensationFeature::AMPLITUDE] = rhs.p_Impl->m_style[CompensationFeature::AMPLITUDE];
		p_Impl->m_style[CompensationFeature::PHASE]     = rhs.p_Impl->m_style[CompensationFeature::PHASE];
		p_Impl->m_style[CompensationFeature::SYNC_ANLG] = rhs.p_Impl->m_style[CompensationFeature::SYNC_ANLG];
		p_Impl->m_style[CompensationFeature::SYNC_DIG]  = rhs.p_Impl->m_style[CompensationFeature::SYNC_DIG];
	}

	// Assignment Constructor
	CompensationFunction& CompensationFunction::operator =(const CompensationFunction &rhs)
	{
		if (this == &rhs) return *this;
		ListBase < CompensationPointSpecification >::operator =(rhs);
		p_Impl->m_style[CompensationFeature::AMPLITUDE] = rhs.p_Impl->m_style[CompensationFeature::AMPLITUDE];
		p_Impl->m_style[CompensationFeature::PHASE] = rhs.p_Impl->m_style[CompensationFeature::PHASE];
		p_Impl->m_style[CompensationFeature::SYNC_ANLG] = rhs.p_Impl->m_style[CompensationFeature::SYNC_ANLG];
		p_Impl->m_style[CompensationFeature::SYNC_DIG] = rhs.p_Impl->m_style[CompensationFeature::SYNC_DIG];
		return *this;
	}

	void CompensationFunction::SetStyle(const CompensationFeature feat, const CompensationFunction::InterpolationStyle style)
	{
		p_Impl->m_style[feat] = style;
	}

	CompensationFunction::InterpolationStyle CompensationFunction::GetStyle(const CompensationFeature feat) const
	{
		return p_Impl->m_style[feat];
	}

	class CompensationTable::Impl
	{
	public:
		Impl(CompensationTable* const parent, const IMSSystem&);
		Impl(CompensationTable* const parent, int LUTDepth, const MHz& lower_freq, const MHz& upper_freq);
		~Impl();

		bool Interpolate(const CompensationTable& other);
		void Load(const std::string& fileName, const RFChannel& chan = RFChannel::all);

		CompensationTable* const m_parent;
		int m_LUTDepth;
		double m_LowerFrequency; // in MHz
		double m_UpperFrequency;
	};

	CompensationTable::Impl::Impl(CompensationTable* const parent, const IMSSystem& iMS) : m_parent(parent)
	{
		m_LUTDepth = iMS.Synth().GetCap().LUTDepth;
		m_LowerFrequency = iMS.Synth().GetCap().lowerFrequency ;
		m_UpperFrequency = iMS.Synth().GetCap().upperFrequency ;
	};

	CompensationTable::Impl::Impl(CompensationTable* const parent, int LUTDepth, const MHz& lower_freq, const MHz& upper_freq)
		: m_parent(parent), /*m_LUTDepth(LUTDepth), */m_LowerFrequency((double)lower_freq), m_UpperFrequency((double)upper_freq) {
		// If user has given a value greater than 32, assume it is a cardinal value
		if (LUTDepth > 32) {
			m_LUTDepth = 0;
			while (LUTDepth >>= 1) {
				m_LUTDepth++;
			}
		}
		// otherwise a power of 2
		else {
			m_LUTDepth = LUTDepth;
		}
	}

	CompensationTable::Impl::~Impl() {	};

	// Default Constructor required for ImageProject's CompensationTableList
	CompensationTable::CompensationTable() : CompensationTable(IMSSystem(), CompensationPoint())
	{	}

	// Empty Constructor
	CompensationTable::CompensationTable(const IMSSystem& iMS) : CompensationTable(iMS, CompensationPoint())
	{	}

	CompensationTable::CompensationTable(int LUTDepth, const MHz& lower_freq, const MHz& upper_freq) 
		: CompensationTable(LUTDepth, lower_freq, upper_freq, CompensationPoint())
	{	}

	// Fill Constructor
	CompensationTable::CompensationTable(const IMSSystem& iMS, const CompensationPoint& pt) : DequeBase<CompensationPoint>(), p_Impl(new Impl(this, iMS))
	{
		int LUTSize = (1 << p_Impl->m_LUTDepth);
		DequeBase<CompensationPoint>::insert(DequeBase<CompensationPoint>::begin(), LUTSize, pt);
	}

	CompensationTable::CompensationTable(int LUTDepth, const MHz& lower_freq, const MHz& upper_freq, const CompensationPoint& pt) 
		: DequeBase<CompensationPoint>(), p_Impl(new Impl(this, LUTDepth, lower_freq, upper_freq))
	{
		int LUTSize = (1 << p_Impl->m_LUTDepth);
		DequeBase<CompensationPoint>::insert(DequeBase<CompensationPoint>::begin(), LUTSize, pt);
	}

	struct iMSLUTFileHeader2
	{
		std::string signature;
		std::uint16_t protver;
		std::uint16_t length;
		double start_freq;  // in MHz
		double end_freq;    // in MHz
		std::uint16_t pt_size;
		std::uint16_t chan_count;

		iMSLUTFileHeader2() {};
		iMSLUTFileHeader2(const std::string& s) : signature(s) {};
	};

	// File Read Constructor
	CompensationTable::CompensationTable(const IMSSystem& iMS, const std::string& fileName, const RFChannel& chan) : CompensationTable(iMS)
	{
		p_Impl->Load(fileName, chan);
	}

	CompensationTable::CompensationTable(int LUTDepth, const MHz& lower_freq, const MHz& upper_freq,
		const std::string& fileName, const RFChannel& chan) : CompensationTable(LUTDepth, lower_freq, upper_freq)
	{
		p_Impl->Load(fileName, chan);
	}

	// Create compensation table data from another input of different dimensions
	bool CompensationTable::Impl::Interpolate(const CompensationTable& other)
	{
		if (other.p_Impl->m_LowerFrequency > m_UpperFrequency)
		{
			return false; // Frequencies must overlap
		}
		if (other.p_Impl->m_UpperFrequency < m_LowerFrequency)
		{
			return false; // Frequencies must overlap
		}

		unsigned int index = 0, prev_index = 0;
		unsigned int LUTSize = (1 << m_LUTDepth);
		unsigned int OtherLUTSize = (1 << other.p_Impl->m_LUTDepth);

		while (index < LUTSize) {
			MHz lut_freq = CalculateFrequencyAtIndex(m_UpperFrequency, m_LowerFrequency, index, LUTSize);
			unsigned int other_index = CalculateIndexFromFrequency(other.p_Impl->m_UpperFrequency, other.p_Impl->m_LowerFrequency, lut_freq, OtherLUTSize);
			unsigned int syncd = 0;

			if (other_index < (OtherLUTSize - 1)) {
				MHz other_lfreq = CalculateFrequencyAtIndex(other.p_Impl->m_UpperFrequency, other.p_Impl->m_LowerFrequency, other_index, OtherLUTSize);
				MHz other_ufreq = CalculateFrequencyAtIndex(other.p_Impl->m_UpperFrequency, other.p_Impl->m_LowerFrequency, other_index + 1, OtherLUTSize);

				m_parent->DequeBase<CompensationPoint>::operator[](index).Amplitude(
					Percent(lin_interp(other_lfreq, other[other_index].Amplitude(), other_ufreq, other[other_index + 1].Amplitude(), lut_freq))
				);
				m_parent->DequeBase<CompensationPoint>::operator[](index).Phase(
					Degrees(lin_interp(other_lfreq, other[other_index].Phase(), other_ufreq, other[other_index + 1].Phase(), lut_freq))
				);
				m_parent->DequeBase<CompensationPoint>::operator[](index).SyncAnlg(
					Degrees(lin_interp(other_lfreq, other[other_index].SyncAnlg(), other_ufreq, other[other_index + 1].SyncAnlg(), lut_freq))
				);
			}
			else {
				m_parent->DequeBase<CompensationPoint>::operator[](index).Amplitude(other[other_index].Amplitude());
				m_parent->DequeBase<CompensationPoint>::operator[](index).Phase(other[other_index].Phase());
				m_parent->DequeBase<CompensationPoint>::operator[](index).SyncAnlg(other[other_index].SyncAnlg());
			}

			while (prev_index <= other_index) {
				syncd |= other[prev_index++].SyncDig();
			}
			m_parent->DequeBase<CompensationPoint>::operator[](index).SyncDig(syncd);

			index++;
		}
		return true;
	}

	bool ReadCompensationTableHeader(iMSLUTFileHeader2& hdr, const std::string& fileName) {
		const std::string FileHeader = "iMS_LUT";

		char * header;
		std::ifstream ifile(fileName, std::ios::binary | std::ios::in | std::ios::ate);

		if (ifile.is_open())
		{
			unsigned int size = static_cast<unsigned int>(ifile.tellg());
			header = new char[64]; // Header block always 64 bytes
			if (size < 64)
			{
				ifile.close();
				return false;
			}
			ifile.seekg(0, std::ios::beg);
			ifile.read(header, 64);
			hdr = iMSLUTFileHeader2(std::string(header, FileHeader.size()));
			hdr.protver = header[8] | (static_cast<std::uint16_t>(header[9]) << 8);
			hdr.length = static_cast<std::uint16_t>(header[10]) | (static_cast<std::uint16_t>(header[11]) << 8);
			std::uint64_t d = PCharToUInt<std::uint64_t>(&header[12]);
			hdr.start_freq = *reinterpret_cast<double*>(&d); // TODO: Fix this by providing a definition for PCharToDouble
			d = PCharToUInt<std::uint64_t>(&header[20]);
			hdr.end_freq = *reinterpret_cast<double*>(&d);
			hdr.pt_size = static_cast<std::uint16_t>(header[28]) | (static_cast<std::uint16_t>(header[29]) << 8);
			if (hdr.protver > 2) {
				hdr.chan_count = static_cast<std::uint16_t>(header[30]) | (static_cast<std::uint16_t>(header[31]) << 8);
			}
			else hdr.chan_count = 1;

			// Sanity checks
			if (hdr.signature != FileHeader)
			{
				delete[] header;
				ifile.close();
				return false; // not an iMS LUT file
			}
			if (hdr.protver < 2)
			{
				delete[] header;
				ifile.close();
				return false; // unsupported protocol version
			}
			if ((size - 64) != static_cast<unsigned int>(hdr.chan_count * hdr.length * hdr.pt_size))
			{
				delete[] header;
				ifile.close();
				return false; // unexpected file length
			}
			delete[] header;
			ifile.close();
		}
		else {
			return false;
		}

		return true;
	}

	void CompensationTable::Impl::Load(const std::string& fileName, const RFChannel& chan)
	{
		iMSLUTFileHeader2 hdr;
		if (!ReadCompensationTableHeader(hdr, fileName)) return;
		if ((hdr.start_freq > m_UpperFrequency) || (hdr.end_freq < m_LowerFrequency))
		{
			return; // Frequencies must overlap
		}

		std::ifstream ifile(fileName, std::ios::binary | std::ios::in);

		if (ifile.is_open())
		{
			// All set!
			int offset = 64;
			if (chan <= hdr.chan_count) offset += ((static_cast<unsigned int>(chan) - 1) * static_cast<unsigned int>(hdr.length * hdr.pt_size));
			ifile.seekg(offset, std::ios::beg);
			char * data;
			data = new char[hdr.pt_size];

			int i = 0, j = 0;

			Percent pct(0.0);
			Degrees deg(0.0);
			std::uint32_t syncd;
			double synca;

			std::uint64_t d;

			do
			{
				ifile.read(data, hdr.pt_size);

				// Amplitude
				d = PCharToUInt<std::uint64_t>(&data[0]);
				pct = *reinterpret_cast<double*>(&d);
				
				// Phase
				d = PCharToUInt<std::uint64_t>(&data[8]);
				deg = *reinterpret_cast<double*>(&d);
				
				// Sync Digital
				syncd = PCharToUInt<std::uint32_t>(&data[16]);
				
				// Sync Analog
				d = PCharToUInt<std::uint64_t>(&data[20]);
				synca = *reinterpret_cast<double*>(&d);
			} while (m_parent->FrequencyAt(i) > CalculateFrequencyAtIndex(hdr.end_freq, hdr.start_freq, j++, hdr.length));

			for (i = 0; i < (1 << m_LUTDepth); )
			{
				m_parent->DequeBase<CompensationPoint>::operator[](i).Amplitude(pct);
				m_parent->DequeBase<CompensationPoint>::operator[](i).Phase(deg);
				m_parent->DequeBase<CompensationPoint>::operator[](i).SyncDig(syncd);
				m_parent->DequeBase<CompensationPoint>::operator[](i).SyncAnlg(synca);

				i++;

				syncd = 0;
				while ((m_parent->FrequencyAt(i) >= CalculateFrequencyAtIndex(hdr.end_freq, hdr.start_freq, j, hdr.length)) && (j < hdr.length))
				{
					ifile.read(data, hdr.pt_size);

					// Amplitude
					d = PCharToUInt<std::uint64_t>(&data[0]);
					pct = *reinterpret_cast<double*>(&d);

					// Phase
					d = PCharToUInt<std::uint64_t>(&data[8]);
					deg = *reinterpret_cast<double*>(&d);

					// Sync Digital
					syncd |= PCharToUInt<std::uint32_t>(&data[16]);

					// Sync Analog
					d = PCharToUInt<std::uint64_t>(&data[20]);
					synca = *reinterpret_cast<double*>(&d);

					j++;
				}

			}
			delete[] data;
		}
		else throw std::exception();
		ifile.close();
	}

	// Non-Volatile Memory Recall Constructor
	CompensationTable::CompensationTable(const IMSSystem& iMS, const int entry) : CompensationTable(iMS)
	{	}

	CompensationTable::CompensationTable(const IMSSystem& iMS, const CompensationTable& tbl) : CompensationTable(iMS)
	{
		if ((p_Impl->m_LUTDepth != tbl.p_Impl->m_LUTDepth) ||
			(p_Impl->m_LowerFrequency != tbl.p_Impl->m_LowerFrequency) ||
			(p_Impl->m_UpperFrequency != tbl.p_Impl->m_UpperFrequency)) {
			p_Impl->Interpolate(tbl);
		}
		else {
			DequeBase<CompensationPoint>::operator =(tbl);
		}
	}

	CompensationTable::CompensationTable(int LUTDepth, const MHz& lower_freq, const MHz& upper_freq, const CompensationTable& tbl)
		: CompensationTable(LUTDepth, lower_freq, upper_freq)
	{
		if ((p_Impl->m_LUTDepth != tbl.p_Impl->m_LUTDepth) ||
			(p_Impl->m_LowerFrequency != tbl.p_Impl->m_LowerFrequency) ||
			(p_Impl->m_UpperFrequency != tbl.p_Impl->m_UpperFrequency)) {
			p_Impl->Interpolate(tbl);
		}
		else {
			DequeBase<CompensationPoint>::operator =(tbl);
		}
	}

	CompensationTable::~CompensationTable() { 
		delete p_Impl; p_Impl = nullptr;
	}

	// Copy Constructor
	CompensationTable::CompensationTable(const CompensationTable &rhs) : DequeBase<CompensationPoint>(rhs), 
		p_Impl(new Impl(this, rhs.p_Impl->m_LUTDepth, rhs.p_Impl->m_LowerFrequency, rhs.p_Impl->m_UpperFrequency))
	{	}

	// Assignment Constructor
	CompensationTable& CompensationTable::operator =(const CompensationTable &rhs)
	{
		if (this == &rhs) return *this;
		p_Impl->m_LUTDepth = rhs.p_Impl->m_LUTDepth;
		p_Impl->m_LowerFrequency = rhs.p_Impl->m_LowerFrequency;
		p_Impl->m_UpperFrequency = rhs.p_Impl->m_UpperFrequency;
		DequeBase<CompensationPoint>::operator =(rhs);
		return *this;
	}

	void FunctionInterpolate(const std::map<double, double>& func, 
		const MHz& lower, const MHz& upper,
		const CompensationFunction::InterpolationStyle style,
		std::vector<double>& result, int result_size)
	{
		std::vector<double> freq(func.size()), val(func.size());
		int i = 0;

		result = std::vector<double>(result_size, 0.0);

		if ((func.size() < 1) || (result_size < 1)) return;

		for (auto& f : func) {
			freq[i] = f.first;
			val[i++] = f.second;
		}

		unsigned int index = 0;

		switch (style) {
		case CompensationFunction::InterpolationStyle::BSPLINE: 
		{
			tk::spline spline;
			//spline.set_boundary(tk::spline::second_deriv, 0.0, tk::spline::first_deriv, -2.0, false);
			spline.set_points(freq, val);

			while (index < result_size) {
				MHz lut_freq = CalculateFrequencyAtIndex(upper, lower, index, result_size);
				result[index++] = spline(lut_freq);
			}

			break;
		}

		case CompensationFunction::InterpolationStyle::LINEAR: 
		{
			std::map<double, double>::const_iterator it = func.begin(), it2 = func.begin();
			double f1 = lower;
			double f2 = it2->first;
			double v1 = it->second;
			double v2 = it2->second;
			while (index < result_size) {
				MHz lut_freq = CalculateFrequencyAtIndex(upper, lower, index, result_size);
				while (lut_freq > f2) {
					it = it2; f1 = f2; v1 = v2;
					if (it2 != func.end()) {
						it2++;
					}
					if (it2 != func.end()) {
						f2 = it2->first;
						v2 = it2->second;
					}
					else {
						f2 = upper;
					}
				}
				result[index++] = lin_interp(f1, v1, f2, v2, lut_freq);
			}
			break;
		}

		case CompensationFunction::InterpolationStyle::LINEXTEND:
		{
			std::map<double, double>::const_iterator it = func.begin(), it2 = func.begin();
			if (it2 != func.end()) it2++;
			double f1 = it->first;
			double f2 = it2->first;
			double v1 = it->second;
			double v2 = it2->second;
			while (index < result_size) {
				MHz lut_freq = CalculateFrequencyAtIndex(upper, lower, index, result_size);
				while ((lut_freq > f2) && (it2 != func.end())) {
					it2++;
					if (it2 != func.end()) {
						it = it2; f1 = f2; v1 = v2;
						f2 = it2->first;
						v2 = it2->second;
					}
				}
				result[index++] = lin_interp(f1, v1, f2, v2, lut_freq);
			}
			break;
		}

		case CompensationFunction::InterpolationStyle::SPOT:
		{
			std::map<double, double>::const_iterator it = func.begin();
			while (index < result_size) {
				MHz lut_freq = CalculateFrequencyAtIndex(upper, lower, index, result_size);
				if (it != func.end()) {
					if (lut_freq > it->first) {
						result[index++] = it->second;
						it++;
					}
					else {
						result[index++] = 0.0;
					}
				}
				else {
					result[index++] = 0.0;
				}
			}
			break;
		}

		case CompensationFunction::InterpolationStyle::STEP: 
		{
			std::map<double, double>::const_iterator it = func.begin();
			double d = 0.0;
			while (index < result_size) {
				MHz lut_freq = CalculateFrequencyAtIndex(upper, lower, index, result_size);
				if (it != func.end()) {
					if (lut_freq > it->first) {
						d = it->second;
						it++;
					}
				}
				result[index++] = d;
			}
			break;
		}

		}

	}

	bool CompensationTable::ApplyFunction(const CompensationFunction& func, const CompensationFeature feat, CompensationModifier modifier)
	{
		std::map<double, double> func_map;
		std::vector<double> result;
		for (auto it = func.begin(); it != func.end(); ++it)
		{
			switch (feat) {
			case CompensationFeature::AMPLITUDE: func_map.emplace(it->Freq(), it->Spec().Amplitude()); break;
			case CompensationFeature::PHASE: func_map.emplace(it->Freq(), it->Spec().Phase()); break;
			case CompensationFeature::SYNC_DIG: func_map.emplace(it->Freq(), it->Spec().SyncDig()); break;
			case CompensationFeature::SYNC_ANLG: func_map.emplace(it->Freq(), it->Spec().SyncAnlg()); break;
			}
		}

		FunctionInterpolate(func_map, p_Impl->m_LowerFrequency, p_Impl->m_UpperFrequency,
			func.GetStyle(feat), result, this->Size());

		for (unsigned int index = 0; index < this->Size(); index++) {
			switch (feat) {
			case CompensationFeature::AMPLITUDE: {
				if (modifier == CompensationModifier::MULTIPLY) {
					double val = DequeBase<CompensationPoint>::operator[](index).Amplitude();
					result[index] *= (val / 100.0);
				}
				DequeBase<CompensationPoint>::operator[](index).Amplitude(Percent(result[index]));
				break;
			}
			case CompensationFeature::PHASE: {
				if (modifier == CompensationModifier::MULTIPLY) {
					double val = DequeBase<CompensationPoint>::operator[](index).Phase();
					result[index] *= (val / 360.0);
				}
				DequeBase<CompensationPoint>::operator[](index).Phase(Degrees(result[index]));
				break;
			}
			case CompensationFeature::SYNC_DIG: {
				if (modifier == CompensationModifier::MULTIPLY) {
					uint32_t val = DequeBase<CompensationPoint>::operator[](index).SyncDig();
					result[index] = val | (static_cast<uint32_t>(result[index]));
				}
				DequeBase<CompensationPoint>::operator[](index).SyncDig(static_cast<uint32_t>(result[index]));
				break;
			}
			case CompensationFeature::SYNC_ANLG: {
				if (modifier == CompensationModifier::MULTIPLY) {
					double val = DequeBase<CompensationPoint>::operator[](index).SyncAnlg();
					result[index] *= val;
				}
				DequeBase<CompensationPoint>::operator[](index).SyncAnlg(result[index]);
				break;
			}
			}
		}
		return true;
	}

	bool CompensationTable::ApplyFunction(const CompensationFunction& func, CompensationModifier modifier)
	{
		this->ApplyFunction(func, CompensationFeature::AMPLITUDE, modifier);
		this->ApplyFunction(func, CompensationFeature::PHASE, modifier);
		this->ApplyFunction(func, CompensationFeature::SYNC_DIG, modifier);
		this->ApplyFunction(func, CompensationFeature::SYNC_ANLG, modifier);
		return true;
	}

	const std::size_t CompensationTable::Size() const { return DequeBase<CompensationPoint>::size(); };

	const MHz CompensationTable::FrequencyAt(const unsigned int index) const
	{
		return CalculateFrequencyAtIndex(p_Impl->m_UpperFrequency, p_Impl->m_LowerFrequency, index, this->Size());
	}

	const MHz CompensationTable::LowerFrequency() const
	{
		return MHz(p_Impl->m_LowerFrequency);
	}

	const MHz CompensationTable::UpperFrequency() const
	{
		return MHz(p_Impl->m_UpperFrequency);
	}

	const bool CompensationTable::Save(const std::string& fileName) const
	{
		return false;
	}

	class CompensationTableExporter::Impl
	{
	public:
		Impl(const int chan_count) : m_chan_count(chan_count) {
			m_gtbl = nullptr;
			m_ctbl = std::vector<std::shared_ptr<CompensationTable>>(RFChannel::max, nullptr);
		}
		Impl(const IMSSystem& ims) : Impl(ims.Synth().GetCap().channels) {}
		~Impl() {}

		bool ExportLUTFile(bool global_lut = true);

		std::shared_ptr<CompensationTable> m_gtbl;
		std::vector<std::shared_ptr<CompensationTable>> m_ctbl;

		const int m_chan_count;
		std::string m_name;
	};

	bool CompensationTableExporter::Impl::ExportLUTFile(bool global_lut) {
		const std::string FileHeader = "iMS_LUT";
		const int CompensationPointSize = sizeof(std::uint64_t) * 3 + sizeof(std::uint32_t);
		char * header;
		std::ofstream ofile(m_name, std::ios::binary | std::ios::out);
		std::unique_ptr<CompensationTable> local_table;

		if (global_lut) {
			if (m_gtbl == nullptr) return false;
			local_table = std::make_unique<CompensationTable>(m_gtbl->Size(), m_gtbl->LowerFrequency(), m_gtbl->UpperFrequency(), *m_gtbl);
		} else {
			// Check each of the Comp Tables is valid
			for (int i = RFChannel::min; i <= m_chan_count; i++) {
				if (m_ctbl[i - RFChannel::min] == nullptr) return false;
			}
			local_table = std::make_unique<CompensationTable>(m_ctbl[0]->Size(), m_ctbl[0]->LowerFrequency(), m_ctbl[0]->UpperFrequency(), *m_ctbl[0]);
		}

		if (ofile.is_open())
		{
			header = new char[64]; // Header block always 64 bytes
			iMSLUTFileHeader2 hdr(FileHeader);
			hdr.protver = 3;
			hdr.length = static_cast<std::uint16_t>(local_table->Size());
			hdr.start_freq = local_table->LowerFrequency();
			hdr.end_freq = local_table->UpperFrequency();
			hdr.pt_size = CompensationPointSize;
			if (global_lut)
				hdr.chan_count = 1;
			else
				hdr.chan_count = m_chan_count;

#if defined(_WIN32)
			strncpy_s(header, 64, hdr.signature.c_str(), 8);
#else
			strncpy(header, hdr.signature.c_str(), 8);
#endif
			UIntToPChar<std::uint16_t>(&header[8], hdr.protver);
			UIntToPChar<std::uint16_t>(&header[10], hdr.length);

			std::uint64_t i = *reinterpret_cast<std::uint64_t*>(&hdr.start_freq);
			UIntToPChar<std::uint64_t>(&header[12], i);

			i = *reinterpret_cast<std::uint64_t*>(&hdr.end_freq);
			UIntToPChar<std::uint64_t>(&header[20], i);

			UIntToPChar<std::uint16_t>(&header[28], hdr.pt_size);
			UIntToPChar<std::uint16_t>(&header[30], hdr.chan_count);

			ofile.write(header, 64);
			delete[] header;

			// Done with Header, now let's move onto the table contents
			char * data;
			data = new char[CompensationPointSize];
			for (int j = 0; j < hdr.chan_count; ) {
				for (int i = 0; i < hdr.length; i++)
				{
					// Amplitude
					Percent pct = local_table->operator[](i).Amplitude();
					std::uint64_t d = *reinterpret_cast<std::uint64_t*>(&pct);
					UIntToPChar<std::uint64_t>(&data[0], d);

					// Phase
					Degrees deg = local_table->operator[](i).Phase();
					d = *reinterpret_cast<std::uint64_t*>(&deg);
					UIntToPChar<std::uint64_t>(&data[8], d);

					// Sync Digital
					std::uint32_t syncd = local_table->operator[](i).SyncDig();
					UIntToPChar<std::uint32_t>(&data[16], syncd);

					// Sync Analog
					double synca = local_table->operator[](i).SyncAnlg();
					d = *reinterpret_cast<std::uint64_t*>(&synca);
					UIntToPChar<std::uint64_t>(&data[20], d);

					ofile.write(data, CompensationPointSize);
				}
				if (++j != hdr.chan_count) {
					local_table.release();
					local_table = std::make_unique<CompensationTable>(m_ctbl[j]->Size(), m_ctbl[j]->LowerFrequency(), m_ctbl[j]->UpperFrequency(), *m_ctbl[j]);
				}
			}
			delete[] data;
		}
		else return false;
		ofile.close();
		return true;
	}

	CompensationTableExporter::CompensationTableExporter(const IMSSystem& ims) : p_Impl(new Impl(ims))
	{}

	CompensationTableExporter::CompensationTableExporter(const int channels) : p_Impl(new Impl(channels))
	{}

	CompensationTableExporter::CompensationTableExporter() : p_Impl(new Impl(1))
	{}

	CompensationTableExporter::CompensationTableExporter(const CompensationTable& tbl) : p_Impl(new Impl(1))
	{
		p_Impl->m_gtbl = std::make_shared<CompensationTable>(tbl);
	}

	CompensationTableExporter::~CompensationTableExporter() {
		delete p_Impl;
		p_Impl = nullptr;
	}

	void CompensationTableExporter::ProvideGlobalTable(const CompensationTable& tbl)
	{
		p_Impl->m_gtbl = std::make_shared<CompensationTable>(tbl);
	}

	void CompensationTableExporter::ProvideChannelTable(const RFChannel& chan, const CompensationTable& tbl)
	{
		if (chan == RFChannel::all) {
			p_Impl->m_gtbl = std::make_shared<CompensationTable>(tbl);
		}
		else {
			p_Impl->m_ctbl[chan - RFChannel::min] = std::make_shared<CompensationTable>(tbl);
		}
	}

	bool CompensationTableExporter::ExportGlobalLUT(const std::string& fileName)
	{
		p_Impl->m_name = fileName;
		return p_Impl->ExportLUTFile(true);
	}

	bool CompensationTableExporter::ExportChannelLUT(const std::string& fileName)
	{
		p_Impl->m_name = fileName;
		return p_Impl->ExportLUTFile(false);
	}

	class CompensationTableImporter::Impl
	{
	public:
		Impl(const std::string& fileName);
		~Impl() {}

		const std::string m_name;
		bool m_valid;
		iMSLUTFileHeader2 hdr;
	};
	
	CompensationTableImporter::Impl::Impl(const std::string& fileName) : m_name(fileName)
	{
		m_valid = ReadCompensationTableHeader(hdr, fileName);
	}

	CompensationTableImporter::CompensationTableImporter(const std::string& fileName) : p_Impl(new Impl(fileName))
	{}

	CompensationTableImporter::~CompensationTableImporter()
	{
		delete p_Impl;
		p_Impl = nullptr;
	}

	bool CompensationTableImporter::IsValid() const
	{
		return p_Impl->m_valid;
	}

	bool CompensationTableImporter::IsGlobal() const
	{
		return (p_Impl->hdr.chan_count == 1);
	}

	int CompensationTableImporter::Channels() const
	{
		return (p_Impl->hdr.chan_count);
	}

	int CompensationTableImporter::Size() const
	{
		return (p_Impl->hdr.length);
	}

	MHz CompensationTableImporter::LowerFrequency() const
	{
		return (p_Impl->hdr.start_freq);
	}

	MHz CompensationTableImporter::UpperFrequency() const
	{
		return (p_Impl->hdr.end_freq);
	}

	CompensationTable CompensationTableImporter::RetrieveGlobalLUT()
	{
		if (p_Impl->m_valid) {
			return CompensationTable(p_Impl->hdr.length,
				p_Impl->hdr.start_freq,
				p_Impl->hdr.end_freq,
				p_Impl->m_name,
				RFChannel::all);
		}
		else {
			return CompensationTable();
		}
	}

	CompensationTable CompensationTableImporter::RetrieveChannelLUT(RFChannel& chan)
	{
		if ((p_Impl->m_valid) && (chan <= p_Impl->hdr.chan_count))  {
			return CompensationTable(p_Impl->hdr.length,
				p_Impl->hdr.start_freq,
				p_Impl->hdr.end_freq,
				p_Impl->m_name,
				chan);
		}
		else {
			return CompensationTable();
		}
	}

	class CompensationTableDownload::Impl
	{
	public:
		Impl(IMSSystem&, const CompensationTable& table, const RFChannel& chan = RFChannel::all);
		~Impl();

		IMSSystem& myiMS;
		RFChannel m_channel;
		const CompensationTable& m_TableRef;
		std::shared_ptr<CompensationTable> m_Table;

		CompensationEventTrigger m_Event;

		void AddPointToVector(std::vector<std::uint8_t>&, const CompensationPoint&);

		class ResponseReceiver : public IEventHandler
		{
		public:
			ResponseReceiver(CompensationTableDownload::Impl* dl) : m_parent(dl) {};
			void EventAction(void* sender, const int message, const int param);
		private:
			CompensationTableDownload::Impl* m_parent;
		};
		ResponseReceiver* Receiver;

		class VerifyResult : public IEventHandler
		{
		public:
			VerifyResult(CompensationTableDownload::Impl* dl) : m_parent(dl) {};
			void EventAction(void* sender, const int message, const int param);
		private:
			CompensationTableDownload::Impl* m_parent;
		};
		VerifyResult* vfyResult;

		std::list <MessageHandle> dl_list;
		MessageHandle dl_final;
		mutable std::mutex dl_list_mutex;

		bool downloaderRunning{ false };
		std::thread downloadThread;
		mutable std::mutex m_dlmutex;
		std::condition_variable m_dlcond;
		void DownloadWorker();

		std::thread verifyThread;
		mutable std::mutex m_vfymutex;
		std::condition_variable m_vfycond;
		void VerifyWorker();

		std::thread RxThread;
		mutable std::mutex m_rxmutex;
		std::condition_variable m_rxcond;
		std::deque<int> rxok_list;
		std::deque<int> rxerr_list;
		void RxWorker();

		bool VerifyStarted{ false };

		BulkVerifier verifier;

		FileSystemWriter *fsw;
	};

	CompensationTableDownload::Impl::Impl(IMSSystem& iMS, const CompensationTable& table, const RFChannel& chan) :
		myiMS(iMS), m_TableRef(table), m_channel(chan), Receiver(new ResponseReceiver(this)), vfyResult(new VerifyResult(this)), verifier(iMS)
	{
		downloaderRunning = true;

		// Subscribe listeners
		IConnectionManager * const myiMSConn = myiMS.Connection();
		myiMSConn->MessageEventSubscribe(MessageEvents::SEND_ERROR, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::TIMED_OUT_ON_SEND, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_RECEIVED, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_VALID, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_TIMED_OUT, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_CRC, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_INVALID, Receiver);

		verifier.BulkVerifierEventSubscribe(BulkVerifierEvents::VERIFY_SUCCESS, vfyResult);
		verifier.BulkVerifierEventSubscribe(BulkVerifierEvents::VERIFY_FAIL, vfyResult);

		// Start two new threads to run the download / verify in the background
		downloadThread = std::thread(&CompensationTableDownload::Impl::DownloadWorker, this);
		verifyThread = std::thread(&CompensationTableDownload::Impl::VerifyWorker, this);

		// And a thread to receive the download responses
		RxThread = std::thread(&CompensationTableDownload::Impl::RxWorker, this);
	}

	CompensationTableDownload::Impl::~Impl() {
		// Unblock worker thread
		downloaderRunning = false;
		m_dlcond.notify_one();
		m_vfycond.notify_one();
		m_rxcond.notify_one();

		downloadThread.join();
		verifyThread.join();
		RxThread.join();

		// Unsubscribe listener
		IConnectionManager * const myiMSConn = myiMS.Connection();
		myiMSConn->MessageEventUnsubscribe(MessageEvents::SEND_ERROR, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::TIMED_OUT_ON_SEND, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_RECEIVED, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_ERROR_VALID, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_TIMED_OUT, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_ERROR_CRC, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_ERROR_INVALID, Receiver);

		verifier.BulkVerifierEventUnsubscribe(BulkVerifierEvents::VERIFY_SUCCESS, vfyResult);
		verifier.BulkVerifierEventUnsubscribe(BulkVerifierEvents::VERIFY_FAIL, vfyResult);

		delete Receiver;
		delete vfyResult;
	}

	void CompensationTableDownload::Impl::ResponseReceiver::EventAction(void* sender, const int message, const int param)
	{
		switch (message)
		{
		case (MessageEvents::RESPONSE_RECEIVED) :
		case (MessageEvents::RESPONSE_ERROR_VALID) : {

			// Add response to verify list for checking by rx processing thread
			{
				std::unique_lock<std::mutex> lck{ m_parent->m_rxmutex };
				m_parent->rxok_list.push_back(param);
				m_parent->m_rxcond.notify_one();
				lck.unlock();
			}
			break;
		}
		case (MessageEvents::TIMED_OUT_ON_SEND) :
		case (MessageEvents::SEND_ERROR) :
		case (MessageEvents::RESPONSE_TIMED_OUT) :
		case (MessageEvents::RESPONSE_ERROR_CRC) :
		case (MessageEvents::RESPONSE_ERROR_INVALID) : {

			// Add error to list and trigger processing thread if handle exists
			{
				std::unique_lock<std::mutex> lck{ m_parent->m_rxmutex };
				m_parent->rxerr_list.push_back(param);
				m_parent->m_rxcond.notify_one();
				lck.unlock();
			}
			break;
		}
		}
	}

	CompensationTableDownload::CompensationTableDownload(IMSSystem& iMS, const CompensationTable& tbl, const RFChannel& chan)
		: p_Impl(new Impl(iMS, tbl, chan))	
	{}

	CompensationTableDownload::~CompensationTableDownload() 
	{ delete p_Impl; p_Impl = nullptr; }

	// Pass the verify results back to the user
	void CompensationTableDownload::Impl::VerifyResult::EventAction(void* sender, const int message, const int param)
	{
		switch (message)
		{
		case (BulkVerifierEvents::VERIFY_SUCCESS) : m_parent->m_Event.Trigger<int>((void *)m_parent, CompensationEvents::VERIFY_SUCCESS, 0); break;
		case (BulkVerifierEvents::VERIFY_FAIL) : m_parent->m_Event.Trigger<int>((void *)m_parent, CompensationEvents::VERIFY_FAIL, m_parent->verifier.Errors()); break;
		}
	}

	bool CompensationTableDownload::StartDownload()
	{
		// Make sure Synthesiser is present
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();
		HostReport *iorpt;
		std::uint16_t data;

		if (!p_Impl->m_channel.IsAll()) {
			// Channel Scoped Compensation requested.  Confirm support in iMS firmware
			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, SYNTH_REG_Chan_Scope);
			DeviceReport Resp = myiMSConn->SendMsgBlocking(*iorpt);
			delete iorpt;
			if (Resp.Done()) {
				data = Resp.Payload<std::uint16_t>();
			}
			else return false;

			if (!(data & 0x100))
				// no support for channel scoped compensation
				return false;
		}

		if (p_Impl->m_Table == nullptr) {
			// Create Local copy of table that is reinterpreted to match connected device
			if (p_Impl->m_channel.IsAll())
				p_Impl->m_Table = std::make_shared<CompensationTable>(p_Impl->myiMS, p_Impl->m_TableRef);
			else
				p_Impl->m_Table = std::make_shared<CompensationTable>(p_Impl->myiMS.Synth().GetCap().LUTDepth - 2, 
					p_Impl->myiMS.Synth().GetCap().lowerFrequency, p_Impl->myiMS.Synth().GetCap().upperFrequency,
					p_Impl->m_TableRef);
		}

		std::unique_lock<std::mutex> lck{ p_Impl->m_dlmutex, std::try_to_lock };

		if (!lck.owns_lock()) {
			// Mutex lock failed, Downloader must be busy, try again later
			return false;
		}
		p_Impl->dl_list.clear();
		//VerifyReset();

		if (p_Impl->m_channel.IsAll()) {
			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Chan_Scope);
			iorpt->Payload<std::uint16_t>(0);
		}
		else {
			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_IO_Config_Mask);
			iorpt->Payload<std::uint16_t>(1 << (p_Impl->m_channel-1));
			if (NullMessage == myiMSConn->SendMsg(*iorpt))
			{
				delete iorpt;
				return false;
			}
			delete iorpt;

			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, SYNTH_REG_Chan_Scope);
			iorpt->Payload<std::uint16_t>(0xF);
		}

		if (NullMessage == myiMSConn->SendMsg(*iorpt))
		{
			delete iorpt;
			return false;
		}
		delete iorpt;

		p_Impl->m_dlcond.notify_one();
		return true;
	}

	//bool CompensationTableDownload::StartDownload(CompensationTableBank bank, int start_addr) : m_bank(bank), m_startaddr(start_addr)
	//{
	//}

	bool CompensationTableDownload::StartVerify()
	{
		// Make sure Synthesiser is present
		if (!p_Impl->myiMS.Synth().IsValid()) return false;

		if (!p_Impl->m_channel.IsAll()) {
			// Channel Scoped Compensation requested.  Confirm support in iMS firmware
			IConnectionManager * const myiMSConn = p_Impl->myiMS.Connection();

			HostReport *iorpt;

			iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, SYNTH_REG_Chan_Scope);
			DeviceReport Resp = myiMSConn->SendMsgBlocking(*iorpt);
			delete iorpt;
			std::uint16_t data;
			if (Resp.Done()) {
				data = Resp.Payload<std::uint16_t>();
			}
			else return false;

			if (!(data & 0x100))
				// no support for channel scoped compensation
				return false;
		}

		if (p_Impl->m_Table == nullptr) {
			// Create Local copy of table that is reinterpreted to match connected device
			if (p_Impl->m_channel.IsAll())
				p_Impl->m_Table = std::make_shared<CompensationTable>(p_Impl->myiMS, p_Impl->m_TableRef);
			else
				p_Impl->m_Table = std::make_shared<CompensationTable>(p_Impl->myiMS.Synth().GetCap().LUTDepth - 2,
					p_Impl->myiMS.Synth().GetCap().lowerFrequency, p_Impl->myiMS.Synth().GetCap().upperFrequency,
					p_Impl->m_TableRef);
		}

		std::unique_lock<std::mutex> lck{ p_Impl->m_vfymutex, std::try_to_lock };

		if (!lck.owns_lock()) {
			// Mutex lock failed, Verifier must be busy, try again later
			return false;
		}

		p_Impl->VerifyStarted = true;
		p_Impl->verifier.VerifyReset();
		p_Impl->m_vfycond.notify_one();
		return true;
	}

	int CompensationTableDownload::GetVerifyError()
	{
		return (p_Impl->verifier.GetVerifyError());
	}

/*	bool CompensationTableDownload::VerifyInProgress() const
	{
		std::unique_lock<std::mutex> vfylck{ p_Impl->m_vfymutex };
		bool verify = p_Impl->verifier.VerifyInProgress() || p_Impl->VerifyStarted;
		vfylck.unlock();
		return (verify);
	}*/

	void CompensationTableDownload::CompensationTableDownloadEventSubscribe(const int message, IEventHandler* handler)
	{
		p_Impl->m_Event.Subscribe(message, handler);
	}

	void CompensationTableDownload::CompensationTableDownloadEventUnsubscribe(const int message, const IEventHandler* handler)
	{
		p_Impl->m_Event.Unsubscribe(message, handler);
	}

	void CompensationTableDownload::Impl::AddPointToVector(std::vector<std::uint8_t>& lut_data, const CompensationPoint& pt)
	{
		std::uint16_t phase = PhaseRenderer::RenderAsCompensationPoint(myiMS, pt.Phase());
		lut_data.push_back(static_cast<std::uint8_t>(phase & 0xFF));
		lut_data.push_back(static_cast<std::uint8_t>((phase >> 8) & 0xFF));
		std::uint16_t ampl = AmplitudeRenderer::RenderAsCompensationPoint(myiMS, pt.Amplitude());
		lut_data.push_back(static_cast<std::uint8_t>(ampl & 0xFF));
		lut_data.push_back(static_cast<std::uint8_t>((ampl >> 8) & 0xFF));
		std::uint16_t sync_dgtl = pt.SyncDig();
		sync_dgtl &= ((1 << myiMS.Synth().GetCap().LUTSyncDBits) - 1);
		lut_data.push_back(static_cast<std::uint8_t>(sync_dgtl & 0xFF));
		lut_data.push_back(static_cast<std::uint8_t>((sync_dgtl >> 8) & 0xFF));
		double anlg = std::max<double>(0.0, std::min<double>(1.0, pt.SyncAnlg()));
		std::uint16_t sync_anlg = static_cast<std::uint16_t>(std::floor(anlg * (pow(2.0, (myiMS.Synth().GetCap().LUTSyncABits))-1.0) + 0.5));
		lut_data.push_back(static_cast<std::uint8_t>(sync_anlg & 0xFF));
		lut_data.push_back(static_cast<std::uint8_t>((sync_anlg >> 8) & 0xFF));
	}

	// CompensationTable Downloading Thread
	void CompensationTableDownload::Impl::DownloadWorker()
	{
		while (downloaderRunning) {
			std::unique_lock<std::mutex> lck{ m_dlmutex };
			m_dlcond.wait(lck);

			// Allow thread to terminate 
			if (!downloaderRunning) break;

			// Download loop
			IConnectionManager * const myiMSConn = myiMS.Connection();
			HostReport *iorpt;
			int lut_index = 0;
			int length = static_cast<int>(m_Table->Size());
			int buf_bytes = 512;

			dl_final = NullMessage;

			std::vector<std::uint8_t> lut_data;
			CompensationTable::const_iterator it = m_Table->cbegin();
			while ((lut_index < length) && (it < m_Table->cend()))
			{
				std::uint16_t lut_addr;

				if (buf_bytes <= 0)
				{
					// Clear Buffer every kB to prevent Hardware overrun
					do {
						{
							std::unique_lock<std::mutex> dllck{ dl_list_mutex };
							if (dl_list.empty()) break;
							dllck.unlock();
						}
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
					} while (1);
					buf_bytes = 512;
				}

				if (m_channel.IsAll()) {
					// Compensation Table applies to all channels
					buf_bytes -= 64;
					lut_addr = 8 * lut_index;
					// Add up to 64 bytes of data to vector
					for (int i = 0; i < 64; i += 8)
					{
						CompensationPoint pt = (*it);
						AddPointToVector(lut_data, pt);

						if ((++it == m_Table->cend()) || (++lut_index == length))
						{
							break;
						}
					}
				}
				else {
					// Compensation Table applies to one channel
					buf_bytes -= 8;
					lut_addr = 8 * ((lut_index << 2) + m_channel - 1);
					CompensationPoint pt = (*it);
					AddPointToVector(lut_data, pt);
					++it; ++lut_index;
				}

				iorpt = new HostReport(HostReport::Actions::LUT_ENTRY, HostReport::Dir::WRITE, lut_addr);
				iorpt->Payload<std::vector<std::uint8_t>>(lut_data);
				MessageHandle h = myiMSConn->SendMsg(*iorpt);
				delete iorpt;

				// Add message handle to download list so we can check the responses
				std::unique_lock<std::mutex> dllck{ dl_list_mutex };
				dl_list.push_back(h);
				dllck.unlock();

				lut_data.clear();
			}
			
			std::unique_lock<std::mutex> dllck{ dl_list_mutex };
			if (!dl_list.empty()) dl_final = dl_list.back();
			dllck.unlock();

			// Release lock, wait for next download trigger
			lck.unlock();
		}
	}

	// CompensationTable Verifying Thread
	void CompensationTableDownload::Impl::VerifyWorker()
	{
		std::unique_lock<std::mutex> lck{ m_vfymutex };
		while (downloaderRunning) {
			m_vfycond.wait(lck);

			// Allow thread to terminate 
			if (!downloaderRunning) break;

			IConnectionManager * const myiMSConn = myiMS.Connection();

			// Verify loop
			HostReport* iorpt;

			int lut_index = 0;
			int length = static_cast<int>(m_Table->Size());
			int buf_bytes = 1024;

			std::vector<std::uint8_t> lut_data;
			CompensationTable::const_iterator it = m_Table->cbegin();
			while ((lut_index < length) && (it < m_Table->cend()))
			{
				// Not a great hack: adding a gap ensures packets aren't coalesced (even with TCP_NODELAY enabled)
				std::this_thread::sleep_for(std::chrono::milliseconds(5));

				if (buf_bytes <= 0)
				{
					verifier.WaitUntilBufferClear();
					buf_bytes = 1024;
				}

				std::uint16_t lut_addr;
				if (m_channel.IsAll()) {
					// Compensation Table applies to all channels
					buf_bytes -= 64;
					lut_addr = 8 * lut_index;
					// Add up to 64 bytes of data to vector
					for (int i = 0; i < 64; i += 8)
					{
						CompensationPoint pt = (*it);
						AddPointToVector(lut_data, pt);

						if ((++it == m_Table->cend()) || (++lut_index == length))
						{
							break;
						}
					}
				}
				else {
					// Compensation Table applies to one channel
					buf_bytes -= 8;
					lut_addr = 8 * ((lut_index << 2) + m_channel - 1);
					CompensationPoint pt = (*it);
					AddPointToVector(lut_data, pt);
					++it; ++lut_index;
				}
				iorpt = new HostReport(HostReport::Actions::LUT_ENTRY, HostReport::Dir::READ, lut_addr);
				ReportFields f = iorpt->Fields();
				f.len = static_cast<std::uint16_t>(lut_data.size());
				iorpt->Fields(f);
				MessageHandle h = myiMSConn->SendMsg(*iorpt);
				delete iorpt;

				// Add CompensationTable data to verify memory
				std::shared_ptr<VerifyChunk> chunk(new VerifyChunk(h, lut_data, lut_addr));
				verifier.AddChunk(chunk);

				lut_data.clear();

			}

			verifier.Finalize();
			// Wait for next download trigger
			VerifyStarted = false;
		}
	}

	// CompensationTable Readback Verify Data Processing Thread
	void CompensationTableDownload::Impl::RxWorker()
	{
		std::unique_lock<std::mutex> lck{ m_rxmutex };
		while (downloaderRunning) {
			// Release lock implicitly, wait for next download trigger
			m_rxcond.wait(lck);

			// Allow thread to terminate 
			if (!downloaderRunning) break;

//			IConnectionManager * const myiMSConn = myiMS.Connection();

			while (!rxok_list.empty())
			{
				int param = rxok_list.front();
				rxok_list.pop_front();

				std::unique_lock<std::mutex> dllck{ dl_list_mutex };

				for (std::list<MessageHandle>::iterator iter = dl_list.begin();
					iter != dl_list.end();)
				{
					MessageHandle handle = static_cast<MessageHandle>(*iter);
					if (handle != (param))
					{
						++iter;
						continue;
					}

					// Remove from list
					iter = dl_list.erase(iter);

					// Download Finished?
					if (handle == dl_final)
					{
						m_Event.Trigger<int>((void *)this, CompensationEvents::DOWNLOAD_FINISHED, 0);
					}
				}
				dllck.unlock();

			}

			while (!rxerr_list.empty())
			{
				int param = rxerr_list.front();
				rxerr_list.pop_front();

				std::unique_lock<std::mutex> dllck{ dl_list_mutex };

				for (std::list<MessageHandle>::iterator iter = dl_list.begin();
					iter != dl_list.end();)
				{
					MessageHandle handle = static_cast<MessageHandle>(*iter);
					if (handle != (param))
					{
						++iter;
						continue;
					}

					// Remove from list
					iter = dl_list.erase(iter);
					m_Event.Trigger<int>((void *)this, CompensationEvents::DOWNLOAD_ERROR, handle);
				}
				dllck.unlock();

			}

		}
	}

	const FileSystemIndex CompensationTableDownload::Store(FileDefault def, const std::string& FileName) const
	{
		FileSystemManager fsm(p_Impl->myiMS);
		std::uint32_t addr;

		std::vector<std::uint8_t> data;
		for (CompensationTable::const_iterator it = p_Impl->m_Table->cbegin(); it != p_Impl->m_Table->cend(); ++it)
		{
			p_Impl->AddPointToVector(data, (*it));
		}

		if (!fsm.FindSpace(addr, data)) return -1;
		FileSystemTableEntry fste(FileSystemTypes::COMPENSATION_TABLE, addr, data.size(), def, FileName);
		p_Impl->fsw = new FileSystemWriter(p_Impl->myiMS, fste, data);

		FileSystemIndex result = p_Impl->fsw->Program();
		delete p_Impl->fsw;
		return result;
	}

}
