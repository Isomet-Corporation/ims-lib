/*-----------------------------------------------------------------------------
/ Title      : Connection Settings Implementation for Ethernet Connections
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

#include "CS_ETH.h"
#include "boost/asio/ip/address.hpp"

#include <algorithm>
#include <memory>

namespace iMS {

	using namespace boost::asio;

	class CS_ETH::Impl {
	public:
		Impl() {}
		~Impl() {}

		void update_from_data();
		void update_from_strings();

		std::vector<std::uint8_t> settings_data;
		bool use_dhcp;
		ip::address addr;
		ip::address netmask;
		ip::address gw;

		static const std::string CS_IDENT;
	};

	CS_ETH::CS_ETH() : pImpl(new Impl())
	{
		pImpl->use_dhcp = true;
		pImpl->addr = ip::make_address_v4("192.168.1.10");
		pImpl->netmask = ip::make_address_v4("255.255.255.0");
		pImpl->gw = ip::make_address_v4("192.168.1.1");
		pImpl->update_from_strings();
	}

	CS_ETH::CS_ETH(bool use_dhcp,
		std::string addr,
		std::string netmask,
		std::string gw) : pImpl(new Impl())
	{
		pImpl->use_dhcp = use_dhcp;
		pImpl->addr = ip::make_address_v4(addr);
		pImpl->netmask = ip::make_address_v4(netmask);
		pImpl->gw = ip::make_address_v4(gw);
		pImpl->update_from_strings();
	}

	CS_ETH::CS_ETH(std::vector<std::uint8_t> process_data) : pImpl(new Impl())
	{
		this->ProcessData(process_data);
	}

	CS_ETH::~CS_ETH() {
		delete pImpl;
		pImpl = nullptr;
	}

	void CS_ETH::Impl::update_from_data() {
		this->use_dhcp = settings_data[0] ? true : false;
		std::array<std::uint8_t, 4> addr, mask, gw;
		std::copy_n(settings_data.begin() + 1, 4, addr.begin());
		std::copy_n(settings_data.begin() + 5, 4, mask.begin());
		std::copy_n(settings_data.begin() + 9, 4, gw.begin());
		this->addr = ip::make_address_v4(addr);
		this->netmask = ip::make_address_v4(mask);
		this->gw = ip::make_address_v4(gw);
	}

	void CS_ETH::Impl::update_from_strings() {
		settings_data.clear();
		std::uint8_t dhcp = static_cast<std::uint8_t>(this->use_dhcp);
		auto addr = this->addr.to_v4().to_bytes();
		auto mask = this->netmask.to_v4().to_bytes();
		auto gw = this->gw.to_v4().to_bytes();
		settings_data.resize(sizeof(dhcp) + sizeof(addr) + sizeof(mask) + sizeof(gw));
		settings_data[0] = (dhcp);
		std::copy_n(addr.begin(), addr.size(), settings_data.begin() + 1);
		std::copy_n(mask.begin(), mask.size(), settings_data.begin() + 5);
		std::copy_n(gw.begin(), gw.size(), settings_data.begin() + 9);
	}

	void CS_ETH::UseDHCP(bool dhcp) {
		pImpl->use_dhcp = dhcp;
		pImpl->update_from_strings();
	}

	bool CS_ETH::UseDHCP() const { return pImpl->use_dhcp; }

	void CS_ETH::Address(const std::string& addr) {
		pImpl->addr = ip::make_address_v4(addr);
		pImpl->update_from_strings();
	}

	std::string CS_ETH::Address() const {
		return pImpl->addr.to_v4().to_string();
	}

	void CS_ETH::Netmask(const std::string& mask) {
		pImpl->netmask = ip::make_address_v4(mask);
		pImpl->update_from_strings();
	}

	std::string CS_ETH::Netmask() const {
		return pImpl->netmask.to_v4().to_string();
	}

	void CS_ETH::Gateway(const std::string& gw) {
		pImpl->gw = ip::make_address_v4(gw);
		pImpl->update_from_strings();
	}

	std::string CS_ETH::Gateway() const {
		return pImpl->gw.to_v4().to_string();
	}

	const std::string& CS_ETH::Ident() const {
		return Impl::CS_IDENT;
	}

	void CS_ETH::ProcessData(const std::vector<std::uint8_t>& data) {
		pImpl->settings_data = data;
		pImpl->update_from_data();
	}

	const std::vector<std::uint8_t>& CS_ETH::ProcessData() const {
		return pImpl->settings_data;
	}

	const std::string CS_ETH::Impl::CS_IDENT = "CS_ETH";

    std::shared_ptr<IConnectionSettings> CS_ETH::Clone() const
    {
        return std::make_shared<CS_ETH>(*this);
    }

    CS_ETH::CS_ETH(const CS_ETH &rhs) : pImpl(new Impl())
    {
		pImpl->settings_data = rhs.pImpl->settings_data;
		pImpl->use_dhcp = rhs.pImpl->use_dhcp;
		pImpl->addr = rhs.pImpl->addr;
		pImpl->netmask = rhs.pImpl->netmask;
		pImpl->gw = rhs.pImpl->gw;
    }

    CS_ETH &CS_ETH::operator =(const CS_ETH &rhs)
    {
		if (this == &rhs) return *this;
		pImpl->settings_data = rhs.pImpl->settings_data;
		pImpl->use_dhcp = rhs.pImpl->use_dhcp;
		pImpl->addr = rhs.pImpl->addr;
		pImpl->netmask = rhs.pImpl->netmask;
		pImpl->gw = rhs.pImpl->gw;
        return *this;
    }

}

#endif
