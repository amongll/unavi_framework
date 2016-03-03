/*
 * URtmfpOflow.cpp
 *
 *  Created on: 2014-11-10
 *      Author: li.lei
 */

#include "unavi/rtmfp/URtmfpOflow.h"
#include "unavi/rtmfp/URtmfpSession.h"
#include "unavi/rtmfp/URtmfpFlowChunk.h"
#ifndef RTMFP_SESSION_DEBUG
#include "unavi/rtmfp/URtmfpProto.h"
#else
#include "unavi/rtmfp/URtmfpDebugProto.h"
#endif

using namespace std;
using namespace unavi;
using namespace urtmfp;

SendPendingMsg::SendPendingMsg():
	content(NULL),
	user_set_to(0),
	size(0),
	used(0),
	flag(0)
{
}

SendPendingMsg::~SendPendingMsg()
{
	if(content)delete []content;
}

SendBufEntry::~SendBufEntry()
{
	if (chunk_data)delete []chunk_data;
}

SendBufEntry::SendBufEntry(URtmfpOflow& flow):
	UNaviEvent(),
	seq_id(0),
	chunk_data(NULL),
	flag(0),
	used(0),
	size(0),
	resend_count(0),
	resend_rtt(0),
	user_set_to(0),
	target(flow)
{
}

void SendBufEntry::process_event()
{
	//该事件发生时，是确实发生了超时, 在ack返回时，清理SendBuf项超时事件自动清理

	bool abort = false;
	if ( user_set_to > 0 ) {
		if ( expire_at/1000 >= user_set_to ) {
			abort = true;
		}
	}
	else {
		if ( resend_rtt >= URtmfpOflow::Max_backoff_rtt || resend_count >= URtmfpOflow::Max_backoff_count )
			abort = true;
	}

	if ( abort ) {
		target.cleanup_onfly(seq_id);
		target.session.send_timedout(target.flow_id);
		return;
	}
	else {
		resend_count++;
		//resend_rtt保存有一下次重发的超时设置
		resend_rtt = target.rtt_backoff(resend_rtt);
		target.resend_buf_entry(*this);
		//todo: 发送

		if ( user_set_to > 0 ) {
			uint64_t cur_ml = UNaviCycle::curtime_ml();
			if ( cur_ml + resend_rtt > user_set_to ) {
				set_expire_at(user_set_to * 1000);
				return;
			}
		}
		set_expire(resend_rtt * 1000);
		return;
	}
}

//对那些未重发过的，且受到更大seq id的缓冲项目立即重发(10ms之后)。NACK逻辑
void URtmfpOflow::trigger_nack_resend(uint64_t max_acked)
{
	FlyIter it_e = onfly_buf.upper_bound(max_acked);
	FlyIter it;
	for ( it = onfly_buf.begin(); it != it_e; it++) {
		if ( it->second->resend_count > 0)
			continue;

		uint64_t cur_mc = UNaviCycle::curtime_mc();
		if ( cur_mc < it->second->expire_at && it->second->expire_at - cur_mc > 10000 ) {
			it->second->set_expire(10000); //10ms之后重发
		}
	}
}

URtmfpOflow::URtmfpOflow(URtmfpSession& ssn, uint32_t flowid):
	URtmfpFlow(ssn,flowid,false),
	recycled_onfly_cnt(0),
	recycled_pending_cnt(0),
	acked(false),
	fsn(0),
	seq_gen(0),
	notified_buf_sz(session.proto.cur_suggest_sendbuf(session.pipe_id)),
	onfly_sz(0),
	pending_count(0),
	pending_size(0),
	finished(false),
	is_rejected(false),
	buf_prober(*this)
{
}

URtmfpOflow::~URtmfpOflow()
{
	FlyIter it;
	for ( it = onfly_buf.begin(); it != onfly_buf.end(); it++ ) {
		delete it->second;
	}

	while( !recycled_onfly_nodes.empty() ) {
		SendBufEntry* e = dynamic_cast<SendBufEntry*>(recycled_onfly_nodes.get_head());
		e->quit_list();
		delete e;
	}

	while ( !pending_msgs.empty() ) {
		SendPendingMsg* e = dynamic_cast<SendPendingMsg*>(pending_msgs.get_head());
		e->quit_list();
		delete e;
	}

	while ( !recycled_pending_nodes.empty() ) {
		SendPendingMsg* e = dynamic_cast<SendPendingMsg*>(recycled_pending_nodes.get_head());
		e->quit_list();
		delete e;
	}
}

void URtmfpOflow::set_user_meta(const uint8_t* meta, uint32_t size)
{
	URtmfpOpt* opt  = opts.push_opt(0,meta,size);
	opt->dup();
}

