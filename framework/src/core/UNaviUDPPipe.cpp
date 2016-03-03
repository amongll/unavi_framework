/*
 * UNaviUDPPipe.cpp
 *
 *  Created on: 2014-9-24
 *      Author: dell
 */

#include "unavi/core/UNaviUDPPipe.h"
#include "unavi/core/UNaviCycle.h"
#include <fcntl.h>
#include "unavi/core/UNaviWorker.h"
#include "unavi/core/UNaviIOSvc.h"
#include "unavi/core/UCoreException.h"

using namespace std;
using namespace unavi;

UNaviUDPPipe::UNaviUDPPipe(UNaviIOSvc& iosvc,UNaviWorker& _worker):
	svc(iosvc),
	worker(_worker),
	queue_sync_r(true),
	queue_sync_w(true),
	_mtu(iosvc.mtu),
	proto_id(iosvc.proto_id),
	work_in_free_cnt(0),
	io_in_free_cnt(0),
	work_out_free_cnt(0),
	io_out_free_cnt(0),
	work_out_free_cnt2(0),
	in_notify_ev(NULL),
	in_notify_eo(NULL),
	out_notify_ev(NULL),
	out_notify_eo(NULL),
	out_notify_proced(true),
	in_notify_proced(true),
	work_push_listsz(0),
	io_push_listsz(0),
	out_peer_closed(false),
	in_peer_closed(false)
{
	handler_pair[0] = svc.svc_id;
	handler_pair[1] = worker.worker_id;
	if (svc.writer.cycle == worker.cycle) {
		queue_sync_w = false;
	}
	else {
		init_work_notify();
	}
	if (svc.reader.cycle == worker.cycle) {
		queue_sync_r = false;
	}
	else {
		init_io_notify();
	}
	svc.worker_pipes[worker.worker_id] =
	worker.iosvc_pipes[svc.svc_id] = this;
	worker.proto_pipes[proto_id].push_back(pipe_id);

	pthread_cond_init(&in_notify_cond,NULL);
	pthread_cond_init(&out_notify_cond,NULL);
}


UNaviUDPPipe::~UNaviUDPPipe()
{
	UNaviUDPPacket* u;
	while( true ) {
		u = dynamic_cast<UNaviUDPPacket*>(work_pull_list.get_head());
		if (!u) break;
		delete u;
	}
	while( true ) {
		u = dynamic_cast<UNaviUDPPacket*>(work_in_free.get_head());
		if (!u) break;
		delete u;
	}
	while( true ) {
		u = dynamic_cast<UNaviUDPPacket*>(work_out_free.get_head());
		if (!u) break;
		delete u;
	}
	while( true ) {
		u = dynamic_cast<UNaviUDPPacket*>(work_out_free_2.get_head());
		if (!u) break;
		delete u;
	}
	while( true ) {
		u = dynamic_cast<UNaviUDPPacket*>(work_push_list.get_head());
		if (!u) break;
		delete u;
	}
	while( true ) {
		u = dynamic_cast<UNaviUDPPacket*>(io_push_list.get_head());
		if (!u) break;
		delete u;
	}
	while( true ) {
		u = dynamic_cast<UNaviUDPPacket*>(io_pull_list.get_head());
		if (!u) break;
		delete u;
	}
	while( true ) {
		u = dynamic_cast<UNaviUDPPacket*>(io_in_free.get_head());
		if (!u) break;
		delete u;
	}
	while( true ) {
		u = dynamic_cast<UNaviUDPPacket*>(io_out_free.get_head());
		if (!u) break;
		delete u;
	}
	if (queue_sync_r) {
		delete in_notify_ev;
		delete in_notify_eo;
		::close(in_notify_pipe[0]);
		::close(in_notify_pipe[1]);
		in_notify_pipe[1] = -1;
		in_notify_pipe[0] = -1;
	}

	if (queue_sync_w) {
		delete out_notify_ev;
		delete out_notify_eo;
		::close(out_notify_pipe[0]);
		::close(out_notify_pipe[1]);
		out_notify_pipe[1] = -1;
		out_notify_pipe[0] = -1;
	}

	pthread_cond_destroy(&in_notify_cond);
	pthread_cond_destroy(&out_notify_cond);
}

