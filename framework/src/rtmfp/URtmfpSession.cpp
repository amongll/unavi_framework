/*
 * URtmfpSession.cpp
 *
 *  Created on: 2014-10-20
 *      Author: li.lei
 */

#include "unavi/rtmfp/URtmfpSession.h"
#ifndef RTMFP_SESSION_DEBUG
#include "unavi/rtmfp/URtmfpProto.h"
#else
#include "unavi/rtmfp/URtmfpDebugProto.h"
#endif
#include "unavi/rtmfp/URtmfpFlowChunk.h"
#include "unavi/rtmfp/URtmfpSessionChunk.h"

#include "unavi/frame/UOMBPeerApp.h"
#include "unavi/core/UNaviLog.h"

#include <cstdarg>

using namespace unavi;
using namespace urtmfp;
using namespace std;

#ifndef RTMFP_SESSION_DEBUG
URtmfpSession::URtmfpSession(URtmfpProto& mgr, const uint32_t n_ssid, const uint32_t f_ssid, SessionMod ssmod,
		const uint32_t ppid):
#else
URtmfpSession::URtmfpSession(URtmfpDebugProto& mgr, const uint32_t n_ssid, const uint32_t f_ssid, SessionMod ssmod,
	const uint32_t ppid, URtmfpPeerId* peerid):
#endif
	proto(mgr),
	far_ssid(f_ssid),
	near_ssid(n_ssid),
	mod(ssmod),
	pipe_id(ppid),
	peer_addr(&sa_peer_addr),
	cur_readable(NULL),
	cur_writable(NULL),
	ssn_checker(*this),
	sys_pingid(0),
	user_pingid(0),
	recycled_ass_cnt(0),
	recycled_msg_cnt(0),
	emerg_ack_timer(*this),
	idle_ack_timer(*this),
	ack_smart_sender(*this),
	out_packet(f_ssid,ssmod),
	send_pending(false),
	smart(*this),
	stage(SESSION_ALIVE),
	prev_chunk(INAVLID_CHUNK),
	prev_iflow(NULL),
	prev_iflow_seq(0),
	prev_fsn_off(0),
	oflow_id_gen(0),
	had_msg_ever(false),
	peer_id(peerid),
	idle_limit(0),
	packet_startup_time_ml(0),
	frame(NULL)
{
	init_rtt();
	last_active_tm = last_in_tm = last_out_tm = UNaviCycle::curtime_ml();
}

void URtmfpSession::cleanup()
{
	if ( frame ) {
		delete frame;
		frame = NULL;
	}

	PingIter pit ;
	for ( pit = user_pings.begin(); pit != user_pings.end(); pit++) {
		delete pit->second;
	}
	user_pings.clear();

	if ( send_pending && out_packet.packet ) {
		out_packet.packet->cancel_out_packet();
		out_packet.packet = NULL;
		send_pending  = false;
	}

	FlowIter fit ;
	for (fit = input_flows.begin(); fit != input_flows.end(); fit ++) {
		delete dynamic_cast<URtmfpIflow*>(*fit);
	}
	for (fit = output_flows.begin(); fit != output_flows.end(); fit ++) {
		delete dynamic_cast<URtmfpOflow*>(*fit);
	}

	input_flows.clear();
	output_flows.clear();

	UNaviListable* nd;
	while ( (nd = recycled_ass_nodes.get_head())) {
		delete nd;
	}
	while ( (nd = recycled_msg_nodes.get_head())) {
		delete nd;
	}
	if ( peer_id) {
		delete peer_id;
		peer_id = NULL;
	}

	UNaviWorker::quit_event(smart);
	UNaviWorker::quit_event(idle_ack_timer);
	UNaviWorker::quit_event(emerg_ack_timer);
	UNaviWorker::quit_event(ack_smart_sender);
}

void URtmfpSession::rtt_check(const URtmfpPacket& in)
{
	if (in.has_timestamp()) {
		if ( rd_ts!= in.timestamp) {
			rd_ts = in.timestamp;
			rd_ts_chgtm = UNaviCycle::curtime_ml() - basetime_ml;
		}
	}

	if (in.has_timestamp_echo()) {
		if (rd_tse != in.timestamp_echo) {
			rd_tse = in.timestamp_echo;
			uint64_t cur_tm = UNaviCycle::curtime_ml() - basetime_ml;
			if ( cur_tm - wr_ts_chgtm > 128000 ) {
				rd_tse = -1;
				return;
			}

			uint32_t cur_tick = cur_tm/4 % 65536;
			uint32_t rtt_tick = (cur_tick - rd_tse) % 65536;

			if (rtt_tick < 32768) {
				int32_t rtt = rtt_tick * 4;
				if (smooth_rtt == -1) {
					smooth_rtt = rtt;
					rtt_va = rtt / 2;
				}
				else {
					int32_t rtt_diff = smooth_rtt - rtt;
					if (rtt_diff < 0)
						rtt_diff = -rtt_diff;
					rtt_va = (3 * rtt_va + rtt_diff) / 4;
					smooth_rtt = (7 * smooth_rtt + rtt) / 8;
				}
				this->measured_rtt = smooth_rtt + 4 * rtt_va + 200;
				this->effective_rtt = this->measured_rtt > 250 ? this->measured_rtt : 250;
			}
		}
	}
}

void URtmfpSession::pkt_time_process(URtmfpPacket& out)
{
	uint64_t cur_tm = UNaviCycle::curtime_ml() - basetime_ml;
	uint16_t cur_send_ts = cur_tm/4 % 65536;
	out.packet->seek(5);
	if (wr_ts != cur_send_ts) {
		wr_ts = cur_send_ts;
		wr_ts_chgtm = cur_tm;
		out.set_timestamp(wr_ts);
	}

	if ( rd_ts != -1 ) {
		uint64_t elapse_tm = cur_tm - rd_ts_chgtm;
		uint16_t send_echo;
		if (elapse_tm < 128000) {
			elapse_tm /= 4;
			send_echo = (rd_ts + elapse_tm) % 65536;
			if ( send_echo != wr_tse ) {
				wr_tse = send_echo;
				out.set_timestamp_echo(wr_tse);
			}
		}
		else {
			rd_ts = -1;
			rd_ts_chgtm = -1;
		}
	}

	packet_startup_time_ml = UNaviCycle::curtime_ml();
}

void URtmfpSession::init_rtt()
{
	basetime_ml = UNaviCycle::curtime_ml();

	effective_rtt = 500;
	measured_rtt = 250;

	rd_ts = -1;
	rd_ts_chgtm = -1;
	rd_tse = -1;

	wr_ts = -1;
	wr_ts_chgtm = -1;
	wr_tse = -1;

	smooth_rtt = -1;
	rtt_va = 0;
}

#ifdef RTMFP_SESSION_DEBUG
void URtmfpSession::send_packet_trace(URtmfpPacket &opkt,bool piggy_ack,const char* fmt,...)
{
	char buf[1024];
	va_list vl;
	va_start(vl,fmt);
	vsnprintf(buf,sizeof(buf),fmt,vl);
	va_end(vl);

	UNaviLogInfra::declare_logger_tee(urtmfp::log_id, TEE_ORANGE);

	ostringstream oss;
	oss<<"Session send_packet >>"<<buf<< " time:"<< UNaviCycle::curtime_ml() << " rtt:" << get_rtt();
	udebug_log(urtmfp::log_id, oss.str().c_str());
	send_packet(opkt,piggy_ack);
	UNaviLogInfra::declare_logger_tee(urtmfp::log_id, TEE_NONE);
}
#endif
void URtmfpSession::pkt_time_justify(URtmfpPacket& outpkt)
{
	uint64_t tm_diff = UNaviCycle::curtime_diff_ml(packet_startup_time_ml);
	if (tm_diff > 10) {
		uint64_t justify_tm = 0;
		int prev_off = outpkt.packet->pos;
		int off = 5;
		if ( outpkt.has_timestamp() ) {
			justify_tm = outpkt.timestamp*4 + tm_diff;
			justify_tm >>= 2;
			justify_tm %= 65536;
			outpkt.packet->seek(off);
			outpkt.set_timestamp(justify_tm/4);
			off += 2;
		}

		if ( outpkt.has_timestamp_echo() ) {
			justify_tm = outpkt.timestamp_echo*4 + tm_diff;
			justify_tm >>= 2;
			justify_tm %= 65536;
			outpkt.packet->seek(off);
			outpkt.set_timestamp_echo(justify_tm);
		}

		outpkt.packet->seek(prev_off);
	}
}
void URtmfpSession::send_packet(URtmfpPacket& outpkt, bool piggy_ack)
{
	pkt_time_justify(outpkt);
	if (piggy_ack)try_piggy_ack(outpkt);
	outpkt.send_out(*peer_addr);
}

