/*-----------------------------------------------------------------------------
/ Title      : Event Handler default Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/EventManager/src/IEventHandler.cpp $
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

#include "IEventHandler.h"
#include <vector>
#include <cstdint>

namespace iMS {

	IEventHandler::IEventHandler()
	{
		// Unique ID integer for each new instance
		mID = mIDCount++;
	}

	int IEventHandler::mIDCount = 1;

	bool IEventHandler::operator == (const IEventHandler e)
	{
		return (mID == e.mID);
	}

}
