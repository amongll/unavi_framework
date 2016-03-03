/*
 * URtmfpDebugProto.cpp
 *
 *  Created on: 2014-10-21
 *      Author: li.lei
 */

#include "unavi/rtmfp/URtmfpDebugProto.h"
#include "unavi/rtmfp/URtmfpUtil.h"
#include "unavi/rtmfp/URtmfpDebugHandChunk.h"
#include "unavi/rtmfp/URtmfpSessionChunk.h"
#include "unavi/rtmfp/URtmfpFlowChunk.h"
#include "unavi/rtmfp/URtmfpPacket.h"
#include "unavi/rtmfp/URtmfpPeerId.h"

using namespace unavi;
using namespace urtmfp;
using namespace std;

URtmfpDebugProto::URtmfpDebugProto() :
	dispatcher(NULL),
	cur_decode_session(NULL)
{
	memset(pipes_band_monitor, 0x00, sizeof(pipes_band_monitor));
}

URtmfpDebugProto::~URtmfpDebugProto()
{
	//todo
}

void URtmfpDebugProto::run_init()
{
	::srand(::time(NULL)); //todo: 跨平台

	dispatcher = new SmartDisper(*this);

	session_id_gen = UNaviCycle::worker_id();
}

void URtmfpDebugProto::preproc(UNaviUDPPacket& udp)
{
}

UNaviProto::DispatchInfo URtmfpDebugProto::dispatch(UNaviUDPPacket& udp,
    bool pre_proced)
{
	static DispatchInfo drop_ret = { DISP_DROP, 0 };
	DispatchInfo ret = { DISP_OK, 0 };

	if (udp.used - 4 < CIPHER_CBC_BLOCK) {
		return drop_ret;
	}

	uint32_t ssid = URtmfpUtil::ssid_decode(udp.buf);
	if (ssid == 0) {
		try
		{
			URtmfpPacket rtmfp(ssid, udp);
			URtmfpChunk chunk, hchunk;
			do {
				chunk = rtmfp.decode_chunk();
				switch (chunk.type)
				{
				case DEBUG_SIMPLE_IHELLO_CHUNK:
				case DEBUG_SIMPLE_RHELLO_CHUNK:
				case DEBUG_SIMPLE_RHELLO_ACK_CHUNK:
					hchunk = chunk;
					break;
				default:
					if (!chunk.is_padding())
						return drop_ret;
					break;
				}
			}
			while (!chunk.is_padding());

			switch (hchunk.type) {
			case DEBUG_SIMPLE_IHELLO_CHUNK: {
				static hash<AddressKey> hasher;
				UDebugIHelloChunk check(hchunk.value, hchunk.length);
				check.URtmfpChunk::decode();
				AddressKey k(&udp.sa_addr);
				ret.session_disting = hasher(k);
				return ret;
			}
			case DEBUG_SIMPLE_RHELLO_CHUNK: {
				UDebugRHelloChunk check(hchunk.value, hchunk.length);
				check.URtmfpChunk::decode();
				ret.session_disting = check.init_ssid;
				return ret;
			}
			case DEBUG_SIMPLE_RHELLO_ACK_CHUNK: {
				UDebugRHelloAckChunk check(hchunk.value, hchunk.length);
				check.URtmfpChunk::decode();
				ret.session_disting = check.resp_ssid;
				return ret;
			}
			default:
				break;
			}
		}
		catch (UNaviException& e) {
			return drop_ret;
		}
	}
	else {
		ret.session_disting = ssid;
		return ret;
	}

	return drop_ret;
}

URtmfpDebugHand* URtmfpDebugProto::new_active_hand(const sockaddr* peer_addr)
{
	URtmfpDebugHand* ret = new URtmfpDebugHand;
	ret->near_id = (session_id_gen += UNaviWorker::worker_count());
	ret->is_initor = true;
	ret->start_check(10000,1000);
	pending_inits[ret->near_id].reset(ret);

	switch(peer_addr->sa_family)
	{
	case AF_INET:
		memcpy(&ret->in_addr,peer_addr,sizeof(sockaddr_in));
		break;
	case AF_INET6:
		memcpy(&ret->in6_addr, peer_addr, sizeof(sockaddr_in6));
		break;
	default:
		break;
	}
	return ret;
}

