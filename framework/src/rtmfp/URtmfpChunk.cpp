/*
 * URtmfpChunk.cpp
 *
 *  Created on: 2014-10-20
 *      Author: li.lei
 */

#include "unavi/rtmfp/URtmfpChunk.h"
#include "unavi/rtmfp/URtmfpUtil.h"
#include "unavi/util/UNaviUtil.h"
#include "unavi/core/UNaviLog.h"

using namespace urtmfp;
using namespace std;

static std::ostream& operator<<(std::ostream& os, const URtmfpChunk& chunk)
{
	chunk.print(os);
	return os;
}

URtmfpChunk::URtmfpChunk(ChunkType tp, const uint8_t* content, uint16_t sz):
	type(tp),
	value(content),
	length(sz),
	direct(DECODE_CHUNK)
{

}

void URtmfpChunk::decode()throw(URtmfpException)
{
	decode();
#ifdef RTMFP_DEBUG
	ostringstream oss;
	oss<<"[ChunkType:"<<URtmfpUtil::chunk_type_str(type)
		<<" len:"<<length << " " << *this<<" ]";
	udebug_log(urtmfp::log_id, oss.str().c_str());
#endif
	return;
}

URtmfpChunk::~URtmfpChunk()
{
}

URtmfpChunk::URtmfpChunk(ChunkType tp) :
	type(tp),
	value(NULL),
	length(0),
	direct(ENCODE_CHUNK)
{}

URtmfpChunk::URtmfpChunk(const uint8_t* raw, uint32_t pkt_left):
	type(INAVLID_CHUNK),
	value(NULL),
	length(0),
	direct(DECODE_CHUNK)
{
	ChunkType* ptype = const_cast<ChunkType*>(&type);
	uint32_t* plength = const_cast<uint32_t*>(&length);
	const uint8_t** pvalue = const_cast<const uint8_t**>(&value);

	if (pkt_left == 0) {
#ifdef RTMFP_DEBUG
#endif
		URtmfpException e(URTMFP_IMPL_ERROR);
		uexception_throw_v(e,LOG_FATAL,urtmfp::log_id);
	}

	switch (*raw) {
	case IHELLO_CHUNK:
	case FWD_IHELLO_CHUNK:
	case RHELLO_CHUNK:
	case REDIRECT_CHUNK:
	case IIKEYING_CHUNK:
	case RIKEYING_CHUNK:
	case COOKIE_CHANGE_CHUNK:
	case PING_CHUNK:
	case PING_REPLY_CHUNK:
	case DATA_CHUNK:
	case NEXT_DATA_CHUNK:
	case BITMAP_ACK_CHUNK:
	case RANGES_ACK_CHUNK:
	case BUFFER_PROBE_CHUNK:
	case FLOW_EXP_CHUNK:
	case CLOSE_ACK_CHUNK:
	case CLOSE_CHUNK:
	case PADING0_CHUNK:
	case PADING1_CHUNK:
	case FRAGMENT_CHUNK:
#ifdef RTMFP_SESSION_DEBUG
	case DEBUG_SIMPLE_IHELLO_CHUNK:
	case DEBUG_SIMPLE_RHELLO_CHUNK:
	case DEBUG_SIMPLE_RHELLO_ACK_CHUNK:
#endif
		break;
	default: {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
		break;
	}
	}
	*ptype = (ChunkType)*raw;
	if (type == PADING0_CHUNK || type == PADING1_CHUNK) {
		*plength = 0;
		*pvalue = NULL;
		return;
	}
	*plength = UNaviUtil::ntoh_u16(raw+1);
	if ( length == 0) {
		*pvalue = NULL;
		return;
	}
	if ( length > pkt_left - 3) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}
	*pvalue = raw + 3;
}

