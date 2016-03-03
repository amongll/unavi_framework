/*
 * UNaviIOSvc.cpp
 *
 *  Created on: 2014-9-24
 *      Author: li.lei
 */

#include "unavi/core/UNaviIOSvc.h"
#include "unavi/core/UNaviWorker.h"
#include "unavi/util/UNaviLock.h"
#include "unavi/core/UNaviLog.h"
#include <fcntl.h>
#include "unavi/core/UCoreException.h"

using namespace std;
using namespace unavi;

uint16_t UNaviIOSvc::s_svc_id = 0;
UNaviIOSvc* UNaviIOSvc::svcs[] = {NULL};

UNaviIOSvc* UNaviIOSvc::new_svc(UNaviCycle::Ref r_cycle, UNaviCycle::Ref w_cycle,
	uint16_t mtu, uint8_t proto_id,const sockaddr* binded, int prev_thr)
{
	if (s_svc_id == UNAVI_MAX_IOSVC)
		return NULL;

	int socket_fd = ::socket(binded->sa_family,SOCK_DGRAM,0);
	if (socket_fd == -1)
		return NULL;
	//TODO: socket configuralbe option
	int sock_opti = 0;
	::setsockopt(socket_fd,SOL_SOCKET, SO_REUSEADDR, (const void*)&sock_opti,sizeof(int));
	sock_opti=1048576*10;
	::setsockopt(socket_fd,SOL_SOCKET,SO_RCVBUF,(void*)&sock_opti,sizeof(int));
	sock_opti=1048576*10;
	::setsockopt(socket_fd,SOL_SOCKET,SO_SNDBUF,(void*)&sock_opti,sizeof(int));

	//todo: 跨平台问题
	sock_opti = ::fcntl(socket_fd,F_GETFL,0);
	sock_opti |= O_NONBLOCK;
	if ( 0 != ::fcntl(socket_fd,F_SETFL,sock_opti) ) {
		::close(socket_fd);
		socket_fd = -1;
		UCoreException e(UCORE_SYS_ERROR,"%s", strerror(errno));
		uexception_throw(e, LOG_FATAL, -1);
	}

	if ( 0 != ::bind(socket_fd, binded,
		binded->sa_family==AF_INET?sizeof(sockaddr_in):sizeof(sockaddr_in6))) {
		::close(socket_fd);
		socket_fd = -1;
		UCoreException e(UCORE_SYS_ERROR,"%s", strerror(errno));
		uexception_throw(e, LOG_FATAL, -1);
	}

	UNaviIOSvc* ret = new UNaviIOSvc(r_cycle,w_cycle,mtu, proto_id, binded, socket_fd, prev_thr);
	vector<UNaviWorker*> workers = UNaviWorker::get_all();
	vector<UNaviWorker*>::iterator it;
	for (it=workers.begin(); it!=workers.end(); it++) {
		UNaviUDPPipe* pipe = new UNaviUDPPipe(*ret,**it);
	}
	svcs[ret->svc_id] = ret;
	return ret;
}

void UNaviIOSvc::recycle_all_svc()
{
	for (int i=0; i<s_svc_id; i++) {
		if (svcs[i]) {
			delete svcs[i];
		}
	}
}

vector<UNaviIOSvc*> UNaviIOSvc::get_all()
{
	vector<UNaviIOSvc*> ret;
	for (int i=0; i<s_svc_id; i++) {
		if (svcs[i]) {
			ret.push_back(svcs[i]);
		}
	}
	return ret;
}

