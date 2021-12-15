/*-----------------------------------------------------------------------------
/ Title      : I/O Reports Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Messaging/src/IOReport.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2020-07-30 21:50:24 +0100 (Thu, 30 Jul 2020) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 465 $
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

#include "IOReport.h"
#include "IMSTypeDefs.h"
#include "PrivateUtil.h"
#include <iostream>
#include <iomanip>
#include <utility>
#include <cctype>

#if defined _WIN32 && defined _DEBUG
#include "crtdbg.h"
#define DEBUG_NEW new( _NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

namespace iMS
{

	ReportFields::ReportFields(const ReportFields& f)
	{
		this->ID = f.ID;
		this->hdr = f.hdr;
		this->context = f.context;
		this->addr = f.addr;
		this->len = f.len;
	}

	ReportFields& ReportFields::operator= (const ReportFields& f)
	{
		this->ID = f.ID;
		this->hdr = f.hdr;
		this->context = f.context;
		this->addr = f.addr;
		this->len = f.len;
		return *this;
	}
	IOReport::IOReport() {}

	IOReport::IOReport(const ReportFields fields) : m_fields(fields) {};

/*	IOReport::IOReport(const ReportFields fields, const std::vector<std::uint8_t>& payload) :
		m_fields(fields), m_payload(payload) {};

	IOReport::IOReport(const ReportFields fields, const std::uint8_t& payload) : m_fields(fields)
	{
		this->Payload<std::uint8_t>(payload);
	}

	IOReport::IOReport(const ReportFields fields, const std::uint16_t& payload) : m_fields(fields)
	{
		this->Payload<std::uint16_t>(payload);
	}

	IOReport::IOReport(const ReportFields fields, const std::uint32_t& payload) : m_fields(fields)
	{
		this->PayloadAsUint32(payload);
	}

	IOReport::IOReport(const ReportFields fields, const std::string& payload) : m_fields(fields)
	{
		this->PayloadAsString(payload);
	}*/

	template <typename T>
	IOReport::IOReport(const ReportFields fields, const T& data) : m_fields(fields)
	{
		this->m_payload = VarToBytes<T>(data);
	}

	IOReport::IOReport(const IOReport& r)
	{
		this->m_fields = r.m_fields;
		this->m_payload = r.m_payload;
	}

	IOReport& IOReport::operator = (const IOReport& r)
	{
		this->m_fields = r.m_fields;
		this->m_payload = r.m_payload;
		return *this;
	}
	
	// Get/Setters for Report fields + Data
	void IOReport::Fields(const ReportFields fields) {
		m_fields = fields;
	}

	ReportFields IOReport::Fields()
	{
		return m_fields;
	}

	const ReportFields& IOReport::Fields() const
	{
		return m_fields;
	}

	void IOReport::ClearPayload()
	{
		m_payload.clear();
	}

