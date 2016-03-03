/*
 * URtmfpDebugHandChunk.h
 *
 *  Created on: 2014-12-3
 *      Author: dell
 */

#ifndef URTMFPDEBUGHANDCHUNK_H_
#define URTMFPDEBUGHANDCHUNK_H_
#include "unavi/rtmfp/URtmfpChunk.h"
#include "unavi/rtmfp/URtmfpDebugHand.h"

URTMFP_NMSP_BEGIN

class UDebugIHelloChunk : public URtmfpChunk
{
public:
	UDebugIHelloChunk(const uint8_t* in=NULL, uint32_t sz=0):
		URtmfpChunk(DEBUG_SIMPLE_IHELLO_CHUNK,in,sz)
	{}
	UDebugIHelloChunk(uint32_t iid):
		URtmfpChunk(DEBUG_SIMPLE_IHELLO_CHUNK),
		init_ssid(iid)
	{}

	virtual void print(std::ostream& os)const ;

	virtual ~UDebugIHelloChunk(){}
	virtual uint32_t encode(uint8_t* buf)const ;
	virtual uint32_t encode_occupy()const;
	virtual void decode()throw(URtmfpException);
	uint32_t init_ssid;
};

class UDebugRHelloChunk : public URtmfpChunk
{
public:
	UDebugRHelloChunk(const uint8_t* in, uint32_t sz):
		URtmfpChunk(DEBUG_SIMPLE_RHELLO_CHUNK,in,sz)
	{}
	UDebugRHelloChunk(uint32_t iid,uint32_t rid):
		URtmfpChunk(DEBUG_SIMPLE_RHELLO_CHUNK),
		resp_ssid(rid),
		init_ssid(iid)
	{}
	virtual ~UDebugRHelloChunk(){}

	virtual void print(std::ostream& os)const ;

	virtual uint32_t encode(uint8_t* buf)const;
	virtual uint32_t encode_occupy()const ;
	virtual void decode()throw(URtmfpException);
	uint32_t init_ssid;
	uint32_t resp_ssid;
};

class UDebugRHelloAckChunk : public URtmfpChunk
{
public:
	UDebugRHelloAckChunk(const uint8_t* in=NULL, uint32_t sz=0):
		URtmfpChunk(DEBUG_SIMPLE_RHELLO_ACK_CHUNK,in,sz)
	{}
	UDebugRHelloAckChunk(uint32_t iid,uint32_t rid):
		URtmfpChunk(DEBUG_SIMPLE_RHELLO_ACK_CHUNK),
		init_ssid(iid),
		resp_ssid(rid)
	{}
	virtual ~UDebugRHelloAckChunk(){}

	virtual void print(std::ostream& os)const ;

	virtual uint32_t encode(uint8_t* buf)const;
	virtual uint32_t encode_occupy()const ;
	virtual void decode()throw(URtmfpException);
	uint32_t init_ssid;
	uint32_t resp_ssid;
};

URTMFP_NMSP_END

#endif /* URTMFPDEBUGHANDCHUNK_H_ */
