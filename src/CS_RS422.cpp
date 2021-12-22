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

namespace iMS {

	class CS_RS422::Impl {
	public:
		Impl() {}
		~Impl() {}

		void update_from_data();
		void update_from_int();

		std::vector<std::uint8_t> settings_data;
		std::uint32_t baud_rate;

		static const std::string CS_IDENT;
	};

	CS_RS422::CS_RS422() : pImpl(new Impl()) 
	{
		this->BaudRate(115200);
	}

	CS_RS422::CS_RS422(unsigned int baud_rate) : pImpl(new Impl())
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
		settings_data.resize(4);
		this->baud_rate = static_cast<std::uint32_t>(settings_data[0]) |
			(static_cast<std::uint32_t>(settings_data[1]) << 8) |
			(static_cast<std::uint32_t>(settings_data[2]) << 16) |
			(static_cast<std::uint32_t>(settings_data[3]) << 24);
	}

	void CS_RS422::Impl::update_from_int() {
		settings_data.clear();
		settings_data.push_back(static_cast<std::uint8_t>(baud_rate & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((baud_rate >> 8) & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((baud_rate >> 16) & 0xff));
		settings_data.push_back(static_cast<std::uint8_t>((baud_rate >> 24) & 0xff));
	}

	void CS_RS422::BaudRate(const unsigned int& baud)
	{
		pImpl->baud_rate = baud;
		pImpl->update_from_int();
	}

	unsigned int CS_RS422::BaudRate() const 
	{
		return pImpl->baud_rate;
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
}

#endif
