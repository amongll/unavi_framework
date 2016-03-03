/*
 * URtmfpPacket.h
 *
 *  Created on: 2014-10-20
 *      Author: li.lei
 */

#include "unavi/rtmfp/URtmfpPacket.h"
#include "unavi/rtmfp/URtmfpUtil.h"
#include "unavi/rtmfp/URtmfpFlowChunk.h"
#include <cstdarg>

using namespace unavi;
using namespace urtmfp;
using namespace std;

URtmfpPacket::URtmfpPacket(uint32_t ssid , UNaviUDPPacket& in):
	session_id(ssid),
	packet(&in),
	in(true),
	mod(FORBIDDEN_MOD),
	flags(0),
	timestamp(0),
	timestamp_echo(0)
{
	uint8_t* pflag = const_cast<uint8_t*>(&flags);
	SessionMod* pmod = const_cast<SessionMod*>(&mod);
	in.seek(4);
	*pflag = in.read_u8();
	*pmod = URtmfpUtil::decode_session_mod(flags);
	if (has_timestamp()) {
		uint16_t* pecho = const_cast<uint16_t*>(&timestamp);
		*pecho = in.read_u16();
	}
	if (has_timestamp_echo()) {
		uint16_t* pecho = const_cast<uint16_t*>(&timestamp_echo);
		*pecho = in.read_u16();
	}
	//in.seek(5);
}

URtmfpPacket::URtmfpPacket(uint32_t ssid, SessionMod arg_mod):
    session_id(ssid),
    packet(NULL),
    in(false),
    mod(arg_mod),
    flags(0),
    timestamp(0),
    timestamp_echo(0)
{
	//保留头部4字节抑或sessionid，1字节flag，4字节时戳，不论是否发送时戳
	//packet->reset();
	//packet->seek(5,UNaviUDPPacket::SEEK_ABS);
}

#ifdef RTMFP_SESSION_DEBUG
void URtmfpPacket::push_chunk_trace(const URtmfpChunk& chunk, const char* fmt, ...)
{
	char buf[2048];
	int pos = packet->pos;
	va_list vl;
	va_start(vl,fmt);
	int off = vsnprintf(buf,sizeof(buf), fmt, vl);
	va_end(vl);
	sprintf(buf+off, " packet pos:%d", pos);

	uint32_t sz = chunk.encode(packet->cur());
	packet->seek(sz, UNaviUDPPacket::SEEK_REL);

	traces.push(SendChunkTrace(chunk.type, sz, buf));
}
void URtmfpPacket::print_trace()
{
	ostringstream oss;
	oss<<"Out packet traces: [\n";
	while(traces.empty()==false) {
		SendChunkTrace& trace = traces.top();
		oss<<"\t"<<URtmfpUtil::chunk_type_str(trace.type)<<" "<<trace.len<<" >>"<<trace.desc<<std::endl;
		traces.pop();
	}
	oss<<"]\n";
	udebug_log(urtmfp::log_id,oss.str().c_str());
}

#endif

void URtmfpPacket::push_chunk(const URtmfpChunk& chunk)
{
	uint32_t sz = chunk.encode(packet->cur());
	packet->seek(sz, UNaviUDPPacket::SEEK_REL);
#ifdef RTMFP_SESSION_DEBUG
	traces.push(SendChunkTrace(chunk.type, sz));
#endif
}

URtmfpPacket::~URtmfpPacket()
{
	if (packet && in==false ) {
		packet->cancel_out_packet();
		packet = NULL;
	}
}

void URtmfpPacket::padding()
{
	uint32_t sz = packet->used - 4;
	int pading_cnt = CIPHER_CBC_BLOCK - sz%CIPHER_CBC_BLOCK;
	if ( pading_cnt == CIPHER_CBC_BLOCK ) return;

	::memset(packet->buf + packet->pos, 0x00, pading_cnt);
	packet->seek(pading_cnt, UNaviUDPPacket::SEEK_REL);
}