void UNaviUDPPipe::init_io_notify()
{
	if ( 0 != pipe(in_notify_pipe) ) {
		throw UCoreException(UCORE_SYS_ERROR, "UDPIOPipe pipe() failed. %d %s", errno, strerror(errno));
	}
	int iflag = ::fcntl(in_notify_pipe[0],F_GETFL,0);
	iflag |= O_NONBLOCK;
	::fcntl(in_notify_pipe[0],F_SETFL,iflag);
	iflag = ::fcntl(in_notify_pipe[1],F_GETFL,0);
	iflag |= O_NONBLOCK;
	::fcntl(in_notify_pipe[1],F_SETFL,iflag);

	in_notify_ev = new PipeNotifyEv(*this,true);
	worker.event_regist(*in_notify_ev);
	in_notify_eo = new PipeNotifyEo(*this,true);
	svc.writer.event_regist(*in_notify_eo);

}

void UNaviUDPPipe::init_work_notify()
{
	if ( 0 != pipe(out_notify_pipe) ) {
		throw UCoreException(UCORE_SYS_ERROR, "UDPIOPipe pipe() failed. %d %s", errno, strerror(errno));
	}
	int iflag = ::fcntl(out_notify_pipe[0],F_GETFL,0);
	iflag |= O_NONBLOCK;
	::fcntl(out_notify_pipe[0],F_SETFL,iflag);
	iflag = ::fcntl(out_notify_pipe[1],F_GETFL,0);
	iflag |= O_NONBLOCK;
	::fcntl(out_notify_pipe[1],F_SETFL,iflag);

	out_notify_ev = new PipeNotifyEv(*this,false);
	svc.writer.event_regist(*out_notify_ev);
	out_notify_eo = new PipeNotifyEo(*this,false);
	svc.reader.event_regist(*out_notify_eo);
}

void UNaviUDPPipe::PipeNotifyEo::process_event()
{
	if (in) {
		target._io_pipe_notify();
	}
	else {
		target._work_pipe_notify();
	}
}

void UNaviUDPPipe::PipeNotifyEv::process_event()
{
	//TODO: ¿çÆ½Ì¨
	char a;
	int ret;
	int read_fd = (int)(svc2worker?target.in_notify_pipe[0]:target.out_notify_pipe[0]);
	if (read_fd != -1) {
		ret = ::read(read_fd,(void*)&a,1);
		if (ret<=0) {
			if (ret==0 || (errno!=EWOULDBLOCK&&errno!=EAGAIN)) {
				if (svc2worker)
					target.in_notify_pipe[0] = -1;
				else
					target.out_notify_pipe[0] = -1;
				::close(read_fd);
			}
		}
	}

	if (svc2worker) {
		UNaviScopedLock lk(&target.in_notify_statuslk);
		target.in_notify_proced = true;
		pthread_cond_signal(&target.in_notify_cond);
		lk.reset();

		target.worker.process(target);
	}
	else {
		UNaviScopedLock lk(&target.out_notify_statuslk);
		target.out_notify_proced = true;
		pthread_cond_signal(&target.out_notify_cond);
		lk.reset();

		target.svc.process(target);
	}
}


void UNaviUDPPipe::work_notify(uint32_t pipesz)
{
	if (queue_sync_w) {
		if (pipesz >= 4096) {
			if (out_notify_eo->expire_active())
				worker.event_quit(*out_notify_eo);
			_work_pipe_notify(true);
		}
		else {
			if (out_notify_eo->expire_active() == false) {
				out_notify_eo->set_expire(100);
			}
		}
	}
	else {
		svc.process(*this);
	}
}

void UNaviUDPPipe::io_notify(uint32_t ppsz)
{
	if (queue_sync_r) {
		if ( ppsz >= 4096) {
			if (in_notify_eo->expire_active())
				svc.writer.event_quit(*in_notify_eo);
			_io_pipe_notify(true);
		}
		else {
			if (in_notify_eo->expire_active()==false) {
				in_notify_eo->set_expire(100);
			}
		}
	}
	else {
		worker.process(*this);
	}
}

