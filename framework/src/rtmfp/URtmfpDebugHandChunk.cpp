/*
 * URtmfpDebugHandChunk.cpp
 *
 *  Created on: 2014-12-3
 *      Author: dell
 */

#include "unavi/rtmfp/URtmfpDebugHandChunk.h"
#include "unavi/rtmfp/URtmfpUtil.h"

using namespace urtmfp;
using namespace unavi;
using namespace std;

void UDebugIHelloChunk::print(std::ostream& os)const
{
	os << "init_ssid:" << init_ssid ;
}

void UDebugRHelloChunk::print(std::ostream& os)const
{

	os << "init_ssid:" << init_ssid  << " resp_ssid:" << resp_ssid ;
}

void UDebugRHelloAckChunk::print(std::ostream& os)const
{
	os << "init_ssid:" << init_ssid  << " resp_ssid:" << resp_ssid ;
}

uint32_t UDebugIHelloChunk::encode(uint8_t* buf) const
{
	uint32_t sz = URtmfpUtil::vln_encode(init_ssid, buf+3);
	UNaviUtil::hton_u16(sz,buf+1);
	buf[0] = DEBUG_SIMPLE_IHELLO_CHUNK;
	return sz + 3;
}

uint32_t UDebugIHelloChunk::encode_occupy() const
{
	return 3 + URtmfpUtil::vln_length(init_ssid);
}

void UDebugIHelloChunk::decode()throw(URtmfpException)
{
	uint64_t v;
	URtmfpUtil::vln_decode(value,v);
	init_ssid = v;
}

uint32_t UDebugRHelloChunk::encode(uint8_t* buf) const
{
	uint32_t len = URtmfpUtil::vln_encode(init_ssid, buf + 3);
	len += URtmfpUtil::vln_encode(resp_ssid,buf+len+3);
	UNaviUtil::hton_u16(len,buf+1);
	buf[0] = DEBUG_SIMPLE_RHELLO_CHUNK;
	return len + 3;
}

uint32_t UDebugRHelloChunk::encode_occupy() const
{
	return 3 + URtmfpUtil::vln_length(init_ssid) +
		URtmfpUtil::vln_length(resp_ssid);
}

void UDebugRHelloChunk::decode()throw(URtmfpException)
{
	uint64_t v;
	uint32_t off;
	off = URtmfpUtil::vln_decode(value,v);
	init_ssid = v;
	URtmfpUtil::vln_decode(value+off,v);
	resp_ssid = v;
}

uint32_t UDebugRHelloAckChunk::encode(uint8_t* buf) const
{
	uint32_t len = URtmfpUtil::vln_encode(init_ssid, buf+3);
	len += URtmfpUtil::vln_encode(resp_ssid,buf+len+3);
	UNaviUtil::hton_u16(len,buf+1);
	buf[0] = DEBUG_SIMPLE_RHELLO_ACK_CHUNK;
	return len + 3;
}

uint32_t UDebugRHelloAckChunk::encode_occupy() const
{
	return 3 + URtmfpUtil::vln_length(init_ssid) +
		URtmfpUtil::vln_length(resp_ssid);
}

void UDebugRHelloAckChunk::decode()throw(URtmfpException)
{
	uint64_t v;
	uint32_t off;
	off = URtmfpUtil::vln_decode(value,v);
	init_ssid = v;
	URtmfpUtil::vln_decode(value+off,v);
	resp_ssid = v;
}

