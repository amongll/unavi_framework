/*
 * UNaviPollEpoll.h
 *
 *  Created on: 2014-10-13
 *      Author: dell
 */

#ifndef UNAVIPOLLEPOLL_H_
#define UNAVIPOLLEPOLL_H_

#ifdef __linux__
#include "unavi/core/UNaviPoll.h"
#include <sys/epoll.h>

UNAVI_NMSP_BEGIN

class UNaviEPoll : public UNaviPoll
{
public:
	UNaviEPoll();
	virtual ~UNaviEPoll() ;

	virtual bool regist_read(int fd) ;
	virtual bool regist_write(int fd) ;
	virtual bool del_read(int fd) ;
	virtual bool del_write(int fd) ;
	virtual int poll(UNaviCycle* cycle, int64_t timeout) ;

private:
	//TODO: 跨平台问题
	int epoll_fd;
	std::vector<epoll_event> impl_poll_events;
};

UNAVI_NMSP_END
#endif

#endif /* UNAVIPOLLEPOLL_H_ */