UNaviIOSvc::UNaviIOSvc(UNaviCycle::Ref r_cycle, UNaviCycle::Ref w_cycle,
	uint16_t _mtu, uint8_t _proto_id,const sockaddr* binded, int fd, int disp_thr):
	reader(*this,r_cycle),
	writer(*this,w_cycle),
	mtu(_mtu),
	svc_id(s_svc_id++),
	proto_id(_proto_id),
	socket_fd(fd),
	free_pkts_cnt(0),
	ev_read(NULL),
	prev_cnt(0),
	inband_limit_KBPS(262144),
	outband_limit_KBPS(262144)
{
	if ( binded->sa_family == AF_INET ) {
		memcpy(&in_addr,binded,sizeof(sockaddr_in));
	}
	else if ( binded->sa_family == AF_INET6 ) {
		memcpy(&in6_addr,binded,sizeof(sockaddr_in6));
	}
	memset(worker_pipes,0x00,sizeof(worker_pipes));

	if (disp_thr>0) {
		if (disp_thr>32)
			disp_thr=32;
		prev_cnt = disp_thr;
		//init_prev_procs(disp_thr);
	}

	recv_hdr.msg_iov = read_v;
	recv_hdr.msg_iovlen  = 2;
	recv_hdr.msg_control = NULL;
	recv_hdr.msg_controllen = 0;

	read_v[0].iov_len = mtu;
	read_v[1].iov_len = 65536;
	read_v[1].iov_base = read_buf2;
}

UNaviIOSvc::~UNaviIOSvc()
{
	for (int i=0; i<UNAVI_MAX_WORKER; i++) {
		if (worker_pipes[i]) {
			delete worker_pipes[i];
		}
	}

	UNaviListable* pkt;
	while( !free_pkts.empty() ) {
		pkt = free_pkts.get_head();
		delete pkt;
	}

	if (socket_fd!=-1)
		::close(socket_fd);

	svcs[svc_id] = NULL;
}

UNaviIOReader::UNaviIOReader(UNaviIOSvc& o, UNaviCycle::Ref cycle):
	UNaviHandler(cycle.get()),
	svc(o),
	input_pkts(0),
	input_size(0),
	bandwidth(0),
	ev_stat(NULL)
{
}

UNaviIOReader::~UNaviIOReader()
{
	if (svc.ev_read) {
		event_quit(*svc.ev_read);
		delete svc.ev_read;
	}

	if (ev_stat) {
		event_quit(*ev_stat);
		delete ev_stat;
	}
}

void UNaviIOReader::run_init()
{
	svc.ev_read = new UNaviIOSvc::IOSvcReadEv(svc);
	event_regist(*svc.ev_read);
	ev_stat = new UNaviIOReader::IOSvcStatEv(*this);
	event_regist(*ev_stat);

	if (svc.prev_cnt>0) {
		svc.init_prev_procs(svc.prev_cnt);
	}
}

void UNaviIOReader::cleanup()
{
	event_quit(*svc.ev_read);
	delete svc.ev_read;
	svc.ev_read = NULL;

	event_quit(*ev_stat);
	delete ev_stat;
	ev_stat = NULL;

	UNaviListable* pkt;
	while( !svc.free_pkts.empty() ) {
		pkt = svc.free_pkts.get_head();
		delete pkt;
	}

	if (svc.prev_cnt)
		svc.destroy_prev_proces();
}

void UNaviIOReader::process(UNaviHandlerPipe& pipe)
{
	svc.process(pipe);
}

void UNaviIOReader::IOSvcStatEv::process_event()
{
	if (last_calc) {
		UNaviScopedLock lk(&target.band_lock);
		int64_t diff = UNaviCycle::curtime_diff_ml(last_calc);
		target.bandwidth = (double)target.input_size/diff/1024*1000;
		/*if (target.input_size) {
			printf("input: %f KB/s %d pkts/s\n", (double)target.input_size/diff/1024*1000,
				(int)((double)target.input_pkts/diff*1000));
		}*/
		target.input_size = 0;
		target.input_pkts = 0;
	}
	last_calc = UNaviCycle::curtime_ml();
	set_expire(1000000);
}

void UNaviIOSvc::process(UNaviHandlerPipe& pipe)
{
	UNaviList ret;
	UDPPrevEntry* e;
	while(true) {
		pipe.h1_pull(ret);
		if (ret.empty())
			break;
		while(!ret.empty()) {
			e = dynamic_cast<UDPPrevEntry*>(ret.get_head());
			e->quit_list();
			if (e->disp_info.type == UNaviProto::DISP_OK) {
				UNaviUDPPipe* upipe = worker_pipes[e->disp_info.session_disting % UNaviWorker::worker_count()];
				e->udp->pipe_id = upipe->pipe_id;
				upipe->io_push(*e->udp);
			}
			else {
				e->udp->reset(mtu);
				if (free_pkts_cnt < 5000 ) {
					free_pkts.insert_head(*e->udp);
					free_pkts_cnt++;
				}
				else
					delete e->udp;
			}
			pipe.h1_release_in(*e);
		}
	}
}

