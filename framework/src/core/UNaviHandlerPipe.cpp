/*
 * UNaviHandlerPipe.cpp
 *
 *  Created on: 2014-10-22
 *      Author: li.lei 
 */


#include "unavi/core/UNaviCycle.h"
#include <fcntl.h>
#include "unavi/core/UNaviHandler.h"
#include "unavi/core/UCoreException.h"
#include "unavi/core/UNaviHandlerPipe.h"

using namespace std;
using namespace unavi;

UNaviHandlerPipe::UNaviHandlerPipe(UNaviHandler& h1, UNaviHandler& h2):
	handler1(h1),
	handler2(h2),
	queue_sync(true),
	h1_push_listsz(0),
	h1_push_free_cnt(0),
	h2_pull_free_cnt(0),
	h2_push_listsz(0),
	h2_push_free_cnt(0),
	h1_pull_free_cnt(0),
	h12_notify_eo(NULL),
	h12_notify_ev(NULL),
	h12_notify_proced(true),
	h12_peer_closed(false),
	h21_notify_eo(NULL),
	h21_notify_ev(NULL),
	h21_notify_proced(true),
	h21_peer_closed(false)
{
	h12_notify_pipe[0] = -1;
	h12_notify_pipe[1] = -1;
	h21_notify_pipe[0] = -1;
	h21_notify_pipe[1] = -1;

	if (handler1.cycle == handler2.cycle) {
		queue_sync = false;
	}
	else {
		init_h12_notify();
		init_h21_notify();
	}

	pthread_cond_init(&h12_notify_cond,NULL);
	pthread_cond_init(&h21_notify_cond,NULL);
}

UNaviHandlerPipe::~UNaviHandlerPipe()
{
	UHandlerPipeEntry* u;
	while(true) {
		u = dynamic_cast<UHandlerPipeEntry*>(h1_push_list.get_head());
		if (!u) break;
		delete u;
	}
	while(true) {
		u = dynamic_cast<UHandlerPipeEntry*>(h1_push_free.get_head());
		if (!u) break;
		delete u;
	}
	while(true) {
		u = dynamic_cast<UHandlerPipeEntry*>(h2_pull_list.get_head());
		if (!u) break;
		delete u;
	}
	while(true) {
		u = dynamic_cast<UHandlerPipeEntry*>(h2_pull_free.get_head());
		if (!u) break;
		delete u;
	}

	while(true) {
		u = dynamic_cast<UHandlerPipeEntry*>(h2_push_list.get_head());
		if (!u) break;
		delete u;
	}
	while(true) {
		u = dynamic_cast<UHandlerPipeEntry*>(h2_push_free.get_head());
		if (!u) break;
		delete u;
	}
	while(true) {
		u = dynamic_cast<UHandlerPipeEntry*>(h1_pull_list.get_head());
		if (!u) break;
		delete u;
	}
	while(true) {
		u = dynamic_cast<UHandlerPipeEntry*>(h1_pull_free.get_head());
		if (!u) break;
		delete u;
	}

	if (queue_sync) {
		delete h12_notify_eo;
		delete h12_notify_ev;
		delete h21_notify_eo;
		delete h21_notify_ev;

		if (h12_notify_pipe[0]!=-1)::close(h12_notify_pipe[0]);
		if (h12_notify_pipe[1]!=-1)::close(h12_notify_pipe[1]);
		if (h21_notify_pipe[0]!=-1)::close(h21_notify_pipe[0]);
		if (h21_notify_pipe[1]!=-1)::close(h21_notify_pipe[1]);
	}
	pthread_cond_destroy(&h12_notify_cond);
	pthread_cond_destroy(&h21_notify_cond);
}

void UNaviHandlerPipe::h1_pull(UNaviList& dst)
{
	if (h1_pull_list.empty() == false) {
		dst.take(h1_pull_list);
	}
	else if (!queue_sync)
		return;

	UNaviScopedLock lk(&h21_sync);
	if (h2_push_list.empty()) return;
	h1_pull_list.swap(h2_push_list);
	h2_push_listsz=0;

	h1_pull_free.giveto(h2_push_free);
	h2_push_free_cnt += h1_pull_free_cnt;
	h1_pull_free_cnt = 0;
	lk.reset();

	dst.take(h1_pull_list);
	return;
}

void UNaviHandlerPipe::h1_release_in(UHandlerPipeEntry& ipkt)
{
	ipkt.recycle();
	if ( queue_sync ) {
		if (h1_pull_free_cnt >= 8192) {
			delete &ipkt;
			return;
		}
		h1_pull_free.insert_tail(ipkt);
		h1_pull_free_cnt++;
	}
	else {
		if (h2_push_free_cnt < 8192) {
			h2_push_free.insert_tail(ipkt);
			h2_push_free_cnt++;
		}
		else
			delete &ipkt;
	}
}

UHandlerPipeEntry* UNaviHandlerPipe::h1_recycled_out()
{
	UNaviScopedLock lk(queue_sync?&h12_sync:NULL);
	if (h1_push_free.empty())
		return NULL;

	UHandlerPipeEntry* entry = dynamic_cast<UHandlerPipeEntry*>(h1_push_free.get_head());
	entry->quit_list();
	h1_push_free_cnt--;

	return entry;
}

