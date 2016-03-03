/*
 * UNaviHandler.cpp
 *
 *  Created on: 2014-9-25
 *      Author: dell
 */

#include "unavi/core/UNaviHandler.h"
#include "unavi/core/UCoreException.h"

using namespace std;
using namespace unavi;

UNaviHandler::UNaviHandler(UNaviCycle* _cycle)
	: cycle(_cycle)
{
	if (!cycle || cycle->runned == true)
		throw UCoreException(UCORE_IMPL_ERROR, "at<%s:%d>", __PRETTY_FUNCTION__,__LINE__);

	cycle->regist_handler(*this);
	if (handler_id == -1)
		throw UCoreException(UCORE_IMPL_ERROR, "at<%s:%d>", __PRETTY_FUNCTION__,__LINE__);
}

UNaviHandler::~UNaviHandler()
{
	UNaviListable* ev;
	while(!ready_events.empty()) {
		ev = ready_events.get_head();
		ev->quit_list();
	}
}

void UNaviHandler::event_regist(UNaviEvent& ev)
{
	if (ev.handler_id == -1)
		ev.handler_id = (uint32_t)handler_id;
	else if (ev.handler_id != handler_id) {
		throw UCoreException(UCORE_ABUSED_ERROR, "UNaviEvent abused. at<%s:%d>", __PRETTY_FUNCTION__,__LINE__);
	}

	if (ev.event_mask & UNaviEvent::EV_TIMEOUT) {
		if ( (ev.active_mask & UNaviEvent::EV_TIMEOUT) == 0) {
			cycle->regist_timer_event(ev);
		}
		else {
			if (ev.expire_at != ev.active_expire_at) {
				cycle->modify_timer_event(ev, ev.expire_at);
			}
		}
	}
	else if (ev.active_mask & UNaviEvent::EV_TIMEOUT ){
		cycle->quit_timer_event(ev);
		uint64_t* p_at = const_cast<uint64_t*>(&ev.expire_at);
		*p_at = 0;
	}

	if (ev.event_mask & UNaviEvent::EV_RDWR) {
		if ((ev.active_mask & UNaviEvent::EV_RDWR) == 0) {
			cycle->regist_file_event(ev);
		}
		else {
			if ((ev.active_mask & UNaviEvent::EV_RDWR)
			    != (ev.event_mask & UNaviEvent::EV_RDWR)) {
				cycle->modify_file_event(ev, ev.event_mask);
			}
		}
	}
	else if (ev.active_mask & UNaviEvent::EV_RDWR) {
		cycle->quit_file_event(ev);
	}

	if (ev.event_mask & UNaviEvent::EV_ALREADY) {
		if ((ev.active_mask & UNaviEvent::EV_ALREADY) == 0) {
			cycle->regist_already_event(ev);
		}
	}
	else if ( ev.active_mask & UNaviEvent::EV_ALREADY) {
		cycle->quit_already_event(ev);
	}
}

void UNaviHandler::event_quit(UNaviEvent& ev, uint32_t quit_mask)
{
	if (ev.handler_id != handler_id)
		throw UCoreException(UCORE_ABUSED_ERROR, "at<%s:%d>", __PRETTY_FUNCTION__,__LINE__);

	if ((quit_mask | UNaviEvent::EV_RDWR) && (ev.active_mask & UNaviEvent::EV_RDWR))
		cycle->quit_file_event(ev);
	if ((quit_mask | UNaviEvent::EV_TIMEOUT)&&(ev.active_mask & UNaviEvent::EV_TIMEOUT))
		cycle->quit_timer_event(ev);
	if ((quit_mask | UNaviEvent::EV_ALREADY) && (ev.active_mask & UNaviEvent::EV_ALREADY))
		cycle->quit_already_event(ev);
}

void UNaviHandler::process_events()
{
	UNaviEvent* ev;
	while (!ready_events.empty()) {
		ev = dynamic_cast<UNaviEvent*>(ready_events.get_head());
		ev->quit_list();
		ev->fired_mask = ev->fired_cycle;
		ev->fired_cycle = 0;
		ev->process_event();
	}
}

