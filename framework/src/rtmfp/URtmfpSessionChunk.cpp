/*
 * URtmfpSessionChunk.cpp
 *
 *  Created on: 2014-11-12
 *      Author: li.lei
 */

#include "unavi/rtmfp/URtmfpSessionChunk.h"

using namespace unavi;
using namespace urtmfp;
using namespace std;

uint32_t UPingChunk::encode_occupy()const
{
	int32_t t = ping_id;
	if ( ping_id < 0 )
		t = -ping_id;

	return 4 + URtmfpUtil::vln_length(t) + ping_size;
}

uint32_t UPingChunk::encode(uint8_t* buf) const
{
	buf[0] = PING_CHUNK;
	buf[3] = ping_id > 0 ? '+' : '-';
	int sz = URtmfpUtil::vln_encode(ping_id>0?ping_id:(-ping_id),buf+4);
	::memcpy(buf+4+sz, ping_raw, ping_size);
	sz += ping_size + 1;
	UNaviUtil::hton_u16(sz, buf+1);

	const uint8_t** p_raw = const_cast<const uint8_t**>(&value);
	uint32_t* p_len = const_cast<uint32_t*>(&length);

	*p_raw = buf + 3;
	*p_len = sz + 3;
	return length;
}

void UPingChunk::decode()throw(URtmfpException)
{
	if ( length == 0) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}
	ping_raw = value;
	ping_size = length;
}

uint32_t UPingReplyChunk::encode_occupy()const
{
	return ping_size + 4;
}

uint32_t UPingReplyChunk::encode(uint8_t* buf)const
{
	buf[0] = PING_REPLY_CHUNK;
	UNaviUtil::hton_u16(ping_size, buf+1);
	::memcpy(buf+3, ping_raw, ping_size);

	const uint8_t** p_raw = const_cast<const uint8_t**>(&value);
	uint32_t* p_len = const_cast<uint32_t*>(&length);

	*p_raw = buf + 3;
	*p_len = 3 + ping_size;
	return length;
}

void UPingReplyChunk::decode()throw(URtmfpException)
{
	if ( length <= 1) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}

	uint64_t v;
	if ( value[0] != '-' && value[0] != '+' ) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}

	int sz = URtmfpUtil::vln_decode(value+1,v);
	if ( sz == -1 ) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}

	if (value[0] == '-')
		ping_id = -v;
	else
		ping_id = v;

	if ( sz + 1 == length ) {
		ping_raw = NULL;
		ping_size = 0;
		return;
	}

	ping_raw = value + 1 + sz;
	ping_size = length - 1 - sz;
}

uint32_t UCloseChunk::encode_occupy() const
{
	return 3;
}

uint32_t UCloseChunk::encode(uint8_t* buf) const
{
	buf[0] = CLOSE_CHUNK;
	buf[1] = 0;
	buf[2] = 0;
	return 3;
}

void UCloseChunk::decode()throw(URtmfpException)
{
	if ( length != 0) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}
}

uint32_t UCloseAckChunk::encode_occupy() const
{
	return 3;
}

uint32_t UCloseAckChunk::encode(uint8_t* buf) const
{
	buf[0] = CLOSE_ACK_CHUNK;
	buf[1] = 0;
	buf[2] = 0;
	return 3;
}

void UCloseAckChunk::decode()throw(URtmfpException)
{
	if ( length != 0) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}
}

