/*
 * UNaviCycle.cpp
 *
 *  Created on: 2014-9-25
 *      Author: li.lei 
 */

#include "unavi/core/UNaviCycle.h"
#include "unavi/core/UCoreException.h"
#include "unavi/core/UNaviHandler.h"
#include "unavi/util/UNaviUtil.h"
#include "unavi/core/UNaviWorker.h"
#include "unavi/core/UNaviIOSvc.h"
#include "unavi/core/UNaviLog.h"

using namespace std;
using namespace unavi;

static pthread_once_t self_once = PTHREAD_ONCE_INIT;
static pthread_key_t self_key ;

static void key_once(void)
{
	pthread_key_create(&self_key,NULL);
}

UNaviCycle::UNaviCycle(PollType ptype):
	runned(false),
	stoped(true),
	file_events(1024),
	handler_id_gen(0),
	poll(NULL),
	_worker_id(-1),
	_worker(NULL),
	cycle_time_gets(0),
	log_cronner(NULL)
{
	poll = UNaviPoll::new_poller(ptype);
	cycle_time = UNaviUtil::get_time_mc();
	log_cronner = new UNaviLogCronor(this);
}

UNaviCycle::~UNaviCycle()
{
	if (poll) delete poll;
	if (log_cronner) delete log_cronner;
}

UNaviCycle::Ref UNaviCycle::new_cycle(PollType ptype)
{
	UNaviCycle* obj = new UNaviCycle(ptype);
	return Ref(*obj);
}

uint64_t UNaviCycle::curtime_mc()
{
	UNaviCycle* obj = cycle();
	if (obj)
		return obj->_cycle_time();
	else
		return UNaviUtil::get_time_mc();
}

uint64_t UNaviCycle::curtime_ml()
{
	UNaviCycle* obj = cycle();
	if (obj)
		return obj->_cycle_time()/1000;
	else
		return UNaviUtil::get_time_ml();
}

int64_t UNaviCycle::curtime_diff_mc(uint64_t micro_sec)
{

	return (int64_t)curtime_mc() - (int64_t)micro_sec;
}

int64_t UNaviCycle::curtime_diff_ml(uint64_t milli_sec)
{
	return (int64_t)curtime_ml() - (int64_t)milli_sec;
}

uint16_t UNaviCycle::worker_id()
{
	return (uint16_t)(cycle()->_worker_id);
}

UNaviWorker* UNaviCycle::worker()
{
	UNaviCycle* self = cycle();
	if (!self ) return NULL;
	return self->_worker;
}

uint32_t UNaviCycle::pipe_id(UNaviIOSvc* svc)
{
	UNaviUDPPipe::PipeId pid;
	pid.pair[0] = svc->svc_id;
	pid.pair[1] = (uint16_t)(cycle()->_worker_id);
	return pid.pipe_id;
}

static int cycle_key_once = pthread_once(&self_once,key_once);

