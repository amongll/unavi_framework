/*
 * URtmfpMsg.cpp
 *
 *  Created on: 2014-10-29
 *      Author: li.lei
 */

#include "unavi/rtmfp/URtmfpMsg.h"

using namespace unavi;
using namespace urtmfp;
using namespace std;

URtmfpMsg::URtmfpMsg(uint32_t flowid, const uint8_t* content, int32_t size):
	flow_id(flowid),
	raw(content),
	length(size),
	buf_sz(0)
{
}

URtmfpMsg::~URtmfpMsg()
{
	uint8_t** praw = const_cast<uint8_t**>(&raw);
	if (*praw && buf_sz > 0)
		::free(*praw);
}

void URtmfpMsg::set(uint32_t flowid, const uint8_t* buf, int32_t len)
{
	uint32_t* pflowid = const_cast<uint32_t*>(&flow_id);
	uint8_t** praw = const_cast<uint8_t**>(&raw);
	int32_t* plen = const_cast<int32_t*>(&length);

	if ( buf_sz == 0) {
		*praw = (uint8_t*)::malloc(len+1);
		buf_sz = len+1;
	}
	else if ( buf_sz - 1 < len) {
		if (!*praw) {
			*praw = (uint8_t*)::malloc(len+1);
		}
		else {
			*praw = (uint8_t*)::realloc(*praw,len+1);
		}
		buf_sz = len+1;
	}
	::memcpy( *praw, buf, len);
	*praw[len] = 0;
	*plen = len;
}

void URtmfpMsg::set(uint32_t flowid, uint8_t* buf, int32_t len, uint32_t bufsz)
{
	uint32_t* pflowid = const_cast<uint32_t*>(&flow_id);
	uint8_t** praw = const_cast<uint8_t**>(&raw);
	int32_t* plen = const_cast<int32_t*>(&length);
	if ( buf_sz > 0)
		if ( *praw ) ::free(*praw);

	*pflowid = flowid;
	*praw = buf;
	*plen = len;
	buf_sz = bufsz;
}


void URtmfpMsg::push_content(const uint8_t* buf, uint32_t len)
{
	int32_t* plen = const_cast<int32_t*>(&length);
	uint8_t** praw = const_cast<uint8_t**>(&raw);

	if ( buf_sz > 0 &&  length + len <= buf_sz - 1 ) {
		::memcpy(*praw + length, buf, len);
		*plen += len;
		*praw[*plen] = 0;
	}
}

void URtmfpMsg::reset()
{
	uint32_t* pflowid = const_cast<uint32_t*>(&flow_id);
	uint8_t** praw = const_cast<uint8_t**>(&raw);
	int32_t* plen = const_cast<int32_t*>(&length);
	*pflowid = 0xffffffff;
	*plen = 0;

	if ( buf_sz == 0) {
		*praw = NULL;
	}
	else if (buf_sz > 1536) {
		::free(*praw);
		*praw = NULL;
		buf_sz = 0;
	}
}