/*	void IOReport::Payload<std::vector<std::uint8_t>>(const std::vector<std::uint8_t>& payload)
	{
		m_payload = payload;
		m_fields.len = m_payload.size();
	}

	std::vector<std::uint8_t> IOReport::Payload<std::vector<std::uint8_t>>()
	{
		return m_payload;
	}

	const std::vector<std::uint8_t>& IOReport::Payload<std::vector<std::uint8_t>>() const
	{
		return m_payload;
	}

	void IOReport::Payload<std::uint8_t>(const std::uint8_t& payload)
	{
		m_payload.clear();
		m_payload.push_back(payload);
		m_fields.len = 1;
	}

	std::uint8_t IOReport::Payload<std::uint8_t>()
	{
		if (m_payload.size() >= 1) {
			return m_payload[0];
		}
		else {
			return 0;
		}
	}

	void IOReport::Payload<std::uint16_t>(const std::uint16_t& payload)
	{
		m_payload.clear();
		m_payload.push_back(static_cast<std::uint8_t>(payload & 0xFF));
		m_payload.push_back(static_cast<std::uint8_t>((payload >> 8) & 0xFF));
		m_fields.len = 2;
	}

	std::uint16_t IOReport::Payload<std::uint16_t>()
	{
		if (m_payload.size() >= 2)
		{
			std::uint16_t p = m_payload[0];
			p |= (static_cast<std::uint16_t>(m_payload[1])) << 8;
			return p;
		}
		else
		{
			return 0;
		}
	}

	void IOReport::PayloadAsUint32(const std::uint32_t& payload)
	{
		m_payload.clear();
		m_payload.push_back(static_cast<std::uint8_t>(payload & 0xFF));
		m_payload.push_back(static_cast<std::uint8_t>((payload >> 8) & 0xFF));
		m_payload.push_back(static_cast<std::uint8_t>((payload >> 16) & 0xFF));
		m_payload.push_back(static_cast<std::uint8_t>((payload >> 24) & 0xFF));
		m_fields.len = 4;
	}

	std::uint32_t IOReport::PayloadAsUInt32()
	{
		if (m_payload.size() >= 4) {
			std::uint32_t p = m_payload[0];
			p |= (static_cast<std::uint32_t>(m_payload[1])) << 8;
			p |= (static_cast<std::uint32_t>(m_payload[2])) << 16;
			p |= (static_cast<std::uint32_t>(m_payload[3])) << 24;
			return p;
		}
		else {
			return 0;
		}
	}

	void IOReport::PayloadAsString(const std::string& payload)
	{
		if (payload.length() <= PAYLOAD_MAX_LENGTH)
		{
			m_payload.clear();
			for (std::string::const_iterator it = payload.begin(); it != payload.end(); ++it)
			{
				m_payload.push_back(static_cast<std::uint8_t>(*it));
			}
		}
		m_fields.len = m_payload.size();
	}

	std::string IOReport::PayloadAsString()
	{
		if (m_payload.size() >= 1) {
			std::string str(m_payload.begin(), m_payload.end());
			return str;
		}
		else {
			return std::string();
		}
	}*/
	
	template <typename T>
	void IOReport::Payload(const T& t)
	{
		// static_cast is used to select correct overload
		this->PayloadImpl(t, static_cast<T*>(0));
	}

	template void IOReport::Payload<std::uint8_t>(const std::uint8_t& t);
	template void IOReport::Payload<std::uint16_t>(const std::uint16_t& t);
	template void IOReport::Payload<std::uint32_t>(const std::uint32_t& t);
	template void IOReport::Payload<std::int8_t>(const std::int8_t& t);
	template void IOReport::Payload<std::int16_t>(const std::int16_t& t);
#if !defined(__QNXNTO__)
	template void IOReport::Payload<std::int32_t>(const std::int32_t& t);
#endif
	template void IOReport::Payload<int>(const int& t);
	template void IOReport::Payload<double>(const double& t);
	template void IOReport::Payload<float>(const float& t);
	template void IOReport::Payload<char>(const char& t);

	template void IOReport::Payload<std::vector<std::uint8_t>>(const std::vector< std::uint8_t>& t);
	template void IOReport::Payload<std::vector<std::uint16_t>>(const std::vector< std::uint16_t>& t);
	template void IOReport::Payload<std::vector<std::uint32_t>>(const std::vector< std::uint32_t>& t);
	template void IOReport::Payload<std::vector<std::int8_t>>(const std::vector< std::int8_t>& t);
	template void IOReport::Payload<std::vector<std::int16_t>>(const std::vector< std::int16_t>& t);
#if !defined (__QNXNTO__)
	template void IOReport::Payload<std::vector<std::int32_t>>(const std::vector< std::int32_t>& t);
#endif
	template void IOReport::Payload<std::vector<int>>(const std::vector< int>& t);
	template void IOReport::Payload<std::vector<double>>(const std::vector< double>& t);
	template void IOReport::Payload<std::vector<float>>(const std::vector< float>& t);
	template void IOReport::Payload<std::vector<char>>(const std::vector< char>& t);

	template void IOReport::Payload<std::string>(const std::string& t);

	template <typename T>
	void IOReport::PayloadImpl(const T& t, T*)
	{
		m_payload = VarToBytes<T>(t);
		m_fields.len = sizeof(T);
	}

	// Overloaded operators for vector type
	template <typename T>
	void IOReport::PayloadImpl(const std::vector<T>& t, std::vector<T>*)
	{
		m_payload.clear();
		for (typename std::vector<T>::const_iterator it = t.cbegin(); it != t.cend(); ++it)
		{
			const std::vector<std::uint8_t> c = VarToBytes<T>(*it);
			m_payload.insert(m_payload.end(), c.cbegin(), c.cend());
		}
		m_fields.len = static_cast<std::uint16_t>(m_payload.size());
	}

	template <>
	void IOReport::PayloadImpl<std::uint8_t>(const std::vector<std::uint8_t>& t, std::vector<std::uint8_t>*)
	{
		m_payload = t;
		m_fields.len = static_cast<std::uint16_t>(t.size());
	}

	// Template Specialisation for string type
	template <>
	void IOReport::PayloadImpl<std::string>(const std::string& t, std::string*)
	{
		const std::uint8_t* buffer = reinterpret_cast<const std::uint8_t*>(t.c_str());
		std::vector<std::uint8_t> cnv(buffer, buffer + strlen(t.c_str()));
		m_payload = cnv;
		m_fields.len = static_cast<std::uint16_t>(cnv.size());
	}

	template <typename T>
	T IOReport::Payload() const {
		// static_cast is used to select correct overload
		return (this->PayloadImpl(static_cast<T*>(0)));
	}

	template std::uint8_t IOReport::Payload<std::uint8_t>() const;
	template std::uint16_t IOReport::Payload<std::uint16_t>() const;
	template std::uint32_t IOReport::Payload<std::uint32_t>() const;
	template std::int8_t IOReport::Payload<std::int8_t>() const;
	template std::int16_t IOReport::Payload<std::int16_t>() const;
