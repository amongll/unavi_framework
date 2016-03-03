/*
 * UNaviWorker.cpp
 *
 *  Created on: 2014-9-25
 *      Author: dell
 */

#include "unavi/core/UNaviWorker.h"
#include "unavi/core/UNaviIOSvc.h"
#include "unavi/core/UCoreException.h"
#include "unavi/frame/UOMBPeerApp.h"

using namespace std;
using namespace unavi;

uint16_t UNaviWorker::s_worker_id = 0;
UNaviWorker* UNaviWorker::s_workers[] = {NULL};

UNaviWorker* UNaviWorker::new_worker(UNaviCycle::Ref runer)
{
	if (s_worker_id == UNAVI_MAX_IOSVC)
		return NULL;
	vector<const UNaviHandler*> alreadys = runer->get_handlers();
	vector<const UNaviHandler*>::const_iterator it_h;
	for (it_h = alreadys.begin(); it_h != alreadys.end(); it_h++) {
		if ( dynamic_cast<const UNaviWorker*>(*it_h)) {
			return NULL; //cycle仅允许有一个worker
		}
	}

	UNaviWorker* ret = new UNaviWorker(runer);
	vector<UNaviIOSvc*> svcs = UNaviIOSvc::get_all();
	vector<UNaviIOSvc*>::iterator it;
	for (it=svcs.begin(); it!=svcs.end(); it++) {
		UNaviUDPPipe* pipe = new UNaviUDPPipe(**it,*ret);
	}
	s_workers[ret->worker_id] = ret;
	return ret;
}

int UNaviWorker::worker_count()
{
	return s_worker_id;
}

UNaviWorker* UNaviWorker::get_worker(uint16_t worker_id)
{
	return s_workers[worker_id];
}

UOMBPeerBusiEntry::UOMBPeerBusiEntry():
	obj(NULL),
	busi(NULL)
{}

void UOMBPeerBusiEntry::recycle()
{
	obj = NULL;
	busi = NULL;
}

void UNaviWorker::push_peer_business(UOMBPeerShadow* obj, UOMBPeerBusiness* busi)
{
	UOMBPeerBusiEntry* ppe = dynamic_cast<UOMBPeerBusiEntry*>(omb_peer_busi_pipe->recycled_push_node());
	if ( !ppe ) {
		ppe = new UOMBPeerBusiEntry;
		ppe->obj = obj;
		ppe->busi = busi;
	}

	omb_peer_busi_pipe->push(*ppe);
}

void UNaviWorker::recycle_all_workers()
{
	for (int i=0; i<s_worker_id; i++) {
		if (s_workers[i]) {
			delete s_workers[i];
		}
	}
}

vector<UNaviWorker*> UNaviWorker::get_all()
{
	vector<UNaviWorker*> ret;
	for (int i=0; i<s_worker_id; i++) {
		if (s_workers[i]) {
			ret.push_back(s_workers[i]);
		}
	}
	return ret;
}

UNaviWorker::UNaviWorker(UNaviCycle::Ref _cycle):
	UNaviHandler(_cycle.get()),
	worker_id(s_worker_id++),
	omb_peer_busi_pipe(NULL)
{
	memset(iosvc_pipes,0x00,sizeof(iosvc_pipes));
	memset(protos,0x00,sizeof(protos));

	int i;
	for (i=0; i<UNaviProtoTypes::proto_count(); i++) {
		protos[i] = UNaviProtoTypes::proto(i)->copy();
		if (protos[i] == NULL)
			throw UCoreException(UCORE_IMPL_ERROR, "Proto:%d copy() impl problem.", i);
	}

	omb_peer_busi_pipe = new UNaviHandlerSPipe(*this);
}

UNaviWorker::~UNaviWorker()
{
	for (int i=0; i<UNaviProtoTypes::proto_count(); i++) {
		if (!protos[i]) continue;
		protos[i]->cleanup();
		delete protos[i];
	}
	if (omb_peer_busi_pipe)
		delete omb_peer_busi_pipe;
	s_workers[worker_id] = NULL;
}

