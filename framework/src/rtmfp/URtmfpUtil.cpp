/*
 * URtmfpUtil.cpp
 *
 *  Created on: 2014-10-15
 *      Author: li.lei
 */

#include "unavi/rtmfp/URtmfpUtil.h"
#include "unavi/rtmfp/URtmfpException.h"
#include "unavi/core/UNaviLog.h"

using namespace unavi;
using namespace urtmfp;
using namespace std;

const char* URtmfpException::err_desc[] =
{
	"input chunk type error",
	"output chunk type error",
	"input chunk decode error",
	"output chunk encode error",
	"invalid input data/next data chunk",
	"rtmfp abused",
	"impl error.",
	"peer communication failed",
	"peer proto invalid.",
	"flow relating failed",
	"ping reply invalid",
	"unknown rtmfp error"
};

const uint64_t urtmfp::Vln_size_mark[] =
{
	0x7f,
	0x3fff,
	0x1fffff,
	0xfffffff,
	0x7ffffffff,
	0x3ffffffffff,
	0x1ffffffffffff,
	0xffffffffffffff,
	0x7fffffffffffffff,
	0xffffffffffffffff
};

const uint8_t urtmfp::Addr_orig_bits[] =
{
	0x00,
	0x01,
	0x02,
	0x03
};

const uint8_t urtmfp::Session_mod_bits[] =
{
	0x00,
	0x01,
	0x02,
	0x03
};

const uint8_t urtmfp::Flow_frag_ctrl_bits[] =
{
	0x00,
	0x10,
	0x20,
	0x30
};

const uint8_t urtmfp::Flow_frag_ctrl_clear_bit = 0xcf;
const uint8_t urtmfp::Flow_frag_abn_bit = 0x02;
const uint8_t urtmfp::Flow_frag_fin_bit = 0x01;
const uint8_t urtmfp::Flow_frag_opt_bit = 0x80;

uint32_t URtmfpUtil::vln_encode(uint64_t v, uint8_t* buf, size_t sz)
{
	if (buf == NULL ) {
		return vln_length(v);
	}

	uint8_t tmp[10];
	uint8_t idx = 9;
	uint8_t bit_flag = 0x00;
	do {
		tmp[idx--] = (v & 0x7f) | bit_flag;
		bit_flag = 0x80;
	} while(v>>=7);

	idx = 9-idx;
	if ( sz == 0)
		memcpy(buf, tmp+10-idx, idx);
	else
		memcpy(buf, tmp+10-idx, sz>idx?idx:sz);
	return idx;
}

uint32_t URtmfpUtil::vln_length(uint64_t v)
{
	int32_t ret = 0;
	while ( v & ~Vln_size_mark[ret++] );
	return ret;
}

int32_t URtmfpUtil::vln_decode(const uint8_t* vln,uint64_t& v)
{
	int32_t cnt = 0;
	uint64_t swp= v;
	v=0;
	do {
		++cnt;
		v = v<<7 | (*vln&0x7f);
		if (!(*(vln++)&0x80))break;
	} while (cnt<=10);

	if (cnt==11) {
		v=swp;
		return -1;
	}

	return cnt;
}

uint32_t URtmfpUtil::addr_encode(const sockaddr& addr, AddressOrig orig, uint8_t* buf, size_t sz)
{
	if ( addr_enc_length(addr) > sz ) return 0;

	if (addr.sa_family==AF_INET) {
		*(buf++) = Addr_orig_bits[orig];
		memcpy(buf,&((sockaddr_in*)&addr)->sin_addr.s_addr,4);
		buf += 4;
		memcpy(buf,&((sockaddr_in*)&addr)->sin_port, 2);
		return 7;
	}
	else if (addr.sa_family==AF_INET6)
	{
		*(buf++) = Addr_orig_bits[orig] | 0x80;
		memcpy(buf,&((sockaddr_in6*)&addr)->sin6_addr.s6_addr,16);
		buf += 16;
		memcpy(buf,&((sockaddr_in6*)&addr)->sin6_port, 2);
		return 19;
	}
	else
		return 0;
}

