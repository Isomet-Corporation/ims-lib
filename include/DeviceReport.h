/*-----------------------------------------------------------------------------
/ Title      : Device Report (iMS to Host) Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Messaging/h/DeviceReport.h $
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

#ifndef IMS_DEVICE_REPORT_H__
#define IMS_DEVICE_REPORT_H__

#include "IOReport.h"

namespace iMS
{
	class DeviceReport : public IOReport
	{
	public:
		DeviceReport();
		~DeviceReport();
		DeviceReport(const DeviceReport &);
		DeviceReport &operator =(const DeviceReport &);

		// Provide a byte received from the transport layer
		void Parse(const std::uint8_t rxchar);

		// Reset Parser 
		void ResetParser();

		// Methods for establishing various success or failure modes
		const bool Idle() const;
		const bool Done() const;
		const bool RxCRC() const;
		const bool UnexpectedChar() const;
		const bool TxCRC() const;
		const bool TxTimeout() const;
		const bool HardwareAlarm() const;
		const bool GeneralError() const;

	private:
		class Impl;
		Impl * p_Impl;
	};

}

#endif