uint32_t UNaviWorker::rand_proto_pipeid(uint8_t proto_id)
{
	if (proto_id >= UNaviProtoTypes::max_protos)
		throw UCoreException(UCORE_ABUSED_ERROR, "Proto id:%d not declared. at<%s:%d>", proto_id,
			__PRETTY_FUNCTION__,__LINE__);
	vector<uint32_t>& ppids = s_workers[UNaviCycle::worker_id()]->proto_pipes[proto_id];
	if (ppids.size()==0)
		throw UCoreException(UCORE_IMPL_ERROR, "at<%s:%d>",
			__PRETTY_FUNCTION__,__LINE__);
	return ppids[::rand()%ppids.size()];
}

void UNaviWorker::run_init()
{
	UOMBPeerSvc::init_svc_map();

	for (int i=0; i<UNaviProtoTypes::proto_count(); i++) {
		protos[i]->run_init();
	}
}

void UNaviWorker::current_bandwidth(uint32_t pipe_id,double& in,double& out,uint32_t& inlimit,uint32_t& olimit)
{
	UNaviUDPPipe::PipeId pid;
	pid.pipe_id = pipe_id;
	s_workers[pid.pair[1]]->iosvc_pipes[pid.pair[0]]->bandwidth(in,out,inlimit,olimit);
}

int UNaviWorker::preset_mtu(uint32_t pipe_id)
{
	UNaviUDPPipe::PipeId pid;
	pid.pipe_id = pipe_id;
	return s_workers[pid.pair[1]]->iosvc_pipes[pid.pair[0]]->_mtu;
}

void UNaviWorker::cleanup()
{
	for (int i=0; i<UNaviProtoTypes::proto_count(); i++) {
		if (!protos[i]) continue;
		protos[i]->cleanup();
		delete protos[i];
	}

	for (int i=0; i<UNAVI_MAX_IOSVC; i++) {
		if (iosvc_pipes[i]) {
			iosvc_pipes[i]->close_work_notify();
		}
	}

	if ( omb_peer_busi_pipe ) {
		omb_peer_busi_pipe->close();
		delete omb_peer_busi_pipe;
		omb_peer_busi_pipe = NULL;
	}

	memset(protos,0x00,sizeof(protos));
	memset(iosvc_pipes,0x00,sizeof(iosvc_pipes));

	UOMBPeerSvc::clean_svc_map();
}

void UNaviWorker::process(UNaviUDPPipe& pipe)
{
	UNaviList ret;
	UNaviUDPPacket* pkt;
	while(true) {
		pipe.work_pull(ret);
		if (ret.empty())
			break;
		while(!ret.empty()) {
			pkt = dynamic_cast<UNaviUDPPacket*>(ret.get_head());
			pkt->quit_list();
			protos[pipe.proto_id]->process(*pkt);
		}
	}
}

void UNaviWorker::process(UNaviHandlerSPipe& pipe)
{
	if (&pipe != omb_peer_busi_pipe)
		return;
	UNaviList ret;
	UOMBPeerBusiEntry* ppe;
	while(true) {
		omb_peer_busi_pipe->pull(ret);
		if(ret.empty())
			break;
		while(!ret.empty()) {
			ppe = dynamic_cast<UOMBPeerBusiEntry*>(ret.get_head());
			ppe->quit_list();
			UOMBPeerApp::run_peer_business(*ppe->obj,*ppe->busi);
			omb_peer_busi_pipe->release_in(*ppe);
		}
	}
}

void UNaviWorker::regist_event(UNaviEvent& timer)
{
	s_workers[UNaviCycle::worker_id()]->event_regist(timer);
}

void UNaviWorker::quit_event(UNaviEvent& timer, uint32_t quit_mask)
{
	s_workers[UNaviCycle::worker_id()]->event_quit(timer,quit_mask);
}