void URtmfpOflow::relate_inflow(URtmfpIflow* flow, FlowRelateDir relate_dir)
{
	if (relate_dir == ACTIVE_RELATING) {
		relating = flow;
		uint8_t vln_buf[MAX_VLN_SIZE];
		uint32_t sz = URtmfpUtil::vln_encode(flow->flow_id,vln_buf);
		URtmfpOpt* opt = opts.push_opt(0x0a,vln_buf,sz);
		opt->dup();
	}
	else if (relate_dir == POSITIVE_RELATED) {
		related.push_back(flow);
	}
}

void URtmfpOflow::cum_ack(uint64_t cum_seq, int notify_buf)
{
	acked = true;
	if ( notify_buf > 0) {
		if ( notified_buf_sz <= 0) {
			if (buf_prober.expire_active())
				UNaviWorker::quit_event(buf_prober);
		}
		notified_buf_sz = notify_buf;
	}
	else if (notify_buf == 0 ) {
		notified_buf_sz = 0;
		if (buf_prober.expire_active() == false) {
			buf_prober.set_expire(100000);
		}
	}

	FlyIter it_e = onfly_buf.upper_bound(cum_seq);
	FlyIter it_b = onfly_buf.begin();

	if (it_e != it_b) {
		do {
			onfly_sz -= it_b->second->used;
			recycle_buf_entry(it_b->second);
			it_b++;
		} while( it_e != it_b);
		onfly_buf.erase(onfly_buf.begin(),it_e);
	}

	FlyIter it = onfly_buf.begin();
	if ( it != onfly_buf.end())
		fsn = it->second->seq_id - 1;
	else
		fsn = seq_gen;

	if ( pending_count > 0 ) { //pending_count大于零说明缓冲区曾满
		do {
			SendPendingMsg* msg = dynamic_cast<SendPendingMsg*>(pending_msgs.get_head());
			if ( available() > 0) {
				msg->quit_list();
				pending_count--;
				pending_size -= msg->used;
				session.send_pending_msg(*this, *msg);
				recycle_pending_msg(msg);
			}
			else
				break;
		} while (!pending_msgs.empty());

		if ( pending_msgs.empty() && available() ) {
			session.writable(this);
		}
	}
}

void URtmfpOflow::BufferProber::process_event()
{
	UBufferProbeChunk chunk(target.flow_id);
	target.session.send_sys_chunk(chunk);
	target.session.last_out_tm = UNaviCycle::curtime_ml();
	if ( target.notified_buf_sz == 0)
		set_expire(100000);
}

void URtmfpOflow::ack_range(uint64_t start, uint64_t end)
{
	if ( start > end) return;

	FlyIter it_b = onfly_buf.lower_bound(start);
	FlyIter it_e = onfly_buf.upper_bound(end);
	FlyIter it_h = it_b;

	if ( it_b != it_e ) {
		for ( ; it_b != it_e ; it_b++) {
			cleanup_onfly(*(it_b->second));
		}

		onfly_buf.erase(it_h,it_e);
	}
}

void URtmfpOflow::push_pending(const uint8_t* content, uint32_t sz,
    int user_to, uint8_t flag)
{
	SendPendingMsg* msg = get_pending_msg();
	msg->flag = flag;
	uint32_t rtt = session.get_rtt();
	if ( user_to > 0) {
		uint64_t cur_ml = UNaviCycle::curtime_ml();
		msg->user_set_to = user_to < rtt ? (cur_ml + rtt): (cur_ml+user_to);
	}

	if ( msg->size < sz) {
		if (msg->content)
			delete []msg->content;
		msg->content = new uint8_t[sz];
		msg->size = sz;
	}
	::memcpy(msg->content, content, sz);
	msg->used = sz;
	pending_msgs.insert_tail(*msg);
	pending_count++;
	pending_size+= sz;
}

void URtmfpOflow::forword_onfly()
{
	FlyIter it = onfly_buf.begin();
	if ( it == onfly_buf.end() )
		return;

	SendBufEntry* e = it->second;
	recycle_buf_entry(e);
	onfly_buf.erase(it);

	it = onfly_buf.begin();
	if (it == onfly_buf.end()) {
		fsn = seq_gen;
	}
	else {
		fsn = it->second->seq_id - 1;
	}

	if ( pending_count > 0 ) {
		do {
			SendPendingMsg* msg = dynamic_cast<SendPendingMsg*>(pending_msgs.get_head());
			if ( available() > 0) {
				msg->quit_list();
				pending_count--;
				pending_size -= msg->used;
				session.send_pending_msg(*this, *msg);
				recycle_pending_msg(msg);
			}
			else
				break;
		} while (!pending_msgs.empty());
	}
}

