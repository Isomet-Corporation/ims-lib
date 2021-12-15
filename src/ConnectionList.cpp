/*-----------------------------------------------------------------------------
/ Title      : Connection List Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/src/ConnectionList.cpp $
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

#include "IConnectionManager.h"
#include "ConnectionList.h"
#if defined (_WIN32)
#include "CM_FTDI.h"
#include "CM_CYUSB.h"
#include "CM_RS422.h"
#endif
#if defined (_WIN32) || defined (__QNXNTO__)
#include "CM_ENET.h"
#endif

#include "PrivateUtil.h"  // for logging

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/log/utility/setup/from_settings.hpp>
#include <boost/log/utility/setup/settings_parser.hpp>
#include "boost/filesystem.hpp"

#if defined(_WIN32)
#include "Shlobj.h"
#endif

#ifdef _DEBUG
#include "crtdbg.h"
#define DEBUG_NEW new( _NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif

#ifdef _WIN32
#include "resource.h"
#include "windows.h"
#else
extern "C" {
#include "settings_ini.h"
}
#endif

// Include symbol for the current memory address of the executing module (DLL or EXE)
// see: http://www.codeguru.com/cpp/w-p/dll/tips/article.php/c3635/Tip-Detecting-a-HMODULEHINSTANCE-Handle-Within-the-Module-Youre-Running-In.htm
#if _MSC_VER >= 1300    // for VC 7.0
// from ATL 7.0 sources
#ifndef _delayimp_h
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif
#endif

namespace pt = boost::property_tree;

static void init_logging()
{
/*	auto log_path = boost::filesystem::temp_directory_path();
	log_path += "/imslog_%5N.log";
	logging::add_file_log
	(
		keywords::file_name = log_path,
		keywords::rotation_size = 10 * 1024 * 1024,
		keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
		keywords::format = "[%TimeStamp%]: (%Severity%) %Message%",
		keywords::open_mode = (std::ios::out | std::ios::app)
	);

	logging::core::get()->set_filter
	(
		logging::trivial::severity >= logging::trivial::info
	);*/

	// Look for a configuration file and read logging settings
	boost::filesystem::path settings_path;
#if defined(_WIN32)
	LPWSTR wszPath = NULL;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, NULL, &wszPath)))
	{
		settings_path = wszPath;
	}
