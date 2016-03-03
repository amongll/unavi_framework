/*
 * UNaviWorkerQueue.h
 *
 *  Created on: 2014-9-24
 *      Author: li.lei
 */

#ifndef UNAVIUDPPIPE_H_
#define UNAVIUDPPIPE_H_

#include "unavi/UNaviCommon.h"
#include "unavi/util/UNaviLock.h"
#include "unavi/core/UNaviUDPPacket.h"
#include "unavi/core/UNaviEvent.h"

UNAVI_NMSP_BEGIN

class UNaviIOSvc;
class UNaviWorker;

class UNaviUDPPipe
{
	friend class UNaviIOSvc;
	friend class UNaviWorker;
public:
	union PipeId
	{
		uint16_t pair[2]; //1: svc, 2: worker
		uint32_t pipe_id;
	};

private:
	static UNaviUDPPipe* get_pipe(uint32_t pipeid);
	void bandwidth(double& inband,double& outband, uint32_t& inlimit, uint32_t& outlimit);

	friend class UNaviUDPPacket;
	UNaviUDPPipe(UNaviIOSvc& iosvc,UNaviWorker& worker);
	~UNaviUDPPipe();

	void work_pull(UNaviList& dst);
	void work_release_in(UNaviUDPPacket& ipkt);
	UNaviUDPPacket* work_new_output(size_t sz);
	void work_push(UNaviUDPPacket& opkt);


	void io_pull(UNaviList& dst);
	void io_release_out(UNaviUDPPacket& opkt);
	size_t io_recycled_pkts(UNaviList& dst);
	void io_push(UNaviUDPPacket& ipkt);

	union {
		uint16_t handler_pair[2]; //1: svc, 2: worker
		uint32_t pipe_id;
	};

	UNaviUDPPacket* new_packet() {
		return new UNaviUDPPacket(_mtu,pipe_id);
	}

	UNaviIOSvc& svc;
	UNaviWorker& worker;

	bool queue_sync_r;
	bool queue_sync_w;
	uint16_t _mtu;
	uint8_t proto_id;

	UNaviLock inqueue_sync;
	UNaviList io_push_list;
	uint32_t io_push_listsz;
	UNaviList io_in_free;
	UNaviList work_pull_list;
	UNaviList work_in_free;
	uint32_t work_in_free_cnt;
	uint32_t io_in_free_cnt;

	UNaviLock outqueue_sync;
	UNaviList work_push_list;
	uint32_t work_push_listsz;
	UNaviList work_out_free;
	UNaviList io_pull_list;
	UNaviList io_out_free;
	uint32_t io_out_free_cnt;
	uint32_t work_out_free_cnt;

	UNaviList work_out_free_2;
	uint32_t work_out_free_cnt2;

	int in_notify_pipe[2];
	int out_notify_pipe[2];

	UNaviLock out_notify_statuslk;
	pthread_cond_t out_notify_cond;
	bool out_notify_proced;
	bool out_peer_closed;

	UNaviLock in_notify_statuslk;
	pthread_cond_t in_notify_cond;
	bool in_notify_proced;
	bool in_peer_closed;

	void init_io_notify();
	void init_work_notify();
	void close_io_notify();
	void close_work_notify();

	void io_notify(uint32_t ppsz);
	void work_notify(uint32_t ppsz);

	void _work_pipe_notify(bool wait=false);
	void _io_pipe_notify(bool wait=false);

	friend class PipeNotifyEo;
	class PipeNotifyEo : public UNaviEvent
	{
	public:
		PipeNotifyEo(UNaviUDPPipe& pp,bool direct):
			UNaviEvent(100),
			target(pp),
			in(direct)
		{}
		virtual void process_event();
		UNaviUDPPipe& target;
		bool in;
	};

	friend class PipeNotifyEv;
	class PipeNotifyEv : public UNaviEvent
	{
	public:
		PipeNotifyEv(UNaviUDPPipe& pp,bool s2w):
			UNaviEvent(s2w?pp.in_notify_pipe[0]:pp.out_notify_pipe[0],UNaviEvent::EV_READ,0),
			target(pp),
			svc2worker(s2w)
		{}
		virtual void process_event();
		UNaviUDPPipe& target;
		bool svc2worker;
	};

	PipeNotifyEv* in_notify_ev;
	PipeNotifyEv* out_notify_ev;

	PipeNotifyEo* in_notify_eo;
	PipeNotifyEo* out_notify_eo;

	UNaviUDPPipe(const UNaviUDPPipe&);
	UNaviUDPPipe& operator=(const UNaviUDPPipe&);
};

UNAVI_NMSP_END

#endif /* UNAVIWORKERQUEUE_H_ */