void URtmfpSession::process_chunk(URtmfpChunk& chunk)
{
	last_in_tm = UNaviCycle::curtime_ml();
	ChunkType _prev_chunk = prev_chunk;
	prev_chunk = chunk.type;

	switch(chunk.type) {
	case FWD_IHELLO_CHUNK:
	case PING_CHUNK:
	case PING_REPLY_CHUNK:
	case DATA_CHUNK:
	case NEXT_DATA_CHUNK:
	case BITMAP_ACK_CHUNK:
	case RANGES_ACK_CHUNK:
	case BUFFER_PROBE_CHUNK:
	case FLOW_EXP_CHUNK:
		if (stage != SESSION_ALIVE ) {
			return;
		}
		break;
	default:
		break;
	}

	switch(chunk.type) {
	case FWD_IHELLO_CHUNK:
		break;
	case PING_CHUNK: {
		UPingChunk &type = dynamic_cast<UPingChunk&>(chunk);
		UPingReplyChunk reply(&type);
		send_sys_chunk(reply);
		break;
	}
	case PING_REPLY_CHUNK:{
		UPingReplyChunk &type = dynamic_cast<UPingReplyChunk&>(chunk);
		if ( type.ping_id < 0 ) {
			// sys ping reply
			//仅检查ping id是否合法。
			if ( type.ping_id < sys_pingid ) { //本端未曾发出过该ping
				if ( 2147483648 - (sys_pingid - type.ping_id) > 1000000 ) {
					URtmfpException e(URTMFP_PING_REPLY_ERROR);
					uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
				}
			}
			else {
				if ( type.ping_id - sys_pingid > 1000000 ) {
					URtmfpException e(URTMFP_PING_REPLY_ERROR);
					uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
				}
			}
		}
		else { //user ping reply
			PingIter it = user_pings.find(type.ping_id);
			if ( it != user_pings.end() ) {
				if ( ::memcmp(type.ping_raw, it->second->ping_raw,type.ping_size) ) {
					URtmfpException e(URTMFP_PING_REPLY_ERROR);
					uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
				}

				last_active_tm = UNaviCycle::curtime_ml();
				delete it->second;
				user_pings.erase(it);
				user_ping_relayed(type.ping_raw, type.ping_size);
				return;
			}
		}
		break;
	}
	case CLOSE_CHUNK:{
		if ( stage == CLOSE_ACK_WAITING) {
			//本端已经发送了close，未收到ack，但是收到了对端的close
			//可以认为是ack。
			UCloseAckChunk ack;
			send_sys_chunk(ack);
			stage = CLOSE_TIMEOUT;
			ssn_checker.set_expire(10000000); //维持20s。
			return;
		}
		else if ( stage == CLOSE_TIMEOUT) {
			UCloseAckChunk ack;
			send_sys_chunk(ack);
			return;
		}
		else {
			peer_closed();
			UCloseAckChunk ack;
			send_sys_chunk(ack);
			cleanup();
			//收到close，并且已经发送了ack。
			stage = CLOSE_TIMEOUT;
			ssn_checker.set_expire(10000000); //维持10s。
		}
		break;
	}
	case CLOSE_ACK_CHUNK:{
		if ( stage == CLOSE_ACK_WAITING ) {
			stage = CLOSE_TIMEOUT;
			ssn_checker.set_expire(10000); //正常的主动关闭流程，立即清理
		}
		else if (stage == SESSION_ALIVE) {
			URtmfpException e(URTMFP_PEER_PROTO_ERROR);
			uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
		}
		else {
			//相向关闭时，无需处理
		}
		break;
	}
	case DATA_CHUNK: {
		last_active_tm = UNaviCycle::curtime_ml();
		UDataChunk &type = dynamic_cast<UDataChunk&>(chunk);
		//FlowFragmentCtrl ctrl = DECODE_FRAG_CTRL(type.frag_flag);
		//type.input_flow->push_data_chunk(type.seq_id,type.fsn_off,
		//	ctrl,type.data,type.size);
		type.input_flow->push_data_chunk(type.seq_id,type.fsn_off,
			type.frag_flag,type.data,type.size);

		uint32_t oflowid;
		if ( type.relating_oflowid >= 0) {
			if ( type.input_flow->relate_notified == false) {
				URtmfpOflow* oflow = have_output_flow(type.relating_oflowid);
				if (!oflow) {
					URtmfpException e(URTMFP_IDATACHUNK_ERROR);
					uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
				}
				outflow_related(oflow, type.input_flow);
			}
		}

		prev_iflow = type.input_flow;
		prev_fsn_off = type.fsn_off;
		prev_iflow_seq = type.seq_id;
		enable_idle_ack();
		break;
	}
	case NEXT_DATA_CHUNK: {
		if ( _prev_chunk != DATA_CHUNK && _prev_chunk != NEXT_DATA_CHUNK ) {
			URtmfpException e(URTMFP_IDATACHUNK_ERROR);
			uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
		}
		UNextDataChunk &type = dynamic_cast<UNextDataChunk&>(chunk);
		//FlowFragmentCtrl ctrl = DECODE_FRAG_CTRL(type.frag_flag);
		//prev_iflow->push_data_chunk(++prev_iflow_seq,prev_fsn_off,ctrl,type.data,type.size);
		prev_iflow->push_data_chunk(++prev_iflow_seq,prev_fsn_off,type.frag_flag,type.data,type.size);
		//enable_idle_ack();
		break;
	}
	case BITMAP_ACK_CHUNK:{
		last_active_tm = UNaviCycle::curtime_ml();
		UBitmapAckChunk& type  = dynamic_cast<UBitmapAckChunk&>(chunk);
		URtmfpOflow* flow = have_output_flow(type.info->flow_id);
		if (!flow)
			return;
		flow->cum_ack(type.info->cum_ack_seq,type.info->buf_available*1024);
		map<uint64_t,uint64_t>::iterator it;
		uint64_t max_seq ;
		for (it = type.info->ranges.begin(); it != type.info->ranges.end(); it++) {
			max_seq = it->second;
			flow->ack_range(it->first,it->second);
		}
		if ( type.info->ranges.size())
			flow->trigger_nack_resend(max_seq);
		break;
	}
	case RANGES_ACK_CHUNK:{
		last_active_tm = UNaviCycle::curtime_ml();
		URangeAckChunk& type  = dynamic_cast<URangeAckChunk&>(chunk);
		URtmfpOflow* flow = have_output_flow(type.info->flow_id);
		if (!flow)
			return;
		flow->cum_ack(type.info->cum_ack_seq,type.info->buf_available*1024);
		map<uint64_t,uint64_t>::iterator it;
		uint64_t max_seq ;
		for (it = type.info->ranges.begin(); it != type.info->ranges.end(); it++) {
			max_seq = it->second;
			flow->ack_range(it->first,it->second);
		}
		if ( type.info->ranges.size())
			flow->trigger_nack_resend(max_seq);
		break;
	}
	case BUFFER_PROBE_CHUNK:{
		last_active_tm = UNaviCycle::curtime_ml();
		UBufferProbeChunk& type  = dynamic_cast<UBufferProbeChunk&>(chunk);
		URtmfpIflow* flow = have_input_flow(type.flowid);
		if (!flow)
			return;
		flow->need_ack = true;
		enable_emerge_ack();
		break;
	}
	case FLOW_EXP_CHUNK: {
		last_active_tm = UNaviCycle::curtime_ml();
		UFlowExcpChunk& type  = dynamic_cast<UFlowExcpChunk&>(chunk);
		URtmfpOflow* flow = have_output_flow(type.flowid);
		if (!flow)
			return;
		outflow_rejected(flow, type.excp_code);
		break;
	}
	default:
		break;
	}
}

