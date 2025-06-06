/*-----------------------------------------------------------------------------
/ Title      : Library Version Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/src/LibVersion.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2024-10-29 15:20:37 +0000 (Tue, 29 Oct 2024) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 614 $
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

#include "LibVersion.h"
#include <sstream>
#include <string>
#include <set>

namespace iMS {

	int LibVersion::GetMajor()
	{
		return int(IMS_API_MAJOR);
	}

	int LibVersion::GetMinor()
	{
		return int(IMS_API_MINOR);
	}

	int LibVersion::GetPatch()
	{
		return int(IMS_API_PATCH);
	}

	std::string LibVersion::GetVersion()
	{
		static std::string Version("");

		if (Version.empty())
		{
			// cache the LibVersion string
			std::ostringstream stream;
			stream << IMS_API_MAJOR << "."
				<< IMS_API_MINOR << "."
				<< IMS_API_PATCH;
			Version = stream.str();
		}

		return Version;
	}

	bool LibVersion::IsAtLeast(int major, int minor, int patch)
	{
		if (IMS_API_MAJOR < major) return false;
		if (IMS_API_MAJOR > major) return true;
		if (IMS_API_MINOR < minor) return false;
		if (IMS_API_MINOR > minor) return true;
		if (IMS_API_PATCH < patch) return false;
		return true;
	}

	bool LibVersion::HasFeature(const std::string &name)
	{
		static std::set<std::string> features;

		if (features.empty())
		{
			// cache the feature list
			//features.insert("FAST_API");
			//features.insert("THREADSAFE");
			features.insert("IMAGE_UUID");
			features.insert("CYUSB");
			features.insert("FILESYSTEM");
			features.insert("TONEBUFFER");
			features.insert("DIAGNOSTICS");
			features.insert("AUXILIARY");
			features.insert("STARTUP");
			features.insert("EXTERNAL_UPDATE");
			features.insert("LARGE_MEMORY");
			features.insert("SEQUENCES");
			features.insert("IMAGEGROUP");
			features.insert("IMAGEPROJECT");
			features.insert("COMPENSATION_FUNCTION");
			features.insert("VELOCITY_COMPENSATION");
			features.insert("ETHERNET");
			features.insert("RS422");
			features.insert("FAN_CONTROL");
			features.insert("SDOR_DELAY");
			features.insert("SDOR_PULSE");
			features.insert("REF_CLOCK");
			features.insert("ENHANCED_TONE");
			features.insert("FX2");
			features.insert("CHANNEL_COMPENSATION");
			features.insert("FAST_SEQUENCE_DOWNLOAD");
			features.insert("CLOCK_GEN");
		}

		return features.find(name) != features.end();
	}

}