#endif
	if (!boost::filesystem::exists(settings_path += L"\\Isomet")) {
		boost::filesystem::create_directory(settings_path);
	}
	if (!boost::filesystem::exists(settings_path += L"\\iMS_SDK")) {
		boost::filesystem::create_directory(settings_path);
	}
	
	auto old_settings = settings_path;
	old_settings += L"\\settings.ini";
	if (boost::filesystem::exists(old_settings)) {
		boost::filesystem::remove(old_settings);
	}
	
	// Changed this as of SDK v1.8.4 so any new installs will pick up the new settings (with asynchronous = true, auto flush = false)
	if (!boost::filesystem::exists(settings_path += L"\\logging.ini")) {
			// Write a default configuration file
#ifdef _WIN32
		HMODULE hModule = reinterpret_cast<HMODULE>(&__ImageBase);

		HRSRC hr = ::FindResource(hModule, MAKEINTRESOURCE(IDR_SETTINGS1), L"SETTINGS");

		if (hr)
		{
			HGLOBAL hg = ::LoadResource(hModule, hr);

			if (hg)
			{
				LPVOID pLockedResource = LockResource(hg);

				if (pLockedResource)
				{
					DWORD dwSize = ::SizeofResource(hModule, hr);
					if (0 != dwSize)
					{
						std::ofstream ofs(settings_path.string(), std::ios::binary);
						ofs.write((const char *)pLockedResource, dwSize);
					}
#else
				{
					{

						pBuffer = (void *)imshw_db;
						unsigned int dwSize = imshw_db_len;
#endif
				}
			}
		}
	}
	
	{
		boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");
		boost::log::register_simple_filter_factory<boost::log::trivial::severity_level, char>("Severity");
		boost::log::add_common_attributes();

		// Open the settings file
		try {
			std::ifstream file(settings_path.string());
			auto settings = logging::parse_settings(file);
			boost::property_tree::ptree pt = settings.property_tree();
			auto log_path = boost::filesystem::temp_directory_path();
			namespace bfs = boost::filesystem;
			bfs::path slash("/");
			std::string logfilename = log_path.string() + slash.make_preferred().string() 
				+ bfs::path("imslog_%5N.log").string();
			if (boost::optional<std::string> fn = pt.get_optional<std::string>("Sinks.LogFile.FileName"))
			{
				logfilename = *fn;
				if ((fn->find('\\') == std::string::npos) && (fn->find('/') == std::string::npos))
				{
					// User didn't specify a path. Use temp folder
					logfilename = log_path.string() + slash.make_preferred().string() + logfilename;
				}
			}
			pt.put<std::string>("Sinks.LogFile.FileName", logfilename);
			settings.property_tree() = pt;
			logging::init_from_settings(settings);
			//logging::init_from_stream(file);

			BOOST_LOG_SEV(iMS::lg::get(), sev::info) << std::string(" << Start Logging >>");
		}
		catch (std::exception&)
		{
			logging::core::get()->set_logging_enabled(false);
		}

	}
}

static void stop_logging()
{
	BOOST_LOG_SEV(iMS::lg::get(), sev::info) << std::string(" << Stop Logging >>");

	auto core = logging::core::get();
	
	core->set_logging_enabled(false);
	core->flush();
	core->remove_all_sinks();
}

namespace iMS
{
	class ConnectionList::Impl
	{
	public:
		Impl();
		~Impl();

		// The list of connection types
		typedef std::list<IConnectionManager*> ConnectionTypesList;
		ConnectionTypesList* connList;
		ListBase<std::string> ModuleNames;

		typedef ConnectionTypesList::iterator iterator;
		typedef ConnectionTypesList::const_iterator const_iterator;
		ConnectionTypesList::iterator begin() 	{ return connList->begin(); }
		ConnectionTypesList::iterator end()   	{ return connList->end(); }

		typedef std::map<std::string, ConnectionConfig> ConnectionConfigMap;
		ConnectionConfigMap* config_map;
	};

	ConnectionList::Impl::Impl()
	{
		connList = new ConnectionTypesList;
		config_map = new ConnectionList::Impl::ConnectionConfigMap;

		init_logging();
		logging::add_common_attributes();

		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("ConnectionList::ConnectionList()");
	}

	ConnectionList::Impl::~Impl()
	{
		BOOST_LOG_SEV(lg::get(), sev::trace) << std::string("ConnectionList::~ConnectionList()");

		for (ConnectionList::Impl::const_iterator iter = connList->begin();
			iter != connList->end();
			iter++)
		{
			IConnectionManager *object = *iter;
			delete object;
		}
		delete connList;
		delete config_map;

		stop_logging();
	}

	ConnectionList::ConnectionConfig::ConnectionConfig() 
		: IncludeInScan(true)
	{}

	ConnectionList::ConnectionConfig::ConnectionConfig(bool inc)
		: IncludeInScan(inc)
	{}

	ConnectionList::ConnectionConfig::ConnectionConfig(const ListBase<std::string>& mask)
		: IncludeInScan(true), PortMask(mask)
	{}

	ConnectionList::ConnectionList() : pImpl(new Impl())
	{
		IConnectionManager* module;
		// connList is a read-only list containing one element for each host connection type.
		// Each element must fully implement the IConnectionManager interface to define methods
		//   for discovering, connecting to and sending messages to/from the iMS.
		// Add in any additional concrete clases that are an implementation of the IConnectionManager interface below
#if defined (_WIN32) 
		try {
			module = new CM_FTDI();
			pImpl->connList->push_back(module);
		}
		catch (std::exception ex) {
			BOOST_LOG_SEV(lg::get(), sev::warning) << "Failed to initilise CM_FTDI Module" << std::endl;
		}
#endif
#if defined (_WIN32) 
		try {
			module = new CM_CYUSB();
			pImpl->connList->push_back(module);
		}
		catch (std::exception ex) {
			BOOST_LOG_SEV(lg::get(), sev::warning) << "Failed to initilise CM_CYUSB Module" << std::endl;
		}
#endif
#if defined (_WIN32) 
		try {
			module = new CM_RS422();
			pImpl->connList->push_back(module);
		}
		catch (std::exception ex) {
			BOOST_LOG_SEV(lg::get(), sev::warning) << "Failed to initilise CM_RS422 Module" << std::endl;
		}
#endif
#if defined (_WIN32) || defined (__QNXNTO__)
		try {
			module = new CM_ENET();
			pImpl->connList->push_back(module);
		}
		catch (std::exception ex) {
			BOOST_LOG_SEV(lg::get(), sev::warning) << "Failed to initilise CM_ENET Module" << std::endl;
		}
#endif

		for (ConnectionList::Impl::const_iterator iter = pImpl->begin();
			iter != pImpl->end();
			iter++)
		{
			pImpl->config_map->emplace(pImpl->connList->back()->Ident(), ConnectionConfig());
			pImpl->ModuleNames.push_back(pImpl->connList->back()->Ident());
		}
	}

	ConnectionList::~ConnectionList()
	{
		delete pImpl;
	}

	ConnectionList::ConnectionConfig& ConnectionList::config(const std::string& module)
	{
		return (*pImpl->config_map)[module];
	}

	const ListBase<std::string>& ConnectionList::modules() const
	{
		return pImpl->ModuleNames;
	}

	std::vector<IMSSystem> ConnectionList::scan()
	{
		std::vector<IMSSystem> fulliMSList;
		for (ConnectionList::Impl::const_iterator iter = pImpl->begin();
			iter != pImpl->end();
			iter++)
		{
			IConnectionManager *object = *iter;

			if ((*pImpl->config_map)[object->Ident()].IncludeInScan) {
				std::vector<IMSSystem> newiMSList = object->Discover((*pImpl->config_map)[object->Ident()].PortMask);

				// Add newly found iMS's to the full list of iMS's
				fulliMSList.insert(fulliMSList.end(), newiMSList.begin(), newiMSList.end());
			}
		}
		return fulliMSList;
	}

}
