/*
 * URtmfpProto.cpp
 *
 *  Created on: 2014-10-21
 *      Author: li.lei
 */

#include "unavi/rtmfp/URtmfpProto.h"
#include "unavi/rtmfp/URtmfpUtil.h"
#include "unavi/rtmfp/URtmfpHandChunk.h"
#include "unavi/rtmfp/URtmfpSessionChunk.h"
#include "unavi/rtmfp/URtmfpFlowChunk.h"

using namespace unavi;
using namespace urtmfp;

URtmfpProto::URtmfpProto() :
	dispatcher(NULL)
{
	memset(pipes_band_monitor, 0x00, sizeof(pipes_band_monitor));
}

URtmfpProto::~URtmfpProto()
{
	//todo
}

void URtmfpProto::run_init()
{
	::srand(::time(NULL)); //todo: 跨平台

	dispatcher = new SmartDisper(*this);
}

void URtmfpProto::preproc(UNaviUDPPacket& udp)
{
	//todo: aes_decode(udp.buf + 4, udp.used - 4);
	URtmfpUtil::ssid_encode(0, udp.buf + 4, udp.buf); //使得dispatch的取session id过程一致
}

UNaviProto::DispatchInfo URtmfpProto::dispatch(UNaviUDPPacket& udp,
    bool pre_proced)
{
	static DispatchInfo drop_ret = { DISP_DROP, 0 };
	static DispatchInfo prev_proc_ret = { DISP_PRE_PROC, 0 };
	DispatchInfo ret = { DISP_OK, 0 };

	if (udp.used - 4 < CIPHER_CBC_BLOCK) {
		return drop_ret;
	}

	uint32_t ssid = URtmfpUtil::ssid_decode(udp.buf);
	if (!pre_proced) {
		if (ssid == 0) {
			return prev_proc_ret;
		}
		else {
			ret.session_disting = ssid;
			return ret;
		}
	}
	else {
		if (ssid == 0) {
			//已经 default key AES解密之后的包
			//todo: 根据tag或者cookie算出session_disting
			try
			{
				URtmfpPacket rtmfp(ssid, udp);
				URtmfpChunk chunk, hchunk;
				do {
					chunk = rtmfp.decode_chunk();
					switch (chunk.type)
					{
					case IHELLO_CHUNK:
						case RHELLO_CHUNK:
						case REDIRECT_CHUNK:
						case IIKEYING_CHUNK:
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
				case IHELLO_CHUNK: {
					UIHelloChunk check(hchunk.value, hchunk.length);
					ret.session_disting = ::rand_r(&rand_seed);
					return ret;
				}
				case RHELLO_CHUNK: {
					URHelloChunk check(hchunk.value, hchunk.length);
					//todo: 根据tag分发
					return ret;
				}
				case REDIRECT_CHUNK: {
					URedirectChunk check(hchunk.value, hchunk.length);
					//todo: 根据tag分发
					return ret;
				}
				case IIKEYING_CHUNK: {
					UIIKeyingChunk check(hchunk.value, hchunk.length);
					//todo : 根据cookie分发
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
			return drop_ret;
		}
	}

	return drop_ret;
}

void URtmfpProto::init_band_checker(uint32_t pipe)
{
	UNaviUDPPipe::PipeId ppid;
	ppid.pipe_id = pipe;
	if (pipes_band_monitor[ppid.pair[0]] == NULL) {
		BandCheckTimer* band = new BandCheckTimer(*this, ppid.pipe_id);
		pipes_band_monitor[ppid.pair[0]] = band;
	}
}

void URtmfpProto::process(UNaviUDPPacket& udp)
{
	init_band_checker(udp.get_pipe_id());

	uint32_t ssid = URtmfpUtil::ssid_decode(udp.buf);
	if (ssid == 0) {
		//已经经过了aes解密
		URtmfpPacket rtmfp(ssid, udp);
		URtmfpChunk hchunk = rtmfp.decode_chunk();
		try {
			switch (hchunk.type) {
			case IHELLO_CHUNK: {
				UIHelloChunk check(hchunk.value, hchunk.length);
				check.URtmfpChunk::decode();
				//todo:
				break;
			}
			case RHELLO_CHUNK: {
				URHelloChunk check(hchunk.value, hchunk.length);
				check.URtmfpChunk::decode();
				//todo:
				break;
			}
			case REDIRECT_CHUNK: {
				URedirectChunk check(hchunk.value, hchunk.length);
				check.URtmfpChunk::decode();
				//todo:
				break;
			}
			case IIKEYING_CHUNK: {
				UIIKeyingChunk check(hchunk.value, hchunk.length);
				check.URtmfpChunk::decode();
				//todo:
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
		HANDIter phand = pending_inits.find(ssid);

		if (pss == sessions.end()) {
			if (phand == pending_inits.end()) {
				udp.release_in_packet();
				return;
			}
			else {
				////todo: aes_decode(udp.buf + 4, udp.used - 4);
				try {
					URtmfpPacket rtmfp(ssid, udp);
					if (rtmfp.mod != STARTUP_MOD) {
						udp.release_in_packet();
						return;
					}

					URtmfpChunk chunk, chunk2;
					do {
						chunk = rtmfp.decode_chunk();
						switch (chunk.type)
						{
						case COOKIE_CHANGE_CHUNK:
							case RIKEYING_CHUNK:
							chunk2 = chunk;
							break;
						default:
							if (!chunk.is_padding()) {
								udp.release_in_packet();
								return;
							}
							break;
						}
					}
					while (!chunk.is_padding());

					switch (chunk2.type) {
					case COOKIE_CHANGE_CHUNK: {
						UCookieChangeChunk check(chunk2.value, chunk2.length);
						check.URtmfpChunk::decode();
						phand->second->process_chunk(check);
						break;
					}
					case RIKEYING_CHUNK: {
						URIKeyingChunk check(chunk2.value, chunk2.length);
						check.URtmfpChunk::decode();
						phand->second->process_chunk(check);
						break;
					}
					default:
						break;
					}
					udp.release_in_packet();
				} // end try
				catch (const UNaviException& e) {
					//todo: 协议错误清理相关语境
					udp.release_in_packet();
					return;
				}
			}
		}
		else {
			////todo: aes_decode(udp.buf + 4, udp.used - 4);
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
#ifdef RTMFP_DEBUG
				rtmfp.print_header(std::cout);
#endif
				do {
					chunk = rtmfp.decode_chunk();
					pss->second->rtt_check(rtmfp);
					switch (chunk.type) {
					case FWD_IHELLO_CHUNK: {
						UFIHelloChunk check(chunk.value, chunk.length);
						check.URtmfpChunk::decode();
						pss->second->process_chunk(check);
						break;
					}
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

				pss->second->prev_chunk = PADING0_CHUNK;

				udp.release_in_packet();
				return;
			}
			catch (const UNaviException& e) {
				//todo: 协议错误，清理相关语境
				udp.release_in_packet();
				const URtmfpException* pe = dynamic_cast<const URtmfpException*>(&e);
				if (pe) {
					pss->second->broken(pe->e);
				}

				return;
			}
		}
	}
	return;
}

void URtmfpProto::cleanup()
{
	for(int i=0; i<UNAVI_MAX_IOSVC; i++) {
		if ( pipes_band_monitor[i] )
			delete pipes_band_monitor[i];
	}
	if ( dispatcher ) delete dispatcher;
}

UNaviProto* URtmfpProto::copy()
{
	return new URtmfpProto;
}

uint32_t URtmfpProto::cur_suggest_recvbuf(uint32_t pipeid) const
{
	UNaviUDPPipe::PipeId ppid;
	ppid.pipe_id = pipeid;
	BandCheckTimer* timer = pipes_band_monitor[ppid.pair[0]];
	if (timer) {
		return timer->suggest_recvbuf;
	}
	return Default_recvbuf;
}

uint32_t URtmfpProto::cur_suggest_sendbuf(uint32_t pipeid) const
{
	UNaviUDPPipe::PipeId ppid;
	ppid.pipe_id = pipeid;
	BandCheckTimer* timer = pipes_band_monitor[ppid.pair[0]];
	if (timer) {
		return timer->suggest_sendbuf;
	}
	return Default_sendbuf;
}

URtmfpProto::BandCheckTimer::BandCheckTimer(URtmfpProto& proto,uint32_t arg_ppid):
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

void URtmfpProto::BandCheckTimer::process_event()
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
		if (suggest_recvbuf == URtmfpProto::Max_recvbuf) {
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

			if (suggest_recvbuf < URtmfpProto::Max_recvbuf) {
				suggest_recvbuf += in_step * 4096;
				if (suggest_recvbuf > URtmfpProto::Max_recvbuf) {
					suggest_recvbuf = URtmfpProto::Max_recvbuf;
					in_step = 0;
					in_tune_direct = 0;
				}
			}
		}
	}
	else if (will_direct == -1) {
		if (suggest_recvbuf == URtmfpProto::Min_recvbuf) {
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
				suggest_recvbuf = URtmfpProto::Min_recvbuf;
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
		if (suggest_sendbuf == URtmfpProto::Max_sendbuf) {
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

			if (suggest_sendbuf < URtmfpProto::Max_sendbuf) {
				suggest_sendbuf += out_step * 4096;
				if (suggest_sendbuf > URtmfpProto::Max_sendbuf) {
					suggest_sendbuf = URtmfpProto::Max_sendbuf;
					out_step = 0;
					out_tune_direct = 0;
				}
			}
		}
	}
	else if (will_direct == -1) {
		if (suggest_sendbuf == URtmfpProto::Min_sendbuf) {
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
				suggest_sendbuf = URtmfpProto::Min_sendbuf;
				out_step = 0;
				out_tune_direct = 0;
			}
			else {
				suggest_sendbuf = will_suggest;
			}
		}
	}
}

void URtmfpProto::read_ready(URtmfpSession& ss)
{
	read_ready_sessions.insert_tail(ss.ready_link);
	dispatcher->set_already();
}

void URtmfpProto::read_empty(URtmfpSession& ss)
{
	ss.ready_link.quit_list();
	if ( read_ready_sessions.empty() )
		UNaviWorker::quit_event(*dispatcher, UNaviEvent::EV_ALREADY);
}

URtmfpProto::SmartDisper::SmartDisper(URtmfpProto& oo):
	target(oo)
{
	UNaviWorker::regist_event(*this);
}

void URtmfpProto::SmartDisper::process_event()
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

/**
void URtmfpProto::_new_session(URtmfpPeerReady* frame, uint32_t pipe_id, uint32_t n_ssid, uint32_t f_ssid, SessionMod mod)
{
	URtmfpSession* ssn = new URtmfpSession(*this,n_ssid,f_ssid,mod,pipe_id);
	frame->set_ready_impl(ssn);
	ssn->frame = frame;

	sessions[n_ssid].reset(ssn);
}*/