void URtmfpDebugProto::delete_hand(uint32_t near_ssid, bool is_initor)
{
	if ( is_initor ) {
		pending_inits.erase(near_ssid);
	}
	else {
		HANDIter it = pending_resps.find(near_ssid);
		if ( it != pending_resps.end() ) {
			AddressKey k(&it->second->sa_addr);
			pair<AddrIdxIter,AddrIdxIter> ret = accept_hand_addr_idx.equal_range(k);
			AddrIdxIter ia;
			for(ia = ret.first; ia != ret.second; ia++) {
				if ( ia->second == near_ssid ) {
					accept_hand_addr_idx.erase(ia);
					break;
				}
			}
			pending_resps.erase(it);
		}
	}
}

void URtmfpDebugProto::delete_session(uint32_t near_ssid)
{
	SSNIter is = sessions.find(near_ssid);
	if (is == sessions.end() ) return;
	if ( is->second->mod == INITIATOR_MOD ) {
		sessions.erase(is);
		return;
	}
	else if (is->second->mod == RESPONDER_MOD) {
		AddressKey k(is->second->peer_addr);
		pair<AddrIdxIter,AddrIdxIter> ret = accept_ssn_addr_idx.equal_range(k);
		AddrIdxIter ia;
		for(ia = ret.first; ia != ret.second; ia++) {
			if ( ia->second == is->first ) {
				accept_ssn_addr_idx.erase(ia);
				break;
			}
		}
		sessions.erase(is);
		return;
	}
}

void URtmfpDebugProto::init_band_checker(uint32_t pipe)
{
	UNaviUDPPipe::PipeId ppid;
	ppid.pipe_id = pipe;
	if (pipes_band_monitor[ppid.pair[0]] == NULL) {
		BandCheckTimer* band = new BandCheckTimer(*this, ppid.pipe_id);
		pipes_band_monitor[ppid.pair[0]] = band;
	}
}

