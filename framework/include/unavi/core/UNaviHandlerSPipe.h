/*
 * UNaviHandlerSPipe.h
 *
 *  Created on: 2014-12-2
 *      Author: dell
 */

#ifndef UNAVIHANDLERSPIPE_H_
#define UNAVIHANDLERSPIPE_H_

#include "unavi/UNaviCommon.h"
#include "unavi/util/UNaviLock.h"
#include "unavi/core/UNaviEvent.h"
#include "unavi/util/UNaviListable.h"

UNAVI_NMSP_BEGIN
/*
 * �ڷ�UNaviCycle�߳��ڵģ������ĳ��UNaviHander�ĵ���ͨѶ��
 */
class UHandlerPipeEntry;
class UNaviHandlerSPipe
{
public:
	UNaviHandlerSPipe(UNaviHandler& h);

	void pull(UNaviList& dst);
	void release_in(UHandlerPipeEntry& ipkt); //h1�ͷŵ�entry��h2���ɵ�entry�������
	UHandlerPipeEntry* recycled_push_node();
	void push(UHandlerPipeEntry& opkt);
	void close()
	{
		close_notify();
	}

	~UNaviHandlerSPipe();
private:
	UNaviHandler& handler;

	UNaviLock sync;
	UNaviList push_list;
	uint32_t push_listsz;
	UNaviList push_free;
	uint32_t push_free_cnt;
	UNaviList pull_list;
	UNaviList pull_free;
	uint32_t pull_free_cnt;

	friend class SPipeNotifyEv;
	class SPipeNotifyEv : public UNaviEvent
	{
	public:
		SPipeNotifyEv(UNaviHandlerSPipe& pp):
			UNaviEvent(pp.notify_pipe[0],UNaviEvent::EV_READ,0),
			target(pp)
		{}
		virtual void process_event();
		UNaviHandlerSPipe& target;
	};

	void init_notify();
	void close_notify();
	void notify(uint32_t pipesz);
	void _pipe_notify(bool wait=false);
	SPipeNotifyEv* notify_ev;
	int notify_pipe[2];
	UNaviLock notify_statuslk;
	pthread_cond_t notify_cond;
	bool notify_proced;
	bool peer_closed;

	UNaviHandlerSPipe(const UNaviHandlerSPipe&);
	UNaviHandlerSPipe& operator=(const UNaviHandlerSPipe&);
};

UNAVI_NMSP_END

#endif /* UNAVIHANDLERSPIPE_H_ */
