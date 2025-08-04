/*-----------------------------------------------------------------------------
/ Title      : Private Util Functions Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/h/PrivateUtil.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2021-12-15 10:49:10 +0000 (Wed, 15 Dec 2021) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 516 $
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

#ifndef IMS_PRIVATEUTIL_H__
#define IMS_PRIVATEUTIL_H__

#include <array>

extern "C" {
#include "sqlite3.h"
}

#include <boost/log/common.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/sources/record_ostream.hpp>

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace sev = logging::trivial;


namespace iMS {

	// Debug Logger
//	extern boost::log::sources::severity_logger_mt< logging::trivial::severity_level > lg;
	BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(lg, boost::log::sources::severity_logger_mt< logging::trivial::severity_level >);

	template <typename T>
	std::vector<std::uint8_t> VarToBytes(const T &data);

	template <typename T>
	T BytesToVar(const std::vector<std::uint8_t> &input_bytes);

	template <class container, typename T>
	void AppendVarToContainer(container& iter, const T& data)
	{
		auto vec = VarToBytes<T>(data);
		std::move(std::begin(vec), std::end(vec), std::back_inserter(iter));
	}

	template <typename T>
	T PCharToUInt(const char * const c);

	template <typename T>
	void UIntToPChar(char * const c, const T& t);

	std::string UUIDToStr(const std::array<std::uint8_t, 16>& arr);
	std::array<std::uint8_t, 16> StrToUUID(const std::string& str);

	sqlite3* get_db();

    bool float_compare(double a, double b, double epsilon = 1e-6);

}
#endif
