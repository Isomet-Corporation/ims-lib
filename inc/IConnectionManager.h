/*-----------------------------------------------------------------------------
/ Title      : Connection Manager Interface Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/h/IConnectionManager.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2021-09-13 10:43:17 +0100 (Mon, 13 Sep 2021) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 501 $
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

#ifndef IMS_CONNECTION_MANAGER_H__
#define IMS_CONNECTION_MANAGER_H__

#include "IMSSystem.h"
#include "MessageEvent.h"
#include "Message.h"
#include "boost/container/deque.hpp"
#include "Containers.h"

#include <list>
#include <array>

namespace iMS
{
	// This is the interface between user code and the underlying 
	//	functions that communicate with the hardware
	class IConnectionManager
	{
	public:
		// override default constructor
		virtual ~IConnectionManager() {}

		// A human readable name identifier for the Connection Type
		virtual const std::string& Ident() const = 0;

		// Iterate through each of the available ports on the interface looking for 
		//	   possible connections, probe each candidate to see if it responds with a  
		//	   recognisable identity and return a list of the discovered devices to the user
		virtual std::vector<IMSSystem> Discover(const ListBase<std::string>& PortMask) = 0;

		// Connect to a device
		virtual void Connect(const std::string&) = 0;

		// Disconnect from a device
		virtual void Disconnect() = 0;

		// Send an I/O Report
		virtual MessageHandle SendMsg(HostReport const& Rpt) = 0;
		virtual DeviceReport SendMsgBlocking(HostReport const& Rpt) = 0;

		// Get an I/O Response to obtain data from the system
		virtual const DeviceReport Response(const MessageHandle) const = 0;

		// Transfer a block of data to the system memory
		virtual bool MemoryDownload(boost::container::deque<std::uint8_t>& arr, std::uint32_t start_addr, int image_index, const std::array<std::uint8_t, 16>& uuid) = 0;

		// Transfer a block of data from the system memory
		virtual bool MemoryUpload(boost::container::deque<std::uint8_t>& arr, std::uint32_t start_addr, int len, int image_index, const std::array<std::uint8_t, 16>& uuid) = 0;

		// Get current status of Memory transfer
		virtual int MemoryProgress() = 0;

		// True if there is an open connection to the device
		virtual const bool& Open() const = 0;

		// Sign up to Message Events
		virtual void MessageEventSubscribe(const int message, IEventHandler* handler) = 0;

		// Disregard any future events
		virtual void MessageEventUnsubscribe(const int message, const IEventHandler* handler) = 0;
	};

}

#endif // CONNECTION_MANAGER_H
