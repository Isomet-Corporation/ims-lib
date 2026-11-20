/*-----------------------------------------------------------------------------
/ Title      : Connection Manager Interface Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/h/IConnectionManager.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2023-11-24 08:01:48 +0000 (Fri, 24 Nov 2023) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 589 $
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

#include "IConnectionSettings.h"
#include "IMSSystem.h"
#include "MessageEvent.h"
#include "Message.h"
#include "boost/container/deque.hpp"
#include "Containers.h"

#include <list>
#include <memory>
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
		virtual std::vector<std::shared_ptr<IMSSystem>> Discover(const ListBase<std::string>& PortMask, std::shared_ptr<IConnectionSettings>) = 0;

		// Connect to a device
		virtual void Connect(const std::string&) = 0;

		// Disconnect from a device
		virtual void Disconnect() = 0;

		// Set Connection Timeouts
		virtual void SetTimeouts(int send_timeout_ms, int rx_timeout_ms, int free_timeout_ms, int discover_timeout_ms) = 0;

		// When enabled, the sender waits for each message to complete (response, error or
		//     timeout) before transmitting the next, so only one request is ever in flight.
		//     Only honoured by connection types that require it (currently CM_ETH, which otherwise
		//     chooses per device).  An explicit setting overrides the automatic choice.
		virtual void SetMessageSerialisation(bool enable) = 0;

		// Message protocol selection (09-26-015-SPEC-001).  V1 (the default) always uses the v1
		//     protocol.  AUTO probes the device's protocol info block when a system is initialised
		//     and uses v2 if the device and the connection type support it.
		enum class ProtocolPreference { V1, AUTO };
		virtual void SetProtocolPreference(ProtocolPreference pref) = 0;

		// Choose the protocol for the current connection according to the preference.  Called
		//     by IMSSystem::Initialise() before any other traffic; uses v1 requests only.
		virtual void NegotiateProtocol() = 0;

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
