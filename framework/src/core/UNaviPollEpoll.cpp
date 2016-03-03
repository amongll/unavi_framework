/*
 * UNaviEpoll.cpp
 *
 *  Created on: 2014-10-13
 *      Author: li.lei
 */

#ifdef __linux__

#include "unavi/core/UNaviPollEpoll.h"
#include "unavi/core/UCoreException.h"
#include "unavi/core/UNaviCycle.h"

using namespace std;
using namespace unavi;

UNaviEPoll::UNaviEPoll():
	epoll_fd(-1)
{
	impl_poll_events.resize(1024);
	epoll_fd = epoll_create(1024);
	if (epoll_fd==-1)
		throw UCoreException(UCORE_SYS_ERROR, "epoll_create() failed. %d %s", errno, strerror(errno));
}

UNaviEPoll::~UNaviEPoll()
{
	::close(epoll_fd);
}

bool UNaviEPoll::regist_read(int fd)
{
	if (fd == -1) return false;
//	if ( fd >= impl_poll_events.size() )
//		impl_poll_events.resize(fd+100);

	epoll_event ee;
	memset(&ee,0x00,sizeof(epoll_event));
	ee.events |= EPOLLIN;
	ee.data.u64 = 0;
	ee.data.fd = fd;
	if (0==epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&ee)) {
		return true;
	}
	return false;
}

bool UNaviEPoll::regist_write(int fd)
{
	if (fd == -1) return false;
//	if ( fd >= impl_poll_events.size() )
//		impl_poll_events.resize(fd+100);

	epoll_event ee;
	memset(&ee,0x00,sizeof(epoll_event));
	ee.events |= EPOLLOUT;
	ee.data.u64 = 0;
	ee.data.fd = fd;
	if (0==epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&ee)) {
		return true;
	}
	return false;
}

bool UNaviEPoll::del_read(int fd)
{
	if (fd == -1) return false;
//	if ( fd >= impl_poll_events.size() )
//		return true;

	epoll_event ee;
	memset(&ee,0x00,sizeof(epoll_event));
	ee.events |= EPOLLIN;
	ee.data.u64 = 0;
	ee.data.fd = fd;
	if (0==epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,&ee)) {
		return true;
	}
	return false;
}

bool UNaviEPoll::del_write(int fd)
{
	if (fd == -1) return false;
//	if ( fd >= impl_poll_events.size() )
//		return true;

	epoll_event ee;
	memset(&ee,0x00,sizeof(epoll_event));
	ee.events |= EPOLLOUT;
	ee.data.u64 = 0;
	ee.data.fd = fd;
	if (0==epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,&ee)) {
		return true;
	}
	return false;
}

int UNaviEPoll::poll(UNaviCycle* cycle, int64_t timeout)
{
	int retval, numevents=0;

	int to_ml = timeout/1000;
	if (timeout < 0)to_ml = -1;
	if (to_ml==0 && timeout > 0)
		to_ml = 1;

	retval = ::epoll_wait(epoll_fd, &(impl_poll_events[0]), impl_poll_events.size(),to_ml/*,&mask*/);
	if (retval > 0) {
		numevents = retval;
		for (int j = 0; j < numevents; j++) {
			UNaviEvent::FileEventMask mask = 0;
			epoll_event &e = impl_poll_events[j];

			if (e.events & EPOLLIN) mask |= UNaviEvent::EV_READ;
			if (e.events & EPOLLOUT) mask |= UNaviEvent::EV_WRITE;
			if (e.events & EPOLLERR) mask |= UNaviEvent::EV_RDWR;
			if (e.events & EPOLLHUP) mask |= UNaviEvent::EV_RDWR;
			cycle->fire_file_event(e.data.fd,mask);
		}
	}
	return numevents;
}

#endif