void UNaviCycle::run(void*)
{
	if ( runned )
		return;

	if ( pthread_getspecific(self_key) ) {
		throw UCoreException(UCORE_IMPL_ERROR, "at:<%s:%d>", __PRETTY_FUNCTION__,__LINE__);
		//TODO:
	}

	pthread_setspecific(self_key, (const void*)this);
	runned = true;

	vector<UNaviHandler*>::iterator it_h;
	for (it_h = handlers.begin(); it_h != handlers.end(); it_h++) {
		UNaviWorker* wok = dynamic_cast<UNaviWorker*>(*it_h);
		if (wok) {
			_worker_id = wok->id();
			_worker = wok;
		}
		(*it_h)->run_init();
	}

	int poll_ret = 0;
	TimerIt tmend,tmbegin;
	UNaviListable* tm_link;
	UNaviEvent* ev;

	vector<int> handler_flag;
	vector<int>::iterator it_flag;
	bool has_ev = false;
	int64_t tmo;
	handler_flag.resize(handlers.size(),0);

	while(!canceled()) {
		cycle_time = UNaviUtil::get_time_mc();
		tmend = timers.upper_bound(cycle_time);

		//already的处理只在此处，保证其在定时器和io事件之后被处理
		while ( !alreadys.empty() ) {
			tm_link = alreadys.get_head();
			tm_link->quit_list();
			ev = unavi_list_data(tm_link,UNaviEvent,already_link);
			ev->fired_cycle |= UNaviEvent::EV_ALREADY;
			ev->active_mask &= ~UNaviEvent::EV_ALREADY;
			ev->event_mask &= ~UNaviEvent::EV_ALREADY;
			handlers[ev->handler_id]->ready_events.insert_head(*ev);
			handler_flag[ev->handler_id] = 1;
			has_ev = true;
		}

		for ( tmbegin = timers.begin(); tmbegin != tmend; tmbegin++) {
			while( !tmbegin->second.empty() ) {
				tm_link = tmbegin->second.get_head();
				tm_link->quit_list();
				ev = unavi_list_data(tm_link,UNaviEvent,timer_link);
				ev->fired_cycle |= UNaviEvent::EV_TIMEOUT;
				ev->active_mask &= ~UNaviEvent::EV_TIMEOUT;
				ev->event_mask &= ~UNaviEvent::EV_TIMEOUT;
				ev->active_expire_at = 0;
				//uint64_t* p_at = const_cast<uint64_t*>(&ev->expire_at);
				//*p_at = 0;
				handlers[ev->handler_id]->ready_events.insert_head(*ev);
				handler_flag[ev->handler_id] = 1;
				has_ev = true;
			}
		}
		timers.erase(timers.begin(),tmend);

		if (has_ev) {
			for (tmo=0,it_flag = handler_flag.begin(); it_flag!= handler_flag.end(); it_flag++,tmo++) {
				if (*it_flag) {
					handlers[tmo]->process_events();
					*it_flag = 0;
				}
			}
		}
		has_ev = false;

		cycle_time = UNaviUtil::get_time_mc();
		tmo = -1;
		if (timers.size()) {
			tmo = timers.begin()->first - cycle_time;
			if (tmo < 0) tmo = 0;
		}

		if (alreadys.empty() == false)
			tmo = 0;

		poll_ret = poll->poll(this,tmo);
		cycle_time = UNaviUtil::get_time_mc();

		tmend = timers.upper_bound(cycle_time);
		for ( tmbegin = timers.begin(); tmbegin != tmend; tmbegin++) {
			while( !tmbegin->second.empty() ) {
				tm_link = tmbegin->second.get_head();
				tm_link->quit_list();
				ev = unavi_list_data(tm_link,UNaviEvent,timer_link);
				ev->fired_cycle |= UNaviEvent::EV_TIMEOUT;
				ev->active_mask &= ~UNaviEvent::EV_TIMEOUT;
				ev->event_mask &= ~UNaviEvent::EV_TIMEOUT;
				ev->active_expire_at = 0;
				//uint64_t* p_at = const_cast<uint64_t*>(&ev->expire_at);
				//*p_at = 0;
				handlers[ev->handler_id]->ready_events.insert_head(*ev);
			}
		}
		timers.erase(timers.begin(),tmend);

		//读写事件肯定在定时器事件之前被处理
		for (it_h = handlers.begin(); it_h != handlers.end(); it_h++) {
			(*it_h)->process_events();
			//(*it_h)->process();
		}
	}

	//清理定时器事件。
	for(tmbegin = timers.begin(); tmbegin != timers.end(); tmbegin++) {
		while ( !tmbegin->second.empty() ) {
			tm_link = tmbegin->second.get_head();
			tm_link->quit_list();
			ev = unavi_list_data(tm_link,UNaviEvent,timer_link);
			ev->active_mask &= ~UNaviEvent::EV_TIMEOUT;
		}
	}
	timers.clear();

	while ( !alreadys.empty() ) {
		tm_link = alreadys.get_head();
		tm_link->quit_list();
		ev = unavi_list_data(tm_link,UNaviEvent,already_link);
		ev->active_mask &= ~UNaviEvent::EV_ALREADY;
		ev->event_mask &= ~UNaviEvent::EV_ALREADY;
	}

	//清理文件event
	FDEventIt it_fd;
	for(it_fd = file_events.begin(); it_fd!=file_events.end(); it_fd++) {
		UNaviList& ev_l = it_fd->second.second;
		while( !ev_l.empty() ) {
			tm_link = ev_l.get_head();
			tm_link->quit_list();
			ev = unavi_list_data(tm_link,UNaviEvent,fdevent_link);
			if (ev->active_mask & UNaviEvent::EV_READ) {
				poll->del_read(ev->fd);
			}
			if (ev->active_mask & UNaviEvent::EV_WRITE) {
				poll->del_write(ev->fd);
			}
			ev->active_mask &= ~UNaviEvent::EV_RDWR;
		}
	}
	file_events.clear();

	//清理并删除各绑定handler
	for (it_h = handlers.begin(); it_h != handlers.end(); it_h++) {
		(*it_h)->cleanup();
		//delete *it_h;
	}
	handlers.clear();

	delete poll;
	poll = NULL;
	stoped = true;
}

UNaviCycle* UNaviCycle::cycle()
{
	UNaviCycle* cycle =(UNaviCycle*)pthread_getspecific(self_key);
	return cycle;
}

void UNaviCycle::regist_handler(UNaviHandler& handler)
{
	if (runned) return;

	handler.handler_id = handler_id_gen++;
	if (handlers.size() < handler_id_gen ) {
		handlers.resize(handler_id_gen,NULL);
	}

	handlers[handler.handler_id] = &handler;
}

void UNaviCycle::regist_file_event(UNaviEvent& ev)
{
	if (ev.fd == -1 )
		return;

	pair<FDEventIt,bool> ret =
	file_events.insert(make_pair(ev.fd,make_pair(0,UNaviList())));
	ret.first->second.second.insert_head(ev.fdevent_link);
	modify_file_event(ev, ev.event_mask&UNaviEvent::EV_RDWR);
}