URtmfpIflow* URtmfpSession::get_curssn_iflow(uint32_t flowid)
{
	if ( URtmfpDebugProto::get_worker_proto()->cur_decode_session==NULL)
		return NULL;
#ifndef RTMFP_SESSION_DEBUG
	return (URtmfpProto::get_worker_proto()->cur_decode_session->get_input_flow(flowid));
#else
	return (URtmfpDebugProto::get_worker_proto()->cur_decode_session->get_input_flow(flowid));
#endif
}

void URtmfpSession::outflow_rejected(URtmfpOflow* flow, uint64_t code)
{
	flow->rejected(code);
	frame->outflow_rejected(flow->flow_id,code);
}

void URtmfpSession::outflow_related(URtmfpOflow* oflow, URtmfpIflow* iflow)
{
	iflow->relate_notified = true;
	iflow->relate_outflow(oflow,ACTIVE_RELATING);
	oflow->relate_inflow(iflow, POSITIVE_RELATED);
	frame->outflow_related(oflow->flow_id, iflow->flow_id);
}

void URtmfpSession::send_timedout(uint32_t flowid)
{
#ifdef RTMFP_SESSION_DEBUG
	udebug_log(urtmfp::log_id, "session:%d send timedout", near_ssid);
#endif
	frame->send_timedout(flowid);
}

void URtmfpSession::user_ping_relayed(const uint8_t* relay, uint32_t sz)
{
	frame->user_ping_relayed(relay,sz);
}

void URtmfpSession::user_ping_timedout(const uint8_t* relay, uint32_t sz)
{
	frame->user_ping_timedout(relay,sz);
}

void URtmfpSession::peer_closed()
{
#ifdef RTMFP_SESSION_DEBUG
	udebug_log(urtmfp::log_id, "session:%d recv peer close", near_ssid);
#endif
	if (frame) {
		frame->ready_impl = NULL;
		frame->peer_closed();
		frame = NULL;
	}
}

void URtmfpSession::close()
{
	if ( stage == SESSION_ALIVE ) {
#ifdef RTMFP_SESSION_DEBUG
	udebug_log(urtmfp::log_id, "session:%d active close", near_ssid);
#endif
		UCloseChunk chunk;
		send_sys_chunk(chunk);
		stage = CLOSE_ACK_WAITING;
		cleanup();
		ssn_checker.active_close_deadline = UNaviCycle::curtime_ml() + 10000;
		ssn_checker.set_expire(500000); //如果没有ack，则每半秒发送一次close
		return;
	}
}

void URtmfpSession::SessionChecker::process_event()
{
	if (target.stage == SESSION_ALIVE) {
		uint64_t curtm = UNaviCycle::curtime_ml();
		uint64_t active_diff = UNaviCycle::curtime_diff_ml(target.last_active_tm);
		if ( target.idle_limit > 0 && active_diff >= target.idle_limit) {
#ifndef RTMFP_SESSION_DEBUG
			target.idle_timedout();
#endif

		}
		else if (active_diff >= 5000) { //5秒中没有活跃负载包，自动发起ping
#ifndef RTMFP_SESSION_DEBUG
			if ( UNaviCycle::curtime_diff_ml(target.last_out_tm) > 5000)
#else
			if ( UNaviCycle::curtime_diff_ml(target.last_out_tm) > 60000)
#endif
			{
				target.sys_ping();
			}
		}

		if ( target.last_out_tm > target.last_in_tm ) {
#ifndef RTMFP_SESSION_DEBUG
			if ( target.last_out_tm - target.last_in_tm >= 30000 ){ //30s没有任何响应
				target.broken(URTMFP_E_PEER_COMM_FAILED);
				return;
			}
#endif
		}
		set_expire(1000000);
		return;
	}

	if (CLOSE_ACK_WAITING == target.stage) {
		if ( UNaviCycle::curtime_ml() >= active_close_deadline ) {
			target.proto.delete_session(target.near_ssid);
			return;
		}
		UCloseChunk ck;
		target.send_sys_chunk(ck);
		set_expire(500000);
		return;
	}
	else if (CLOSE_TIMEOUT == target.stage) {
		target.proto.delete_session(target.near_ssid);
	}
	else if (SESSION_BROKEN == target.stage) {
		target.proto.delete_session(target.near_ssid);
	}
}

void URtmfpSession::sys_ping()
{
	if ( sys_pingid == -2147483648 )
		sys_pingid = 0;

	sys_pingid--;
	UPingChunk obj(sys_pingid,NULL,0);
	send_sys_chunk(obj);
	last_out_tm = UNaviCycle::curtime_ml();
}

void URtmfpSession::user_ping(const uint8_t* ping_raw, uint32_t sz, uint64_t timeout)
{
	if (user_pingid == 2147483647)
		user_pingid = 0;
	user_pingid++;

	pair<PingIter,bool> ret = user_pings.insert(make_pair(user_pingid, (Ping*)NULL));
	if ( ret.second == true) {
		ret.first->second = new Ping;
	}


	Ping& obj = *ret.first->second;
	if (obj.ping_raw) {
		delete []obj.ping_raw;
	}
	obj.target = this;
	obj.ping_raw = new uint8_t[sz];
	::memcpy(obj.ping_raw,ping_raw,sz);
	obj.sz = sz;
	if ( timeout <= 200 && timeout > 0)
		timeout = 200;
	if ( timeout == 0)
		timeout = get_rtt() * 1.2;
	obj.set_expire(timeout*1000);

	UPingChunk chunk(user_pingid, ping_raw, sz);
	last_out_tm = last_active_tm = UNaviCycle::curtime_ml();
	send_sys_chunk(chunk);
}

void URtmfpSession::Ping::process_event()
{
	target->user_ping_timedout(ping_raw,sz);
	target->user_pings.erase(id);
	delete this;
}

void URtmfpSession::set_idle_limit(uint64_t limit)
{
	idle_limit = limit;
}

void URtmfpSession::idle_timedout()
{
#ifdef RTMFP_SESSION_DEBUG
	udebug_log(urtmfp::log_id, "session:%d idle timedout", near_ssid);
#endif
	if (frame) frame->idle_timedout();
}

void URtmfpSession::broken(URtmfpError e)
{
#ifdef RTMFP_SESSION_DEBUG
	udebug_log(urtmfp::log_id, "session:%d broken %s", near_ssid, URtmfpException::err_desc[e]);
#endif
	if (frame){
		frame->ready_impl = NULL;
		frame->broken(e);
		frame = NULL;
	}
	this->close();
	stage = SESSION_BROKEN;
	UNaviWorker::quit_event(ssn_checker);
	ssn_checker.set_already();
}

bool URtmfpSession::send_would_overwhelm(uint32_t flowid)
{
	URtmfpOflow* flow = NULL;
	if ( cur_writable && cur_writable->flow_id == flowid)
		flow = cur_writable;
	else
		flow = have_output_flow(flowid);

	if (flow == NULL)
		return false;
	if (flow->available() <= 0 ) {
		uint32_t sugg = proto.cur_suggest_sendbuf(pipe_id);
		if (flow->pending_size >= sugg)
			return true;
	}
	return false;
}