void UNaviUDPPipe::_work_pipe_notify(bool wait)
{
	char a='\0';
	int ret;

	if (out_notify_pipe[1]!=-1) {

		UNaviScopedLock lk(&out_notify_statuslk);

		if (out_peer_closed == true) return;

		if ( out_notify_proced == false ) {
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

			pthread_cond_signal(&in_notify_cond);
			pthread_cond_timedwait(&out_notify_cond,&out_notify_statuslk.impl,&to);
			if (out_peer_closed || out_notify_proced == false) return;
		}
		out_notify_proced = false;
		lk.reset();

		while(1) {
			ret = ::write(out_notify_pipe[1],(const void*)&a,1);
			if (ret==-1 && (errno==EWOULDBLOCK||errno==EAGAIN)) {
				timespec ts;
				ts.tv_sec = 0;
				ts.tv_nsec = 10;
				if (-1==nanosleep(&ts,NULL))
					break;
			}
			else
				break;
		}
		//UNaviScopedLock lk2(&out_notify_statuslk);
	}
}

void UNaviUDPPipe::_io_pipe_notify(bool wait)
{
	char a='\0';
	int ret;
	if (in_notify_pipe[1]!=-1) {

		UNaviScopedLock lk(&in_notify_statuslk);

		if (in_peer_closed) return;

		if ( in_notify_proced == false ) {
			if (wait == false ) return;
			timespec to;
			timeval tv;
			::gettimeofday(&tv,NULL);
			if (tv.tv_usec >= 999900 ) {
				to.tv_sec = tv.tv_sec+1;
				to.tv_nsec = (tv.tv_usec+900)%1000000 * 1000;
			}
			else {
				to.tv_sec = tv.tv_sec;
				to.tv_nsec = (tv.tv_usec+900) * 1000;
			}
			pthread_cond_timedwait(&in_notify_cond,&in_notify_statuslk.impl,&to);
			if (in_peer_closed || in_notify_proced == false) return;
		}
		in_notify_proced = false;
		lk.reset();

		while(1) {
			ret = ::write(in_notify_pipe[1],(const void*)&a,1);
			if (ret==-1 && (errno==EWOULDBLOCK||errno==EAGAIN)) {
				timespec ts;
				ts.tv_sec = 0;
				ts.tv_nsec = 10;
				if (-1==nanosleep(&ts,NULL))
					break;
			}
			else
				break;
		}
	}
}


void UNaviUDPPipe::close_io_notify()
{
	//if (queue_sync_r) {
		//::close(in_notify_pipe[1]);
		//in_notify_pipe[1] = -1;
	//	UNaviScopedLock lk(&in_notify_statuslk);
	//	pthread_cond_signal(&in_notify_cond);
	//}

	UNaviScopedLock lk(&out_notify_statuslk);
	out_notify_proced = true;
	out_peer_closed = true;
	pthread_cond_signal(&out_notify_cond);
}

void UNaviUDPPipe::close_work_notify()
{
	//if (queue_sync_w) {
		//::close(out_notify_pipe[1]);
		//out_notify_pipe[1] = -1;
	//	pthread_cond_signal(&in_notify_cond);
	//	pthread_cond_signal(&out_notify_cond);
	//}
	UNaviScopedLock lk(&in_notify_statuslk);
	in_notify_proced = true;
	in_peer_closed = true;
	pthread_cond_signal(&in_notify_cond);
}

void UNaviUDPPipe::work_pull(UNaviList& ret)
{
	if (!work_pull_list.empty()) {
		ret.take(work_pull_list);
		return;
	}
	else if (!queue_sync_r)
		return;

	UNaviUDPPacket* u;
	UNaviScopedLock lk2(&inqueue_sync);
	if ( io_push_list.empty() ) return ;
	work_pull_list.swap(io_push_list);
	io_push_listsz=0;

	work_in_free.giveto(io_in_free);
	io_in_free_cnt+=work_in_free_cnt;
	work_in_free_cnt=0;

	lk2.reset();

	ret.take(work_pull_list);
	return ;
}

void UNaviUDPPipe::work_release_in(UNaviUDPPacket& ipkt)
{
	ipkt.reset(_mtu);
	if ( queue_sync_r ) {

		if (work_in_free_cnt >= 8192) {
			delete &ipkt;
			return;
		}
		work_in_free.insert_tail(ipkt);
		work_in_free_cnt++;
	}
	else {
		if (io_in_free_cnt < 8192 ) {
			io_in_free.insert_tail(ipkt);
			io_in_free_cnt++;
			//io_in_free.print_list('h');
		}
		else
			delete &ipkt;
	}
}

