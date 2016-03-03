/*
 * UNaviPoll.h
 *
 *  Created on: 2014-9-26
 *      Author: dell
 */

#ifndef UNAVIPOLL_H_
#define UNAVIPOLL_H_

#include "unavi/UNaviCommon.h"
#include "unavi/core/UNaviEvent.h"

UNAVI_NMSP_BEGIN

class UNaviCycle;

class UNaviPoll
{
public:
	enum PollType
	{
		SELECT
#ifdef __linux__
		,EPOLL
#endif
	};
	UNaviPoll(){};
	virtual ~UNaviPoll(){};

	static UNaviPoll* new_poller(PollType type);

	virtual bool regist_read(int fd) = 0;
	virtual bool regist_write(int fd) = 0;
	virtual bool del_read(int fd) = 0;
	virtual bool del_write(int fd) = 0;
	virtual int poll(UNaviCycle* cycle, int64_t timeout) = 0;
};

UNAVI_NMSP_END

#endif /* UNAVIPOLL_H_ */