void URtmfpDebugProto::process(UNaviUDPPacket& udp)
{
	init_band_checker(udp.get_pipe_id());

	UNaviLogInfra::declare_logger_tee(urtmfp::log_id, TEE_PINK);
	uint32_t ssid = URtmfpUtil::ssid_decode(udp.buf);
	if (ssid == 0) {
		//已经经过了aes解密
		URtmfpPacket rtmfp(ssid, udp);
		URtmfpChunk hchunk = rtmfp.decode_chunk();
		try {
			switch (hchunk.type) {
			case DEBUG_SIMPLE_IHELLO_CHUNK: {
				UDebugIHelloChunk check(hchunk.value, hchunk.length);
				check.URtmfpChunk::decode();
				AddressKey k(&udp.sa_addr);
				uint32_t near_ssid = 0xffffffff;
				pair<AddrIdxIter,AddrIdxIter> ret = accept_hand_addr_idx.equal_range(k);
				AddrIdxIter it_idx;
				for(it_idx = ret.first; it_idx != ret.second; it_idx++) {
					HANDIter hit = pending_resps.find(it_idx->second);
					if ( hit != pending_resps.end() ) {
						if ( hit->second->far_id == check.init_ssid ) {
							//存在重复的握手，发送rhello后立即返回。
							near_ssid = hit->second->near_id;
							hit->second->start_check(10000,1000);
							goto send_rhello;
						}
					}
				}

				ret = accept_ssn_addr_idx.equal_range(k);
				for(it_idx = ret.first; it_idx != ret.second; it_idx++) {
					SSNIter sit = sessions.find(it_idx->second);
					if ( sit != sessions.end() ) {
						if ( sit->second->far_ssid == check.init_ssid ) {
							return;
						}
					}
				}

				{
					near_ssid = (session_id_gen += UNaviWorker::worker_count());
					URtmfpDebugHand* new_hand = new URtmfpDebugHand;
					new_hand->far_id = check.init_ssid;
					new_hand->near_id = near_ssid;
					new_hand->is_initor = false;
					new_hand->pipe_id = udp.get_pipe_id();
					switch(udp.sa_addr.sa_family){
					case AF_INET:
						memcpy(&new_hand->in_addr,&udp.in_peer_addr,sizeof(sockaddr_in));
						break;
					case AF_INET6:
						memcpy(&new_hand->in6_addr,&udp.in6_peer_addr,sizeof(sockaddr_in6));
						break;
					default:
						break;
					}
					pending_resps[near_ssid].reset(new_hand);
					new_hand->start_check(10000,1000);
					accept_hand_addr_idx.insert(std::make_pair(k,near_ssid));
				}

				send_rhello:
				//发送rhello，并且设置定时器，定时发送rhello。
				URtmfpPacket out_packet(0, STARTUP_MOD);
				UDebugRHelloChunk rchunk(check.init_ssid,near_ssid);
				UNaviUDPPacket* oudp = UNaviUDPPacket::new_outpacket(udp.get_pipe_id());
				out_packet.set(*oudp);
				out_packet.push_chunk(rchunk);
				out_packet.send_out(udp.sa_addr);

				udp.release_in_packet();
				break;
			}
			case DEBUG_SIMPLE_RHELLO_CHUNK: {
				UDebugRHelloChunk check(hchunk.value, hchunk.length);
				check.URtmfpChunk::decode();

				HANDIter it= pending_inits.find(check.init_ssid);
				if ( it != pending_inits.end() ) {
					//发送RHELLO_ACK
					//建立session，从hand中删除语境。
					URtmfpPacket out_packet(0, STARTUP_MOD);
					UDebugRHelloAckChunk rchunk(check.init_ssid,check.resp_ssid);
					UNaviUDPPacket* oudp = UNaviUDPPacket::new_outpacket(udp.get_pipe_id());
					out_packet.set(*oudp);
					out_packet.push_chunk(rchunk);
					out_packet.send_out(udp.sa_addr);

					URtmfpSession* new_session = new URtmfpSession(*this, check.init_ssid, check.resp_ssid,
						INITIATOR_MOD, udp.get_pipe_id(), new URtmfpPeerId(&udp.sa_addr));
					sessions[check.init_ssid].reset(new_session);
					new_session->update_peer_addr(udp.sa_addr);
					URtmfpPeerReady* frame = dynamic_cast<URtmfpPeerReady*>(it->second->frame);
					URtmfpDebugPeerHand* hand_frame = it->second->frame;
					frame->set_ready_impl(new_session);
					it->second->frame->unbind_hand_impl();
					pending_inits.erase(it);

					try {
						hand_frame->connected();
					}
					catch(...)
					{}
				}
				else {
					SSNIter itssn = sessions.find(check.init_ssid);
					if (itssn != sessions.end() && check.resp_ssid == itssn->second->far_ssid &&
						itssn->second->mod == INITIATOR_MOD ) {
						URtmfpPacket out_packet(0, STARTUP_MOD);
						UDebugRHelloAckChunk rchunk(check.init_ssid,check.resp_ssid);
						UNaviUDPPacket* oudp = UNaviUDPPacket::new_outpacket(udp.get_pipe_id());
						out_packet.set(*oudp);
						out_packet.push_chunk(rchunk);
						out_packet.send_out(udp.sa_addr);
					}
				}
				udp.release_in_packet();
				break;
			}
			case DEBUG_SIMPLE_RHELLO_ACK_CHUNK: {
				UDebugRHelloAckChunk check(hchunk.value, hchunk.length);
				check.URtmfpChunk::decode();
				HANDIter it = pending_resps.find(check.resp_ssid);
				if ( it != pending_resps.end() ) {
					//被动连接建立。被动连接建立之后，还不存在frame对象。只有初始消息中的签名信息可以决定
					//上层的connection实体对应的svc类型。
					URtmfpSession* new_session = new URtmfpSession(*this, check.resp_ssid,
						check.init_ssid, RESPONDER_MOD, udp.get_pipe_id(), new URtmfpPeerId(&udp.sa_addr));
					sessions[check.resp_ssid].reset(new_session);
					URtmfpDebugPeerHand* hand_frame = it->second->frame;
					if (hand_frame)
						hand_frame->connected();
					delete_hand(check.resp_ssid,false);
					new_session->update_peer_addr(udp.sa_addr);
				}
				udp.release_in_packet();
				break;
			}
			default:
				break;
			}
		}
		catch (const UNaviException& e) {
			//todo: 协议错误，清理相关语境
			udp.release_in_packet();
		}
	}
	else {
		SSNIter pss = sessions.find(ssid);

		if (pss == sessions.end()) {
			udp.release_in_packet();
			return;
		}
		else {
			////todo: aes_decode(udp.buf + 4, udp.used - 4);
			URtmfpError e_code;
			try {
				URtmfpPacket rtmfp(ssid, udp);
				//输入包的mod是错误的。
				switch (rtmfp.mod) {
				case STARTUP_MOD:
				case FORBIDDEN_MOD:
					udp.release_in_packet();
					pss->second->broken(URTMFP_PEER_PROTO_ERROR);
					return;
				default:
					if (rtmfp.mod == pss->second->mod) {
						udp.release_in_packet();
						pss->second->broken(URTMFP_PEER_PROTO_ERROR);
						return;
					}
					break;
				}

				URtmfpChunk chunk;
				ostringstream oss;
				rtmfp.print_header(oss);
				udebug_log(urtmfp::log_id,oss.str().c_str());

				pss->second->rtt_check(rtmfp);
				cur_decode_session = NULL;
				do {
					chunk = rtmfp.decode_chunk();
					switch (chunk.type) {
					case PING_CHUNK: {
						UPingChunk check(chunk.value, chunk.length);
						check.URtmfpChunk::decode();
						pss->second->process_chunk(check);
						break;
					}
					case PING_REPLY_CHUNK: {
						UPingReplyChunk check(chunk.value, chunk.length);
						check.URtmfpChunk::decode();
						pss->second->process_chunk(check);
						break;
					}
					case CLOSE_CHUNK: {
						UCloseChunk check(chunk.value, chunk.length);
						check.URtmfpChunk::decode();
						pss->second->process_chunk(check);
						break;
					}
					case CLOSE_ACK_CHUNK: {
						UCloseAckChunk check(chunk.value, chunk.length);
						check.URtmfpChunk::decode();
						pss->second->process_chunk(check);
						break;
					}
					case DATA_CHUNK: {
						cur_decode_session = pss->second.get();
						UDataChunk check(chunk.value, chunk.length);
						check.URtmfpChunk::decode();
						pss->second->process_chunk(check);
						break;
					}
					case NEXT_DATA_CHUNK: {
						UNextDataChunk check(chunk.value, chunk.length);
						check.URtmfpChunk::decode();
						pss->second->process_chunk(check);
						break;
					}
					case BITMAP_ACK_CHUNK: {
						UBitmapAckChunk check(chunk.value, chunk.length);
						check.URtmfpChunk::decode();
						pss->second->process_chunk(check);
						break;
					}
					case RANGES_ACK_CHUNK: {
						URangeAckChunk check(chunk.value, chunk.length);
						check.URtmfpChunk::decode();
						pss->second->process_chunk(check);
						break;
					}
					case BUFFER_PROBE_CHUNK: {
						UBufferProbeChunk check(chunk.value, chunk.length);
						check.URtmfpChunk::decode();
						pss->second->process_chunk(check);
						break;
					}
					case FLOW_EXP_CHUNK: {
						UFlowExcpChunk check(chunk.value, chunk.length);
						check.URtmfpChunk::decode();
						pss->second->process_chunk(check);
						break;
					}
					default:
						if (!chunk.is_padding()) {
							udp.release_in_packet();
							return;
						}
						break;
					}
				}
				while (!chunk.is_padding());

				cur_decode_session = NULL;
				pss->second->prev_chunk = PADING0_CHUNK;
				UNaviLogInfra::declare_logger_tee(urtmfp::log_id, TEE_NONE);
				udp.release_in_packet();
				return;
			}
			catch (const UNaviException& e) {
				//todo: 协议错误，清理相关语境
				ostringstream oss;
				udp.dump(oss);
				udebug_log(urtmfp::log_id,"\n%s",oss.str().c_str());
				cur_decode_session = NULL;
				udp.release_in_packet();
				const URtmfpException* pe = dynamic_cast<const URtmfpException*>(&e);
				if (pe) {
					e_code = pe->e;
				}
			}
			UNaviLogInfra::declare_logger_tee(urtmfp::log_id, TEE_NONE);
			pss->second->broken(e_code);
			return;
		}
	}
	UNaviLogInfra::declare_logger_tee(urtmfp::log_id, TEE_NONE);
	return;
}

