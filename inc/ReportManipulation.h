/*-----------------------------------------------------------------------------
/ Title      : I/O Report Private Functions Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Messaging/h/ReportManipulation.h $
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

#ifndef IMS_REPORT_SERIALIZER_H__
#define IMS_REPORT_SERIALIZER_H__

#include "IOReport.h"

#include <queue>
#include <cstdint>

namespace iMS
{

	class CRCGenerator
	{
	public:
		CRCGenerator();
		CRCGenerator(std::queue<std::uint8_t>&);
		~CRCGenerator();

		void updateCRC(std::queue<std::uint8_t>&);
		std::uint16_t CRC() const;
	private:
		const unsigned int CRC_POLY = 0x8005;

		std::uint16_t m_CRC;
	};

	class ReportSerializer
	{
	public:
		ReportSerializer();
		~ReportSerializer();

		// Method for converting an IO Report into a queue of bytes, ready for transmission
		void Serialize(const IOReport* const);

		// Method for retrieving the byte queue
		const std::vector<std::uint8_t>& Stream() const;

	private:
		std::vector<std::uint8_t> byteStream;
	};

	enum class ReportParserState {
		// Normal state before any characters processed
		IDLE,                       
		// State machine idle, attempted to process a character that wasn't expected (not an ID)
		IDLE_UNEXPECTED_CHAR,
		// Mid-processing
		PARSING,
		// Finished processing.  Parsed OK, no CRC error either end, no alarms or errors
		COMPLETE,  
		// Finished processing, received data contained a CRC error.
		CRC_ERROR                       
	};

	class ReportParser
	{
	public:
		ReportParser();
		~ReportParser();

		// Method for parsing a byte stream received from a device
		void Parse(IOReport* const rpt, const std::uint8_t);
		void ResetParser();

		// Method for establishing parser state
		ReportParserState ParserState() const;
	
	private:
		enum class RxDataState {
			SM_ID = 0,
			SM_HDR0,
			SM_HDR1,
			SM_LEN0,
			SM_LEN1,
			SM_RSVD0,
			SM_RSVD1,
			SM_DATA,
			SM_CRC0,
			SM_CRC1,
			SM_COMPLETE
		};
		RxDataState rx_state;
		ReportParserState pState;
		unsigned int datacount;
		std::uint16_t receivedCRC;
		std::queue<std::uint8_t> crc_queue;
	};

}
#endif

