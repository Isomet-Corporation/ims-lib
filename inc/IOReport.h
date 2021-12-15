/*-----------------------------------------------------------------------------
/ Title      : I/O Report Base Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Messaging/h/IOReport.h $
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

#ifndef IMS_IO_REPORT_H__
#define IMS_IO_REPORT_H__

#include "IMSTypeDefs.h"
#include <vector>
#include <cstdint>

namespace iMS
{

	enum class ReportTypes : std::uint8_t
	{
		HOST_REPORT_ID_SYNTH = 1,
		DEVICE_REPORT_ID_SYNTH = 2,
		HOST_REPORT_ID_CTRLR = 4,
		DEVICE_REPORT_ID_CTRLR = 5,
		INTERRUPT_REPORT_ID_CTRLR = 73,
		NULL_REPORT = 255
	};

	struct ReportFields
	{
		ReportTypes ID;
		std::uint8_t hdr;
		std::uint8_t context = 0;
		std::uint16_t len;
		std::uint16_t addr;

		ReportFields() : ID(ReportTypes::NULL_REPORT), hdr(0), len(0), addr(0) {};
		ReportFields(ReportTypes ID, std::uint8_t hdr, std::uint16_t len, std::uint16_t addr) : ID(ID), hdr(hdr), len(len), addr(addr) {};
		ReportFields(ReportTypes ID, std::uint8_t hdr, std::uint8_t context, std::uint16_t len, std::uint16_t addr) : ID(ID), hdr(hdr), context(context), len(len), addr(addr) {};
		ReportFields(const ReportFields&);
		ReportFields& operator= (const ReportFields&);
	};

	// This class is the base definition for an I/O Report.
	// It allows the user to set and retrieve the basic data structures
	// that make up a report.  It is extended by Host and Device reports
	// to add additional functionality that interface the report to the iMS hardware
	class IOReport
	{
	public:
		static const unsigned int PAYLOAD_MAX_LENGTH = 64;
		static const unsigned int OVERHEAD_MAX_LENGTH = 9;

		IOReport();
		IOReport(const ReportFields);
		template <typename T>
		IOReport(const ReportFields, const T&);
		virtual ~IOReport() {};
		IOReport(const IOReport&);  // copy constructor
		IOReport& operator= (const IOReport&);  // assignment

		// Get/Setters for Report fields + Data
		void Fields(const ReportFields);
		ReportFields Fields();
		const ReportFields& Fields() const;

		void ClearPayload();

		template <typename T>
		void Payload(const T& t);

		template <typename T>
		T Payload() const;

		// Stream operator overload to simplify debugging
		friend std::ostream& operator<< (std::ostream& stream, const IOReport&);

	protected:
		ReportFields m_fields;
		std::vector<std::uint8_t> m_payload;
	private:
		template <typename T>
		void PayloadImpl(const T& t, T*);

		template <typename T>
		void PayloadImpl(const std::vector<T>& t, std::vector<T>*);

		template <typename T>
		T PayloadImpl(T*) const;

		template <typename T>
		std::vector<T> PayloadImpl(std::vector<T>*) const;
	};

}

#endif
