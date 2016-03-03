/*
 * UNaviWorkerQueue.h
 *
 *  Created on: 2014-9-24
 *      Author: li.lei
 */

#ifndef UNAVIHANDLERPIPE_H_
#define UNAVIHANDLERPIPE_H_

#include "unavi/UNaviCommon.h"
#include "unavi/util/UNaviLock.h"
#include "unavi/core/UNaviEvent.h"
#include "unavi/util/UNaviListable.h"

UNAVI_NMSP_BEGIN

class UNaviHandler;

struct UHandlerPipeEntry : public UNaviListable
{
	UHandlerPipeEntry(){}
	virtual void recycle(){};
private:
	UHandlerPipeEntry(const UHandlerPipeEntry&);
};

class UNaviHandlerPipe
{
public:
	UNaviHandlerPipe(UNaviHandler& h1,UNaviHandler& h2);

	void h1_pull(UNaviList& dst);
	void h1_release_in(UHandlerPipeEntry& ipkt); //h1释放的entry是h2生成的entry子类对象
	UHandlerPipeEntry* h1_recycled_out(); //h2释放的由h1生成的entry，如果没有recycle entry，需要h1自己new一个entry
	void h1_push(UHandlerPipeEntry& opkt);
	void h1_close()
	{
		close_h12_notify();
	}

	void h2_pull(UNaviList& dst);
	void h2_release_in(UHandlerPipeEntry& opkt);
	UHandlerPipeEntry* h2_recycled_out();
	void h2_push(UHandlerPipeEntry& ipkt);
	void h2_close()
	{
		close_h21_notify();
	}

	~UNaviHandlerPipe();
private:
	UNaviHandler& handler1;
	UNaviHandler& handler2;

	bool queue_sync;

	UNaviLock h12_sync;
	UNaviList h1_push_list;
	uint32_t h1_push_listsz;
	UNaviList h1_push_free;
	uint32_t h1_push_free_cnt;
	UNaviList h2_pull_list;
	UNaviList h2_pull_free;
	uint32_t h2_pull_free_cnt;

	UNaviLock h21_sync;
	UNaviList h2_push_list;
	uint32_t h2_push_listsz;
	UNaviList h2_push_free;
	uint32_t h2_push_free_cnt;
	UNaviList h1_pull_list;
	UNaviList h1_pull_free;
	uint32_t h1_pull_free_cnt;

	friend class PipeNotifyEo;
	class PipeNotifyEo : public UNaviEvent
	{
	public:
		PipeNotifyEo(UNaviHandlerPipe& pp,bool h1_h2):
			UNaviEvent(100),
			target(pp),
			h12(h1_h2)
		{}
		virtual void process_event();
		UNaviHandlerPipe& target;
		bool h12; //true: 1->2, false: 2->1
	};

	class PipeNotifyEv : public UNaviEvent
	{
	public:
		PipeNotifyEv(UNaviHandlerPipe& pp,bool h1_h2):
			UNaviEvent(h1_h2?pp.h12_notify_pipe[0]:pp.h21_notify_pipe[0],UNaviEvent::EV_READ,0),
			target(pp),
			h12(h1_h2)
		{}
		virtual void process_event();
		UNaviHandlerPipe& target;
		bool h12; //true: 1->2, false: 2->1
	};

	void init_h12_notify();
	void close_h12_notify();
	void h12_notify(uint32_t pipesz);
	void _h12_pipe_notify(bool wait=false);
	int h12_notify_pipe[2];
	PipeNotifyEv* h12_notify_ev;
	PipeNotifyEo* h12_notify_eo;
	UNaviLock h12_notify_statuslk;
	pthread_cond_t h12_notify_cond;
	bool h12_notify_proced;
	bool h12_peer_closed;

	void init_h21_notify();
	void close_h21_notify();
	void h21_notify(uint32_t pipesz);
	void _h21_pipe_notify(bool wait=false);
	int h21_notify_pipe[2];
	PipeNotifyEv* h21_notify_ev;
	PipeNotifyEo* h21_notify_eo;
	UNaviLock h21_notify_statuslk;
	pthread_cond_t h21_notify_cond;
	bool h21_notify_proced;
	bool h21_peer_closed;

	UNaviHandlerPipe(const UNaviHandlerPipe&);
	UNaviHandlerPipe& operator=(const UNaviHandlerPipe&);
};

UNAVI_NMSP_END

#endif /* UNAVIWORKERQUEUE_H_ */
