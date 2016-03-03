/*
 * UNaviCycle.h
 *
 *  Created on: 2014-9-24
 *      Author: dell
 */

#ifndef UNAVICYCLE_H_
#define UNAVICYCLE_H_

#include "unavi/util/UNaviUtil.h"
#include "unavi/util/UNaviRef.h"
#include "unavi/util/UNaviThread.h"
#include "unavi/core/UNaviPoll.h"

UNAVI_NMSP_BEGIN
class UNaviHandler;
class UNaviWorker;
class UNaviIOSvc;

class UNaviEPoll;
class UNaviSelectPoll;
class UNaviLogger;

class UNaviCycle : public UNaviThread
{
public:
	typedef UNaviEvent::EventMask EventMask;
	typedef UNaviRef<UNaviCycle> Ref;
	friend class UNaviRef<UNaviCycle>;
	typedef UNaviPoll::PollType PollType;

public:
	static uint64_t curtime_mc(); // 1/1000000 second;
	static uint64_t curtime_ml();//1/1000 second;
	static int64_t curtime_diff_mc(uint64_t micro_sec);
	static int64_t curtime_diff_ml(uint64_t milli_sec);
	static UNaviCycle* cycle();
	static uint16_t worker_id();
	static UNaviWorker* worker();
	static uint32_t pipe_id(UNaviIOSvc*);
	static Ref new_cycle(PollType ptype=UNaviPoll::SELECT);

	//直接调用run，可以在第三方thread中运行，或者在主线程下运行。
	//如果要使用UNaviThread设施，调用thread_start
	virtual void run(void*);

	std::vector<const UNaviHandler*> get_handlers()const {
		std::vector<const UNaviHandler*> ret;
		ret.assign(handlers.begin(),handlers.end());
		return ret;
	}
private:
	friend class UNaviHandler;
	friend class UNaviEvent;
	friend class UNaviLogger;

	UNaviCycle(PollType tp);
	~UNaviCycle();

	void regist_handler(UNaviHandler& handler);
	void regist_file_event(UNaviEvent& ev);
	void regist_timer_event(UNaviEvent& ev);

	void modify_file_event(UNaviEvent& ev, EventMask new_mask);
	void modify_timer_event(UNaviEvent& ev, uint64_t newtime);
	void regist_already_event(UNaviEvent& ev);

	void quit_timer_event(UNaviEvent& ev);
	void quit_file_event(UNaviEvent& ev);
	void quit_already_event(UNaviEvent& ev);

	friend class UNaviEPoll;
	friend class UNaviSelectPoll;
	void fire_file_event(int fd, EventMask fired);

	bool runned;
	bool stoped;

	typedef std::map<uint64_t, UNaviList>::iterator TimerIt;
	std::map<uint64_t, UNaviList> timers;
	UNaviList alreadys; //UNaviEvent

	typedef std::pair<EventMask,UNaviList> FDEvents;
	typedef hash_map<int,FDEvents>::iterator FDEventIt;
	hash_map<int,FDEvents> file_events;

	uint16_t handler_id_gen;
	std::vector<UNaviHandler*> handlers;

	UNaviHandler* log_cronner;

	UNaviPoll* poll;

	uint64_t cycle_time;
	uint32_t cycle_time_gets;

	uint64_t _cycle_time()
	{
		if ( (++cycle_time_gets%100) == 0) {
			cycle_time = UNaviUtil::get_time_mc();
			return cycle_time;
		}
		else
			return cycle_time;
	}

	int32_t _worker_id;
	UNaviWorker* _worker;
	//static pthread_once_t self_once;
	//static pthread_key_t self_key;
	static void key_once(void);
};

UNAVI_NMSP_END

#endif /* UNAVICYCLE_H_ */
