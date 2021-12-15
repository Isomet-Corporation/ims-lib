/*-----------------------------------------------------------------------------
/ Title      : Connection Manager Header Implementation for FTDI Connections
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/h/CM_FTDI.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2019-03-08 23:22:46 +0000 (Fri, 08 Mar 2019) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 386 $
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

#ifndef IMS_CM_FTDI_H__
#define IMS_CM_FTDI_H__

#if defined(_WIN32)

//------------------------------------------------------------------------------
// include 3rd party Device Driver libraries
//
#include "ftd2xx.h"

#include "CM_Common.h"

namespace iMS
{
	// This is a concrete class that implements ConnectionManager via CM_Common.
	//	It is used to communicate with underlying hardware that uses the FTDI
	//	Device Driver
	class CM_FTDI : public CM_Common
	{
	public:
		CM_FTDI();
		~CM_FTDI();

		const std::string& Ident() const;

		// Implement ConnectionManager Interface
		std::vector<IMSSystem> Discover(const ListBase<std::string>& PortMask);
		void Connect(const std::string&);
		void Disconnect();
		bool MemoryDownload(boost::container::deque<std::uint8_t>& arr, std::uint32_t start_addr, int image_index, const std::array<std::uint8_t, 16>& uuid);
		bool MemoryUpload(boost::container::deque<std::uint8_t>& arr, std::uint32_t start_addr, int len, int image_index, const std::array<std::uint8_t, 16>& uuid);

		// Declare a class that stores information pertaining to the status of any fast memory transfers
		class FastTransfer;

	private:
		// Make this object non-copyable
		CM_FTDI(const CM_FTDI &);
		const CM_FTDI &operator =(const CM_FTDI &);

		class Impl;
		Impl * p_Impl;

		void MessageSender();
		void ResponseReceiver();
		void MemoryTransfer();
		void InterruptReceiver();
	};
}

#endif

#endif
