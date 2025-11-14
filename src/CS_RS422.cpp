/*-----------------------------------------------------------------------------
/ Title      : Connection Settings Implementation for Serial Connections
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg.qytek.lan/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/src/CM_ENET.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2018-03-23 18:32:16 +0000 (Fri, 23 Mar 2018) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 326 $
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

#if defined(_WIN32) || defined(__QNXNTO__) || defined(__linux__)

#include "CS_RS422.h"
#include <memory>

namespace iMS {

	class CS_RS422::Impl {
	public:
		Impl() {}
		~Impl() {}

		void update_from_data();
		void update_from_int();

		std::vector<std::uint8_t> settings_data;
		std::uint32_t baud_rate;
        DataBitsSetting data_bits;
        ParitySetting parity;
        StopBitsSetting stop_bits;

		static const std::string CS_IDENT;
	};

    CS_RS422::CS_RS422(unsigned int baud_rate,
            DataBitsSetting data_bits,
            ParitySetting parity,
            StopBitsSetting stop_bits) : pImpl(new Impl())
	{
		this->BaudRate(baud_rate);
	}

	CS_RS422::CS_RS422(std::vector<std::uint8_t> process_data) : pImpl(new Impl())
	{
		this->ProcessData(process_data);
	}

	CS_RS422::~CS_RS422() {
		delete pImpl;
		pImpl = nullptr;
	}

	void CS_RS422::Impl::update_from_data() {
		settings_data.resize(16);
		this->baud_rate = static_cast<std::uint32_t>(settings_data[0]) |
			(static_cast<std::uint32_t>(settings_data[1]) << 8) |
			(static_cast<std::uint32_t>(settings_data[2]) << 16) |
			(static_cast<std::uint32_t>(settings_data[3]) << 24);
        std::uint32_t _data_bits = static_cast<std::uint32_t>(settings_data[4]) |
			(static_cast<std::uint32_t>(settings_data[5]) << 8) |
			(static_cast<std::uint32_t>(settings_data[6]) << 16) |
			(static_cast<std::uint32_t>(settings_data[7]) << 24);
        std::uint32_t _parity = static_cast<std::uint32_t>(settings_data[8]) |
			(static_cast<std::uint32_t>(settings_data[9]) << 8) |
			(static_cast<std::uint32_t>(settings_data[10]) << 16) |
			(static_cast<std::uint32_t>(settings_data[11]) << 24);
        std::uint32_t _stop_bits = static_cast<std::uint32_t>(settings_data[12]) |
			(static_cast<std::uint32_t>(settings_data[13]) << 8) |
			(static_cast<std::uint32_t>(settings_data[14]) << 16) |
			(static_cast<std::uint32_t>(settings_data[15]) << 24);

        switch(_data_bits) {
            case 2: this->data_bits = DataBitsSetting::BITS_7; break;
            case 3: this->data_bits = DataBitsSetting::BITS_8; break;
            default: this->data_bits = DataBitsSetting::BITS_8; break;
        }

        switch(_parity) {
            case 0: this->parity = ParitySetting::NONE; break;
            case 1: this->parity = ParitySetting::ODD; break;
            case 2: this->parity = ParitySetting::EVEN; break;
            default: this->parity = ParitySetting::NONE; break;
        }

        switch(_stop_bits) {
            case 0: this->stop_bits = StopBitsSetting::BITS_1; break;
            case 1: this->stop_bits = StopBitsSetting::BITS_2; break;
            default: this->stop_bits = StopBitsSetting::BITS_1; break;
        }
	}

	void CS_RS422::Impl::update_from_int() {
		settings_data.clear();
		settings_data.push_back(static_cast<std::uint8_t>(baud_rate & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((baud_rate >> 8) & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((baud_rate >> 16) & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((baud_rate >> 24) & 0xff));

        std::uint32_t _data_bits, _parity, _stop_bits;

        switch(this->data_bits) {
//            case DataBitsSetting::BITS_7: _data_bits = 2; break;   // Must be 8 bits!
            case DataBitsSetting::BITS_8: _data_bits = 3; break;
            default: _data_bits = 3; break;
        }
		settings_data.push_back(static_cast<std::uint8_t>(_data_bits & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((_data_bits >> 8) & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((_data_bits >> 16) & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((_data_bits >> 24) & 0xff));        

        switch(this->parity) {
            case ParitySetting::NONE: _parity = 0; break;
            case ParitySetting::ODD:  _parity = 1; break;
            case ParitySetting::EVEN: _parity = 2; break;
            default: _parity = 0; break;
        }
		settings_data.push_back(static_cast<std::uint8_t>(_parity & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((_parity >> 8) & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((_parity >> 16) & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((_parity >> 24) & 0xff));   

        switch(this->stop_bits) {
            case StopBitsSetting::BITS_1: _stop_bits = 0; break;
            case StopBitsSetting::BITS_2: _stop_bits = 1; break;
            default: _stop_bits = 0; break;
        }
		settings_data.push_back(static_cast<std::uint8_t>(_stop_bits & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((_stop_bits >> 8) & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((_stop_bits >> 16) & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((_stop_bits >> 24) & 0xff));        
    }

	void CS_RS422::BaudRate(unsigned int baud)
	{
		pImpl->baud_rate = baud;
		pImpl->update_from_int();
	}

	unsigned int CS_RS422::BaudRate() const 
	{
		return pImpl->baud_rate;
	}

    void CS_RS422::DataBits(DataBitsSetting data_bits)
	{
		pImpl->data_bits = data_bits;
		pImpl->update_from_int();
	}

	CS_RS422::DataBitsSetting CS_RS422::DataBits() const 
	{
		return pImpl->data_bits;
	}

    void CS_RS422::Parity(ParitySetting parity)
	{
		pImpl->parity = parity;
		pImpl->update_from_int();
	}

	CS_RS422::ParitySetting CS_RS422::Parity() const 
	{
		return pImpl->parity;
	}

    void CS_RS422::StopBits(StopBitsSetting stop_bits)
	{
		pImpl->stop_bits = stop_bits;
		pImpl->update_from_int();
	}

	CS_RS422::StopBitsSetting CS_RS422::StopBits() const 
	{
		return pImpl->stop_bits;
	}

	const std::string& CS_RS422::Ident() const 
	{
		return Impl::CS_IDENT;
	}

	void CS_RS422::ProcessData(const std::vector<std::uint8_t>& data) 
	{
		pImpl->settings_data = data;
		pImpl->update_from_data();
	}

	const std::vector<std::uint8_t>& CS_RS422::ProcessData() const 
	{
		return pImpl->settings_data;
	}

	const std::string CS_RS422::Impl::CS_IDENT = "CS_RS422" ;

    
    std::shared_ptr<IConnectionSettings> CS_RS422::Clone() const
    {
        return std::make_shared<CS_RS422>(*this);
    }

    CS_RS422::CS_RS422(const CS_RS422 &rhs) : pImpl(new Impl())
    {
		pImpl->settings_data = rhs.pImpl->settings_data;
		pImpl->baud_rate = rhs.pImpl->baud_rate;
        pImpl->data_bits = rhs.pImpl->data_bits;
        pImpl->parity = rhs.pImpl->parity;
        pImpl->stop_bits = rhs.pImpl->stop_bits;
    }

    CS_RS422 &CS_RS422::operator =(const CS_RS422 &rhs)
    {
		if (this == &rhs) return *this;
		pImpl->settings_data = rhs.pImpl->settings_data;
		pImpl->baud_rate = rhs.pImpl->baud_rate;
        pImpl->data_bits = rhs.pImpl->data_bits;
        pImpl->parity = rhs.pImpl->parity;
        pImpl->stop_bits = rhs.pImpl->stop_bits;
        return *this;
    }

}

#endif