void UNaviIOSvc::process(UNaviUDPPipe& pipe)
{
	if ( socket_fd == -1)
		return;
	UNaviUDPPacket* pkt;
	UNaviList ret;
	while (true) {
		pipe.io_pull(ret);
		if (ret.empty())
			break;
		while(!ret.empty()) {
			pkt = dynamic_cast<UNaviUDPPacket*>(ret.get_head());
			pkt->quit_list();

			if (socket_fd != -1) {
				socklen_t peer_addr_len = pkt->sa_addr.sa_family==AF_INET?sizeof(sockaddr_in):
					sizeof(sockaddr_in6);
				ssize_t sret = ::sendto(socket_fd,pkt->buf,pkt->used,0,&pkt->sa_addr,peer_addr_len);
				if (sret == -1 || sret==0 ) {
					//TODO: log
					cout << strerror(errno) << " " << errno << endl;
				}
				else {
					writer.output_size += sret;
					writer.output_pkts ++;
				}
			}
			pipe.io_release_out(*pkt);
		}
	}
}

void UNaviIOSvc::close_all_pipenotify()
{
	for (int i=0; i<UNAVI_MAX_WORKER; i++) {
		if (worker_pipes[i]) {
			worker_pipes[i]->close_io_notify();
		}
	}
}

UNaviIOWriter::UNaviIOWriter(UNaviIOSvc& o, UNaviCycle::Ref cycle):
	UNaviHandler(cycle.get()),
	svc(o),
	output_size(0),
	output_pkts(0),
	bandwidth(0),
	ev_stat(NULL)
{

}

UNaviIOWriter::~UNaviIOWriter()
{
	if (ev_stat) {
		event_quit(*ev_stat);
		delete ev_stat;
	}
}

void UNaviIOWriter::process(UNaviUDPPipe& pipe)
{
	svc.process(pipe);
}

void UNaviIOWriter::run_init()
{
	ev_stat = new UNaviIOWriter::IOSvcStatEv(*this);
	event_regist(*ev_stat);
}

void UNaviIOWriter::cleanup()
{
	event_quit(*ev_stat);
	delete ev_stat;
	ev_stat=NULL;

	svc.close_all_pipenotify();
}

void UNaviIOWriter::IOSvcStatEv::process_event()
{
	if (last_calc) {
		UNaviScopedLock lk(&target.band_lock);
		int64_t diff = UNaviCycle::curtime_diff_ml(last_calc);
		target.bandwidth = (double)target.output_size/diff/1024*1000;
		/*
		if (target.output_size) {
			printf("output: %f KB/s %d pkts/s\n", (double)target.output_size/diff/1024*1000,
				(int)((double)target.output_pkts/diff*1000));
		}*/
		target.output_size = 0;
		target.output_pkts = 0;
	}
	last_calc = UNaviCycle::curtime_ml();
	set_expire(1000000);
}

