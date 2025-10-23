/*-----------------------------------------------------------------------------
/ Title      : Event Trigger Interface Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/EventManager/h/IEventTrigger.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2020-11-20 16:06:38 +0000 (Fri, 20 Nov 2020) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 475 $
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

#ifndef IMS_EVENT_TRIGGER_H__
#define IMS_EVENT_TRIGGER_H__

#include <vector>
#include <map>
#include <cstdint>
#include <shared_mutex>

namespace iMS {

	class IEventHandler;

	class IEventTrigger
	{
	public:
		IEventTrigger();
		virtual ~IEventTrigger();

		void Subscribe(const int message, IEventHandler* handler);
		void Unsubscribe(const int message, const IEventHandler* handler);
		int Subscribers(const int message);

		template <typename T>
		void Trigger(void* sender, const int message, const T param);

		template <typename T, typename T2>
		void Trigger(void* sender, const int message, const T param, const T2 param2);

	protected:
		void updateCount(int count);
		typedef std::vector<IEventHandler*> EventHandlerList;
		typedef std::map<int, EventHandlerList*> EventHandlerMap;
		EventHandlerMap mMap;
		int messageCount;
		std::shared_mutex m_mapMutex;
	};

}

#endif
