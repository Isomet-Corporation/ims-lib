/*-----------------------------------------------------------------------------
/ Title      : EEPROM Operations Source
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/src/EEPROM.cpp $
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

#include "EEPROM.h"
#include "IConnectionManager.h"
#include "PrivateUtil.h"
#include "IEventTrigger.h"
#include "Message.h"

#include <thread>

namespace iMS {
	
	class EEPROMEventTrigger :
		public IEventTrigger
	{
	public:
		EEPROMEventTrigger() { updateCount(EEPROMEvents::Count); }
		~EEPROMEventTrigger() {};
	};

	class EEPROM::Impl
	{
	public:
		const std::uint16_t EEPROM_CACHE_SIZE = 1024;

		Impl(const IMSSystem&);
		~Impl();

		EEPROMEventTrigger m_Event;
		std::vector<std::uint8_t> m_EEData;
		std::list<MessageHandle> m_EEReadList;
		std::list<MessageHandle> m_EEWriteList;
		MessageHandle m_EEFinalRead;
		MessageHandle m_EEFinalWrite;
		bool m_AccessFailure;
		mutable std::mutex m_EEList_mutex;

		template <typename T>
		void EEPROMDataImpl(const T& t, T*);

		template <typename T>
		void EEPROMDataImpl(const std::vector<T>& t, std::vector<T>*);

		template <typename T>
		T EEPROMDataImpl(T*) const;

		template <typename T>
		std::vector<T> EEPROMDataImpl(std::vector<T>*) const;

		class ResponseReceiver : public IEventHandler
		{
		public:
			ResponseReceiver(EEPROM::Impl* sf) : m_parent(sf) {};
			void EventAction(void* sender, const int message, const int param);
		private:
			EEPROM::Impl* m_parent;
		};
		ResponseReceiver* Receiver;
	private:
		const IMSSystem& myiMS;
	};

	EEPROM::Impl::Impl(const IMSSystem& iMS) : m_EEFinalRead(NullMessage), m_EEFinalWrite(NullMessage), m_AccessFailure(false),
			Receiver(new ResponseReceiver(this)), myiMS(iMS)
	{
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

	EEPROM::Impl::~Impl()
	{
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

	EEPROM::EEPROM(const IMSSystem& iMS) : p_Impl(new Impl(iMS)), myiMS(iMS) {}

	EEPROM::~EEPROM() { delete p_Impl; p_Impl = nullptr; }

	template <typename T>
	void EEPROM::EEPROMData(const T& t)
	{
		// static_cast is used to select correct overload
		p_Impl->EEPROMDataImpl(t, static_cast<T*>(0));
	}

	template <typename T>
	void EEPROM::Impl::EEPROMDataImpl(const T& t, T*)
	{
		m_EEData = VarToBytes<T>(t);
	}

	// Overloaded operators for vector type
	template <typename T>
	void EEPROM::Impl::EEPROMDataImpl(const std::vector<T>& t, std::vector<T>*)
	{
		m_EEData.clear();
		for (typename std::vector<T>::const_iterator it = t.cbegin(); it != t.cend(); ++it)
		{
			const std::vector<std::uint8_t> c = VarToBytes<T>(*it);
			m_EEData.insert(m_EEData.end(), c.cbegin(), c.cend());
		}
	}

	template <>
	void EEPROM::Impl::EEPROMDataImpl<std::uint8_t>(const std::vector<std::uint8_t>& t, std::vector<std::uint8_t>*)
	{
		m_EEData = t;
	}

	// Template Specialisation for string type
	template <>
	void EEPROM::Impl::EEPROMDataImpl<std::string>(const std::string& t, std::string*)
	{
		const std::uint8_t* buffer = reinterpret_cast<const std::uint8_t*>(t.c_str());
		std::vector<std::uint8_t> cnv(buffer, buffer + strlen(t.c_str()));
		cnv.push_back('\0');
		m_EEData = cnv;
	}

	template <typename T>
	T EEPROM::EEPROMData() const {
		// static_cast is used to select correct overload
		return (p_Impl->EEPROMDataImpl(static_cast<T*>(0)));
	}

	template <typename T>
	T EEPROM::Impl::EEPROMDataImpl(T*) const
	{
		return (BytesToVar<T>(m_EEData));
	}

	template <typename T>
	std::vector<T> EEPROM::Impl::EEPROMDataImpl(std::vector<T>*) const
	{
		std::vector<T> ret;
		ret.reserve(m_EEData.size());
		for (std::vector<std::uint8_t>::const_iterator it = m_EEData.cbegin(); it != m_EEData.cend();)
		{
			if ((m_EEData.cend() - it) >= (int)sizeof(T))
			{
				std::vector<std::uint8_t> cnv(it, it + (sizeof(T)));
				it += sizeof(T);
				ret.push_back(BytesToVar<T>(cnv));
			}
			else break;
		}
		return (ret);
	}

	template <>
	std::vector<std::uint8_t> EEPROM::Impl::EEPROMDataImpl<std::uint8_t>(std::vector<std::uint8_t>*) const
	{
		return m_EEData;
	}

	template <>
	std::string EEPROM::Impl::EEPROMDataImpl<std::string>(std::string*) const
	{
		std::size_t size = m_EEData.size();
		for (std::vector<std::uint8_t>::const_iterator it = m_EEData.cbegin(); it != m_EEData.cend(); ++it)
		{
			if ((*it) == '\0') {
				size = (it - m_EEData.cbegin());
				size += 1;
				break;
			}
		}
		std::string s(reinterpret_cast<const char *>(m_EEData.data()), size);
		return (s);
	}

	void EEPROM::Impl::ResponseReceiver::EventAction(void* sender, const int message, const int param)
	{
		switch (message)
		{
		case (MessageEvents::RESPONSE_RECEIVED) :
		case (MessageEvents::RESPONSE_ERROR_VALID) : {

			// Add response to verify list for checking by rx processing thread
			{
				if (!m_parent->m_EEReadList.empty()) {
					std::unique_lock<std::mutex> lck{ m_parent->m_EEList_mutex };
					if (!m_parent->m_EEReadList.empty() && (param == m_parent->m_EEReadList.front()))
					{
						m_parent->m_EEReadList.pop_front();
						IConnectionManager * const myiMSConn = m_parent->myiMS.Connection();
						std::vector<std::uint8_t> packet_data = myiMSConn->Response(param).Payload < std::vector<std::uint8_t> >();
						m_parent->m_EEData.insert(m_parent->m_EEData.cend(), packet_data.cbegin(), packet_data.cend());
						if (param == m_parent->m_EEFinalRead) {
							if (m_parent->m_AccessFailure) {
								m_parent->m_Event.Trigger<int>(this, EEPROMEvents::EEPROM_ACCESS_FAILED, param);
							}
							else {
								m_parent->m_Event.Trigger<int>(this, EEPROMEvents::EEPROM_READ_DONE, param);
							}
						}
					}
					lck.unlock();
				}
				if (!m_parent->m_EEWriteList.empty()) {
					std::unique_lock<std::mutex> lck{ m_parent->m_EEList_mutex };
					if (!m_parent->m_EEWriteList.empty() && (param == m_parent->m_EEWriteList.front()))
					{
						m_parent->m_EEWriteList.pop_front();
						if (param == m_parent->m_EEFinalWrite) {
							if (m_parent->m_AccessFailure) {
								m_parent->m_Event.Trigger<int>(this, EEPROMEvents::EEPROM_ACCESS_FAILED, param);
							}
							else {
								m_parent->m_Event.Trigger<int>(this, EEPROMEvents::EEPROM_WRITE_DONE, param);
							}
						}
					}
					lck.unlock();
				}
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
				if (!m_parent->m_EEReadList.empty()) {
					std::unique_lock<std::mutex> lck{ m_parent->m_EEList_mutex };
					if (!m_parent->m_EEReadList.empty() && (param == m_parent->m_EEReadList.front()))
					{
						m_parent->m_EEReadList.pop_front();
						//m_parent->m_EEReadList.clear();
						//m_parent->m_Event.Trigger<int>(this, EEPROMEvents::EEPROM_ACCESS_FAILED, param);
						m_parent->m_AccessFailure = true;
						if (param == m_parent->m_EEFinalRead) m_parent->m_Event.Trigger<int>(this, EEPROMEvents::EEPROM_ACCESS_FAILED, param);
					}
					lck.unlock();
				}
				if (!m_parent->m_EEWriteList.empty()) {
					std::unique_lock<std::mutex> lck{ m_parent->m_EEList_mutex };
					if (!m_parent->m_EEWriteList.empty() && (param == m_parent->m_EEWriteList.front()))
					{
						m_parent->m_EEWriteList.pop_front();
						//m_parent->m_Event.Trigger<int>(this, EEPROMEvents::EEPROM_ACCESS_FAILED, param);
						m_parent->m_AccessFailure = true;
						if (param == m_parent->m_EEFinalWrite) m_parent->m_Event.Trigger<int>(this, EEPROMEvents::EEPROM_ACCESS_FAILED, param);
					}
					lck.unlock();
				}
			}
			break;
		}
		}
	}

	int EEPROM::ReadEEPROM(TARGET ee, unsigned int addr, std::size_t size)
	{
		MessageHandle h;
		if (!myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = myiMS.Connection();

		HostReport *iorpt;

		// Reset buffer
		p_Impl->m_EEData.clear();
		p_Impl->m_EEReadList.clear();
		p_Impl->m_EEFinalRead = NullMessage;
		p_Impl->m_AccessFailure = false;

		HostReport::Actions actions;
		switch (ee)
		{
		case TARGET::SYNTH: actions = HostReport::Actions::SYNTH_EEPROM;  break;
		case TARGET::AO_DEVICE: actions = HostReport::Actions::AOD_EEPROM; break;
		case TARGET::RF_AMPLIFIER: actions = HostReport::Actions::RFA_EEPROM; break;
		default: return -1;
		}
		iorpt = new HostReport(actions, HostReport::Dir::READ, static_cast<std::uint16_t>(addr & 0xFFFF));
		int transfer_size = 0;
		for (unsigned int i = 0; i < size; i += transfer_size)
		{
			ReportFields f = iorpt->Fields();
			if ((size - i) > iorpt->PAYLOAD_MAX_LENGTH)
			{
				f.len = iorpt->PAYLOAD_MAX_LENGTH;
			}
			else
			{
				f.len = (static_cast<std::uint16_t>(size)-i);
			}
			if (((addr + i) / p_Impl->EEPROM_CACHE_SIZE) != ((addr + i + f.len) / p_Impl->EEPROM_CACHE_SIZE)) {
				// beginning and end in different eeprom cache regions.  Shorten transfer to this region only
				f.len = (p_Impl->EEPROM_CACHE_SIZE - ((addr + i) % p_Impl->EEPROM_CACHE_SIZE));
			}
			f.addr = static_cast<std::uint16_t>((addr + i) & 0xFFFF);
			f.context = static_cast<std::uint8_t>(((addr + i) >> 16) & 0xFF);
			iorpt->Fields(f);
			h = myiMSConn->SendMsg(*iorpt);
			{
				std::unique_lock<std::mutex> lck{ p_Impl->m_EEList_mutex };
				p_Impl->m_EEReadList.push_back(h);
				lck.unlock();
			}
			if (!(transfer_size = f.len)) break;
		}
		delete iorpt;

		{
			std::unique_lock<std::mutex> lck{ p_Impl->m_EEList_mutex };
			p_Impl->m_EEFinalRead = p_Impl->m_EEReadList.back();
			lck.unlock();
		}
		return p_Impl->m_EEFinalRead;
	}

	bool EEPROM::WriteEEPROM(TARGET ee, unsigned int addr)
	{
		MessageHandle h;
		if (!myiMS.Synth().IsValid()) return false;

		IConnectionManager * const myiMSConn = myiMS.Connection();

		HostReport *iorpt;

		HostReport::Actions actions;
		switch (ee)
		{
			case TARGET::SYNTH: actions = HostReport::Actions::SYNTH_EEPROM;  break;
			case TARGET::AO_DEVICE: actions = HostReport::Actions::AOD_EEPROM; break;
			case TARGET::RF_AMPLIFIER: actions = HostReport::Actions::RFA_EEPROM; break;
			default: return false;
		}

		p_Impl->m_EEWriteList.clear();
		p_Impl->m_EEFinalWrite = NullMessage;
		p_Impl->m_AccessFailure = false;

		iorpt = new HostReport(actions, HostReport::Dir::WRITE, static_cast<std::uint16_t>(addr & 0xFFFF));
		int transfer_size = 0;
		for (unsigned int i = 0; i < p_Impl->m_EEData.size(); i += transfer_size)
		{
			iorpt->ClearPayload();
			ReportFields f = iorpt->Fields();
			f.addr = static_cast<std::uint16_t>((addr + i) & 0xFFFF);
			f.context = static_cast<std::uint8_t>(((addr + i) >> 16) & 0xFF);
			iorpt->Fields(f);
			std::vector<std::uint8_t>::const_iterator first = p_Impl->m_EEData.cbegin() + i;
			std::vector<std::uint8_t>::const_iterator last;
			if (std::distance(first, p_Impl->m_EEData.cend()) > (int)iorpt->PAYLOAD_MAX_LENGTH)
			{
				last = first + iorpt->PAYLOAD_MAX_LENGTH;
			}
			else
			{
				last = p_Impl->m_EEData.cend();
			}
			if (((addr + i) / p_Impl->EEPROM_CACHE_SIZE) != ((addr + i + std::distance(first, last)) / p_Impl->EEPROM_CACHE_SIZE)) {
				// beginning and end in different eeprom cache regions.  Shorten transfer to this region only
				last = first + (p_Impl->EEPROM_CACHE_SIZE - ((addr + i) % p_Impl->EEPROM_CACHE_SIZE));

				// And add a delay to allow for region change
				std::this_thread::sleep_for(std::chrono::milliseconds(50));

			}
			iorpt->Payload<std::vector<std::uint8_t>>(std::vector<std::uint8_t>(first, last));
			h = myiMSConn->SendMsg(*iorpt);
			{
				std::unique_lock<std::mutex> lck{ p_Impl->m_EEList_mutex };
				p_Impl->m_EEWriteList.push_back(h);
				lck.unlock();
			}
			if (!(transfer_size = std::distance(first, last))) break;
		}
		delete iorpt;

		p_Impl->m_EEFinalWrite = p_Impl->m_EEWriteList.back();
		return true;
	}

	void EEPROM::EEPROMEventSubscribe(const int message, IEventHandler* handler)
	{
		p_Impl->m_Event.Subscribe(message, handler);
	}

	void EEPROM::EEPROMEventUnsubscribe(const int message, const IEventHandler* handler)
	{
		p_Impl->m_Event.Unsubscribe(message, handler);
	}

	/// \cond TEMPLATE_INSTANTIATIONS
	// Declare all supported instantiations of EEPROM data templates
	 template void EEPROM::EEPROMData<std::uint8_t>(const std::uint8_t& t);
	 template void EEPROM::EEPROMData<std::uint16_t>(const std::uint16_t& t);
	 template void EEPROM::EEPROMData<std::uint32_t>(const std::uint32_t& t);
	 template void EEPROM::EEPROMData<std::int8_t>(const std::int8_t& t);
	 template void EEPROM::EEPROMData<std::int16_t>(const std::int16_t& t);
	 template void EEPROM::EEPROMData<int>(const int& t);
	 template void EEPROM::EEPROMData<double>(const double& t);
	 template void EEPROM::EEPROMData<float>(const float& t);
	 template void EEPROM::EEPROMData<char>(const char& t);

	 template void EEPROM::EEPROMData<std::vector<std::uint8_t>>(const std::vector< std::uint8_t>& t);
	 template void EEPROM::EEPROMData<std::vector<std::uint16_t>>(const std::vector< std::uint16_t>& t);
	 template void EEPROM::EEPROMData<std::vector<std::uint32_t>>(const std::vector< std::uint32_t>& t);
	 template void EEPROM::EEPROMData<std::vector<std::int8_t>>(const std::vector< std::int8_t>& t);
	 template void EEPROM::EEPROMData<std::vector<std::int16_t>>(const std::vector< std::int16_t>& t);
	 template void EEPROM::EEPROMData<std::vector<int>>(const std::vector< int>& t);
	 template void EEPROM::EEPROMData<std::vector<double>>(const std::vector< double>& t);
	 template void EEPROM::EEPROMData<std::vector<float>>(const std::vector< float>& t);
	 template void EEPROM::EEPROMData<std::vector<char>>(const std::vector< char>& t);

	 template void EEPROM::EEPROMData<std::string>(const std::string& t);

	 template std::uint8_t EEPROM::EEPROMData<std::uint8_t>() const;
	 template std::uint16_t EEPROM::EEPROMData<std::uint16_t>() const;
	 template std::uint32_t EEPROM::EEPROMData<std::uint32_t>() const;
	 template std::int8_t EEPROM::EEPROMData<std::int8_t>() const;
	 template std::int16_t EEPROM::EEPROMData<std::int16_t>() const;
	 template int EEPROM::EEPROMData<int>() const;
	 template double EEPROM::EEPROMData<double>() const;
	 template float EEPROM::EEPROMData<float>() const;
	 template char EEPROM::EEPROMData<char>() const;

	 template std::vector< std::uint8_t> EEPROM::EEPROMData<std::vector<std::uint8_t>>() const;
	 template std::vector< std::uint16_t> EEPROM::EEPROMData<std::vector<std::uint16_t>>() const;
	 template std::vector< std::uint32_t> EEPROM::EEPROMData<std::vector<std::uint32_t>>() const;
	 template std::vector< std::int8_t> EEPROM::EEPROMData<std::vector<std::int8_t>>() const;
	 template std::vector< std::int16_t> EEPROM::EEPROMData<std::vector<std::int16_t>>() const;
	 template std::vector< int> EEPROM::EEPROMData<std::vector<int>>() const;
	 template std::vector< double> EEPROM::EEPROMData<std::vector<double>>() const;
	 template std::vector< float> EEPROM::EEPROMData<std::vector<float>>() const;
	 template std::vector< char> EEPROM::EEPROMData<std::vector<char>>() const;

	 template std::string EEPROM::EEPROMData<std::string>() const;
	/// \endcond

}