void URtmfpOflow::push_onfly(uint64_t seq, const uint8_t* content,
    uint32_t sz, int user_to, uint8_t flag)
{
	SendBufEntry* fly = get_buf_entry();
	fly->flag = flag;
	uint32_t rtt = session.get_rtt();
	if (user_to > 0) {
		uint64_t cur_ml = UNaviCycle::curtime_ml();
		fly->user_set_to = user_to < rtt ? (cur_ml + rtt): (cur_ml+user_to);
	}

	fly->seq_id = seq;
	fly->resend_rtt = rtt;

	if (fly->size < sz) {
		if (fly->chunk_data)delete []fly->chunk_data;
		fly->chunk_data = new uint8_t[sz];
		fly->size = sz;
	}
	::memcpy(fly->chunk_data, content, sz);
	fly->used = sz;

	pair<FlyIter,bool> iret = onfly_buf.insert(make_pair(seq, fly));
	if (iret.second == false) {
		recycle_buf_entry(iret.first->second);
		iret.first->second = fly;
	}

	onfly_sz += sz;

	//设置超时事件。
	UNaviWorker::regist_event(*fly);
	fly->set_expire(rtt*1000);
}

//两种情形可以驱动onfly buf的清理： 重传达到上限，或者ack抵达
void URtmfpOflow::cleanup_onfly(uint64_t seq)
{
	FlyIter it = onfly_buf.find(seq);
	if ( it == onfly_buf.end() )
		return;

	SendBufEntry* e = it->second;
	onfly_buf.erase(it);
	cleanup_onfly(*e);
}

void URtmfpOflow::cleanup_onfly(SendBufEntry& e)
{
	if ( e.seq_id == fsn + 1) {
		FlyIter it = onfly_buf.begin();
		if ( it != onfly_buf.end())
			fsn = it->second->seq_id - 1;
		else
			fsn = e.seq_id;
	}

	onfly_sz -= e.used;
	recycle_buf_entry(&e);

	//检查是否有Pending消息待发送, 在avaialbe空间足够时，发送pending消息
	if ( pending_count > 0 ) {
		do {
			SendPendingMsg* msg = dynamic_cast<SendPendingMsg*>(pending_msgs.get_head());
			if ( available() > 0) {
				msg->quit_list();
				pending_count--;
				pending_size -= msg->used;
				session.send_pending_msg(*this, *msg);
				recycle_pending_msg(msg);
			}
			else
				break;
		} while (!pending_msgs.empty());

		if ( pending_msgs.empty() && available() ) {
			session.writable(this);
		}
	}
}

int URtmfpOflow::available()
{
	uint32_t sugg = session.proto.cur_suggest_sendbuf(session.pipe_id);
	if ( notified_buf_sz >= 0 && sugg > notified_buf_sz ) {
		sugg = notified_buf_sz;
	}

	if ( onfly_sz >= sugg )
		return 0;
	else {
		return (sugg - onfly_sz);
	}
}

uint64_t URtmfpOflow::rtt_backoff(uint64_t last_rtt)
{
	uint64_t ret = last_rtt*1.8 + last_rtt*(0.003 * (rand()%100));
	if ( ret >= Max_backoff_rtt ) return Max_backoff_rtt;
	return ret;
}

SendBufEntry* URtmfpOflow::get_buf_entry()
{
	if ( recycled_onfly_nodes.empty() ) {
		return new SendBufEntry(*this);
	}
	SendBufEntry* e = dynamic_cast<SendBufEntry*>(recycled_onfly_nodes.get_head());
	e->quit_list();
	recycled_onfly_cnt--;
	return e;
}

void URtmfpOflow::recycle_buf_entry(SendBufEntry* nd)
{
	if ( recycled_onfly_cnt > 100 ) {
		delete nd;
		return;
	}
	nd->seq_id = 0;
	nd->flag = 0;
	nd->used = 0;
	nd->resend_count = 0;
	nd->resend_rtt = 0;
	nd->user_set_to = 0;
	UNaviWorker::quit_event(*nd);
	recycled_onfly_nodes.insert_head(*nd);
	recycled_onfly_cnt++;
}

void URtmfpOflow::resend_buf_entry(SendBufEntry& buf)
{
	session.resend_data_chunk(*this,buf);
}

SendPendingMsg* URtmfpOflow::get_pending_msg()
{
	if ( recycled_pending_nodes.empty() )
		return new SendPendingMsg;

	SendPendingMsg* e = dynamic_cast<SendPendingMsg*>(recycled_pending_nodes.get_head());
	e->quit_list();
	recycled_pending_cnt--;
	return e;
}

void URtmfpOflow::recycle_pending_msg(SendPendingMsg* nd)
{
	if ( recycled_pending_cnt > 10) {
		delete nd;
		return;
	}
	nd->user_set_to = 0;
	nd->used = 0;
	nd->flag = 0;
	recycled_pending_nodes.insert_head(*nd);
	recycled_pending_cnt++;
}
