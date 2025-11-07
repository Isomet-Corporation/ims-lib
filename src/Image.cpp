/*-----------------------------------------------------------------------------
/ Title      : Isomet Image Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ImageOps/src/Image.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2025-01-08 21:34:12 +0000 (Wed, 08 Jan 2025) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 655 $
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

#include "Image.h"
#include "ToneBuffer.h"
#include "Image_p.h"
#include "PrivateUtil.h"

#include <sstream>

// Required for UUID
#if defined(__QNXNTO__)
#include <process.h>
#include <time.h>
#endif
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

namespace iMS {

	class ImageFormat::Impl {
	public:
		Impl();
		Impl(std::shared_ptr<IMSSystem>);

		int num_chan{ 4 };
		unsigned int n_ampl_bytes{ 1 };
		bool ampl_en{ true };
		unsigned int n_phs_bytes{ 2 };
		bool phase_en{ true };
		unsigned int n_freq_bytes{ 2 };
		unsigned int n_sync_bytes{ 2 };
		unsigned int n_synca{ 2 };
		bool n_syncd{ 1 };
		bool pairs_combine{ false };
		bool all_combine{ false };
	};

	ImageFormat::Impl::Impl() : num_chan(4) {}

	ImageFormat::Impl::Impl(std::shared_ptr<IMSSystem> ims) {
		/* In earlier iMSP Controller firmwares, only 4 channel images are supported */
		if (ims->Ctlr().Model() == "iMSP" && ims->Ctlr().GetVersion().revision <= 63) {
			num_chan = 4;
		}
		else
		{
			num_chan = ims->Synth().GetCap().channels;
			if (num_chan < RFChannel::min) num_chan = RFChannel::min;
			if (num_chan > RFChannel::max) num_chan = RFChannel::max;

			n_ampl_bytes = 1 + (ims->Synth().GetCap().amplBits-1) / 8;
			ampl_en = true;
			n_phs_bytes = 1 + (ims->Synth().GetCap().phaseBits-1) / 8;
			phase_en = true;
			n_freq_bytes = 2; // For backwards compatibility.  Must be manually set for higher resolution  // ims.Synth().GetCap().freqBits / 8;
			n_synca = 2;
			n_syncd = true;
			n_sync_bytes = 2;
			pairs_combine = false;
			all_combine = false;
		}
	}

	ImageFormat::ImageFormat() : p_Impl(new Impl()) {}

	ImageFormat::ImageFormat(std::shared_ptr<IMSSystem> ims) : p_Impl(new Impl(ims)) {}

	/// \brief Copy Constructor
	ImageFormat::ImageFormat(const ImageFormat& rhs) : p_Impl(new Impl())
	{
		p_Impl->num_chan = rhs.p_Impl->num_chan;
		p_Impl->n_ampl_bytes = rhs.p_Impl->n_ampl_bytes;
		p_Impl->ampl_en = rhs.p_Impl->ampl_en;
		p_Impl->n_phs_bytes = rhs.p_Impl->n_phs_bytes;
		p_Impl->phase_en = rhs.p_Impl->phase_en;
		p_Impl->n_freq_bytes = rhs.p_Impl->n_freq_bytes;
		p_Impl->n_synca = rhs.p_Impl->n_synca;
		p_Impl->n_syncd = rhs.p_Impl->n_syncd;
		p_Impl->n_sync_bytes = rhs.p_Impl->n_sync_bytes;
		p_Impl->pairs_combine = rhs.p_Impl->pairs_combine;
		p_Impl->all_combine = rhs.p_Impl->all_combine;
	}

	/// \brief Assignment Constructor
	ImageFormat& ImageFormat::operator =(const ImageFormat& rhs)
	{
		if (this == &rhs) return *this;

		p_Impl->num_chan = rhs.p_Impl->num_chan;
		p_Impl->n_ampl_bytes = rhs.p_Impl->n_ampl_bytes;
		p_Impl->ampl_en = rhs.p_Impl->ampl_en;
		p_Impl->n_phs_bytes = rhs.p_Impl->n_phs_bytes;
		p_Impl->phase_en = rhs.p_Impl->phase_en;
		p_Impl->n_freq_bytes = rhs.p_Impl->n_freq_bytes;
		p_Impl->n_synca = rhs.p_Impl->n_synca;
		p_Impl->n_syncd = rhs.p_Impl->n_syncd;
		p_Impl->n_sync_bytes = rhs.p_Impl->n_sync_bytes;
		p_Impl->pairs_combine = rhs.p_Impl->pairs_combine;
		p_Impl->all_combine = rhs.p_Impl->all_combine;

		return *this;
	}

	int ImageFormat::Channels() const
	{
		return p_Impl->num_chan;
	}

	void ImageFormat::Channels(int value)
	{
		if (value > 0)
		{
			p_Impl->num_chan = value;
		}
	}

	int ImageFormat::FreqBytes() const
	{
		return p_Impl->n_freq_bytes;
	}

	void ImageFormat::FreqBytes(int value)
	{
		if (value > 0)
		{
			p_Impl->n_freq_bytes = value;
		}
	}

	int ImageFormat::AmplBytes() const
	{
		return p_Impl->n_ampl_bytes;
	}

	void ImageFormat::AmplBytes(int value)
	{
		if (value > 0) {
			p_Impl->n_ampl_bytes = value;
		}
	}

	int ImageFormat::PhaseBytes() const
	{
		return p_Impl->n_phs_bytes;
	}

	void ImageFormat::PhaseBytes(int value)
	{
		if (value > 0) {
			p_Impl->n_phs_bytes = value;
		}
	}

	int ImageFormat::SyncBytes() const
	{
		return p_Impl->n_sync_bytes;
	}

	void ImageFormat::SyncBytes(int value)
	{
		if (value > 0) {
			p_Impl->n_sync_bytes = value;
		}
	}

	bool ImageFormat::EnableAmpl() const { return p_Impl->ampl_en; }
	void ImageFormat::EnableAmpl(bool value) { p_Impl->ampl_en = value; }

	bool ImageFormat::EnablePhase() const { return p_Impl->phase_en; }
	void ImageFormat::EnablePhase(bool value) { p_Impl->phase_en = value; }

	bool ImageFormat::CombineChannelPairs() const { return p_Impl->pairs_combine; }
	void ImageFormat::CombineChannelPairs(bool value) { p_Impl->pairs_combine = value; }

	bool ImageFormat::CombineAllChannels() const { return p_Impl->all_combine; }
	void ImageFormat::CombineAllChannels(bool value) { p_Impl->all_combine = value; }

	int ImageFormat::SyncAnlgChannels() const
	{
		return p_Impl->n_synca;
	}

	void ImageFormat::SyncAnlgChannels(int value)
	{
		if (value >= 0) {
			p_Impl->n_synca = value;
		}
	}

	bool ImageFormat::EnableSyncDig() const
	{
		return p_Impl->n_syncd;
	}

	void ImageFormat::EnableSyncDig(bool value)
	{
		p_Impl->n_syncd = value;
	}

	unsigned int ImageFormat::GetFormatSpec() const 
	{
		unsigned int fmt = 0x8000;  // bit 15 enables format spec
		fmt |= (p_Impl->num_chan - RFChannel::min) & 0x3;
		if (p_Impl->ampl_en) {
			fmt |= (((p_Impl->n_ampl_bytes - 1) % 3) + 1) << 2;
		}
		if (p_Impl->phase_en) {
			fmt |= (((p_Impl->n_phs_bytes - 1) % 3) + 1) << 4;
		}
		fmt |= ((p_Impl->n_freq_bytes - 1) % 4) << 6;
		fmt |= (p_Impl->n_synca % 3) << 8;
		fmt |= (p_Impl->n_syncd ? 1 : 0) << 10;
		fmt |= ((p_Impl->n_sync_bytes - 1) % 4) << 11;
		fmt |= (p_Impl->all_combine) ? (1 << 13) : 0;
		fmt |= (p_Impl->pairs_combine) ? (1 << 14) : 0;
		return fmt;
	}

	ImagePoint::ImagePoint(FAP fap) {
		m_fap[0] = fap;
		m_fap[1] = fap;
		m_fap[2] = fap;
		m_fap[3] = fap;
		m_synca[0] = 0.0;
		m_synca[1] = 0.0;
		m_syncd = 0;
	};

	ImagePoint::ImagePoint(FAP ch1, FAP ch2, FAP ch3, FAP ch4) {
		m_fap[0] = ch1;
		m_fap[1] = ch2;
		m_fap[2] = ch3;
		m_fap[3] = ch4;
		m_synca[0] = 0.0;
		m_synca[1] = 0.0;
		m_syncd = 0;
	}

	ImagePoint::ImagePoint(FAP fap, float synca, unsigned int syncd) {
		m_fap[0] = fap;
		m_fap[1] = fap;
		m_fap[2] = fap;
		m_fap[3] = fap;
		m_synca[0] = synca;
		m_synca[1] = synca;
		m_syncd = syncd;
	};

	ImagePoint::ImagePoint(FAP ch1, FAP ch2, FAP ch3, FAP ch4, float synca_1, float synca_2, unsigned int syncd) {
		m_fap[0] = ch1;
		m_fap[1] = ch2;
		m_fap[2] = ch3;
		m_fap[3] = ch4;
		m_synca[0] = synca_1;
		m_synca[1] = synca_2;
		m_syncd = syncd;
	};

	bool ImagePoint::operator==(ImagePoint const& rhs) const
	{
		bool eq = (m_fap[0] == rhs.m_fap[0]) &&
                    (m_fap[1] == rhs.m_fap[1]) && 
                    (m_fap[2] == rhs.m_fap[2]) && 
                    (m_fap[3] == rhs.m_fap[3]) &&
                    float_compare(m_synca[0], rhs.m_synca[0]) &&
                    float_compare(m_synca[1], rhs.m_synca[1]) &&
                    m_syncd == rhs.m_syncd;
        return eq;
	}

	const FAP& ImagePoint::GetFAP(const RFChannel chan) const
	{
		switch (chan)
		{
		case 1: return m_fap[0]; break;
		case 2: return m_fap[1]; break;
		case 3: return m_fap[2]; break;
		case 4: return m_fap[3]; break;
		default: return m_fap[0]; break;
		}
		return m_fap[0];
	}

	void ImagePoint::SetFAP(const RFChannel chan, const FAP& fap)
	{
		if (chan.IsAll()) {
			m_fap[0] = m_fap[1] = m_fap[2] = m_fap[3] = fap;
		}
		else {
			switch (chan)
			{
			case 1: m_fap[0] = fap; break;
			case 2: m_fap[1] = fap; break;
			case 3: m_fap[2] = fap; break;
			case 4: m_fap[3] = fap; break;
			default: break;
			}
		}
	}

	FAP& ImagePoint::SetFAP(const RFChannel chan) {
		switch (chan)
		{
		case 1: return(m_fap[0]); break;
		case 2: return(m_fap[1]); break;
		case 3: return(m_fap[2]); break;
		case 4: return(m_fap[3]); break;
		default:  break;
		}
		return(m_fap[0]);
	}

	void ImagePoint::SetAll(const FAP& fap)
	{
		m_fap[0] = m_fap[1] = m_fap[2] = m_fap[3] = fap;
	}

	const float& ImagePoint::GetSyncA(int index) const
	{
		return m_synca[index & 1];
	}

	void ImagePoint::SetSyncA(int index, const float& value)
	{
//		float f = fmaxf(fminf(value, 1.0), 0.0);
		float f = ((float)value > 1.0f) ? 1.0f : ((float)value < 0.0f) ? 0.0f : (float)value;

		m_synca[index & 1] = f;
	}

	const unsigned int& ImagePoint::GetSyncD() const 
	{
		return m_syncd;
	}

	void ImagePoint::SetSyncD(const unsigned int& value)
	{
		m_syncd = value;
	}

	class Image::Impl
	{
	public:
		Impl() : clockRate(kHz(100.0)), clockDivide(1), m_desc("image") {};
		Impl(const Frequency& f) : clockRate(f), clockDivide(1), m_desc("image") {};
		Impl(const int div) : clockRate(kHz(100.0)), clockDivide(div), m_desc("image") {};
		~Impl() {}

		Frequency clockRate;
		int clockDivide;
		std::string m_desc;
	};


	Image::Image(const std::string& name) : DequeBase<ImagePoint>(name), p_Impl(new Impl()) {};
	// Fill Constructor
	Image::Image(size_t n, const ImagePoint& pt, const std::string& name) : DequeBase<ImagePoint>(n, pt, name), p_Impl(new Impl()) {};
	Image::Image(size_t n, const ImagePoint& pt, const Frequency& f, const std::string& name) : DequeBase<ImagePoint>(n, pt, name), p_Impl(new Impl(f)) {};
	Image::Image(size_t n, const ImagePoint& pt, const int div, const std::string& name) : DequeBase<ImagePoint>(n, pt, name), p_Impl(new Impl(div)) {};
	// Range Constructor
	Image::Image(Image::const_iterator first, Image::const_iterator last, const std::string& name) : DequeBase<ImagePoint>(first, last, name), p_Impl(new Impl()) {};
	Image::Image(Image::const_iterator first, Image::const_iterator last, const Frequency& f, const std::string& name) : DequeBase<ImagePoint>(first, last, name), p_Impl(new Impl(f)) {};
	Image::Image(Image::const_iterator first, Image::const_iterator last, const int div, const std::string& name) : DequeBase<ImagePoint>(first, last, name), p_Impl(new Impl(div)) {};
	Image::~Image() { delete p_Impl; p_Impl = nullptr; };

	// Copy & Assignment Constructors
	Image::Image(const Image &rhs) : DequeBase<ImagePoint>(rhs), p_Impl(new Impl())
	{
		p_Impl->clockRate = rhs.p_Impl->clockRate;
		p_Impl->clockDivide = rhs.p_Impl->clockDivide;
		p_Impl->m_desc = rhs.p_Impl->m_desc;
	};

	Image& Image::operator =(const Image &rhs)
	{
		if (this == &rhs) return *this;
		DequeBase<ImagePoint>::operator =(rhs);
		p_Impl->clockRate = rhs.p_Impl->clockRate;
		p_Impl->clockDivide = rhs.p_Impl->clockDivide;
		p_Impl->m_desc = rhs.p_Impl->m_desc;
		return *this;
	};

	// Use these to add/insert points to the image
	void Image::AddPoint(const ImagePoint& pt)
	{
		this->push_back(pt);
	}

	Image::iterator Image::InsertPoint(Image::iterator it, const ImagePoint& pt) { return DequeBase<ImagePoint>::insert(it, pt); }
	
	void Image::InsertPoint(iterator it, size_t n, const ImagePoint& pt)
	{
		DequeBase<ImagePoint>::insert(it, n, pt);
	}

	void Image::InsertPoint(iterator it, const_iterator first, const_iterator last)
	{
		DequeBase<ImagePoint>::insert(it, first, last);
	}

	// Remove Elements and Clear Whole Image
	Image::iterator Image::RemovePoint(iterator it)
	{
		return DequeBase<ImagePoint>::erase(it);
	}

	Image::iterator Image::RemovePoint(iterator first, iterator last)
	{
		return DequeBase<ImagePoint>::erase(first, last);
	}

	void Image::Clear()
	{
		DequeBase<ImagePoint>::clear();
	}

	int Image::Size() const
	{
		return static_cast<int>(DequeBase<ImagePoint>::size());
	}

	const Frequency& Image::ClockRate() const
	{ 
		return p_Impl->clockRate;
	}

	void Image::ClockRate(const Frequency& f)
	{ 
		p_Impl->clockRate = f;
	}

	const int Image::ExtClockDivide() const
	{
		return p_Impl->clockDivide;
	}

	void Image::ExtClockDivide(const int div)
	{
		p_Impl->clockDivide = div;
	}

	std::string& Image::Description()
	{
		return p_Impl->m_desc;
	}

	const std::string& Image::Description() const
	{
		return p_Impl->m_desc;
	}

	class ImageTableEntry::Impl
	{
	public:
		Impl() : m_handle(-1), m_address(0), m_format(0), m_npts(0), m_size(0), m_uuid({ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }), m_status(0) {};
		Impl(ImageIndex handle, std::uint32_t address, int n_pts, int size, std::uint32_t fmt, std::array<std::uint8_t, 16> uuid, std::string name) :
			m_handle(handle), m_address(address), m_format(fmt), m_npts(n_pts), m_size(size), m_uuid(uuid), m_name(name), m_status(0){};
		Impl(ImageIndex handle, const std::vector<std::uint8_t>& bytes);

		ImageIndex m_handle;
		std::uint32_t m_address;
		std::uint32_t m_format;
		int m_npts;
		int m_size;
		std::array<std::uint8_t, 16> m_uuid;
		std::string m_name;
		std::uint8_t m_status;
	};

	ImageTableEntry::Impl::Impl(ImageIndex handle, const std::vector<std::uint8_t>& bytes) {
		for (int i = 0; i < 16; i++) m_uuid[i] = bytes[i];
		m_size = bytes[16] | (bytes[17] << 8) | (bytes[18] << 16) | (bytes[19] << 24);
		m_npts = bytes[20] | (bytes[21] << 8) | (bytes[22] << 16) | (bytes[23] << 24);
		m_format = bytes[24] | (bytes[25] << 8) | (bytes[26] << 16) | (bytes[27] << 24);
		m_address = bytes[28] | (bytes[29] << 8) | (bytes[30] << 16) | (bytes[31] << 24);
		m_status = *(bytes.begin() + 32);
		m_name = std::string(bytes.begin() + 33, bytes.end());
		m_handle = handle;
	}

	ImageTableEntry::ImageTableEntry() : p_Impl(new Impl()) {}

	ImageTableEntry::ImageTableEntry(ImageIndex handle, std::uint32_t address, int n_pts, int size, std::uint32_t fmt, std::array<std::uint8_t, 16> uuid, std::string name) :
		p_Impl(new Impl(handle, address, n_pts, size, fmt, uuid, name)) {}

	ImageTableEntry::ImageTableEntry(ImageIndex handle, const std::vector<std::uint8_t>& bytes) :
		p_Impl(new Impl(handle, bytes)) {}

	ImageTableEntry::~ImageTableEntry() { delete p_Impl; p_Impl = nullptr; }

	ImageTableEntry::ImageTableEntry(const ImageTableEntry &rhs) : p_Impl(new Impl()) {
		p_Impl->m_handle = rhs.p_Impl->m_handle;
		p_Impl->m_address = rhs.p_Impl->m_address;
		p_Impl->m_npts = rhs.p_Impl->m_npts;
		p_Impl->m_size = rhs.p_Impl->m_size;
		p_Impl->m_uuid = rhs.p_Impl->m_uuid;
		p_Impl->m_name = rhs.p_Impl->m_name;
	}

	ImageTableEntry &ImageTableEntry::operator= (const ImageTableEntry &rhs) {
		if (this == &rhs) return *this;
		p_Impl->m_handle = rhs.p_Impl->m_handle;
		p_Impl->m_address = rhs.p_Impl->m_address;
		p_Impl->m_npts = rhs.p_Impl->m_npts;
		p_Impl->m_size = rhs.p_Impl->m_size;
		p_Impl->m_uuid = rhs.p_Impl->m_uuid;
		p_Impl->m_name = rhs.p_Impl->m_name;
		return *this;
	}
	const ImageIndex& ImageTableEntry::Handle() const{ return p_Impl->m_handle; }
	const std::uint32_t& ImageTableEntry::Address() const { return p_Impl->m_address; }
	const int& ImageTableEntry::NPts() const { return p_Impl->m_npts; }
	const int& ImageTableEntry::Size() const { return p_Impl->m_size; }
	const std::uint32_t& ImageTableEntry::Format() const { return p_Impl->m_format; }
	const std::array<std::uint8_t, 16>& ImageTableEntry::UUID() const { return p_Impl->m_uuid; }
	const std::string& ImageTableEntry::Name() const { return p_Impl->m_name; }

	bool ImageTableEntry::Matches(const Image& img) const
	{
		return (p_Impl->m_uuid == img.GetUUID());
	}

	ImageTable::ImageTable() {};

	class SequenceEntry::Impl {
	public:
		Impl();
		Impl(const std::array<std::uint8_t, 16>& uuid, const int rpts = 0);

		std::array<std::uint8_t, 16> uuid;
		std::chrono::duration<double> outDelay;
		std::array<MHz, (RFChannel::max - RFChannel::min + 1)> offsets;
		int n_repeats;
	};

	SequenceEntry::Impl::Impl() : n_repeats(0) {}
	SequenceEntry::Impl::Impl(const std::array<std::uint8_t, 16>& uuid, const int rpts) : uuid(uuid), n_repeats(rpts) {}

	SequenceEntry::SequenceEntry() : p_Impl(new Impl()) {}
	SequenceEntry::SequenceEntry(const std::array<std::uint8_t, 16>& uuid, const int rpts) : p_Impl(new Impl(uuid, rpts)) {}

	SequenceEntry::~SequenceEntry() {
		delete p_Impl;
		p_Impl = nullptr;
	}

	SequenceEntry::SequenceEntry(const SequenceEntry& rhs) : p_Impl(new Impl())
	{
		p_Impl->outDelay = rhs.p_Impl->outDelay;
		p_Impl->uuid = rhs.p_Impl->uuid;
		p_Impl->n_repeats = rhs.p_Impl->n_repeats;
		for (RFChannel ch = RFChannel::min; ; ch++) {
			this->SetFrequencyOffset(rhs.GetFrequencyOffset(ch), ch);
			if (ch == RFChannel::max) break;
		}
	}

	SequenceEntry& SequenceEntry::operator =(const SequenceEntry& rhs) {
		if (this == &rhs) return *this;
		p_Impl->outDelay = rhs.p_Impl->outDelay;
		p_Impl->uuid = rhs.p_Impl->uuid;
		p_Impl->n_repeats = rhs.p_Impl->n_repeats;
		for (RFChannel ch = RFChannel::min; ; ch++) {
			this->SetFrequencyOffset(rhs.GetFrequencyOffset(ch), ch);
			if (ch == RFChannel::max) break;
		}
		return *this;
	}

	std::chrono::duration<double>& SequenceEntry::SyncOutDelay()
	{
		return p_Impl->outDelay;
	}

	const std::chrono::duration<double>& SequenceEntry::SyncOutDelay() const
	{
		return p_Impl->outDelay;
	}

	void SequenceEntry::SetFrequencyOffset(const MHz& offset, const RFChannel& chan)
	{
		if (chan.IsAll()) {
			for (int ch = RFChannel::min; ; ch++)
				p_Impl->offsets[ch - RFChannel::min] = offset;
		}
		else {
			p_Impl->offsets[chan - RFChannel::min] = offset;
		}
	}

	const MHz& SequenceEntry::GetFrequencyOffset(const RFChannel& chan) const
	{
		if (!chan.IsAll()) {
			return p_Impl->offsets[chan - RFChannel::min];
		}
		else return p_Impl->offsets[0];
	}

	const int& SequenceEntry::NumRpts() const
	{
		return p_Impl->n_repeats;
	}

	const std::array<std::uint8_t, 16>& SequenceEntry::UUID() const
	{
		return p_Impl->uuid;
	}

	class ImageSequenceEntry::Impl {
	public:
		Impl();
		Impl(const Image&, const ImageRepeats& Rpt = ImageRepeats::NONE);
		Impl(const kHz& InternalClock, const ImageRepeats& Rpt = ImageRepeats::NONE);
		Impl(const int ExtClockDivide, const ImageRepeats& Rpt = ImageRepeats::NONE);

		ImageRepeats rpts;
		Frequency InternalClock;
		int ExtClockDivide;
		std::chrono::duration<double> imgDelay;
	};

	ImageSequenceEntry::Impl::Impl() :
		rpts(ImageRepeats::NONE), InternalClock(kHz(1.0)), ExtClockDivide(1), imgDelay(std::chrono::duration<double>(0))
	{}

	ImageSequenceEntry::Impl::Impl(const Image& img, const ImageRepeats& Rpt) :
		rpts(Rpt), InternalClock(img.ClockRate()), ExtClockDivide(img.ExtClockDivide()), imgDelay(std::chrono::duration<double>(0))
	{}

	ImageSequenceEntry::Impl::Impl(const kHz& InternalClock, const ImageRepeats& Rpt) :
		rpts(Rpt), InternalClock(InternalClock), ExtClockDivide(1), imgDelay(std::chrono::duration<double>(0))
	{}

	ImageSequenceEntry::Impl::Impl(const int ExtClockDivide, const ImageRepeats& Rpt) :
		rpts(Rpt), InternalClock(kHz(1.0)), ExtClockDivide(ExtClockDivide), imgDelay(std::chrono::duration<double>(0))
	{}

	ImageSequenceEntry::ImageSequenceEntry() : p_Impl (new Impl()) {}

	ImageSequenceEntry::ImageSequenceEntry(const Image& img, const ImageRepeats& Rpt, const int rpts) :
		p_Impl(new Impl(img, Rpt)), SequenceEntry(img.GetUUID(), rpts) {}

	ImageSequenceEntry::ImageSequenceEntry(const ImageTableEntry& ite, const kHz& InternalClock, const ImageRepeats& Rpt, const int rpts) :
		p_Impl(new Impl(InternalClock, Rpt)), SequenceEntry(ite.UUID(), rpts) {}

	ImageSequenceEntry::ImageSequenceEntry(const ImageTableEntry& ite, const int ExtClockDivide, const ImageRepeats& Rpt, const int rpts) :
		p_Impl(new Impl(ExtClockDivide, Rpt)), SequenceEntry(ite.UUID(), rpts) {}

	ImageSequenceEntry::ImageSequenceEntry(const SequenceEntry& entry) : SequenceEntry(entry), p_Impl(new Impl()) 
	{
		try {
			const ImageSequenceEntry& rhs = dynamic_cast<const ImageSequenceEntry&>(entry);

			p_Impl->ExtClockDivide = rhs.p_Impl->ExtClockDivide;
			p_Impl->imgDelay = rhs.p_Impl->imgDelay;
			p_Impl->InternalClock = rhs.p_Impl->InternalClock;
			p_Impl->rpts = rhs.p_Impl->rpts;
		}
		catch (std::bad_cast ex) {
			return;
		}
	}

	ImageSequenceEntry::~ImageSequenceEntry() { delete p_Impl; p_Impl = nullptr; }

	ImageSequenceEntry::ImageSequenceEntry(const ImageSequenceEntry &rhs) : p_Impl(new Impl()), SequenceEntry(rhs.UUID(), rhs.NumRpts())
	{
		p_Impl->ExtClockDivide = rhs.p_Impl->ExtClockDivide;
		p_Impl->imgDelay = rhs.p_Impl->imgDelay;
		p_Impl->InternalClock = rhs.p_Impl->InternalClock;
		this->SyncOutDelay() = rhs.SyncOutDelay();
		p_Impl->rpts = rhs.p_Impl->rpts;
		for (RFChannel ch = RFChannel::min; ; ch++) {
			this->SetFrequencyOffset(rhs.GetFrequencyOffset(ch), ch);
			if (ch == RFChannel::max) break;
		}
	}

	ImageSequenceEntry &ImageSequenceEntry::operator =(const ImageSequenceEntry &rhs)
	{
		if (this == &rhs) return *this;
		p_Impl->ExtClockDivide = rhs.p_Impl->ExtClockDivide;
		p_Impl->imgDelay = rhs.p_Impl->imgDelay;
		p_Impl->InternalClock = rhs.p_Impl->InternalClock;
		p_Impl->rpts = rhs.p_Impl->rpts;
		return *this;
	}

	bool ImageSequenceEntry::operator==(SequenceEntry const& rhs) const {

		try {
			const ImageSequenceEntry& ise = dynamic_cast<const ImageSequenceEntry&>(rhs);

			for (RFChannel ch = RFChannel::min; ; ch++) {
				if (SequenceEntry::GetFrequencyOffset(ch) != rhs.GetFrequencyOffset(ch))
					return false;
				if (ch == RFChannel::max) break;
			}
			return ((p_Impl->ExtClockDivide == ise.p_Impl->ExtClockDivide) &&
				(p_Impl->imgDelay == ise.p_Impl->imgDelay) &&
				(p_Impl->InternalClock == ise.p_Impl->InternalClock) &&
				(SequenceEntry::NumRpts() == rhs.NumRpts()) &&
				(p_Impl->rpts == ise.p_Impl->rpts) &&
				(SequenceEntry::SyncOutDelay() == rhs.SyncOutDelay()) &&
				(SequenceEntry::UUID() == rhs.UUID()));
		}
		catch (std::bad_cast ex) {
			return false;
		}
	}

	std::chrono::duration<double>& ImageSequenceEntry::PostImgDelay() {
		return p_Impl->imgDelay;
	};

	const std::chrono::duration<double>& ImageSequenceEntry::PostImgDelay() const
	{
		return p_Impl->imgDelay;
	}

	const int& ImageSequenceEntry::ExtDiv() const
	{
		return p_Impl->ExtClockDivide;
	}

	const Frequency& ImageSequenceEntry::IntOsc() const
	{
		return p_Impl->InternalClock;
	}

	const ImageRepeats& ImageSequenceEntry::RptType() const
	{
		return p_Impl->rpts;
	}

	class ImageSequence::Impl
	{
	public:
		Impl(SequenceTermAction ta, const ImageSequence* term_seq) : action(ta), termValue(0), termInsert(term_seq) {}
		Impl(SequenceTermAction ta, int val) : action(ta), termValue(val), termInsert(nullptr) {}
		Impl() : action(SequenceTermAction::DISCARD), termValue(0), termInsert(nullptr) {}

		SequenceTermAction action;
		int termValue;
		const ImageSequence* termInsert;
	};

	ImageSequence::ImageSequence() : ListBase<std::shared_ptr<SequenceEntry>>(), p_Impl(new Impl()) {}

	ImageSequence::ImageSequence(SequenceTermAction ta, int val) : ListBase<std::shared_ptr<SequenceEntry>>(), p_Impl(new Impl(ta, val)) {}
	ImageSequence::ImageSequence(SequenceTermAction ta, const ImageSequence* insert_before) : ListBase<std::shared_ptr<SequenceEntry>>(), p_Impl(new Impl(ta, insert_before)) {}

	ImageSequence::~ImageSequence() { delete p_Impl; p_Impl = nullptr; }
	ImageSequence::ImageSequence(const ImageSequence &rhs) : ListBase<std::shared_ptr<SequenceEntry>>(rhs), p_Impl(new Impl)
	{
		p_Impl->action = rhs.p_Impl->action;
		p_Impl->termValue = rhs.p_Impl->termValue;
		p_Impl->termInsert = rhs.p_Impl->termInsert;
	}
	ImageSequence &ImageSequence::operator = (const ImageSequence &rhs)
	{
		if (this == &rhs) return *this;
		ListBase<std::shared_ptr<SequenceEntry>>::operator =(rhs);
		p_Impl->action = rhs.p_Impl->action;
		p_Impl->termValue = rhs.p_Impl->termValue;
		p_Impl->termInsert = rhs.p_Impl->termInsert;
		return *this;
	}

	void ImageSequence::OnTermination(SequenceTermAction ta, int val)
	{
		p_Impl->action = ta;
		p_Impl->termValue = val;
		p_Impl->termInsert = nullptr;
	}

	void ImageSequence::OnTermination(SequenceTermAction ta, const ImageSequence* term_seq)
	{
		p_Impl->action = ta;
		p_Impl->termValue = 0;
		p_Impl->termInsert = term_seq;
	}

	const SequenceTermAction& ImageSequence::TermAction() const
	{
		return p_Impl->action;
	}

	const int& ImageSequence::TermValue() const
	{
		return p_Impl->termValue;
	}

	const ImageSequence* ImageSequence::TermInsertBefore() const
	{
		return p_Impl->termInsert;
	}


	// ImageGroup
	class ImageGroup::Impl
	{
	public:
		Impl(const std::time_t& create_time) : m_create_time(create_time),
			m_auth("Nobody"), m_company("No Company"), m_rev("1"), m_desc("No Description") {};
		~Impl() {}

		ImageSequence seq;
		std::time_t m_create_time;
		std::string m_auth;
		std::string m_company;
		std::string m_rev;
		std::string m_desc;
	};

	ImageGroup::ImageGroup(const std::string& name, const std::time_t& create_time, const std::time_t& modified_time)
		: DequeBase<Image>(name, modified_time), p_Impl(new Impl(create_time)) {}

	ImageGroup::ImageGroup(std::size_t n, const std::string& name, const std::time_t& create_time, const std::time_t& modified_time)
		: DequeBase<Image>(name, modified_time), p_Impl(new Impl(create_time)) {
            int i;
            for (i=0; i<n; i++) this->AddImage(Image());
            i=1;
            for (auto& img: *this) {
                std::stringstream ss;
                ss << "Image " << i++;
                img.Name() = ss.str();
//                p_Impl->seq.push_back(std::make_shared<ImageSequenceEntry>(img));
            }
        }

	ImageGroup::~ImageGroup() { delete p_Impl; p_Impl = nullptr; }

	// Copy & Assignment Constructors
	ImageGroup::ImageGroup(const ImageGroup &rhs) : DequeBase<Image>(rhs), p_Impl(new Impl(time(nullptr)))
	{
		this->p_Impl->seq = rhs.p_Impl->seq;
		this->p_Impl->m_create_time = rhs.p_Impl->m_create_time;
	};

	ImageGroup& ImageGroup::operator =(const ImageGroup &rhs)
	{
		if (this == &rhs) return *this;
		DequeBase<Image>::operator =(rhs);
		this->p_Impl->seq = rhs.p_Impl->seq;
		this->p_Impl->m_create_time = rhs.p_Impl->m_create_time;
		return *this;
	};

	// Use these to add/insert points to the image
	void ImageGroup::AddImage(const Image& img)
	{
		DequeBase<Image>::push_back(img);
	}

	ImageGroup::iterator ImageGroup::InsertImage(iterator it, const Image& img)
	{
		return DequeBase<Image>::insert(it, img);
	}

	// Remove Elements and Clear Whole Image
	ImageGroup::iterator ImageGroup::RemoveImage(iterator it)
	{
		return DequeBase<Image>::erase(it);
	}

	ImageGroup::iterator ImageGroup::RemoveImage(iterator first, iterator last)
	{
		return DequeBase<Image>::erase(first, last);
	}

	void ImageGroup::Clear()
	{
		DequeBase<Image>::clear();
		p_Impl->seq.clear();
	}

	int ImageGroup::Size() const
	{
		return static_cast<int>(DequeBase<Image>::size());
	}

	const std::time_t& ImageGroup::CreatedTime() const
	{
		return p_Impl->m_create_time;
	}

	std::string ImageGroup::CreatedTimeFormat() const
	{
		char buffer[80];
		std::strftime(buffer, sizeof(buffer), "%c %Z", std::localtime(&p_Impl->m_create_time));
		return std::string(buffer);
	}

	std::string& ImageGroup::Author() { return p_Impl->m_auth; }
	const std::string& ImageGroup::Author() const { return p_Impl->m_auth; }

	std::string& ImageGroup::Company() { return p_Impl->m_company; }
	const std::string& ImageGroup::Company() const { return p_Impl->m_company; }

	std::string& ImageGroup::Revision() { return p_Impl->m_rev; }
	const std::string& ImageGroup::Revision() const { return p_Impl->m_rev; }

	std::string& ImageGroup::Description() { return p_Impl->m_desc; }
	const std::string& ImageGroup::Description() const { return p_Impl->m_desc; }

	const ImageSequence& ImageGroup::Sequence() const {
		return p_Impl->seq;
	}

	ImageSequence& ImageGroup::Sequence() {
		return p_Impl->seq;
	}

}