#if !defined(__QNXNTO__)
	template std::int32_t IOReport::Payload<std::int32_t>() const;
#endif
	template int IOReport::Payload<int>() const;
	template double IOReport::Payload<double>() const;
	template float IOReport::Payload<float>() const;
	template char IOReport::Payload<char>() const;

	template std::vector< std::uint8_t> IOReport::Payload<std::vector<std::uint8_t>>() const;
	template std::vector< std::uint16_t> IOReport::Payload<std::vector<std::uint16_t>>() const;
	template std::vector< std::uint32_t> IOReport::Payload<std::vector<std::uint32_t>>() const;
	template std::vector< std::int8_t> IOReport::Payload<std::vector<std::int8_t>>() const;
	template std::vector< std::int16_t> IOReport::Payload<std::vector<std::int16_t>>() const;
#if !defined(__QNXNTO__)
	template std::vector< std::int32_t> IOReport::Payload<std::vector<std::int32_t>>() const;
#endif
	template std::vector< int> IOReport::Payload<std::vector<int>>() const;
	template std::vector< double> IOReport::Payload<std::vector<double>>() const;
	template std::vector< float> IOReport::Payload<std::vector<float>>() const;
	template std::vector< char> IOReport::Payload<std::vector<char>>() const;

	template std::string IOReport::Payload<std::string>() const;

	template <typename T>
	T IOReport::PayloadImpl(T*) const
	{
		return (BytesToVar<T>(m_payload));
	}

	template <typename T>
	std::vector<T> IOReport::PayloadImpl(std::vector<T>*) const
	{
		std::vector<T> ret;
		ret.reserve(m_payload.size());
		for (std::vector<std::uint8_t>::const_iterator it = m_payload.cbegin(); it != m_payload.cend();)
		{
			if ((m_payload.cend() - it) >= (int)sizeof(T))
			{
				std::vector<std::uint8_t> cnv(it, it + (sizeof(T)));
				it += sizeof(T);
				ret.push_back(BytesToVar<T>(cnv));
			}
			else break;
		}
		return (ret);
	}

	template <>
	std::vector<std::uint8_t> IOReport::PayloadImpl<std::uint8_t>(std::vector<std::uint8_t>*) const
	{
		return m_payload;
	}

	template <>
	std::string IOReport::PayloadImpl<std::string>(std::string*) const
	{
		std::size_t size = m_payload.size();
		for (std::vector<std::uint8_t>::const_iterator it = m_payload.cbegin(); it != m_payload.cend(); ++it)
		{
			if (!std::isalnum(*it)) {
				size = (it - m_payload.cbegin());
				//size += 1;
				break;
			}
		}
		std::string s(reinterpret_cast<const char *>(m_payload.data()), size);
		return (s);
	}

	std::ostream& operator <<(std::ostream& stream, const IOReport& rpt) {
		stream << std::hex;
		stream << "ID: ";
		stream << std::setfill('0') << std::setw(2) << static_cast<int>(rpt.m_fields.ID) << "   HDR: ";
		stream << std::setfill('0') << std::setw(2) << static_cast<int>(rpt.m_fields.hdr) << "   CONTEXT: ";
		stream << std::setfill('0') << std::setw(2) << static_cast<int>(rpt.m_fields.context) << "   ADDR: ";
		stream << std::setfill('0') << std::setw(2) << rpt.m_fields.addr << "   LEN: ";
		stream << std::setfill('0') << std::setw(2) << rpt.m_fields.len << std::endl << "DATA: ";
		for (unsigned int i = 0; i < rpt.m_payload.size(); i++)
		{
			stream << std::setfill('0') << std::setw(2) << (int)rpt.m_payload[i] << " ";
			if (((i % 16) == 0) && (i != 0)) stream << std::endl;
		}
		return stream;
	}
}
