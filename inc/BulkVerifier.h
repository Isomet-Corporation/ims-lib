/*-----------------------------------------------------------------------------
/ Title      : Bulk Verifier Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/h/BulkVerifier.h $
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

#ifndef IMS_BULKVERIFIER_H__
#define IMS_BULKVERIFIER_H__

#include "IEventTrigger.h"
#include "IMSSystem.h"
#include "Message.h"

namespace iMS {
	class BulkVerifierEvents
	{
	public:
		enum Events {
			VERIFY_SUCCESS,
			VERIFY_FAIL,
			Count
		};
	};

	class VerifyChunk
	{
	public:
		VerifyChunk(MessageHandle handle, std::vector<std::uint8_t> data, int addr) : hMsg(handle), m_chunkData(data), m_addr(addr) {};
		~VerifyChunk() {};

		inline bool match(std::vector<std::uint8_t> rx) const { return (rx == m_chunkData); };
		inline const MessageHandle& handle() const { return hMsg; };
		inline const int& addr() const { return m_addr; };
	private:
		MessageHandle hMsg;
		std::vector<std::uint8_t> m_chunkData;
		int m_addr;
	};

	class BulkVerifier
	{
	public:
		BulkVerifier(const IMSSystem& iMS);
		~BulkVerifier();

		void AddChunk(const std::shared_ptr<VerifyChunk>);
		void WaitUntilBufferClear();
		void Finalize();
		void VerifyReset();
		bool VerifyInProgress() const;
		int GetVerifyError();
		int Errors() const;

		void BulkVerifierEventSubscribe(const int message, IEventHandler* handler);
		void BulkVerifierEventUnsubscribe(const int message, const IEventHandler* handler);
	private:
		// Make this object non-copyable
		BulkVerifier(const BulkVerifier &);
		const BulkVerifier &operator =(const BulkVerifier &);

		const IMSSystem& myiMS;

		class Impl;
		Impl * p_Impl;
	};

}

#endif
