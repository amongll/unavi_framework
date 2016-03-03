/*
 * URtmfpUtil.h
 *
 *  Created on: 2014-10-15
 *      Author: dell
 */

#ifndef URTMFPUTIL_H_
#define URTMFPUTIL_H_

#include "unavi/rtmfp/URtmfpCommon.h"
#include "unavi/rtmfp/URtmfpException.h"
#include "unavi/util/UNaviUtil.h"
#include "unavi/core/UNaviLog.h"

URTMFP_NMSP_BEGIN

struct URtmfpUtil
{
	//sz为零时，表示不需要保护越界
	static uint32_t vln_encode(uint64_t v,uint8_t* buf,size_t sz = 0);
	static uint32_t vln_length(uint64_t v);
	static int32_t vln_decode(const uint8_t* vln,uint64_t& v);

	static uint32_t addr_enc_length(const sockaddr& addr)
	{
		if (addr.sa_family == AF_INET )
			return 7;
		else if (addr.sa_family == AF_INET6 )
			return 19;
		return 0;
	}

	static uint32_t addr_encode(const sockaddr& addr, AddressOrig orig, uint8_t* buf, size_t sz);
	static int32_t addr_decode(const uint8_t* raw, sockaddr_storage& dst, AddressOrig& orig, uint32_t& occupy);

	static uint32_t ssid_decode(const uint8_t* raw)
	{
		const uint32_t* p32 = reinterpret_cast<const uint32_t*>(raw);
		uint32_t n32 = (*p32) ^ ( *(p32+1) ^ *(p32+2));
		return UNaviUtil::ntoh_u32(reinterpret_cast<const uint8_t*>(&n32));
	}

	static void ssid_encode(uint32_t session_id, const uint8_t* pkt_header, uint8_t* raw)
	{
		uint32_t* scrambled = reinterpret_cast<uint32_t*>(raw);
		const uint32_t* pkt32 = reinterpret_cast<const uint32_t*>(pkt_header);
		UNaviUtil::hton_u32(session_id);
		*scrambled = session_id ^ (*pkt32 ^ *(pkt32+1));
	}

	static const char* chunk_type_str(ChunkType type);

	static FlowFragmentCtrl decode_frag_ctrl(uint8_t byte)
	{
		switch(((byte)&0x30) >> 4 )
		{
		case MSG_WHOLE_FRAG:
			return MSG_WHOLE_FRAG;
		case MSG_FIRST_FRAG:
			return MSG_FIRST_FRAG;
		case MSG_LAST_FRAG:
			return MSG_LAST_FRAG;
		case MSG_MIDDLE_FRAG:
			return MSG_MIDDLE_FRAG;
		default: {
			URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
			uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
			break;
		}
		}
	}

	static SessionMod decode_session_mod(uint8_t byte)
	{
		switch ( byte & 0x03 )
		{
		case FORBIDDEN_MOD:
			return FORBIDDEN_MOD;
		case INITIATOR_MOD:
			return INITIATOR_MOD;
		case RESPONDER_MOD:
			return RESPONDER_MOD;
		case STARTUP_MOD:
			return STARTUP_MOD;
		default:{
			URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
			uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
			break;
		}
		}
	}

	static AddressOrig decode_addr_orig(uint8_t byte)
	{
		switch( byte & 0x03 )
		{
		case UNKNOWN_ADDR:
			return UNKNOWN_ADDR;
		case LOCAL_ADDR:
			return LOCAL_ADDR;
		case REMOTE_ADDR:
			return REMOTE_ADDR;
		case RELAY_ADDR:
			return RELAY_ADDR;
		default:
		{
			URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
			uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
			break;
		}
		}
	}
};

class URtmfpOptList;
struct URtmfpOpt
{
	URtmfpOpt():
	type(-1),
	value(NULL),
	length(0),
	copyed(false)
	{}

	URtmfpOpt(int32_t t,const uint8_t* v, uint32_t vsz):
	type(t), value(v), length(vsz),copyed(false)
	{
		if ( length>0 && length != 0xffffffff && value==NULL ) {
			URtmfpException e(URTMFP_IMPL_ERROR);
			uexception_throw_v(e,LOG_NOTICE, urtmfp::log_id);
		}
	}

	URtmfpOpt(const URtmfpOpt& rh):
	type(rh.type),
	value(rh.value),
	length(rh.length),
	copyed(false)
	{}

	~URtmfpOpt()
	{
		if (copyed && value)
			delete []value;
	}

	URtmfpOpt& operator=(const URtmfpOpt& rh);

	uint32_t encode_occupy()const;
	void dup();

	const int32_t type;	//type是vln的，不可能得到-1.
	const uint8_t* value;
	const uint32_t length;
	static const URtmfpOpt Marker;
	static const URtmfpOpt Invalid_opt;
	static URtmfpOpt decode(const uint8_t* content, uint32_t& occupy);
private:
	friend class URtmfpOptList;
	friend bool operator==(const URtmfpOpt& lh, const URtmfpOpt& rh);
	uint32_t encode(uint8_t* content)const;
	bool copyed;
};

bool operator==(const URtmfpOpt& lh, const URtmfpOpt& rh);

struct URtmfpOptList
{
	static URtmfpOptList decode(const uint8_t* content, uint32_t& occupy);
	URtmfpOptList();
	~URtmfpOptList();
	uint32_t encode_occupy()const;
	uint32_t encode(uint8_t* content,uint32_t sz = 0)const;

	URtmfpOpt* push_opt(int32_t type, const uint8_t* value, uint32_t length)
	{
		if (type != -1 && length != 0xffffffff) {
			std::vector<URtmfpOpt>::iterator it = list.insert(list.end(),URtmfpOpt(type,value,length));
			return &(*it);
		}
		return NULL;
	}

	URtmfpOpt* push_opt(const URtmfpOpt& opt)
	{
		if (opt.type != -1 && opt.length != 0xffffffff) {
			std::vector<URtmfpOpt>::iterator it = list.insert(list.end(),opt);
			return &(*it);
		}
		return NULL;
	}

	void dup()
	{
		std::vector<URtmfpOpt>::iterator it;
		for (it=list.begin(); it!=list.end(); it++) {
			it->dup();
		}
	}

	std::vector<URtmfpOpt> list;
	bool invalid;

	static const URtmfpOptList Invalid_list;
	static const URtmfpOptList Empty_list;
private:
	URtmfpOptList(bool is_invalid):
		invalid(is_invalid)
	{}
};

URTMFP_NMSP_END

#endif /* URTMFPUTIL_H_ */
