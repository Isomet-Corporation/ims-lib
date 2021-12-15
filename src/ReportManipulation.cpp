/*-----------------------------------------------------------------------------
/ Title      : I/O Report Functions Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Messaging/src/ReportManipulation.cpp $
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

#include "ReportManipulation.h"

namespace iMS
{
	CRCGenerator::CRCGenerator() : m_CRC(0)
	{ }

	CRCGenerator::CRCGenerator(std::queue<std::uint8_t>& data)
	{
		this->updateCRC(data);
	};

	CRCGenerator::~CRCGenerator() {}

	void CRCGenerator::updateCRC(std::queue<std::uint8_t>& data)
	{
		unsigned int crc = 0;
		do
		{
			crc = crc ^ (static_cast<unsigned int>(data.front()) << 8);       /* Fetch byte from memory, XOR into CRC top byte*/
			data.pop();
			for (int i = 0; i < 8; i++)                   /* Prepare to rotate 8 bits */
			{
				crc = crc << 1;                       /* rotate */
				if ((crc & 0x10000) != 0)             /* bit 15 was set (now bit 16)... */
					crc = (crc ^ CRC_POLY) & 0xFFFF;  /* XOR with polynomial */
				/* and ensure CRC remains 16-bit value */
			}                                         /* Loop for 8 bits */
		} while (!data.empty());
		m_CRC = crc;
	}

	std::uint16_t CRCGenerator::CRC() const
	{
		return m_CRC;
	}

	ReportSerializer::ReportSerializer() {}

	ReportSerializer::~ReportSerializer() {}

	// Method for converting an IO Report into a queue of bytes, ready for transmission
	void ReportSerializer::Serialize(const IOReport* const rpt)
	{
		byteStream.clear();

		// Add header
		const ReportFields& f = rpt->Fields();
		byteStream.push_back(static_cast<std::uint8_t>(f.ID));
		byteStream.push_back(static_cast<std::uint8_t>(f.hdr));
		byteStream.push_back(static_cast<std::uint8_t>(f.context));
		byteStream.push_back(static_cast<std::uint8_t>(f.len & 0xFF));
		byteStream.push_back(static_cast<std::uint8_t>((f.len >> 8) & 0xFF));
		byteStream.push_back(static_cast<std::uint8_t>(f.addr & 0xFF));
		byteStream.push_back(static_cast<std::uint8_t>((f.addr >> 8) & 0xFF));

		// Add payload data
		std::vector<std::uint8_t> p = rpt->Payload<std::vector<std::uint8_t>>();
		unsigned int payload_length = static_cast<unsigned int>(p.size());
		if (payload_length > rpt->PAYLOAD_MAX_LENGTH) payload_length = rpt->PAYLOAD_MAX_LENGTH;
		for (unsigned int i = 0; i < payload_length; i++)
		{
			byteStream.push_back(static_cast<std::uint8_t>(p[i]));
		}

		// Add CRC
		std::queue<std::uint8_t> stream_copy;
		for (std::vector<std::uint8_t>::const_iterator it = byteStream.begin(); it != byteStream.end(); ++it){
			stream_copy.push(*it);
		}
		CRCGenerator crcgen(stream_copy);
		byteStream.push_back(static_cast<std::uint8_t>((crcgen.CRC() & 0xFF)));
		byteStream.push_back(static_cast<std::uint8_t>(((crcgen.CRC() >> 8) & 0xFF)));
	}

	// Method for retrieving the byte queue
	const std::vector<std::uint8_t>& ReportSerializer::Stream() const
	{
		return byteStream;
	}

	ReportParser::ReportParser() : 
		rx_state(RxDataState::SM_ID),
		pState(ReportParserState::IDLE),
		datacount(0)
	{};

	ReportParser::~ReportParser() {}

	void ReportParser::ResetParser()
	{
		rx_state = RxDataState::SM_ID;
		pState = ReportParserState::IDLE;
	}
	// Method for parsing a byte stream received from a device
	void ReportParser::Parse(IOReport* const rpt, const std::uint8_t rxchar)
	{
		if (pState >= ReportParserState::COMPLETE) return;

		// Get copy of report fields
		ReportFields f = rpt->Fields();
		
		// Similar to parsing state machine in microblaze
		std::vector<std::uint8_t> d;
		CRCGenerator crcgen;
		switch (rx_state)
		{
		case RxDataState::SM_ID:
			if ((rxchar == static_cast<std::uint8_t>(ReportTypes::DEVICE_REPORT_ID_SYNTH)) ||
				(rxchar == static_cast<std::uint8_t>(ReportTypes::DEVICE_REPORT_ID_CTRLR)) ||
				(rxchar == static_cast<std::uint8_t>(ReportTypes::INTERRUPT_REPORT_ID_CTRLR)))
			{
				f.ID = static_cast<ReportTypes>(rxchar);
				while (!crc_queue.empty()) crc_queue.pop();
				crc_queue.push(rxchar);
				datacount = 0;
				pState = ReportParserState::PARSING;
				rx_state = RxDataState::SM_HDR0;
			}
			else {
				// Unexpected character in rx stream
				pState = ReportParserState::IDLE_UNEXPECTED_CHAR;
			}
			break;
		case RxDataState::SM_HDR0:
			f.hdr = rxchar;
			crc_queue.push(rxchar);
			rx_state = RxDataState::SM_HDR1;
			break;
		case RxDataState::SM_HDR1:
			// Currently Reserved for Future Use
			f.context = rxchar;
			crc_queue.push(rxchar);
			rx_state = RxDataState::SM_LEN0;
			break;
		case RxDataState::SM_LEN0:
			f.len = (f.len & 0xFF00) | static_cast<unsigned int>(rxchar);
			crc_queue.push(rxchar);
			rx_state = RxDataState::SM_LEN1;
			break;
		case RxDataState::SM_LEN1:
			f.len = (f.len & 0xFF) | (static_cast<unsigned int>(rxchar) << 8);
			crc_queue.push(rxchar);
			rx_state = RxDataState::SM_RSVD0;
			break;
		case RxDataState::SM_RSVD0:
			f.addr = (f.addr & 0xFF00) | static_cast<unsigned int>(rxchar);
			crc_queue.push(rxchar);
			rx_state = RxDataState::SM_RSVD1;
			break;
		case RxDataState::SM_RSVD1:
			f.addr = (f.addr & 0xFF) | (static_cast<unsigned int>(rxchar) << 8);
			crc_queue.push(rxchar);
			rpt->ClearPayload();
			if (!f.len)
			{
				rx_state = RxDataState::SM_CRC0;
			}
			else {
				rx_state = RxDataState::SM_DATA;
			}
			break;
		case RxDataState::SM_DATA:
			// Get copy of report data
			d = rpt->Payload<std::vector<std::uint8_t>>();

			d.push_back(rxchar);
			if (++datacount >= f.len)
			{
				rx_state = RxDataState::SM_CRC0;
			}
			rpt->Payload<std::vector<std::uint8_t>>(d);
			crc_queue.push(rxchar);
			break;
		case RxDataState::SM_CRC0:
			receivedCRC = static_cast<unsigned int>(rxchar);
			rx_state = RxDataState::SM_CRC1;
			break;
		case RxDataState::SM_CRC1:
			receivedCRC = (receivedCRC & 0xFF) | (static_cast<unsigned int>(rxchar) << 8);

			// Check CRC
			crcgen.updateCRC(crc_queue);

			if (crcgen.CRC() != receivedCRC) {
				pState = ReportParserState::CRC_ERROR;
			}
			else {
				pState = ReportParserState::COMPLETE;
			}
			rx_state = RxDataState::SM_COMPLETE;
			break;
		case RxDataState::SM_COMPLETE:
			// Do nothing
			break;
		}
		rpt->Fields(f);
	}

	// Method for establishing parser state
	ReportParserState ReportParser::ParserState() const
	{
		return pState;
	}

}