void UNaviIOSvc::IOSvcReadEv::process_event()
{
	//TODO: 跨平台
	UNaviUDPPipe** pp;
	UNaviUDPPacket* pkt;

	UNaviProto::DispatchInfo disp;

	pp = svc.worker_pipes;
	while (*pp) {
		svc.free_pkts_cnt += (*pp)->io_recycled_pkts(svc.free_pkts);
		pp++;
	}

	while (svc.free_pkts_cnt > 32768) {
		pkt = dynamic_cast<UNaviUDPPacket*>(svc.free_pkts.get_head());
		delete pkt;
		svc.free_pkts_cnt--;
	}

	uint64_t start_mc = UNaviUtil::get_time_mc();

	bool read_blocked = false;
	do {
		UNaviList read_list;
		int read_cnt = 500;
		while(read_cnt--) {
			pkt = dynamic_cast<UNaviUDPPacket*>(svc.free_pkts.get_head());
			if (pkt) {
				svc.free_pkts_cnt --;
				pkt->quit_list();
			}
			else {
				pp = svc.worker_pipes;
				while (*pp) {
					svc.free_pkts_cnt += (*pp)->io_recycled_pkts(svc.free_pkts);
					pp++;
				}

				while (svc.free_pkts_cnt > 32768) {
					pkt = dynamic_cast<UNaviUDPPacket*>(svc.free_pkts.get_head());
					delete pkt;
					svc.free_pkts_cnt--;
				}

				pkt = dynamic_cast<UNaviUDPPacket*>(svc.free_pkts.get_head());
				if (pkt) {
					pkt->quit_list();
					svc.free_pkts_cnt--;
				}
				else
					pkt = new UNaviUDPPacket(svc.mtu);
			}

			svc.read_v[0].iov_base = pkt->buf;
			svc.recv_hdr.msg_namelen = (svc.sa_addr.sa_family==AF_INET)?sizeof(sockaddr_in):sizeof(sockaddr_in6);;
			svc.recv_hdr.msg_name = &pkt->sa_addr;

			ssize_t ret = ::recvmsg(svc.socket_fd,&svc.recv_hdr,0);

			if ( ret > pkt->mtu ) {
				pkt->used = pkt->mtu;
				pkt->set_packet_len(ret);
				::memcpy(pkt->buf + pkt->used, svc.read_v[1].iov_base, svc.read_v[1].iov_len);
				pkt->used = ret;
				svc.reader.input_size += ret;
				svc.reader.input_pkts++;
#ifdef DEBUG
				//模拟丢包
				if ( rand()%5 == 0 ) {
					pkt->reset(svc.mtu);
					if ( svc.free_pkts_cnt < 32768 ) {
						svc.free_pkts.insert_head(*pkt);
						svc.free_pkts_cnt++;
					}
					else {
						delete pkt;
					}
				}
				else {
					read_list.insert_tail(*pkt);
				}
#else
				read_list.insert_tail(*pkt);
#endif
			}
			else if ( ret > 0 ) {
				pkt->used = ret;
				svc.reader.input_size += ret;
				svc.reader.input_pkts++;
#ifdef DEBUG
				//模拟丢包
				if ( rand()%5 == 0 ) {
					pkt->reset(svc.mtu);
					if ( svc.free_pkts_cnt < 32768 ) {
						svc.free_pkts.insert_head(*pkt);
						svc.free_pkts_cnt++;
					}
					else {
						delete pkt;
					}
				}
				else {
					read_list.insert_tail(*pkt);
				}
#else
				read_list.insert_tail(*pkt);
#endif
			}
			else if (ret == -1) {
				if ( errno == EWOULDBLOCK) {
					//cout<<"would block readcount:"<<1000-read_cnt<<endl;
					pkt->reset(svc.mtu);
					svc.free_pkts.insert_head(*pkt);
					svc.free_pkts_cnt++;
					read_blocked = true;
					break;
				}
				else {
					//TODO: log
					pkt->reset(svc.mtu);
					svc.free_pkts.insert_head(*pkt);
					svc.free_pkts_cnt++;
					break;
				}
			}
			else  { // ret == 0
				//TODO: log
				pkt->reset(svc.mtu);
				svc.free_pkts.insert_head(*pkt);
				svc.free_pkts_cnt++;
				break;
			}
		}

		while (!read_list.empty()) {
			pkt = dynamic_cast<UNaviUDPPacket*>(read_list.get_head());
			pkt->quit_list();
			disp = pkt->dispatch(svc.proto_id);
			if ( disp.type == UNaviProto::DISP_DROP) {
				pkt->reset(svc.mtu);
				if (svc.free_pkts_cnt < 32768 ) {
					svc.free_pkts.insert_head(*pkt);
					svc.free_pkts_cnt++;
				}
				else
					delete pkt;
			}
			else if (disp.type == UNaviProto::DISP_OK ) {
				UNaviUDPPipe* pipe = svc.worker_pipes[disp.session_disting % UNaviWorker::worker_count()];
				pkt->pipe_id = pipe->pipe_id;
				pipe->io_push(*pkt);
			}
			else if (svc.prev_procs.size()==0) {
				//没有预处理线程，但是要求做prev_proc
				UNaviProto* proto =  UNaviProtoTypes::proto(svc.proto_id);
				proto->preproc(*pkt);
				disp = proto->dispatch(*pkt,true);
				if (disp.type == UNaviProto::DISP_OK) {
					UNaviUDPPipe* pipe = svc.worker_pipes[disp.session_disting % UNaviWorker::worker_count()];
					pkt->pipe_id = pipe->pipe_id;
					pipe->io_push(*pkt);
				}
				else {
					pkt->reset(svc.mtu);
					if (svc.free_pkts_cnt < 32768 ) {
						svc.free_pkts.insert_head(*pkt);
						svc.free_pkts_cnt++;
					}
					else {
						delete pkt;
					}
				}
			}
			else {
				//随机发送到某个预处理线程
				UNaviHandlerPipe* ppp = svc.prev_pipes[rand()%svc.prev_pipes.size()];
				UDPPrevEntry* pe = dynamic_cast<UDPPrevEntry*>(ppp->h1_recycled_out());
				if (!pe) pe = new UDPPrevEntry;
				pe->udp = pkt;
				ppp->h1_push(*pe);
			}
		}
		if (read_blocked)break;
	} while( UNaviUtil::get_time_mc() - start_mc < 500);

	return;
}