URtmfpChunk URtmfpPacket::decode_chunk()
{
	static URtmfpChunk ret_pading_chunk(PADING0_CHUNK);
#ifdef RTMFP_DEBUG
	decode_traces.push(make_pair(packet->buf[packet->pos],packet->pos));
	try {
#endif
		if ( packet->pos == packet->used )
			return ret_pading_chunk;
		URtmfpChunk tmp(packet->cur(), packet->used - packet->pos);
		packet->read(tmp.occupy());
		return tmp;
#ifdef RTMFP_DEBUG
	}
	catch(const std::exception& e) {
		;
	}
	while(!decode_traces.empty()) {
		std::pair<int,int> pp = decode_traces.top();
		udebug_log(urtmfp::log_id, "decode_trace: type:%02x pos:%d", pp.first,pp.second);
		decode_traces.pop();
	}
	throw URtmfpException(URTMFP_ICHUNK_DECODE_ERROR);
#endif
}

void URtmfpPacket::send_out(const sockaddr& peer, const uint8_t* aes_key, int sz)
{
	padding();

	packet->seek(4);
	(uint8_t&)flags |= Session_mod_bits[mod];
	packet->fillin_u8(flags);

	*((uint8_t*)&flags) = 0;

	if (aes_key) {
		//todo: 加密
	}

	URtmfpUtil::ssid_encode(session_id, packet->buf + 4, packet->buf);

#ifdef RTMFP_SESSION_DEBUG
	{
		UNaviLogInfra::declare_logger_tee(urtmfp::log_id, TEE_ORANGE);
		UNaviUDPPacket* ck_udp = UNaviUDPPacket::new_outpacket(packet->get_pipe_id(), packet->used);
		uint32_t ck_ssid = URtmfpUtil::ssid_decode(packet->buf);
		ck_udp->fillin(packet->buf, packet->used);
		URtmfpPacket check_pkt(ck_ssid, *ck_udp);
		ostringstream oss;
		check_pkt.print_header(oss);
		oss.flush();
		udebug_log(urtmfp::log_id,oss.str().c_str());
		print_trace();
		bool ck_err = false;
		try {
			URtmfpChunk chunk;
			do {
				chunk = check_pkt.decode_chunk();
				if ( chunk.type == BITMAP_ACK_CHUNK) {
					UBitmapAckChunk check(chunk.value , chunk.length);
					check.URtmfpChunk::decode();
				}
				else if (chunk.type == RANGES_ACK_CHUNK) {
					URangeAckChunk check(chunk.value, chunk.length);
					check.URtmfpChunk::decode();
				}
				else if (chunk.type == DATA_CHUNK ) {
					UDataChunk check(chunk.value,chunk.length);
					check.URtmfpChunk::decode();
				}
				else if (chunk.type == NEXT_DATA_CHUNK ) {
					UNextDataChunk check(chunk.value,chunk.length);
					check.URtmfpChunk::decode();
				}
			}
			while (!chunk.is_padding());
		} catch ( const std::exception& e) {
			ck_udp->dump(oss);
			udebug_log(urtmfp::log_id,oss.str().c_str());
			ck_err = true;
		}

		ck_udp->cancel_out_packet();
		UNaviLogInfra::declare_logger_tee(urtmfp::log_id, TEE_NONE);

		if (ck_err) {
			throw URtmfpException(URTMFP_OCHUNK_ENCODE_ERROR);
		}
	}
#endif

	packet->output(peer);
	packet = NULL;
	return;
}

#ifdef RTMFP_DEBUG
void URtmfpPacket::print_header(std::ostream& os)const
{
	if ( ! in ) return;
	os << "SSN nid:" << session_id << " mod:" << mod;
	if ( has_timestamp() ) {
		os << " time:" << timestamp;
	}
	if ( has_timestamp_echo() ) {
		os << " time_echo:" << timestamp_echo;
	}
	os << std::endl;
}
#endif