void URtmfpDebugProto::cleanup()
{
	for(int i=0; i<UNAVI_MAX_IOSVC; i++) {
		if ( pipes_band_monitor[i] )
			delete pipes_band_monitor[i];
	}
	if ( dispatcher ) delete dispatcher;
	pending_inits.clear();
	pending_resps.clear();
	sessions.clear();
}

UNaviProto* URtmfpDebugProto::copy()
{
	return new URtmfpDebugProto;
}

uint32_t URtmfpDebugProto::cur_suggest_recvbuf(uint32_t pipeid) const
{
	UNaviUDPPipe::PipeId ppid;
	ppid.pipe_id = pipeid;
	BandCheckTimer* timer = pipes_band_monitor[ppid.pair[0]];
	if (timer) {
		return timer->suggest_recvbuf;
	}
	return Default_recvbuf;
}

uint32_t URtmfpDebugProto::cur_suggest_sendbuf(uint32_t pipeid) const
{
	UNaviUDPPipe::PipeId ppid;
	ppid.pipe_id = pipeid;
	BandCheckTimer* timer = pipes_band_monitor[ppid.pair[0]];
	if (timer) {
		return timer->suggest_sendbuf;
	}
	return Default_sendbuf;
}

URtmfpDebugProto::BandCheckTimer::BandCheckTimer(URtmfpDebugProto& proto,uint32_t arg_ppid):
	UNaviEvent(1000000),
	target(proto),
	suggest_sendbuf(Default_sendbuf),
	suggest_recvbuf(Default_recvbuf),
	in_tune_direct(0),
	in_step(0),
	out_tune_direct(0),
	out_step(0),
	pipe_id(arg_ppid)
{
	UNaviWorker::regist_event(*this);
}