void UNaviCycle::regist_timer_event(UNaviEvent& ev)
{
	pair<TimerIt,bool> ret = timers.insert(make_pair(ev.expire_at,UNaviList()));
	TimerIt it = ret.first;
	it->second.insert_head(ev.timer_link);
	ev.active_mask |= UNaviEvent::EV_TIMEOUT;
	ev.active_expire_at = it->first;
}

void UNaviCycle::modify_file_event(UNaviEvent& ev, EventMask new_mask)
{
	if (ev.fd == -1)
		return;

	FDEventIt it = file_events.find(ev.fd);

	if ( new_mask & UNaviEvent::EV_READ ) {
		if ((ev.active_mask & UNaviEvent::EV_READ)==0 &&
			(it->second.first & UNaviEvent::EV_READ)==0 ) {
			if (poll->regist_read(ev.fd)) {
				ev.active_mask |= UNaviEvent::EV_READ;
				it->second.first |= UNaviEvent::EV_READ;
			}
		}
	}
	else {
		if (ev.active_mask & UNaviEvent::EV_READ) {
			if (poll->del_read(ev.fd)) {
				ev.active_mask &= ~UNaviEvent::EV_READ;
				it->second.first &= ~UNaviEvent::EV_READ;
			}
		}
	}

	if ( new_mask & UNaviEvent::EV_WRITE ) {
		if ((ev.active_mask & UNaviEvent::EV_WRITE)==0 &&
			(it->second.first & UNaviEvent::EV_WRITE)==0) {
			if (poll->regist_write(ev.fd)) {
				ev.active_mask |= UNaviEvent::EV_WRITE;
				it->second.first |= UNaviEvent::EV_WRITE;
			}
		}
	}
	else {
		if (ev.active_mask & UNaviEvent::EV_WRITE) {
			if (poll->del_write(ev.fd)) {
				ev.active_mask &= ~UNaviEvent::EV_WRITE;
				it->second.first &= ~UNaviEvent::EV_WRITE;
			}
		}
	}
}

void UNaviCycle::modify_timer_event(UNaviEvent& ev, uint64_t new_time)
{
	TimerIt it = timers.find(ev.active_expire_at);
	ev.timer_link.quit_list();
	if (it->second.empty()) {
		timers.erase(it);
	}

	pair<TimerIt,bool> ret = timers.insert(make_pair(ev.expire_at,UNaviList()));
	it = ret.first;
	it->second.insert_head(ev.timer_link);
	ev.active_expire_at = it->first;
}

void UNaviCycle::regist_already_event(UNaviEvent& ev)
{
	alreadys.insert_tail(ev.already_link);
	ev.event_mask |= UNaviEvent::EV_ALREADY;
	ev.active_mask |= UNaviEvent::EV_ALREADY;
}

void UNaviCycle::quit_timer_event(UNaviEvent& ev)
{
	TimerIt it = timers.find(ev.active_expire_at);
	if (it==timers.end())
		return;
	ev.timer_link.quit_list();
	if (it->second.empty()) {
		timers.erase(it);
	}
	ev.active_mask &= ~UNaviEvent::EV_TIMEOUT;
	ev.active_expire_at = 0;
}

void UNaviCycle::quit_already_event(UNaviEvent& ev)
{
	ev.already_link.quit_list();
	ev.active_mask &= ~UNaviEvent::EV_ALREADY;
	ev.event_mask &= ~UNaviEvent::EV_ALREADY;
}

void UNaviCycle::quit_file_event(UNaviEvent& ev)
{
	if (ev.fd == -1)
		return;

	FDEventIt it = file_events.find(ev.fd);
	if (it == file_events.end())
		return;
	EventMask deleted_mask = 0;
	if (ev.active_mask & UNaviEvent::EV_READ) {
		if (poll->del_read(ev.fd))
			deleted_mask |= UNaviEvent::EV_READ;
	}
	if (ev.active_mask & UNaviEvent::EV_WRITE) {
		if (poll->del_write(ev.fd))
			deleted_mask |= UNaviEvent::EV_WRITE;
	}

	ev.active_mask &= ~deleted_mask;
	it->second.first &= ~deleted_mask;
	if (deleted_mask == (ev.active_mask&UNaviEvent::EV_RDWR) ) {
		ev.fdevent_link.quit_list();
	}

	if (it->second.second.empty())
		file_events.erase(it);
}

void UNaviCycle::fire_file_event(int fd, EventMask fired)
{
	FDEventIt it = file_events.find(fd);
	if (it==file_events.end())
		return;

	if ( fired == 0 )
		return;

	UNaviListable* ev_link = it->second.second.senti.next;
	while( ev_link != &it->second.second.senti) {
		UNaviEvent* ev = unavi_list_data(ev_link,UNaviEvent,fdevent_link);
		ev_link = ev_link->next;
		ev->fired_cycle |= (ev->active_mask&fired);
		handlers[ev->handler_id]->ready_events.insert_head(*ev);
	}
}

