/*-----------------------------------------------------------------------------
/ Title      : Device Reports (iMS to Host) Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Messaging/src/DeviceReport.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2017-09-11 23:55:34 +0100 (Mon, 11 Sep 2017) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 300 $
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

#include "DeviceReport.h"
#include "ReportManipulation.h"

namespace iMS
{

	enum DeviceReportHeader : std::uint8_t{
		DEVICE_HDR_ALARM_MASK = 0x80,
		DEVICE_HDR_ALARM_ON = 0x80,
		DEVICE_HDR_ALARM_OFF = 0x00,
		DEVICE_HDR_DATA_ERROR_MASK = 0x40,
		DEVICE_HDR_DATA_OK = 0x40,
		DEVICE_HDR_DATA_ERROR = 0x00,
		DEVICE_HDR_ERROR_TYPE_MASK = 0x20,
		DEVICE_HDR_ERROR_TYPE_GNL = 0x20,
		DEVICE_HDR_ERROR_TYPE_CRC = 0x00,
		DEVICE_HDR_NHF_MASK = 0x10,
		DEVICE_HDR_NHF_TIMEOUT = 0x10,
		DEVICE_HDR_NHF_OK = 0x00
	};

	class DeviceReport::Impl{
	public:
		ReportParser parser;
	};

	DeviceReport::DeviceReport() : p_Impl(new Impl()) {}
	DeviceReport::~DeviceReport() { delete p_Impl; p_Impl = nullptr; }

	// Copy Constructor
	DeviceReport::DeviceReport(const DeviceReport &rhs) : p_Impl(new Impl())
	{
		m_fields = rhs.m_fields;
		m_payload = rhs.m_payload;
		p_Impl->parser = rhs.p_Impl->parser;
	}

	// Assignment Constructor
	DeviceReport& DeviceReport::operator =(const DeviceReport &rhs)
	{
		if (this == &rhs) return *this;
		m_fields = rhs.m_fields;
		m_payload = rhs.m_payload;
		p_Impl->parser = rhs.p_Impl->parser;
		return *this;
	}

	void DeviceReport::Parse(const std::uint8_t rxchar)
	{
		p_Impl->parser.Parse(this, rxchar);
	}

	void DeviceReport::ResetParser()
	{
		p_Impl->parser.ResetParser();
	}

    const bool DeviceReport::Idle() const
	{
		return ((p_Impl->parser.ParserState() == ReportParserState::IDLE) ||
			(p_Impl->parser.ParserState() == ReportParserState::IDLE_UNEXPECTED_CHAR));
	}

	const bool DeviceReport::Done() const
	{
		return ((p_Impl->parser.ParserState() == ReportParserState::COMPLETE) ||
			(p_Impl->parser.ParserState() == ReportParserState::CRC_ERROR));
	}

	const bool DeviceReport::RxCRC() const
	{
		return (p_Impl->parser.ParserState() == ReportParserState::CRC_ERROR);
	}

	const bool DeviceReport::UnexpectedChar() const
	{
		return (p_Impl->parser.ParserState() == ReportParserState::IDLE_UNEXPECTED_CHAR);
	}

	const bool DeviceReport::TxCRC() const
	{
		const ReportFields& f = this->Fields();
		return (
			((f.hdr & DeviceReportHeader::DEVICE_HDR_DATA_ERROR_MASK) ==
			DeviceReportHeader::DEVICE_HDR_DATA_ERROR) &&
			((f.hdr & DeviceReportHeader::DEVICE_HDR_ERROR_TYPE_MASK) ==
			DeviceReportHeader::DEVICE_HDR_ERROR_TYPE_CRC) );
	}

	const bool DeviceReport::TxTimeout() const
	{
		const ReportFields& f = this->Fields();
		return ((f.hdr & DeviceReportHeader::DEVICE_HDR_NHF_MASK) ==
			DeviceReportHeader::DEVICE_HDR_NHF_TIMEOUT);
	}

	const bool DeviceReport::HardwareAlarm() const
	{
		const ReportFields& f = this->Fields();
		return ((f.hdr & DeviceReportHeader::DEVICE_HDR_ALARM_MASK) ==
			DeviceReportHeader::DEVICE_HDR_ALARM_ON);
	}

	const bool DeviceReport::GeneralError() const
	{
		const ReportFields& f = this->Fields();
		return (
			((f.hdr & DeviceReportHeader::DEVICE_HDR_DATA_ERROR_MASK) ==
			DeviceReportHeader::DEVICE_HDR_DATA_ERROR) &&
			((f.hdr & DeviceReportHeader::DEVICE_HDR_ERROR_TYPE_MASK) ==
			DeviceReportHeader::DEVICE_HDR_ERROR_TYPE_GNL));
	}

}