void URtmfpDebugProto::BandCheckTimer::process_event()
{
	double cur_inband;
	double cur_outband;
	uint32_t inband_limit;
	uint32_t outband_limit;

	UNaviWorker::current_bandwidth(pipe_id,cur_inband,
		cur_outband,inband_limit,outband_limit);

	int will_direct = 0;
	if (cur_inband < inband_limit) {
		bool rate = (double) (inband_limit - cur_inband) / cur_inband;
		if (rate > 0.05) {
			will_direct = 1;
		}
	}
	else {
		bool rate = (double) (cur_inband - inband_limit) / cur_inband;
		if (rate > 0.05) {
			will_direct = -1;
		}
	}

	if (will_direct == 0) {
		in_step = 0;
		in_tune_direct = 0;
	}
	else if (will_direct == 1) {
		if (suggest_recvbuf == URtmfpDebugProto::Max_recvbuf) {
			in_step = 0;
			in_tune_direct = 0;
		}
		else {
			if (in_tune_direct == 1) {
				if (in_step == 0) {
					in_step = 1;
				}
				else {
					if (in_step < 8)
						in_step *= 2;
				}
			}
			else {
				in_step = 1;
				in_tune_direct = 1;
			}

			if (suggest_recvbuf < URtmfpDebugProto::Max_recvbuf) {
				suggest_recvbuf += in_step * 4096;
				if (suggest_recvbuf > URtmfpDebugProto::Max_recvbuf) {
					suggest_recvbuf = URtmfpDebugProto::Max_recvbuf;
					in_step = 0;
					in_tune_direct = 0;
				}
			}
		}
	}
	else if (will_direct == -1) {
		if (suggest_recvbuf == URtmfpDebugProto::Min_recvbuf) {
			in_step = 0;
			in_tune_direct = 0;
		}
		else {
			if (in_tune_direct == -1) {
				if (in_step == 0) {
					in_step = 1;
				}
				else {
					if (in_step < 8)
						in_step *= 2;
				}
			}
			else {
				in_step = 1;
				in_tune_direct = -1;
			}

			int will_suggest = (int64_t) suggest_recvbuf - in_step * 4096;
			if (will_suggest < 4096) {
				suggest_recvbuf = URtmfpDebugProto::Min_recvbuf;
				in_step = 0;
				in_tune_direct = 0;
			}
			else {
				suggest_recvbuf = will_suggest;
			}
		}
	}

	will_direct = 0;
	if (cur_outband < outband_limit) {
		bool rate = (double) (outband_limit - cur_outband) / cur_outband;
		if (rate > 0.05) {
			will_direct = 1;
		}
	}
	else {
		bool rate = (double) (cur_outband - outband_limit) / cur_outband;
		if (rate > 0.05) {
			will_direct = -1;
		}
	}

	if (will_direct == 0) {
		out_step = 0;
		out_tune_direct = 0;
	}
	else if (will_direct == 1) {
		if (suggest_sendbuf == URtmfpDebugProto::Max_sendbuf) {
			out_step = 0;
			out_tune_direct = 0;
		}
		else {
			if (out_tune_direct == 1) {
				if (out_step == 0) {
					out_step = 1;
				}
				else {
					if (out_step < 8)
						out_step *= 2;
				}
			}
			else {
				out_step = 1;
				out_tune_direct = 1;
			}

			if (suggest_sendbuf < URtmfpDebugProto::Max_sendbuf) {
				suggest_sendbuf += out_step * 4096;
				if (suggest_sendbuf > URtmfpDebugProto::Max_sendbuf) {
					suggest_sendbuf = URtmfpDebugProto::Max_sendbuf;
					out_step = 0;
					out_tune_direct = 0;
				}
			}
		}
	}
	else if (will_direct == -1) {
		if (suggest_sendbuf == URtmfpDebugProto::Min_sendbuf) {
			out_step = 0;
			out_tune_direct = 0;
		}
		else {
			if (out_tune_direct == -1) {
				if (out_step == 0) {
					out_step = 1;
				}
				else {
					if (out_step < 8)
						out_step *= 2;
				}
			}
			else {
				out_step = 1;
				out_tune_direct = -1;
			}

			int will_suggest = (int64_t) suggest_sendbuf - out_step * 4096;
			if (will_suggest < 4096) {
				suggest_sendbuf = URtmfpDebugProto::Min_sendbuf;
				out_step = 0;
				out_tune_direct = 0;
			}
			else {
				suggest_sendbuf = will_suggest;
			}
		}
	}
}

