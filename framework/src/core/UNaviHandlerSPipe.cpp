/*
 * UNaviHandlerSPipe.cpp
 *
 *  Created on: 2014-12-2
 *      Author: dell
 */

#include "unavi/core/UNaviCycle.h"
#include <fcntl.h>
#include "unavi/core/UNaviHandler.h"
#include "unavi/core/UCoreException.h"
#include "unavi/core/UNaviHandlerSPipe.h"
#include "unavi/core/UNaviHandlerPipe.h"

using namespace std;
using namespace unavi;

UNaviHandlerSPipe::UNaviHandlerSPipe(UNaviHandler& h):
	handler(h),
	push_listsz(0),
	push_free_cnt(0),
	pull_free_cnt(0),
	notify_ev(NULL),
	notify_proced(true),
	peer_closed(false)
{
	notify_pipe[0] = -1;
	notify_pipe[1] = -1;
	init_notify();

	pthread_cond_init(&notify_cond,NULL);
}

UNaviHandlerSPipe::~UNaviHandlerSPipe()
{
	UHandlerPipeEntry* u;
	while(true) {
		u = dynamic_cast<UHandlerPipeEntry*>(push_list.get_head());
		if (!u) break;
		delete u;
	}
	while(true) {
		u = dynamic_cast<UHandlerPipeEntry*>(push_free.get_head());
		if (!u) break;
		delete u;
	}
	while(true) {
		u = dynamic_cast<UHandlerPipeEntry*>(pull_list.get_head());
		if (!u) break;
		delete u;
	}
	while(true) {
		u = dynamic_cast<UHandlerPipeEntry*>(pull_free.get_head());
		if (!u) break;
		delete u;
	}

	delete notify_ev;

	if (notify_pipe[0]!=-1)::close(notify_pipe[0]);
	if (notify_pipe[1]!=-1)::close(notify_pipe[1]);

	pthread_cond_destroy(&notify_cond);
}

void UNaviHandlerSPipe::release_in(UHandlerPipeEntry& ipkt)
{
	ipkt.recycle();
	if (pull_free_cnt >= 8192) {
		delete &ipkt;
		return;
	}
	pull_free.insert_tail(ipkt);
	pull_free_cnt++;

}

void UNaviHandlerSPipe::push(UHandlerPipeEntry& pkt)
{
	uint32_t pipesz=0;

	UNaviScopedLock lk(&sync);
	push_list.insert_tail(pkt);
	pipesz = ++push_listsz;
	lk.reset();

	notify(pipesz);
}

void UNaviHandlerSPipe::pull(UNaviList& dst)
{
	if (pull_list.empty() == false) {
		dst.take(pull_list);
	}

	UNaviScopedLock lk(&sync);
	if (push_list.empty()) return;
	pull_list.swap(push_list);
	push_listsz=0;

	pull_free.giveto(push_free);
	push_free_cnt += pull_free_cnt;
	pull_free_cnt = 0;
	lk.reset();

	dst.take(pull_list);
	return;
}


UHandlerPipeEntry* UNaviHandlerSPipe::recycled_push_node()
{
	UNaviScopedLock lk(&sync);
	if (push_free.empty())
		return NULL;

	UHandlerPipeEntry* entry = dynamic_cast<UHandlerPipeEntry*>(push_free.get_head());
	entry->quit_list();
	push_free_cnt--;

	return entry;
}

void UNaviHandlerSPipe::SPipeNotifyEv::process_event()
{
	char a;
	int ret;
	int read_fd = target.notify_pipe[0];
	if (read_fd != -1) {
		ret = ::read(read_fd,(void*)&a,1);
		if (ret<=0) {
			if (ret==0 || (errno!=EWOULDBLOCK&&errno!=EAGAIN)) {
				target.notify_pipe[0] = -1;
				::close(read_fd);
			}
		}
	}

	UNaviScopedLock lk(&target.notify_statuslk);
	target.notify_proced = true;
	pthread_cond_signal(&target.notify_cond);
	lk.reset();

	target.handler.process(target);
}

void UNaviHandlerSPipe::init_notify()
{
	if ( 0 != pipe(notify_pipe) ) {
		throw UCoreException(UCORE_SYS_ERROR, "HandlerSiPipe pipe() failed. %d %s", errno, strerror(errno));
	}
	int iflag = ::fcntl(notify_pipe[0],F_GETFL,0);
	iflag |= O_NONBLOCK;
	::fcntl(notify_pipe[0],F_SETFL,iflag);
	iflag = ::fcntl(notify_pipe[1],F_GETFL,0);
	iflag |= O_NONBLOCK;
	::fcntl(notify_pipe[1],F_SETFL,iflag);

	notify_ev = new SPipeNotifyEv(*this);
	handler.event_regist(*notify_ev);
}

void UNaviHandlerSPipe::close_notify()
{
	UNaviScopedLock lk(&notify_statuslk);
	notify_proced = true;
	peer_closed = true;
	::close(notify_pipe[1]);
	notify_pipe[1] = -1;
	pthread_cond_signal(&notify_cond);
}

void UNaviHandlerSPipe::notify(uint32_t pipesz)
{
	_pipe_notify(true);
}

void UNaviHandlerSPipe::_pipe_notify(bool wait)
{
	char a='\0';
	int ret;
	if (notify_pipe[1]!=-1) {

		UNaviScopedLock lk(&notify_statuslk);

		if ( peer_closed == true || notify_pipe[1] == -1)
			return;

		if ( notify_proced == false ) {
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

			pthread_cond_timedwait(&notify_cond,&notify_statuslk.impl,&to);
			if (peer_closed == true || notify_proced==false)
				return;
		}
		notify_proced = false;
		lk.reset();

		while(1) {
			ret = ::write(notify_pipe[1],(const void*)&a,1);
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
