/*
 * URtmfpPacket.h
 *
 *  Created on: 2014-10-16
 *      Author: li.lei
 */

#ifndef URTMFPPACKET_H_
#define URTMFPPACKET_H_

#include "unavi/rtmfp/URtmfpCommon.h"
#include "unavi/rtmfp/URtmfpChunk.h"
#include "unavi/core/UNaviUDPPacket.h"
#include <stack>

#ifdef RTMFP_DEBUG
#include <iostream>
#endif

URTMFP_NMSP_BEGIN

class URtmfpProto;
class URtmfpSession;
struct URtmfpPacket : public UNaviListable
{
	const uint32_t session_id;
	UNaviUDPPacket* packet;
	const bool in;
	const SessionMod mod;
	const uint8_t flags;
	const uint16_t timestamp;
	const uint16_t timestamp_echo;

	// ��ʾpacket����critical��������Ҫ������
	bool has_critical() const
	{
		return flags & 0x80;
	}

	// ��ʾ�Զ����ڴ�����session����critical���ݡ�
	bool peer_has_critical() const
	{
		return flags & 0x40;
	}

	bool has_timestamp() const
	{
		return flags & 0x8;
	}

	bool has_timestamp_echo() const
	{
		return flags & 0x4;
	}

	void set_timestamp_echo(uint16_t echo)
	{
		uint8_t* pflag = const_cast<uint8_t*>(&flags);
		*pflag |= 0x4;
		uint16_t* pecho = const_cast<uint16_t*>(&timestamp_echo);
		*pecho = echo;

		//uint16_t prev_pos = packet->pos;
		//packet->seek(7);
		packet->fillin_u16(timestamp_echo);
		//packet->seek(prev_pos);
	}

	void set_timestamp(uint16_t tm)
	{
		uint8_t* pflag = const_cast<uint8_t*>(&flags);
		*pflag |= 0x8;
		uint16_t* pecho = const_cast<uint16_t*>(&timestamp);
		*pecho = tm;

		//uint16_t prev_pos = packet->pos;
		//packet->seek(5);
		packet->fillin_u16(timestamp);
		//packet->seek(prev_pos);
	}

	void set(uint32_t ssid, SessionMod mod, UNaviUDPPacket& out)
	{
		set(ssid);
		set_mod(mod);
		set(out);
	}

	void set(uint32_t ssid)
	{
		uint32_t* pss = const_cast<uint32_t*>(&this->session_id);
		*pss = ssid;
	}

	void set_mod(SessionMod mod)
	{
		SessionMod* pmod = const_cast<SessionMod*>(&this->mod);
		*pmod = mod;
	}
	//�����µ�udp��session modΪ3ʱ��Ĭ�ϱ���5�ֽ���Ϊͷ���� modΪ1����2ʱĬ�ϱ���9�ֽ����ڴ��rtmfp packetͷ����
	void set(UNaviUDPPacket& out)
	{
		packet = &out;
		packet->reset();
		packet->seek(5);
	}

	//�Խ���chunk�ı��룬������packet��
	void push_chunk(const URtmfpChunk& chunk);

#ifdef RTMFP_SESSION_DEBUG
	void push_chunk_trace(const URtmfpChunk& chunk, const char* fmt, ...);
	void print_trace();
#endif

	//aes_keyΪ��ʱ�������м��ܡ�
	void send_out(const sockaddr& peer, const uint8_t* aes_key = NULL, int key_sz = 0);

	static uint32_t min_packet_for_chunk(uint32_t chunk_sz)
	{
		uint32_t ret = 5 + chunk_sz;
		ret += CIPHER_CBC_BLOCK - 1;
		ret -= ret%CIPHER_CBC_BLOCK;
		return ret + 4;
	}

	uint32_t available()const
	{
		uint32_t sz = packet->mtu - 4;
		sz = sz - sz%CIPHER_CBC_BLOCK; //��������packet���ݵĿռ��С��������4�ֽ�ͷ���Լ�ĩβpadding

		return sz - ( packet->used - 4);
	}

#ifdef RTMFP_DEBUG
	void print_header(std::ostream& o)const;
#endif

	URtmfpPacket(uint32_t ssid, UNaviUDPPacket& in);
	URtmfpPacket(uint32_t ssid, SessionMod mod);
	virtual ~URtmfpPacket();
private:
	friend class URtmfpProto;
	friend class URtmfpDebugProto;
	friend class URtmfpSession;

	void padding();

	URtmfpChunk decode_chunk();

#ifdef RTMFP_SESSION_DEBUG
	struct SendChunkTrace {
		SendChunkTrace():
			type(PADING0_CHUNK),
			len(0)
		{}
		SendChunkTrace(ChunkType t,uint32_t l,const char* d=""):
			type(t),
			len(l),
			desc(d)
		{}

		ChunkType type;
		uint32_t len;
		std::string desc;
	};

	std::stack<SendChunkTrace> traces;
	std::stack<std::pair<int,int> > decode_traces;
#endif
};

URTMFP_NMSP_END

#endif /* URTMFPPACKET_H_ */