void URtmfpDebugProto::read_ready(URtmfpSession& ss)
{
	read_ready_sessions.insert_tail(ss.ready_link);
	dispatcher->set_already();
}

void URtmfpDebugProto::read_empty(URtmfpSession& ss)
{
	ss.ready_link.quit_list();
	if ( read_ready_sessions.empty() )
		UNaviWorker::quit_event(*dispatcher, UNaviEvent::EV_ALREADY);
}

URtmfpDebugProto::SmartDisper::SmartDisper(URtmfpDebugProto& oo):
	target(oo)
{
	UNaviWorker::regist_event(*this);
}

void URtmfpDebugProto::SmartDisper::process_event()
{
	UNaviListable* e = target.read_ready_sessions.get_head();
	UNaviListable* n = NULL;
	URtmfpSession* ssn;

	UNaviListable* fe;
	UNaviListable* fn;
	URtmfpIflow* flow;

	while( e ) {
		n = target.read_ready_sessions.get_next(e);
		ssn = unavi_list_data(e, URtmfpSession, ready_link);
		e = n;

		fe = ssn->ready_flows.get_head();
		while( fe ) {
			fn = ssn->ready_flows.get_next(fe);
			flow = URtmfpIflow::get_from_ready_link(fe);
			ssn->readable(flow); //从session->read_msg接口触发ready树的退出
			fe = fn;
		}
	}

	if ( target.read_ready_sessions.empty() == false ) //如果消息没有被读完，则一直递送。
		set_already();
}