int32_t URtmfpUtil::addr_decode(const uint8_t* raw, sockaddr_storage& dst, AddressOrig& orig, uint32_t& occupy)
{
	orig = decode_addr_orig(*raw);
	if ( *raw & 0x80 ) {
		sockaddr_in6* p6 = (sockaddr_in6*)&dst;
		p6->sin6_family = AF_INET6;
		memcpy(&p6->sin6_addr.s6_addr, raw+1, 16);
		memcpy(&p6->sin6_port,raw+17,2);
		occupy=19;
		return 19;
	}
	else {
		sockaddr_in* p4 = (sockaddr_in*)&dst;
		p4->sin_family = AF_INET;
		memcpy(&p4->sin_addr.s_addr, raw+1, 4);
		memcpy(&p4->sin_port,raw+5,2);
		occupy=7;
		return 7;
	}
}

const char* URtmfpUtil::chunk_type_str(ChunkType tp)
{
	switch(tp)
	{
	case IHELLO_CHUNK:
		return "ihello";
	case FWD_IHELLO_CHUNK:
		return "fihello";
	case RHELLO_CHUNK:
		return "rhello";
	case REDIRECT_CHUNK:
		return "redirect";
	case IIKEYING_CHUNK:
		return "iikeying";
	case RIKEYING_CHUNK:
		return "rikeying";
	case COOKIE_CHANGE_CHUNK:
		return "cookie_change";
	case PING_CHUNK:
		return "ping";
	case PING_REPLY_CHUNK:
		return "ping_reply";
	case DATA_CHUNK:
		return "data";
	case NEXT_DATA_CHUNK:
		return "ndata";
	case BITMAP_ACK_CHUNK:
		return "bitmap_ack";
	case RANGES_ACK_CHUNK:
		return "range_ack";
	case BUFFER_PROBE_CHUNK:
		return "buffer_probe";
	case FLOW_EXP_CHUNK:
		return "flow_exp";
	case CLOSE_CHUNK:
		return "close";
	case CLOSE_ACK_CHUNK:
		return "close_ack";
	case FRAGMENT_CHUNK:
		return "fragment";
#ifdef RTMFP_SESSION_DEBUG
	case DEBUG_SIMPLE_IHELLO_CHUNK:
		return "debug_ihello";
	case DEBUG_SIMPLE_RHELLO_CHUNK:
		return "debug_rhello";
	case DEBUG_SIMPLE_RHELLO_ACK_CHUNK:
		return "debug_rhello_ack";
#endif
	case PADING0_CHUNK:
	case PADING1_CHUNK:
		return "padding";
	default:
		return "unknonw";
	}
}

const URtmfpOpt URtmfpOpt::Marker(0,NULL,0xffffffff);/* = URtmfpOpt::URtmfpOpt(0,NULL,0xffffffff);*/
const URtmfpOpt URtmfpOpt::Invalid_opt(-1,NULL,0xffffffff);

URtmfpOpt URtmfpOpt::decode(const uint8_t* content, uint32_t& occupy)
{
	uint64_t encode_length = 0;
	int  e_typesz=0,value_length=0;;

	int32_t sz = URtmfpUtil::vln_decode(content,encode_length);
	if (sz==-1)return Invalid_opt;

	if (encode_length==0){
		occupy=1;
		return Marker;
	}

	uint64_t type_v;
	e_typesz = URtmfpUtil::vln_decode(content+sz,type_v);
	if (e_typesz==-1)return Invalid_opt;

	value_length = encode_length - e_typesz;
	occupy = sz + encode_length;

	if (value_length<0)return Invalid_opt;
	if (value_length==0) {
		return URtmfpOpt(type_v,NULL,0);
	}
	else {
		return URtmfpOpt(type_v,content+sz+e_typesz,value_length);
	}
}

