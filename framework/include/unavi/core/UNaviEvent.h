/*
 * UNaviEvent.h
 *
 *  Created on: 2014-9-25
 *      Author: li.lei
 */

#ifndef UNAVIEVENT_H_
#define UNAVIEVENT_H_

#include "unavi/UNaviCommon.h"
#include "unavi/util/UNaviListable.h"

UNAVI_NMSP_BEGIN

class UNaviHandler;
class UNaviCycle;
class UNaviEvent : public UNaviListable
{
public:
	enum EventType {
		EV_READ = 0x1,
		EV_WRITE = 0x2,
		EV_RDWR = 0x3,
		EV_TIMEOUT = 0x4,
		EV_ALREADY = 0x8
	};

	typedef uint32_t EventMask;
	typedef uint32_t FileEventMask;
public:
	virtual void process_event() = 0;

	bool expired()const
	{
		return fired_mask & EV_TIMEOUT;
	}

	bool readable()const
	{
		return fired_mask & EV_READ;
	}

	bool writeable()const
	{
		return fired_mask & EV_WRITE;
	}

	bool expire_active()const
	{
		return active_mask & EV_TIMEOUT;
	}

	bool read_active()const
	{
		return active_mask & EV_READ;
	}

	bool write_active()const
	{
		return active_mask & EV_WRITE;
	}

	bool already_active()const
	{
		return active_mask & EV_ALREADY;
	}

	const uint64_t expire_at;

	void set_already();
	void set_expire(uint64_t span);
	void set_expire_at(uint64_t time_stamp);
	void set_filemask(FileEventMask add, FileEventMask del);

protected:
	friend class UNaviHandler;
	friend class UNaviCycle;
	UNaviEvent(uint32_t expire=0); //纯定时器事件
	UNaviEvent(int fd, FileEventMask mask, uint64_t expire = 0);//fd事件，可以带超时
	virtual ~UNaviEvent();

private:
	int32_t handler_id;
	int fd;

	EventMask event_mask;

	EventMask active_mask;//cycle操作
	uint64_t active_expire_at;

	EventMask fired_mask;//cycle操作

	//在process_event被调用之前，fired_mask拷贝该值之后，fired_cycle置空
	EventMask fired_cycle;

	UNaviListable timer_link;
	UNaviListable fdevent_link;
	UNaviListable already_link;

	UNaviEvent(const UNaviEvent&);
	UNaviEvent &operator= (const UNaviEvent&);
};

UNAVI_NMSP_END

#endif /* UNAVIEVENT_H_ */
