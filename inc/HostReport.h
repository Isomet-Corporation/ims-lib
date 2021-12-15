/*-----------------------------------------------------------------------------
/ Title      : Host Report (Host to iMS) Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Messaging/h/HostReport.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2021-08-20 22:35:21 +0100 (Fri, 20 Aug 2021) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 489 $
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

#ifndef IMS_REPORT_H__
#define IMS_REPORT_H__

#include "IOReport.h"

#include <memory>
#include <vector>

namespace iMS
{
	class HostReport : public IOReport
	{
	public:
		enum class Actions : std::uint8_t
		{
			PLL_REF = 0x00,
			RF_POWER = 0x01,
			SYNTH_EEPROM = 0x02,
			ASYNC_DAC = 0x03,
			EXT_ADC = 0x04,
			ASYNC_CONTROL = 0x05,
			LUT_ENTRY = 0x06,
			SYNTH_REG = 0x07,
			AOD_TEMP = 0x08,
			AOD_EEPROM = 0x09,
			RFA_ADC12 = 0x0a,
			RFA_ADC34 = 0x0b,
			RFA_TEMP = 0x0c,
			RFA_EEPROM = 0x0d,
			RUN_SCRIPT = 0x0e,
			FAN_CONTROL = 0x0f,
			CTRLR_REG = 0x11,
			CTRLR_IMAGE = 0x12,
			CTRLR_SETTINGS = 0x13,
			CTRLR_IMGDMA = 0x14,
			CTRLR_IMGIDX = 0x15,
			CTRLR_SYNDMA = 0x16,
			CTRLR_SEQQUEUE = 0x17,
			CTRLR_SEQPLAY = 0x18,
			CTRLR_INTREN = 0x19,
			FW_UPGRADE = 0x20,
			WAVE_SHAPING = 0x21,
			CTRLR_FW_UPGRADE = 0x30
		};

		enum class Dir : std::uint8_t
		{
			WRITE = 0x00,
			READ = 0x80
		};

		enum class ImageIndexOperations : std::uint8_t
		{
			ADD_ENTRY = 0x00,
			DEL_ENTRY = 0x01,
			GET_ENTRY = 0x02,
			CHECK_UUID = 0x03,
			GET_TABLE_SIZE = 0x04,
			ERASE_ALL = 0x05
		};

		HostReport();
		HostReport(const ReportFields);
		template<typename T> HostReport(const ReportFields, const T&);
		HostReport(const Actions actions, const Dir dir, const std::uint16_t addr);
		HostReport(const Actions actions, const Dir dir);
		~HostReport();
		HostReport(const HostReport &);
		HostReport &operator =(const HostReport &);

		// Return a serial stream of bytes to pass to the transport mechanism
		const std::vector<std::uint8_t>& SerialStream();
	private:
		class Impl;
		Impl * p_Impl;
	};

}

#endif