void UNaviIOSvc::init_prev_procs(int cnt)
{
	while(cnt--)
	{
		UNaviCycle::Ref cycle = UNaviCycle::new_cycle();
		UInputPProcor* procor = new UInputPProcor(cycle,*this);
		UNaviHandlerPipe* pp = new UNaviHandlerPipe(reader,*procor);

		if (!cycle->thread_start())
			throw UCoreException(UCORE_SYS_ERROR, "IOSvc start prev_proc threads failed");

		prev_cycles.push_back(cycle);
		prev_pipes.push_back(pp);
		prev_procs.push_back(procor);
	}
}

void UNaviIOSvc::destroy_prev_proces()
{
	if (prev_cycles.size()) {
		for(int i=0; i<prev_pipes.size(); i++) {
			prev_cycles[i]->stop();
			prev_pipes[i]->h1_close();
			prev_pipes[i]->h2_close();
		}

		for(int i=0; i<prev_cycles.size(); i++) {
			while(!prev_cycles[i]->thread_wait(1000000));
		}

		for(int i=0; i<prev_procs.size(); i++)
			delete prev_procs[i];
		for(int i=0; i<prev_pipes.size(); i++)
			delete prev_pipes[i];
		prev_cycles.clear();
	}
}

//nothing to to
void UDPPrevEntry::recycle()
{
	udp = NULL;
	disp_info.type = UNaviProto::DISP_DROP;
	disp_info.session_disting = 0;
}

UInputPProcor::UInputPProcor(UNaviCycle::Ref cycle,UNaviIOSvc& _svc):
	UNaviHandler(cycle.get()),
	svc(_svc),
	proto(UNaviProtoTypes::proto(svc.proto_id)->copy())
{
}

UInputPProcor::~UInputPProcor()
{
	delete proto;
}

void UInputPProcor::run_init()
{
}

void UInputPProcor::cleanup()
{
}

void UInputPProcor::process(UNaviHandlerPipe& pipe)
{
	UNaviList ret;
	UDPPrevEntry* pkt, *pkt2;
	while(true) {
		pipe.h2_pull(ret);
		if (ret.empty())
			break;
		while(!ret.empty()) {
			pkt = dynamic_cast<UDPPrevEntry*>(ret.get_head());
			pkt->quit_list();

			pkt2 = dynamic_cast<UDPPrevEntry*>(pipe.h2_recycled_out());
			if (pkt2==NULL) {
				pkt2 = new UDPPrevEntry;
			}
			pkt2->udp = pkt->udp;
			pipe.h2_release_in(*pkt);

			proto->preproc(*pkt2->udp);
			pkt2->disp_info = proto->dispatch(*pkt2->udp,true);
			pipe.h2_push(*pkt2);
		}
	}
}