void UNaviHandlerPipe::h1_push(UHandlerPipeEntry& pkt)
{
	uint32_t pipesz=0;
	if (queue_sync) {
		UNaviScopedLock lk(&h12_sync);
		h1_push_list.insert_tail(pkt);
		pipesz = ++h1_push_listsz;
	}
	else {
		h2_pull_list.insert_tail(pkt);
	}
	h12_notify(pipesz);
}

void UNaviHandlerPipe::h2_pull(UNaviList& dst)
{
	if (h2_pull_list.empty() == false) {
		dst.take(h2_pull_list);
	}
	else if (!queue_sync)
		return;

	UNaviScopedLock lk(&h12_sync);
	if (h1_push_list.empty()) return;
	h2_pull_list.swap(h1_push_list);
	h1_push_listsz=0;

	h2_pull_free.giveto(h1_push_free);
	h1_push_free_cnt += h2_pull_free_cnt;
	h2_pull_free_cnt = 0;
	lk.reset();

	dst.take(h2_pull_list);
	return;
}

void UNaviHandlerPipe::h2_release_in(UHandlerPipeEntry& pkt)
{
	pkt.recycle();
	if ( queue_sync ) {
		if (h2_pull_free_cnt >= 8192) {
			delete &pkt;
			return;
		}
		h2_pull_free.insert_tail(pkt);
		h2_pull_free_cnt++;
	}
	else {
		if (h1_push_free_cnt < 8192 ) {
			h1_push_free.insert_tail(pkt);
			h1_push_free_cnt++;
		}
		else
			delete &pkt;
	}
}

UHandlerPipeEntry* UNaviHandlerPipe::h2_recycled_out()
{
	UNaviScopedLock lk(queue_sync?&h21_sync:NULL);
	if (h2_push_free.empty())
		return NULL;

	UHandlerPipeEntry* entry = dynamic_cast<UHandlerPipeEntry*>(h2_push_free.get_head());
	entry->quit_list();
	h2_push_free_cnt--;

	return entry;
}

void UNaviHandlerPipe::h2_push(UHandlerPipeEntry& ipkt)
{
	uint32_t pipesz=0;
	if (queue_sync) {
		UNaviScopedLock lk(&h21_sync);
		h2_push_list.insert_tail(ipkt);
		pipesz = ++h2_push_listsz;
	}
	else {
		h1_pull_list.insert_tail(ipkt);
	}
	h21_notify(pipesz);
}

void UNaviHandlerPipe::PipeNotifyEo::process_event()
{
	if (h12) {
		target._h12_pipe_notify();
	}
	else
		target._h21_pipe_notify();
}

void UNaviHandlerPipe::PipeNotifyEv::process_event()
{
	char a;
	int ret;
	int read_fd = (int)(h12?target.h12_notify_pipe[0]:target.h21_notify_pipe[0]);
	if (read_fd != -1) {
		ret = ::read(read_fd,(void*)&a,1);
		if (ret<=0) {
			if (ret==0 || (errno!=EWOULDBLOCK&&errno!=EAGAIN)) {
				if (h12)
					target.h12_notify_pipe[0] = -1;
				else
					target.h21_notify_pipe[0] = -1;
				::close(read_fd);
			}
		}
	}

	if (h12) {
		UNaviScopedLock lk(&target.h12_notify_statuslk);
		target.h12_notify_proced = true;
		pthread_cond_signal(&target.h12_notify_cond);
		lk.reset();

		target.handler2.process(target);
	}
	else {
		UNaviScopedLock lk(&target.h21_notify_statuslk);
		target.h21_notify_proced = true;
		pthread_cond_signal(&target.h21_notify_cond);
		lk.reset();

		target.handler1.process(target);
	}
}

void UNaviHandlerPipe::init_h12_notify()
{
	if ( 0 != pipe(h12_notify_pipe) ) {
		throw UCoreException(UCORE_SYS_ERROR, "HandlerBiPipe pipe() failed. %d %s", errno, strerror(errno));
	}
	int iflag = ::fcntl(h12_notify_pipe[0],F_GETFL,0);
	iflag |= O_NONBLOCK;
	::fcntl(h12_notify_pipe[0],F_SETFL,iflag);
	iflag = ::fcntl(h12_notify_pipe[1],F_GETFL,0);
	iflag |= O_NONBLOCK;
	::fcntl(h12_notify_pipe[1],F_SETFL,iflag);

	h12_notify_ev = new PipeNotifyEv(*this,true);
	handler2.event_regist(*h12_notify_ev);
	h12_notify_eo = new PipeNotifyEo(*this,true);
	handler1.event_regist(*h12_notify_eo);
}

void UNaviHandlerPipe::close_h12_notify()
{
	UNaviScopedLock lk(&h21_notify_statuslk);
	h21_notify_proced = true;
	h21_peer_closed = true;
	pthread_cond_signal(&h21_notify_cond);
	lk.reset();

	_h12_pipe_notify(true);
}

