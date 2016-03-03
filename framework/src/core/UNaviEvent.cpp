/*
 * UNaviEvent.cpp
 *
 *  Created on: 2014-9-25
 *      Author: dell
 */

#include "unavi/core/UNaviEvent.h"
#include "unavi/core/UNaviCycle.h"
#include "unavi/core/UNaviHandler.h"
#include "unavi/util/UNaviUtil.h"

using namespace std;
using namespace unavi;

void UNaviEvent::set_already()
{
	if (handler_id==-1) return;

	event_mask |= EV_ALREADY;
	if ( active_mask & EV_ALREADY ) return;
	UNaviCycle::cycle()->regist_already_event(*this);
}

void UNaviEvent::set_expire(uint64_t span)
{
	uint64_t* p_at = const_cast<uint64_t*>(&expire_at);
	*p_at = UNaviCycle::curtime_mc() + span;
	event_mask |= EV_TIMEOUT;

	if (handler_id==-1) return;

	if ( active_mask & EV_TIMEOUT ) {
		if (expire_at != active_expire_at) {
			UNaviCycle::cycle()->modify_timer_event(*this,expire_at);
		}
	}
	else {
		UNaviCycle::cycle()->regist_timer_event(*this);
	}
}

void UNaviEvent::set_expire_at(uint64_t time_stamp)
{
	uint64_t* p_at = const_cast<uint64_t*>(&expire_at);
	*p_at = time_stamp;
	event_mask |= EV_TIMEOUT;
	if (handler_id==-1) return;

	if ( active_mask & EV_TIMEOUT ) {
		if (expire_at != active_expire_at) {
			UNaviCycle::cycle()->modify_timer_event(*this,expire_at);
		}
	}
	else {
		UNaviCycle::cycle()->regist_timer_event(*this);
	}
}

void UNaviEvent::set_filemask(FileEventMask add, FileEventMask del)
{
	event_mask |= add;
	event_mask &= ~del;
	if (handler_id==-1) return;

	if ( (active_mask & EV_RDWR) == (event_mask & EV_RDWR) )
		return;

	if ( active_mask & EV_RDWR ) {
		if ( !(event_mask & EV_RDWR) ) {
			UNaviCycle::cycle()->quit_file_event(*this);
		}
		else {
			UNaviCycle::cycle()->modify_file_event(*this,event_mask);
		}
	}
	else {
		UNaviCycle::cycle()->regist_file_event(*this);
	}
}

UNaviEvent::UNaviEvent(uint32_t expire):
    handler_id(-1),
    fd(-1),
    event_mask(0),
    active_mask(0),
    active_expire_at(0),
    fired_mask(0),
    expire_at(0)
{
    if (expire) {
        uint64_t* p_at = const_cast<uint64_t*>(&expire_at);
        event_mask |= EV_TIMEOUT;
        *p_at = UNaviCycle::curtime_mc() + expire;
    }
}

UNaviEvent::UNaviEvent(int _fd, FileEventMask mask, uint64_t expire):
	handler_id(-1),
	fd(_fd),
	event_mask(mask&EV_RDWR),
	active_mask(0),
	active_expire_at(0),
	fired_mask(0),
	fired_cycle(0),
	expire_at(0)
{
	if (expire) {
		uint64_t* p_at = const_cast<uint64_t*>(&expire_at);
		event_mask |= EV_TIMEOUT;
		*p_at = UNaviCycle::curtime_mc() + expire;
	}
	else if ( event_mask == 0 && (mask & EV_ALREADY) ) {
		event_mask = EV_ALREADY;
	}
}

UNaviEvent::~UNaviEvent()
{
	if ( active_mask & EV_TIMEOUT ) {
		UNaviCycle::cycle()->quit_timer_event(*this);
	}

	if ( active_mask & EV_RDWR ) {
		UNaviCycle::cycle()->quit_file_event(*this);
	}
}