SendStatus URtmfpSession::send_msg(uint32_t arg_flowid,const uint8_t* arg_raw,uint32_t arg_size,
	int32_t to_limit,
	bool is_last, const FlowOptsArg* opts)
{
	URtmfpOflow* flow = NULL;
	bool need_opt = false;
	uint8_t frag_flag = 0;

	if ( stage != SESSION_ALIVE) {
		return SEND_ON_BROKEN_SESSION;
	}

	if ( cur_writable && cur_writable->flow_id == arg_flowid)
		flow = cur_writable;
	else {
		flow = have_output_flow(arg_flowid);
		if (!flow) {
			if (opts) {
				URtmfpIflow* iflow = NULL;
				if (opts->relating_iflow >= 0 ) {
					iflow = have_input_flow(opts->relating_iflow);
					if (!iflow)
						return SEND_DENYED;
				}
				flow = get_output_flow(arg_flowid);
				if (opts->user_meta) {
					flow->set_user_meta(opts->user_meta,opts->meta_size);
				}
				if (opts->relating_iflow >= 0) {
					flow->relate_inflow(iflow,ACTIVE_RELATING);
					iflow->relate_outflow(flow, POSITIVE_RELATED);
				}
				need_opt = true;
				frag_flag |= Flow_frag_opt_bit;
			}
			else {
				flow = get_output_flow(arg_flowid);
			}
		}

		cur_writable = flow;
	}

	if ( flow->finished || flow->is_rejected )
		return SEND_DENYED;

	if ( is_last ) {
		flow->finished = true;
	}

	if ( flow->available() <= 0) {
		//将msg放入flow的pending msg链表中
		//对端通过ack清理本端发送缓冲，或者本端发送超时进行fsn前移，且上层未关闭flow时，
		//将pending msg进行发送处理
		uint32_t sugg = proto.cur_suggest_sendbuf(pipe_id);
		frag_flag = is_last?Flow_frag_fin_bit:0;
		if ( flow->pending_size >= sugg ) {
			do {
				flow->forword_onfly();
			} while(flow->pending_size >= sugg);
			flow->push_pending(arg_raw, arg_size, to_limit, frag_flag);
			return SEND_OVERWHELMED;
		}
		flow->push_pending(arg_raw, arg_size, to_limit, frag_flag);
		return SEND_BUFFERED;
	}

	if (flow->acked==false && flow->opts.list.size() > 0) {
		need_opt = true;
		frag_flag |= Flow_frag_opt_bit;
	}

	last_data_chunk.resend_seqid = 0;

	if ( send_pending == false)
		goto new_packet;

	if (last_data_chunk.flow_id == arg_flowid && flow->fsn == last_data_chunk.fsn) {
		//packet在待发送状态，并且上一个chunk是data/next chunk，且上一个
		//chunk的fsn和flow当前fsn相同，则可以使用next data chunk发送消息
		//即使消息可以分片放入pending send，也不放入。
		uint32_t will_accept = UNextDataChunk::will_accept_for_ndatachunk(flow,out_packet.available());
		if ( will_accept < arg_size )
			goto new_packet;
		flow->seq_gen++;
		{
			frag_flag |= is_last?Flow_frag_fin_bit:0;
			UNextDataChunk chunk(flow, flow->seq_gen, arg_raw,arg_size,frag_flag);
#ifndef RTMFP_SESSION_DEBUG
			out_packet.push_chunk(chunk);
#else
			out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif
		}
	}
	else {
		//上一个chunk不是data chunk，或者不是同一个flowid的。则尝试以data chunk复用rtmfp packet
		if ( need_opt ) {
			if (std::find(cur_packet_flows.begin(),cur_packet_flows.end(),flow->flow_id)
				!= cur_packet_flows.end()) {
				need_opt = false;
			}
		}
		uint32_t will_accept = UDataChunk::will_accept_for_datachunk(flow,out_packet.available(),need_opt);
		if ( will_accept < arg_size )
			goto new_packet;

		flow->seq_gen++;
		{
			frag_flag |= is_last?Flow_frag_fin_bit:0;
			UDataChunk chunk(flow,flow->seq_gen, arg_raw, arg_size,frag_flag);
#ifndef RTMFP_SESSION_DEBUG
			out_packet.push_chunk(chunk);
#else
			out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif

		}

		last_data_chunk.flow_id = flow->flow_id;
		last_data_chunk.fsn = flow->fsn;
		if ( need_opt ) {
			cur_packet_flows.push_back(flow->flow_id);
		}
	}

packet_mutplex:

	flow->push_onfly(flow->seq_gen,arg_raw,arg_size,to_limit, frag_flag);

	if (out_packet.available() < 64) { //如果packet有较多内容，则立即发送，否则pending发送，以复用packet
#ifdef RTMFP_SESSION_DEBUG
		send_packet_trace(out_packet,true,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
		send_packet(out_packet);
#endif
		send_pending = false;
		last_data_chunk.flow_id = -1;
		cur_packet_flows.clear();

		if ( smart.already_active() ) {
			UNaviWorker::quit_event(smart,UNaviEvent::EV_ALREADY);
		}
	}
	else if (smart.already_active() == false ) {
		smart.set_already();
	}

	return SEND_SUCC;

new_packet:
	//使用默认的mtu作为rtmfp封包的大小上限。
	uint32_t will_accept = UDataChunk::will_accept_for_datachunk(flow,
		UNaviWorker::preset_mtu(pipe_id)-9, /*排除packet头部最大9字节*/
		need_opt);
	if (will_accept < arg_size) {
		send_pending = false;
		last_data_chunk.flow_id = -1;
		if ( smart.already_active() ) {
			UNaviWorker::quit_event(smart,UNaviEvent::EV_ALREADY);
		}

		uint32_t pos = 0, chunk_payload = 0, begin = 1;
		frag_flag &= ~Flow_frag_fin_bit;
		do {
			UNaviUDPPacket* udp = UNaviUDPPacket::new_outpacket(pipe_id);
			out_packet.set(*udp);
			pkt_time_process(out_packet);

			chunk_payload = (arg_size - pos) >= will_accept ? will_accept : (arg_size - pos);
			if ( begin == 1) {
				frag_flag &= Flow_frag_ctrl_clear_bit;
				frag_flag |= Flow_frag_ctrl_bits[MSG_FIRST_FRAG];
				begin = 0;
			}
			else if (chunk_payload + pos == arg_size) {
				frag_flag &= Flow_frag_ctrl_clear_bit;
				frag_flag |= Flow_frag_ctrl_bits[MSG_LAST_FRAG];
				if (is_last)
					frag_flag |= Flow_frag_fin_bit;
			}
			else {
				frag_flag &= Flow_frag_ctrl_clear_bit;
				frag_flag |= Flow_frag_ctrl_bits[MSG_MIDDLE_FRAG];
			}
			flow->seq_gen++;
			UDataChunk chunk(flow, flow->seq_gen,arg_raw + pos, chunk_payload, frag_flag);
			pos += chunk_payload;
#ifndef RTMFP_SESSION_DEBUG
			out_packet.push_chunk(chunk);
#else
			out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif

			flow->push_onfly(flow->seq_gen,arg_raw+pos, chunk_payload, to_limit, frag_flag);

			if ( chunk_payload < will_accept && out_packet.available() < 64) { //结尾frag
				send_pending = true;
				last_data_chunk.flow_id = flow->flow_id;
				last_data_chunk.fsn = flow->fsn;

				cur_packet_flows.push_back(flow->flow_id); //此处肯定为空

				if (smart.already_active() == false)
					smart.set_already();
				break;
			}
			else { // payload == accept 或者结尾包已经足够大
#ifdef RTMFP_SESSION_DEBUG
				send_packet_trace(out_packet,true,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
				send_packet(out_packet);
#endif
			}
			//seq id发生变化，可能引起占用空间的变化
			will_accept = UDataChunk::will_accept_for_datachunk(flow,out_packet.available(),need_opt);
		}while ( pos < arg_size);
	}
	else {
		UNaviUDPPacket* udp = UNaviUDPPacket::new_outpacket(pipe_id);
		out_packet.set(*udp);
		pkt_time_process(out_packet);
		flow->seq_gen++;
		frag_flag |= is_last?Flow_frag_fin_bit:0;
		UDataChunk chunk(flow,flow->seq_gen, arg_raw,arg_size,frag_flag);
#ifndef RTMFP_SESSION_DEBUG
		out_packet.push_chunk(chunk);
#else
		out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif
		flow->push_onfly(flow->seq_gen,arg_raw,arg_size,to_limit,frag_flag);
		if (out_packet.available() < 64) {
#ifdef RTMFP_SESSION_DEBUG
			send_packet_trace(out_packet,true,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
			send_packet(out_packet);
#endif
			if ( smart.already_active() ) {
				UNaviWorker::quit_event(smart,UNaviEvent::EV_ALREADY);
			}
		}
		else {
			send_pending = true;
			last_data_chunk.flow_id = flow->flow_id;
			last_data_chunk.fsn = flow->fsn;
			//在此之前cur_packet_flows肯定为空
			cur_packet_flows.push_back(flow->flow_id);
			if (smart.already_active() == false)
				smart.set_already();
		}
	}

	last_active_tm = last_out_tm = UNaviCycle::curtime_ml();
	return SEND_SUCC;
}

void URtmfpSession::send_pending_msg(URtmfpOflow& flow, SendPendingMsg& msg)
{
	bool need_opt = false;
	if (flow.acked==false && flow.opts.list.empty() == false) {
		need_opt = true;
		msg.flag |= Flow_frag_opt_bit;
	}
	else {
		msg.flag &= ~Flow_frag_opt_bit;
	}

	last_data_chunk.resend_seqid = 0;

	if ( send_pending == false)
		goto new_packet;

	if (last_data_chunk.flow_id == flow.flow_id && flow.fsn == last_data_chunk.fsn) {
		//packet在待发送状态，并且上一个chunk是data/next chunk，且上一个
		//chunk的fsn和flow当前fsn相同，则可以使用next data chunk发送消息
		//即使消息可以分片放入pending send，也不放入。
		uint32_t will_accept = UNextDataChunk::will_accept_for_ndatachunk(&flow,out_packet.available());
		if ( will_accept < msg.used)
			goto new_packet;
		flow.seq_gen++;
		{
			UNextDataChunk chunk(&flow, flow.seq_gen, msg.content,msg.used, msg.flag);
#ifndef RTMFP_SESSION_DEBUG
			out_packet.push_chunk(chunk);
#else
			out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif
		}
	}
	else {
		//上一个chunk不是data chunk，或者不是同一个flowid的。则尝试以data chunk复用rtmfp packet
		if (need_opt) {
			if (std::find(cur_packet_flows.begin(),cur_packet_flows.end(),flow.flow_id)
				!= cur_packet_flows.end()) {
				need_opt = false;
			}
		}
		uint32_t will_accept = UDataChunk::will_accept_for_datachunk(&flow,out_packet.available(),need_opt);
		if ( will_accept < msg.used)
			goto new_packet;

		flow.seq_gen++;
		{
			UDataChunk chunk(&flow,flow.seq_gen, msg.content, msg.used, msg.flag);
#ifndef RTMFP_SESSION_DEBUG
			out_packet.push_chunk(chunk);
#else
			out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif
		}

		last_data_chunk.flow_id = flow.flow_id;
		last_data_chunk.fsn = flow.fsn;
		if ( need_opt ) {
			cur_packet_flows.push_back(flow.flow_id);
		}
	}

packet_mutplex:

	flow.push_onfly(flow.seq_gen,msg.content ,msg.used ,msg.user_set_to, msg.flag);

	if (out_packet.available() < 64) { //如果packet有较多内容，则立即发送，否则pending发送，以复用packet
#ifdef RTMFP_SESSION_DEBUG
		send_packet_trace(out_packet,true,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
		send_packet(out_packet);
#endif
		send_pending = false;
		last_data_chunk.flow_id = -1;
		cur_packet_flows.clear();

		if ( smart.already_active() ) {
			UNaviWorker::quit_event(smart,UNaviEvent::EV_ALREADY);
		}
	}
	else if (smart.already_active() == false ) {
		smart.set_already();
	}
	return;

new_packet:

	if ( send_pending ) {
#ifdef RTMFP_SESSION_DEBUG
		send_packet_trace(out_packet,true,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
		send_packet(out_packet);
#endif
		send_pending = false;
		last_data_chunk.flow_id = -1;
		cur_packet_flows.clear();

		if ( smart.already_active() ) {
			UNaviWorker::quit_event(smart,UNaviEvent::EV_ALREADY);
		}
	}

	uint32_t will_accept = UDataChunk::will_accept_for_datachunk(&flow,out_packet.available(),
		need_opt);
	if (will_accept < msg.used) {
		send_pending = false;
		last_data_chunk.flow_id = -1;
		if ( smart.already_active() ) {
			UNaviWorker::quit_event(smart,UNaviEvent::EV_ALREADY);
		}

		uint32_t pos = 0, chunk_payload = 0, begin = 1;
		uint8_t frag_flag = msg.flag;
		bool is_last = msg.flag & Flow_frag_fin_bit;
		frag_flag &= ~Flow_frag_fin_bit;
		do {
			UNaviUDPPacket* udp = UNaviUDPPacket::new_outpacket(pipe_id);
			out_packet.set(*udp);
			pkt_time_process(out_packet);

			chunk_payload = (msg.used - pos) >= will_accept ? will_accept : (msg.used - pos);
			if ( begin == 1) {
				frag_flag &= Flow_frag_ctrl_clear_bit;
				frag_flag |= Flow_frag_ctrl_bits[MSG_FIRST_FRAG];
				begin = 0;
			}
			else if (chunk_payload + pos == msg.used) {
				frag_flag &= Flow_frag_ctrl_clear_bit;
				frag_flag |= Flow_frag_ctrl_bits[MSG_LAST_FRAG];
				if ( is_last )
					frag_flag |= Flow_frag_fin_bit;
			}
			else {
				frag_flag &= Flow_frag_ctrl_clear_bit;
				frag_flag |= Flow_frag_ctrl_bits[MSG_MIDDLE_FRAG];
			}
			flow.seq_gen++;
			UDataChunk chunk(&flow, flow.seq_gen,msg.content + pos, chunk_payload, frag_flag);
			pos += chunk_payload;
#ifndef RTMFP_SESSION_DEBUG
			out_packet.push_chunk(chunk);
#else
			out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif

			flow.push_onfly(flow.seq_gen,msg.content + pos, chunk_payload, msg.user_set_to, frag_flag);

			if ( chunk_payload < will_accept && out_packet.available() < 64) { //结尾frag
				send_pending = true;
				last_data_chunk.flow_id = flow.flow_id;
				last_data_chunk.fsn = flow.fsn;

				cur_packet_flows.push_back(flow.flow_id); //此处肯定为空

				if (smart.already_active() == false)
					smart.set_already();
				break;
			}
			else { // payload == accept 或者结尾包已经足够大
#ifdef RTMFP_SESSION_DEBUG
				send_packet_trace(out_packet,true,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
				send_packet(out_packet);
#endif
			}
			//seq id发生变化，可能引起占用空间的变化
			will_accept = UDataChunk::will_accept_for_datachunk(&flow,out_packet.available(),need_opt);
		} while ( pos < msg.used);
	}
	else {
		UNaviUDPPacket* udp = UNaviUDPPacket::new_outpacket(pipe_id);
		out_packet.set(*udp);
		pkt_time_process(out_packet);

		flow.seq_gen++;
		UDataChunk chunk(&flow,flow.seq_gen, msg.content ,msg.used ,msg.flag);
#ifndef RTMFP_SESSION_DEBUG
		out_packet.push_chunk(chunk);
#else
		out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif
		flow.push_onfly(flow.seq_gen,msg.content, msg.used, msg.user_set_to, msg.flag);
		if (out_packet.available() < 64) {
#ifdef RTMFP_SESSION_DEBUG
			send_packet_trace(out_packet,true,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
			send_packet(out_packet);
#endif
			if ( smart.already_active() ) {
				UNaviWorker::quit_event(smart,UNaviEvent::EV_ALREADY);
			}
		}
		else {
			send_pending = true;
			last_data_chunk.flow_id = flow.flow_id;
			last_data_chunk.fsn = flow.fsn;
			//在此之前cur_packet_flows肯定为空
			cur_packet_flows.push_back(flow.flow_id);
			if (smart.already_active() == false)
				smart.set_already();
		}
	}
	last_active_tm = last_out_tm = UNaviCycle::curtime_ml();
	return;
}

void URtmfpSession::resend_data_chunk(URtmfpOflow& flow, SendBufEntry& resend_entry)
{
	UNaviUDPPacket* udp = NULL;
	bool need_opt = false;
	if (flow.acked == false && flow.opts.list.empty() == false) {
		need_opt=true;
		resend_entry.flag |= Flow_frag_opt_bit;
	}
	else {
		resend_entry.flag &= ~Flow_frag_opt_bit;
	}

	if ( send_pending == false || last_data_chunk.resend_seqid == 0)
		goto new_packet;

	if (last_data_chunk.flow_id == flow.flow_id && flow.fsn == last_data_chunk.fsn &&
		resend_entry.seq_id == last_data_chunk.resend_seqid + 1) {
		//packet在待发送状态，并且上一个chunk是data/next chunk，且上一个
		//chunk的fsn和flow当前fsn相同，则可以使用next data chunk发送消息
		//即使消息可以分片放入pending send，也不放入。
		uint32_t will_occupy = UNextDataChunk::will_occupy_for_ndatachunk(&flow,resend_entry.seq_id,
			resend_entry.size);

		if ( will_occupy > out_packet.available() ) {
			goto new_packet;
		}

		{
			UNextDataChunk chunk(&flow, resend_entry.seq_id, resend_entry.chunk_data,
				resend_entry.size, resend_entry.flag);
#ifndef RTMFP_SESSION_DEBUG
			out_packet.push_chunk(chunk);
#else
			out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__,__PRETTY_FUNCTION__);
#endif
			last_data_chunk.resend_seqid = resend_entry.seq_id;
		}
	}
	else {
		//上一个chunk不是data chunk，或者不是同一个flowid的。则尝试以data chunk复用rtmfp packet
		if ( need_opt ) {
			if (std::find(cur_packet_flows.begin(),cur_packet_flows.end(),flow.flow_id)
				!=cur_packet_flows.end()) {
				need_opt = false;
			}
		}
		uint32_t will_occupy = UDataChunk::will_occupy_for_datachunk(&flow,resend_entry.seq_id,
			resend_entry.size, need_opt);

		if ( will_occupy > out_packet.available() ) {
			goto new_packet;
		}

		{
			UDataChunk chunk(&flow, resend_entry.seq_id, resend_entry.chunk_data,
				resend_entry.size, resend_entry.flag);

#ifndef RTMFP_SESSION_DEBUG
			out_packet.push_chunk(chunk);
#else
			out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif
			last_data_chunk.resend_seqid = resend_entry.seq_id;
		}
		last_data_chunk.flow_id = flow.flow_id;
		last_data_chunk.fsn = flow.fsn;

		if ( need_opt ) {
			cur_packet_flows.push_back(flow.flow_id);
		}
	}

packet_mutplex:

	if (out_packet.available() < 64) { //如果packet有较多内容，则立即发送，否则pending发送，以复用packet
#ifdef RTMFP_SESSION_DEBUG
		send_packet_trace(out_packet,true,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
		send_packet(out_packet);
#endif
		send_pending = false;
		last_data_chunk.flow_id = -1;
		last_data_chunk.resend_seqid = 0;
		cur_packet_flows.clear();

		if ( smart.already_active() ) {
			UNaviWorker::quit_event(smart,UNaviEvent::EV_ALREADY);
		}
	}
	else if (smart.already_active() == false ) {
		smart.set_already();
	}
	return ;

new_packet:
	if ( send_pending ) {
#ifdef RTMFP_SESSION_DEBUG
		send_packet_trace(out_packet,true,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
		send_packet(out_packet);
#endif
		send_pending = false;
		last_data_chunk.flow_id = -1;
		last_data_chunk.resend_seqid = 0;
		cur_packet_flows.clear();

		if ( smart.already_active() ) {
			UNaviWorker::quit_event(smart,UNaviEvent::EV_ALREADY);
		}
	}

	udp = UNaviUDPPacket::new_outpacket(pipe_id);
	out_packet.set(*udp);
	pkt_time_process(out_packet);

	//重发的包，肯定可以被一个默认mtu udp包容纳
	UDataChunk chunk(&flow, resend_entry.seq_id, resend_entry.chunk_data,
		resend_entry.size, resend_entry.flag);
#ifndef RTMFP_SESSION_DEBUG
	out_packet.push_chunk(chunk);
#else
	out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif

	if (out_packet.available() < 64) {
#ifdef RTMFP_SESSION_DEBUG
		send_packet_trace(out_packet,true,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
		send_packet(out_packet);
#endif
		if ( smart.already_active() ) {
			UNaviWorker::quit_event(smart,UNaviEvent::EV_ALREADY);
		}
	}
	else {
		send_pending = true;
		last_data_chunk.flow_id = flow.flow_id;
		last_data_chunk.fsn = flow.fsn;
		last_data_chunk.resend_seqid = resend_entry.seq_id;

		cur_packet_flows.push_back(flow.flow_id);

		if (smart.already_active() == false)
			smart.set_already();
	}

	return;
}

void URtmfpSession::send_sys_chunk(URtmfpChunk& chunk)
{
	int mtu = UNaviWorker::preset_mtu(pipe_id);
	UNaviUDPPacket* udp = NULL;
	bool piggy_send = (chunk.type != BITMAP_ACK_CHUNK && chunk.type != RANGES_ACK_CHUNK);
	switch(chunk.type) {
	case FWD_IHELLO_CHUNK:
	case PING_CHUNK:
	case PING_REPLY_CHUNK:
	case BITMAP_ACK_CHUNK:
	case RANGES_ACK_CHUNK:
	case BUFFER_PROBE_CHUNK:
	case FLOW_EXP_CHUNK:
	case CLOSE_ACK_CHUNK:
	case CLOSE_CHUNK:
		break;
	default: {//函数只接受如上类型的chunk
		URtmfpException e(URTMFP_OCHUNK_TYPE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
		break;
	}
	}

	last_data_chunk.flow_id = -1;

	// packet默认使用9字节头部： 4字节session id抑或分量，1字节flag，4字节时戳字段
	uint32_t min_pkt = URtmfpPacket::min_packet_for_chunk(chunk.encode_occupy());
	if ( min_pkt > mtu ) {
		//默认的mtu无法容纳，申请足够空间的packet，直接发送
		if ( send_pending )  {
#ifdef RTMFP_SESSION_DEBUG
		send_packet_trace(out_packet,piggy_send,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
		send_packet(out_packet,piggy_send);
#endif
		}

		udp = UNaviUDPPacket::new_outpacket(pipe_id,min_pkt);
		out_packet.set(*udp);
		pkt_time_process(out_packet);

#ifndef RTMFP_SESSION_DEBUG
		out_packet.push_chunk(chunk);
#else
		out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif
		goto send_now;
	}

	if ( send_pending ) {
		//尝试复用
		if ( out_packet.available() < chunk.encode_occupy() ) {
#ifdef RTMFP_SESSION_DEBUG
		send_packet_trace(out_packet,piggy_send,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
		send_packet(out_packet,piggy_send);
#endif
			goto new_packet;
		}

#ifndef RTMFP_SESSION_DEBUG
		out_packet.push_chunk(chunk);
#else
		out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif
		if ( out_packet.available() < 64 || chunk.type == CLOSE_CHUNK
			|| chunk.type == CLOSE_ACK_CHUNK)
			goto send_now;
		else
			goto send_smart;
	}

new_packet:
	udp = UNaviUDPPacket::new_outpacket(pipe_id);
	out_packet.set(*udp);
	pkt_time_process(out_packet);
#ifndef RTMFP_SESSION_DEBUG
	out_packet.push_chunk(chunk);
#else
	out_packet.push_chunk_trace(chunk,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif

	if (out_packet.available() >= 64 && chunk.type != CLOSE_CHUNK
		&& chunk.type != CLOSE_ACK_CHUNK)
		goto send_smart;

send_now:
	send_pending = false;
#ifdef RTMFP_SESSION_DEBUG
	send_packet_trace(out_packet,piggy_send,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
	send_packet(out_packet,piggy_send);
#endif
	if (smart.already_active())
		UNaviWorker::quit_event(smart,UNaviEvent::EV_ALREADY);
	return;

send_smart:
	send_pending = true;
	if (smart.already_active()==false)
		smart.set_already();
	return;
}

void URtmfpSession::SmartSender::process_event()
{
	if ( target.send_pending == false ) return;
#ifdef RTMFP_SESSION_DEBUG
	target.send_packet_trace(target.out_packet,true,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#else
	target.send_packet(target.out_packet);
#endif
	target.send_pending = false;
	target.last_data_chunk.flow_id = -1;
	target.last_data_chunk.resend_seqid = 0;
	target.cur_packet_flows.clear();
}

void URtmfpSession::AckPrepareTimer::process_event()
{
	FlowIter it = target.input_flows.begin();
	bool has_ack = false;
	for ( ; it != target.input_flows.end(); it++) {

		URtmfpIflow& flow = dynamic_cast<URtmfpIflow&>(**it);

		if ( flow.pending_ack ) {
			if (flow.pending_ack->type == BITMAP_ACK_CHUNK) {
				delete dynamic_cast<UBitmapAckChunk*>(flow.pending_ack);
			}
			else {
				delete dynamic_cast<URangeAckChunk*>(flow.pending_ack);
			}
			flow.pending_ack = NULL;
		}

		if ( flow.need_ack == false )
			continue;

		if ( flow.rejected ) {
			UFlowExcpChunk chunk(flow.flow_id, flow.reject_reason);
			target.send_sys_chunk(chunk);
		}

		has_ack = true;

		const uint8_t* bm_buf;
		const uint8_t* range_buf;
		const uint8_t* head_buf;
		uint32_t bm_sz, range_sz, head_sz;

		head_buf = flow.calc_ack_header(head_sz, flow.last_notified_bufsz);
		flow.get_ack_content(range_buf,range_sz,bm_buf,bm_sz);

		if (range_sz >= bm_sz) {
			flow.pending_ack = new URangeAckChunk(head_buf,head_sz,range_buf,range_sz);
		}
		else{
			flow.pending_ack = new UBitmapAckChunk(head_buf,head_sz,bm_buf,bm_sz);
		}

		target.pending_ack.insert_tail(*flow.pending_ack);
		flow.need_ack = false;
	}

	if (has_ack)
		target.ack_smart_sender.set_already();
}

void URtmfpSession::AckSmartSender::process_event()
{
	FlowIter it = target.input_flows.begin();
	for ( ; it != target.input_flows.end(); it++) {
		URtmfpIflow* flow = dynamic_cast<URtmfpIflow*>(*it);

		if ( flow->need_ack == false )
			continue;

		if ( flow->rejected ) {
			UFlowExcpChunk chunk(flow->flow_id, flow->reject_reason);
			target.send_sys_chunk(chunk);
		}

		if ( flow->pending_ack ) {
			if (flow->pending_ack->type == BITMAP_ACK_CHUNK) {
				delete dynamic_cast<UBitmapAckChunk*>(flow->pending_ack);
			}
			else {
				delete dynamic_cast<URangeAckChunk*>(flow->pending_ack);
			}
			flow->pending_ack = NULL;
		}

		const uint8_t* bm_buf;
		const uint8_t* range_buf;
		const uint8_t* head_buf;
		uint32_t bm_sz, range_sz, head_sz ;

		head_buf = flow->calc_ack_header(head_sz, flow->last_notified_bufsz);
		flow->get_ack_content(range_buf,range_sz,bm_buf,bm_sz);

		if (range_sz >= bm_sz) {
			flow->pending_ack = new URangeAckChunk(head_buf,head_sz,range_buf,range_sz);
		}
		else{
			flow->pending_ack = new UBitmapAckChunk(head_buf,head_sz,bm_buf,bm_sz);
		}

		target.pending_ack.insert_tail(*flow->pending_ack);
		flow->need_ack = false;
	}

	while ( !target.pending_ack.empty() ) {
		URtmfpChunk* chunk = dynamic_cast<URtmfpChunk*>(target.pending_ack.get_head());
		chunk->quit_list();
		target.send_sys_chunk(*chunk);
	}
}

void URtmfpSession::try_piggy_ack(URtmfpPacket& out)
{
	//一个ping chunk最小可能是6字节，flowid avaiable cumack (NULL holes)
	if ( pending_ack.empty() || out.available() < 6) return;
	URtmfpChunk* pch = dynamic_cast<URtmfpChunk*>(pending_ack.get_head());
	int sz=0;
	do {
		sz += pch->encode_occupy();
		pch = dynamic_cast<URtmfpChunk*>(pending_ack.get_next(pch));
	}  while(pch);

	if (out.available() >= sz) {
#ifdef RTMFP_SESSION_DEBUG
		udebug_log(urtmfp::log_id,"Piggy back ack");
#endif
		while ( (pch =dynamic_cast<URtmfpChunk*>(pending_ack.get_head()) ) ) {

#ifndef RTMFP_SESSION_DEBUG
			out.push_chunk(*pch);
#else
			out.push_chunk_trace(*pch,"%s:%d %s",__FILE__,__LINE__, __PRETTY_FUNCTION__);
#endif
			pch->quit_list();
		}
	}
	else
		return;
}

ReadGotten URtmfpSession::read_msg(uint32_t iflow)
{
	static ReadGotten not_flow_ret(READ_NONEXIST_FLOW);
	static ReadGotten null_ret(READ_WOULD_BLOCK);
	static ReadGotten finish_ret(PEER_FINISHED);
	URtmfpIflow* flow = NULL;
	if ( cur_readable && cur_readable->flow_id == iflow)
		flow = cur_readable;
	else {
		flow = have_input_flow(iflow);
	}
	if (!flow)
		return not_flow_ret;

	if ( flow->ready_msgs.empty() ) {
		cur_readable = NULL;
		if (flow->finish_seq && flow->filled_to == *flow->finish_seq)
			return finish_ret;
		else
			return null_ret;
	}

	cur_readable = flow;

	ReadGotten ret(READ_OK);
	ret.msg = dynamic_cast<URtmfpMsg*>(flow->ready_msgs.get_head());
	ret.msg->quit_list();

	ret.flow_meta_size = flow->get_user_meta(ret.flow_meta);

	flow->ready_sum -= ret.msg->length;

	uint32_t sugg_recv_buf = proto.cur_suggest_recvbuf(pipe_id);
	int64_t left = (int64_t)sugg_recv_buf - flow->ready_sum - flow->ass_sum;

	if (left > 0 && flow->last_notified_bufsz ==0) {
		//主动给出ack告知对端读缓冲重新可用。
		flow->need_ack = true;
		enable_emerge_ack();
	}

	if ( flow->ready_msgs.empty() ) {
		iflow_msgs_empty(flow);
	}

	return ret;
}

void URtmfpSession::reject_input_flow(URtmfpIflow* flow, uint64_t code)
{
	flow->reject(code);
}

void URtmfpSession::release_input_msg(URtmfpMsg* msg)
{
	recycle_msgnode(msg);
}

void URtmfpSession::set_ready_iflow(URtmfpIflow* flow)
{
	if ( ready_link.empty() ) {
		proto.read_ready(*this);
	}

	ready_flows.insert_tail(flow->ready_link);
}

void URtmfpSession::iflow_msgs_empty(URtmfpIflow* flow)
{
	flow->ready_link.quit_list();
	if (ready_flows.empty()) {
		proto.read_empty(*this);
	}
}

static inline bool flow_cmp(const URtmfpFlow* l, const URtmfpFlow* r)
{
	return l->flow_id < r->flow_id;
}

URtmfpIflow* URtmfpSession::get_input_flow(uint32_t flowid)
{
	URtmfpFlow flowcheck(*this, flowid);
	URtmfpIflow *flow = NULL;
	FlowIter it = std::lower_bound(input_flows.begin(), input_flows.end(), &flowcheck, flow_cmp);
	if ( it == input_flows.end() || (*it)->flow_id != flowid) {
		flow = new URtmfpIflow(*this, flowid);
		input_flows.insert(it, flow);
	}
	else {
		flow = dynamic_cast<URtmfpIflow*>(*it);
	}
	return flow;
}

URtmfpOflow* URtmfpSession::get_output_flow(uint32_t flowid)
{
	URtmfpFlow flowcheck(*this, flowid, false);
	URtmfpOflow *flow = NULL;
	FlowIter it = std::lower_bound(output_flows.begin(), output_flows.end(), &flowcheck, flow_cmp);
	if ( it == output_flows.end() || (*it)->flow_id != flowid) {
		flow = new URtmfpOflow(*this, flowid);
		output_flows.insert(it, flow);
	}
	else {
		flow = dynamic_cast<URtmfpOflow*>(*it);
	}
	return flow;
}

URtmfpIflow* URtmfpSession::have_input_flow(uint32_t flowid)
{
	URtmfpFlow flowcheck(*this, flowid);
	URtmfpIflow *flow = NULL;
	FlowIter it = std::lower_bound(input_flows.begin(), input_flows.end(), &flowcheck, flow_cmp);
	if ( it == input_flows.end() || (*it)->flow_id != flowid) {
		return NULL;
	}
	else {
		return dynamic_cast<URtmfpIflow*>(*it);
	}
}

URtmfpOflow* URtmfpSession::have_output_flow(uint32_t flowid)
{
	URtmfpFlow flowcheck(*this, flowid);
	URtmfpOflow *flow = NULL;
	FlowIter it = std::lower_bound(output_flows.begin(), output_flows.end(), &flowcheck, flow_cmp);
	if ( it == output_flows.end() || (*it)->flow_id != flowid) {
		return NULL;
	}
	else {
		return dynamic_cast<URtmfpOflow*>(*it);
	}
}

void URtmfpSession::readable(URtmfpIflow* iflow)
{
	cur_readable = iflow;
	if ( mod == RESPONDER_MOD && !had_msg_ever) {
		//被动连接端，在有完整消息之后，才能获知该从哪个OMBSvc获得OMBPeer对象。
#ifndef RTMFP_SESSION_DEBUG
		uint8_t proto_id = UNaviProtoDeclare<URtmfpProto>::proto_id;
#else
		uint8_t proto_id = UNaviProtoDeclare<URtmfpDebugProto>::proto_id;
#endif
		const uint8_t* meta;
		UOMBPeerApp *app = UOMBPeerSvc::start_up_server(proto_id, *peer_id);
		if ( !app ) {
			this->close();
			return;
		}

		URtmfpPeerReady* frame = dynamic_cast<URtmfpPeerReady*>(app->get_connection());
		if (!frame) {
			this->close();
			return;
		}

		frame->set_ready_impl(this);
#ifdef OMB_FRAME_DEBUG
		UNaviLogInfra::declare_logger_tee(urtmfp::log_id, TEE_PINK);
		udebug_log(urtmfp::log_id," Get new accepted URtmfpDebugProto peer. create app peer instance." );
		UNaviLogInfra::declare_logger_tee(urtmfp::log_id, TEE_NONE);
#endif
		had_msg_ever = true;
	}
	frame->readable(iflow->flow_id);
}

void URtmfpSession::writable(URtmfpOflow* oflow)
{
	cur_writable = oflow;
	frame->writable(oflow->flow_id);
}

int64_t URtmfpPeerReady::get_outflow_relating(uint32_t flowid)
{
	URtmfpOflow * o = ready_impl->have_output_flow(flowid);
	if (!o) return -2;
	if ( o->relating == NULL) return -1;
	return o->relating->flow_id;
}

int64_t URtmfpPeerReady::get_inflow_relating(uint32_t flowid)
{
	URtmfpIflow * o = ready_impl->have_input_flow(flowid);
	if (!o) return -2;
	if ( o->relating == NULL) return -1;
	return o->relating->flow_id;
}

void URtmfpPeerReady::get_outflow_related(uint32_t flowid,std::vector<uint32_t>& inflows)
{
	URtmfpOflow * o = ready_impl->have_output_flow(flowid);
	inflows.clear();
	if (!o) return;
	vector<URtmfpIflow*>::iterator it;
	for ( it=o->related.begin(); it!=o->related.end(); it++) {
		inflows.push_back((*it)->flow_id);
	}
	return;
}

void URtmfpPeerReady::get_inflow_related(uint32_t flowid, std::vector<uint32_t>& outflows)
{
	URtmfpIflow * o = ready_impl->have_input_flow(flowid);
	outflows.clear();
	if (!o) return;
	vector<URtmfpOflow*>::iterator it;
	for ( it=o->related.begin(); it!=o->related.end(); it++) {
		outflows.push_back((*it)->flow_id);
	}
	return;
}

bool URtmfpPeerReady::have_outflow_related(uint32_t oflowid,
    uint32_t iflowid)
{
	URtmfpOflow * o = ready_impl->have_output_flow(oflowid);
	if (!o) return false;
	vector<URtmfpIflow*>::iterator it;
	for ( it=o->related.begin(); it!=o->related.end(); it++) {
		if ( iflowid == (*it)->flow_id)
			return true;
	}
	return false;
}

bool URtmfpPeerReady::have_inflow_related(uint32_t iflowid,
    uint32_t oflowid)
{
	URtmfpIflow * o = ready_impl->have_input_flow(iflowid);
	if (!o) return false;
	vector<URtmfpOflow*>::iterator it;
	for ( it=o->related.begin(); it!=o->related.end(); it++) {
		if ( oflowid == (*it)->flow_id)
			return true;
	}
	return false;
}

void URtmfpSession::update_peer_addr(const sockaddr& addr)
{
	int sz = addr.sa_family==AF_INET?sizeof(sockaddr_in):sizeof(sockaddr_in6);
	::memcpy(&sa_peer_addr,&addr,sz);
}

uint32_t URtmfpPeerReady::get_new_outflow(const FlowOptsArg* opts)
{
	uint32_t ret = ready_impl->oflow_id_gen++;
	URtmfpOflow* of = ready_impl->get_output_flow(ret);
	if (opts) {
		if (opts->meta_size>0)
			of->set_user_meta(opts->user_meta,opts->meta_size);

		URtmfpIflow* iflow = NULL;
		if (opts->relating_iflow >= 0 ) {
			iflow = ready_impl->have_input_flow(opts->relating_iflow);
			if (iflow) {
				of->relate_inflow(iflow,ACTIVE_RELATING);
				iflow->relate_outflow(of, POSITIVE_RELATED);
			}
		}
	}
	return ret;
}
