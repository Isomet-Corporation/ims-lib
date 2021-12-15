/*-----------------------------------------------------------------------------
/ Title      : Bulk Verifier CPP
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/src/BulkVerifier.cpp $
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

#include "BulkVerifier.h"
#include "Message.h"
#include "IConnectionManager.h"
#include "IEventHandler.h"

#include <deque>
#include <thread>
#include <condition_variable>

namespace iMS {

	class BulkVerifierEventTrigger :
		public IEventTrigger
	{
	public:
		BulkVerifierEventTrigger() { updateCount(BulkVerifierEvents::Count); }
		~BulkVerifierEventTrigger() {};
	};

	class BulkVerifier::Impl
	{
	public:
		Impl(const IMSSystem&);
		~Impl();

		const IMSSystem& myiMS;
		bool verifierRunning{ false };
		BulkVerifierEventTrigger m_Event;

		mutable std::mutex m_vfymutex;
		std::list <std::shared_ptr<VerifyChunk>> vfy_list;
		MessageHandle vfy_final;
		std::deque<int> error_list;

		class ResponseReceiver : public IEventHandler
		{
		public:
			ResponseReceiver(BulkVerifier::Impl* bv) : m_parent(bv) {};
			void EventAction(void* sender, const int message, const int param);
		private:
			BulkVerifier::Impl* m_parent;
		};
		ResponseReceiver* Receiver;

		std::thread rxVfyThread;
		mutable std::mutex m_rxmutex;
		std::condition_variable m_rxcond;
		std::deque<int> rxok_list;
		std::deque<int> rxerr_list;
		void RxWorker();
	};

	BulkVerifier::Impl::Impl(const IMSSystem& iMS) : myiMS(iMS), Receiver(new ResponseReceiver(this))	
	{
		// Start a thread to receive the verify readback data and process it
		verifierRunning = true;
		rxVfyThread = std::thread(&BulkVerifier::Impl::RxWorker, this);

		// Subscribe listener
		IConnectionManager * const myiMSConn = myiMS.Connection();
		myiMSConn->MessageEventSubscribe(MessageEvents::SEND_ERROR, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::TIMED_OUT_ON_SEND, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_RECEIVED, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_VALID, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_TIMED_OUT, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_CRC, Receiver);
		myiMSConn->MessageEventSubscribe(MessageEvents::RESPONSE_ERROR_INVALID, Receiver);
	}

	BulkVerifier::Impl::~Impl() { 
		// Unblock worker thread
		verifierRunning = false;
		m_rxcond.notify_one();
		rxVfyThread.join();

		// Unsubscribe listener
		IConnectionManager * const myiMSConn = myiMS.Connection();
		myiMSConn->MessageEventUnsubscribe(MessageEvents::SEND_ERROR, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::TIMED_OUT_ON_SEND, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_RECEIVED, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_ERROR_VALID, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_TIMED_OUT, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_ERROR_CRC, Receiver);
		myiMSConn->MessageEventUnsubscribe(MessageEvents::RESPONSE_ERROR_INVALID, Receiver);

		delete Receiver;
	}

	BulkVerifier::BulkVerifier(const IMSSystem& iMS) : myiMS(iMS), p_Impl(new Impl(iMS)) {}


	BulkVerifier::~BulkVerifier() { delete p_Impl; }

	void BulkVerifier::Impl::ResponseReceiver::EventAction(void* sender, const int message, const int param)
	{
		//if (vfy_list.empty()) return;
		switch (message)
		{
		case (MessageEvents::RESPONSE_RECEIVED) :
		case (MessageEvents::RESPONSE_ERROR_VALID) : {

			// Add response to verify list for checking by rx processing thread
			{
				std::unique_lock<std::mutex> lck{ m_parent->m_rxmutex };
				m_parent->rxok_list.push_back(param);
				m_parent->m_rxcond.notify_one();
				lck.unlock();
			}
			break;
		}
		case (MessageEvents::TIMED_OUT_ON_SEND) :
		case (MessageEvents::SEND_ERROR) :
		case (MessageEvents::RESPONSE_TIMED_OUT) :
		case (MessageEvents::RESPONSE_ERROR_CRC) :
		case (MessageEvents::RESPONSE_ERROR_INVALID) : {

			// Add error to list and trigger processing thread if handle exists
			{
				std::unique_lock<std::mutex> lck{ m_parent->m_rxmutex };
				m_parent->rxerr_list.push_back(param);
				m_parent->m_rxcond.notify_one();
				lck.unlock();
			}
			break;
		}
		}
	}

	void BulkVerifier::AddChunk(const std::shared_ptr<VerifyChunk> chunk)
	{
		std::unique_lock<std::mutex> vfylck{ p_Impl->m_vfymutex };

		p_Impl->vfy_final = NullMessage;
		p_Impl->vfy_list.push_back(chunk);

		vfylck.unlock();
	}

	void BulkVerifier::WaitUntilBufferClear()
	{
		// Clear Buffer to prevent Hardware overrun
		do {
			{
				std::unique_lock<std::mutex> vfylck{ p_Impl->m_vfymutex };
				if (p_Impl->vfy_list.empty()) break;
				vfylck.unlock();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		} while (1);
	}

	void BulkVerifier::Finalize(){
		std::unique_lock<std::mutex> vfylck{ p_Impl->m_vfymutex };

		if (!p_Impl->vfy_list.empty()) {
			p_Impl->vfy_final = p_Impl->vfy_list.back()->handle();
		}
		else p_Impl->vfy_final = NullMessage;

		vfylck.unlock();
	}

	void BulkVerifier::VerifyReset()
	{
		std::unique_lock<std::mutex> lck{ p_Impl->m_rxmutex };

		p_Impl->vfy_list.clear();
		p_Impl->error_list.clear();
		p_Impl->rxok_list.clear();
		p_Impl->rxerr_list.clear();

		lck.unlock();
	}

	bool BulkVerifier::VerifyInProgress() const
	{
		std::unique_lock<std::mutex> vfylck{ p_Impl->m_vfymutex };
		bool verify = !p_Impl->vfy_list.empty();
		vfylck.unlock();
		return (verify);
	}

	int BulkVerifier::GetVerifyError()
	{
		if (p_Impl->error_list.empty()) return -1;
		else {
			int i = p_Impl->error_list.front();
			p_Impl->error_list.pop_front();
			return (i);
		}
	}

	int BulkVerifier::Errors() const
	{
		return  static_cast<int>(p_Impl->error_list.size());
	}
	void BulkVerifier::BulkVerifierEventSubscribe(const int message, IEventHandler* handler)
	{
		p_Impl->m_Event.Subscribe(message, handler);
	}

	void BulkVerifier::BulkVerifierEventUnsubscribe(const int message, const IEventHandler* handler)
	{
		p_Impl->m_Event.Unsubscribe(message, handler);
	}

	// Image Readback Verify Data Processing Thread
	void BulkVerifier::Impl::RxWorker()
	{
		std::unique_lock<std::mutex> lck{ m_rxmutex };
		while (verifierRunning) {
			// Release lock implicitly, wait for next download trigger
			m_rxcond.wait(lck);

			// Allow thread to terminate 
			if (!verifierRunning) break;

			while (!rxok_list.empty())
			{
				int param = rxok_list.front();
				rxok_list.pop_front();

				for (std::list<std::shared_ptr<VerifyChunk>>::iterator iter = vfy_list.begin();
					iter != vfy_list.end();)
				{
					std::shared_ptr<VerifyChunk> chunk = static_cast<std::shared_ptr<VerifyChunk>>(*iter);
					if (chunk->handle() != (param))
					{
						++iter;
						continue;
					}

					IConnectionManager* myiMSConn = myiMS.Connection();

					IOReport resp = myiMSConn->Response(param);
					if (!chunk->match(resp.Payload<std::vector<std::uint8_t>>()))
					{
						// Verify Error
						error_list.push_back(chunk->addr());
					}
					else {
						// Verify Match
					}
					// Remove from list
					std::unique_lock<std::mutex> vfylck{ m_vfymutex };
					iter = vfy_list.erase(iter);

					// Verify Finished?
					//if (vfy_list.empty())
					if (vfy_final == param)
					{
						if (error_list.empty())
						{
							// Verify Success
							m_Event.Trigger<int>((void *)this, BulkVerifierEvents::VERIFY_SUCCESS, 0);
						}
						else
						{
							// Verify Fail
							m_Event.Trigger<int>((void *)this, BulkVerifierEvents::VERIFY_FAIL, static_cast<const int>(error_list.size()));
						}
					}
					vfylck.unlock();
				}
			}

			while (!rxerr_list.empty())
			{
				int param = rxerr_list.front();
				rxerr_list.pop_front();

				for (std::list<std::shared_ptr<VerifyChunk>>::iterator iter = vfy_list.begin();
					iter != vfy_list.end();)
				{
					std::shared_ptr<VerifyChunk> chunk = static_cast<std::shared_ptr<VerifyChunk>>(*iter);
					if (chunk->handle() != (param))
					{
						++iter;
						continue;
					}

					// Implicit Verify Error (caused by timeout or similar)
					error_list.push_back(chunk->addr());

					// Remove from list
					std::unique_lock<std::mutex> vfylck{ m_vfymutex };
					iter = vfy_list.erase(iter);

					// Verify Finished?
					if (vfy_list.empty())
					{
						// Verify Fail
						m_Event.Trigger<int>((void *)this, BulkVerifierEvents::VERIFY_FAIL, static_cast<const int>(error_list.size()));
					}
					vfylck.unlock();
				}
			}

		}
	}

}
