/*-----------------------------------------------------------------------------
/ Title      : Connection Manager Header Implementation for RS422 Connections
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/ConnectionManager/h/CM_RS422.h $
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

#ifndef IMS_CM_RS422_H__
#define IMS_CM_RS422_H__

#if defined(_WIN32)

#include "CM_Common.h"

namespace iMS
{
	// This is a concrete class that implements ConnectionManager via CM_Common.
	//	It is used to communicate with underlying hardware that uses the Cypress USB
	//	Device Driver
	class CM_RS422 : public CM_Common
	{
	public:
		CM_RS422();
		~CM_RS422();

		const std::string& Ident() const;

		// Implement ConnectionManager Interface
		std::vector<std::shared_ptr<IMSSystem>> Discover(const ListBase<std::string>& PortMask);
		void Connect(const std::string&);
		void Disconnect();
		void SetTimeouts(int send_timeout_ms = 1000, int rx_timeout_ms = 5000, int free_timeout_ms = 30000, int discover_timeout_ms = 2500);
//		bool MemoryDownload(boost::container::deque<std::uint8_t>& arr, std::uint32_t start_addr, int image_index, const std::array<std::uint8_t, 16>& uuid);
//		bool MemoryUpload(boost::container::deque<std::uint8_t>& arr, std::uint32_t start_addr, int len, int image_index, const std::array<std::uint8_t, 16>& uuid);

	private:
		// Make this object non-copyable
		CM_RS422(const CM_RS422 &);
		const CM_RS422 &operator =(const CM_RS422 &);

        // RS422 Fast Transfer Characteristics
        struct RS422_Policy {
            RS422_Policy(uint32_t addr, int index) : addr(addr), index(index) {}

            static constexpr int DL_TRANSFER_SIZE = 64;
            static constexpr int UL_TRANSFER_SIZE = 64;
            static constexpr int TRANSFER_UNIT    = 64;
            static constexpr long DMA_MAX_TRANSACTION_SIZE = 1024;

            uint32_t addr;
            int index;
        };
        using FastTransfer = CM_Common::FastTransfer<RS422_Policy>;

		class Impl;
		Impl *pImpl;

		void MessageSender();
		void ResponseReceiver();
	//	void MemoryTransfer();
		void InterruptReceiver();

	};

}

#endif

#endif