bool urtmfp::operator ==(const URtmfpOpt& lh, const URtmfpOpt& rh)
{
	if ( lh.type != rh.type ) return false;
	if ( lh.length != rh.length ) return false;
	if ( lh.length == 0xffffffff ) return true;
	return  0==::memcmp(lh.value,rh.value,lh.length);
}

URtmfpOpt& URtmfpOpt::operator =(const URtmfpOpt& rh)
{
	int32_t* ptype = const_cast<int32_t*>(&type);
	*ptype = rh.type;
	uint32_t* plength = const_cast<uint32_t*>(&length);
	*plength = rh.length;
	if (copyed) {
		delete value;
	}
	copyed = false;
	value = rh.value;
	return *this;
}

uint32_t URtmfpOpt::encode_occupy() const
{
	if ( type == -1 || length==0xffffffff) return 0;
	uint32_t encode_length = length + URtmfpUtil::vln_length(type);
	return URtmfpUtil::vln_length(encode_length) + encode_length;
}

void URtmfpOpt::dup()
{
	if (copyed) return;
	copyed = true;
	if (length > 0) {
		uint8_t* tmp = new uint8_t[length];
		::memcpy(tmp,value,length);
		value = tmp;
	}
}

uint32_t URtmfpOpt::encode(uint8_t* content) const
{
	uint32_t sz = 0;
	if ( type == -1 || length==0xffffffff) return 0;
	uint32_t encode_length = length + URtmfpUtil::vln_length(type);
	sz += URtmfpUtil::vln_encode(encode_length,content);
	//if (type == 0) return sz;
	sz += URtmfpUtil::vln_encode(type,content+sz);
	if (length > 0) {
		::memcpy(content+sz, value, length);
	}
	return sz + length;
}

URtmfpOptList URtmfpOptList::decode(const uint8_t* content,
    uint32_t& occupy)
{
	URtmfpOptList ret;
	URtmfpOpt opt;
	uint32_t used=0, opt_occ;
	occupy = 0;
	do {
		opt = URtmfpOpt::decode(content+used,opt_occ);
		used += opt_occ;
		if (opt == URtmfpOpt::Invalid_opt) {
			ret.list.clear();
			return Invalid_list;
		}
		else if (opt == URtmfpOpt::Marker) {
			occupy = used;
			break;
		}
		ret.list.push_back(opt);
	} while(true);
	return ret;
}

const URtmfpOptList URtmfpOptList::Invalid_list = URtmfpOptList::URtmfpOptList(true);
const URtmfpOptList URtmfpOptList::Empty_list = URtmfpOptList::URtmfpOptList(false);

URtmfpOptList::URtmfpOptList():
	invalid(false)
{
}

URtmfpOptList::~URtmfpOptList()
{
}

uint32_t URtmfpOptList::encode_occupy() const
{
	uint32_t sz = 0;
	if (list.size()==0){
		return 1; //marker
	}

	vector<URtmfpOpt>::const_iterator it;
	for (it = list.begin(); it != list.end(); it++) {
		sz += it->encode_occupy();
	}
	sz += 1; //marker;
	return sz;
}

uint32_t URtmfpOptList::encode(uint8_t* content, uint32_t sz) const
{
	if (sz > 0 && encode_occupy() > sz) return 0;
	vector<URtmfpOpt>::const_iterator it;
	sz = 0;
	for (it = list.begin(); it != list.end(); it++) {
		sz += it->encode(content+sz);
	}
	*(content+sz) = 0; //marker
	return sz+1;
}

int urtmfp::log_id = UNaviLogInfra::declare_logger("rtmfp_proto",
	DEFAULT_UNAVI_LOG_ROOT,
#ifdef RTMFP_DEBUG
	LOG_DEBUG,
#else
	LOG_NOTICE,
#endif
	30720,
	4096,
	unavi::TEE_RED);