size_t UNaviUDPPipe::io_recycled_pkts(UNaviList& dst)
{
	UNaviScopedLock lk(queue_sync_r?&inqueue_sync:NULL);
	if (io_in_free.empty()) {
		return 0;
	}

	dst.take(io_in_free);
	size_t tmp = io_in_free_cnt;
	io_in_free_cnt = 0;
	return tmp;
}

void UNaviUDPPipe::io_push(UNaviUDPPacket& ipkt)
{
	uint32_t pipe_sz=0;
	if (queue_sync_r) {
		UNaviScopedLock lk(&inqueue_sync);
		io_push_list.insert_tail(ipkt);
		pipe_sz = ++io_push_listsz;
	}
	else {
		work_pull_list.insert_tail(ipkt);
	}
	io_notify(pipe_sz);
}

void UNaviUDPPipe::io_pull(UNaviList& ret)
{
	if (!io_pull_list.empty()) {
		ret.take(io_pull_list);
		return ;
	}
	else if(!queue_sync_w)
		return ;

	UNaviScopedLock lk2(&outqueue_sync);
	UNaviUDPPacket* u;
	if ( work_push_list.empty() ) return ;
	io_pull_list.swap(work_push_list);
	work_push_listsz=0;
	work_out_free.take(io_out_free);
	work_out_free_cnt+=io_out_free_cnt;
	io_out_free_cnt=0;
	lk2.reset();

	ret.take(io_pull_list);
	return ;
}

UNaviUDPPacket* UNaviUDPPipe::work_new_output(size_t sz)
{
	UNaviUDPPacket* u = dynamic_cast<UNaviUDPPacket*>(work_out_free_2.get_head());
	if (u) {
		u->quit_list();
		u->set_packet_len(sz);
		return u;
	}

	UNaviScopedLock lk(queue_sync_w?&outqueue_sync:NULL);
	work_out_free_2.take(work_out_free);
	work_out_free_cnt2+= work_out_free_cnt;
	work_out_free_cnt = 0;
	lk.reset();

	while (work_out_free_cnt2 > 16384) {
		u = dynamic_cast<UNaviUDPPacket*>(work_out_free_2.get_head());
		delete u;
		work_out_free_cnt2--;
	}

	u = dynamic_cast<UNaviUDPPacket*>(work_out_free_2.get_head());
	if ( u ) {
		u->quit_list();
		u->set_packet_len(sz);
		return u;
	}
	return new UNaviUDPPacket(_mtu>sz?_mtu:sz,pipe_id);
}

void UNaviUDPPipe::work_push(UNaviUDPPacket& opkt)
{
	uint32_t pipesz=0;
	if (queue_sync_w) {
		UNaviScopedLock lk(&outqueue_sync);
		work_push_list.insert_tail(opkt);
		pipesz = ++work_push_listsz;
	}
	else {
		io_pull_list.insert_tail(opkt);
	}
	work_notify(pipesz);
}

void UNaviUDPPipe::io_release_out(UNaviUDPPacket& opkt)
{
	opkt.reset(_mtu);
	if (queue_sync_w) {
		if (io_out_free_cnt >= 8192) {
			delete &opkt;
			return;
		}
		io_out_free_cnt++;
		io_out_free.insert_tail(opkt);
	}
	else {
		if (work_out_free_cnt < 8192 ) {
			work_out_free.insert_tail(opkt);
			work_out_free_cnt++;
		}
		else {
			delete &opkt;
		}
	}
}

UNaviUDPPipe* UNaviUDPPipe::get_pipe(uint32_t pipeid)
{
	PipeId pid;
	pid.pipe_id = pipeid;
	UNaviIOSvc* svc = UNaviIOSvc::get_svc(pid.pair[0]);
	if (!svc || pid.pair[1] >= UNaviWorker::s_worker_id ) return NULL;
	return svc->worker_pipes[pid.pair[1]];
}

void UNaviUDPPipe::bandwidth(double& inband,double& outband, uint32_t& inlimit, uint32_t& outlimit)
{
	UNaviScopedLock lk(&svc.reader.band_lock);
	inband = svc.reader.bandwidth;
	lk.reset();

	UNaviScopedLock lk2(&svc.writer.band_lock);
	outband = svc.writer.bandwidth;
	lk.reset();

	UNaviScopedLock lk3(&svc.band_config_lk);
	inlimit = svc.inband_limit_KBPS;
	outlimit = svc.outband_limit_KBPS;
}