void UNaviHandlerPipe::h12_notify(uint32_t pipesz)
{
	if (queue_sync) {
		if ( pipesz >= 1024) {
			if (h12_notify_eo->expire_active())
				handler1.event_quit(*h12_notify_eo);
			_h12_pipe_notify(true);
		}
		else {
			if (h12_notify_eo->expire_active()==false) {
				h12_notify_eo->set_expire(100);
			}
		}
	}
	else {
		handler2.process(*this);
	}
}

void UNaviHandlerPipe::_h12_pipe_notify(bool wait)
{
	char a='\0';
	int ret;
	if (h12_notify_pipe[1]!=-1) {

		UNaviScopedLock lk(&h12_notify_statuslk);

		if ( h12_peer_closed == true)
			return;

		if ( h12_notify_proced == false ) {
			if (wait == false) return;

			timespec to;
			timeval tv;
			::gettimeofday(&tv,NULL);
			if (tv.tv_usec >= 999900 ) {
				to.tv_sec = tv.tv_sec+1;
				to.tv_nsec = (tv.tv_usec+100)%1000000 * 1000;
			}
			else {
				to.tv_sec = tv.tv_sec;
				to.tv_nsec = (tv.tv_usec+100) * 1000;
			}

			pthread_cond_timedwait(&h12_notify_cond,&h12_notify_statuslk.impl,&to);
			if (h12_peer_closed == true || h12_notify_proced==false)
				return;
		}
		h12_notify_proced = false;
		lk.reset();

		while(1) {
			ret = ::write(h12_notify_pipe[1],(const void*)&a,1);
			if (ret==-1 && (errno==EWOULDBLOCK||errno==EAGAIN)) {
				timespec ts;
				ts.tv_sec = 0;
				ts.tv_nsec = 100;
				if (-1==nanosleep(&ts,NULL))
					break;
			}
			else
				break;
		}
	}
}

void UNaviHandlerPipe::init_h21_notify()
{
	if ( 0 != pipe(h21_notify_pipe) ) {
		throw UCoreException(UCORE_SYS_ERROR, "HandlerBiPipe pipe() failed. %d %s", errno, strerror(errno));
	}
	int iflag = ::fcntl(h21_notify_pipe[0],F_GETFL,0);
	iflag |= O_NONBLOCK;
	::fcntl(h21_notify_pipe[0],F_SETFL,iflag);
	iflag = ::fcntl(h21_notify_pipe[1],F_GETFL,0);
	iflag |= O_NONBLOCK;
	::fcntl(h21_notify_pipe[1],F_SETFL,iflag);

	h21_notify_ev = new PipeNotifyEv(*this,false);
	handler1.event_regist(*h21_notify_ev);
	h21_notify_eo = new PipeNotifyEo(*this,false);
	handler2.event_regist(*h21_notify_eo);
}

void UNaviHandlerPipe::close_h21_notify()
{
	UNaviScopedLock lk(&h12_notify_statuslk);
	h12_notify_proced = true;
	h12_peer_closed = true;
	pthread_cond_signal(&h21_notify_cond);

	_h21_pipe_notify(true);
}

void UNaviHandlerPipe::h21_notify(uint32_t pipesz)
{
	if (queue_sync) {
		if ( pipesz >= 1024) {
			if (h21_notify_eo->expire_active())
				handler2.event_quit(*h21_notify_eo);
			_h21_pipe_notify(true);
		}
		else {
			if (h21_notify_eo->expire_active()==false) {
				h21_notify_eo->set_expire(100);
			}
		}
	}
	else {
		handler1.process(*this);
	}
}

void UNaviHandlerPipe::_h21_pipe_notify(bool wait)
{
	char a='\0';
	int ret;
	if (h21_notify_pipe[1]!=-1) {
		UNaviScopedLock lk(&h21_notify_statuslk);

		if ( h21_peer_closed ) return;

		if ( h21_notify_proced == false ) {
			if (wait == false) return;

			timespec to;
			timeval tv;
			::gettimeofday(&tv,NULL);
			if (tv.tv_usec >= 999900 ) {
				to.tv_sec = tv.tv_sec+1;
				to.tv_nsec = (tv.tv_usec+100)%1000000 * 1000;
			}
			else {
				to.tv_sec = tv.tv_sec;
				to.tv_nsec = (tv.tv_usec+100) * 1000;
			}

			pthread_cond_signal(&h12_notify_cond); //可能同时等待对方，破除一边的等待
			pthread_cond_timedwait(&h21_notify_cond,&h21_notify_statuslk.impl,&to);
			if (h21_peer_closed == true || h21_notify_proced==false)
				return;
		}

		h21_notify_proced = false;
		lk.reset();

		while(1) {
			ret = ::write(h21_notify_pipe[1],(const void*)&a,1);
			if (ret==-1 && (errno==EWOULDBLOCK||errno==EAGAIN)) {
				timespec ts;
				ts.tv_sec = 0;
				ts.tv_nsec = 100;
				if (-1==nanosleep(&ts,NULL))
					break;
			}
			else
				break;
		}
	}
}
