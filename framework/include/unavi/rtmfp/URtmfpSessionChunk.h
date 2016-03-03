/*
 * URtmfpSessionChunk.h
 *
 *  Created on: 2014-10-21
 *      Author: dell
 */

#ifndef URTMFPSESSIONCHUNK_H_
#define URTMFPSESSIONCHUNK_H_
#include "unavi/rtmfp/URtmfpChunk.h"
#include "unavi/rtmfp/URtmfpException.h"
#include "unavi/rtmfp/URtmfpSession.h"

URTMFP_NMSP_BEGIN

class UFIHelloChunk : public URtmfpChunk
{
public:
	UFIHelloChunk(const uint8_t* in=NULL, uint32_t sz=0):
		URtmfpChunk(FWD_IHELLO_CHUNK,in,sz)
	{}
	virtual ~UFIHelloChunk(){}
	virtual uint32_t encode_occupy()const {return 0;}
	virtual uint32_t encode(uint8_t* buf)const{return 0;}
	void decode()throw(URtmfpException){}
};

class UPingChunk : public URtmfpChunk
{
public:
	UPingChunk(int32_t id, const uint8_t* raw, uint32_t sz):
		URtmfpChunk(PING_CHUNK),
		ping_id(id),
		ping_raw(ping_raw),
		ping_size(sz) {}
	UPingChunk(const uint8_t* content, uint32_t sz):
		URtmfpChunk(PING_CHUNK,content,sz)
	{}

	virtual ~UPingChunk(){};
	virtual uint32_t encode_occupy()const;
	virtual uint32_t encode(uint8_t* buf)const;
	virtual void decode()throw(URtmfpException);

#ifdef RTMFP_DEBUG
	virtual void print(std::ostream& os)const
	{
		os<< "ping_size:"<< ping_size << std::endl;
	}
#endif

	const uint8_t* ping_raw;
	uint32_t ping_size;
	int32_t ping_id;
};

class UPingReplyChunk : public URtmfpChunk
{
public:
	UPingReplyChunk(const uint8_t* in, uint32_t sz):
		URtmfpChunk(PING_REPLY_CHUNK, in, sz)
	{}

	UPingReplyChunk(UPingChunk* ping):
		URtmfpChunk(PING_REPLY_CHUNK),
		ping_raw(ping->ping_raw),
		ping_size(ping->ping_size)
	{}

	virtual ~UPingReplyChunk(){};
	virtual uint32_t encode_occupy()const;
	virtual uint32_t encode(uint8_t* buf)const;
	virtual void decode()throw(URtmfpException);

#ifdef RTMFP_DEBUG
	virtual void print(std::ostream& os)const
	{
		os << "ping_id:"<<ping_id << " ping_size:"<<ping_size << std::endl;
	}
#endif

	const uint8_t* ping_raw;
	uint32_t ping_size;

	int32_t ping_id; //本端接收到的ping reply中，有ping_id字段
};

class UCloseChunk : public URtmfpChunk
{
public:
	UCloseChunk(const uint8_t* in, uint32_t sz):
		URtmfpChunk(CLOSE_CHUNK,in,sz)
	{}
	UCloseChunk():
		URtmfpChunk(CLOSE_CHUNK)
	{}

	virtual ~UCloseChunk(){};
	virtual uint32_t encode_occupy()const;
	virtual uint32_t encode(uint8_t* buf)const;
	virtual void decode()throw(URtmfpException);
};

class UCloseAckChunk : public URtmfpChunk
{
public:
	UCloseAckChunk(const uint8_t* in, uint32_t sz):
		URtmfpChunk(CLOSE_ACK_CHUNK,in,sz)
	{}
	UCloseAckChunk():
		URtmfpChunk(CLOSE_CHUNK)
	{}

	virtual ~UCloseAckChunk(){};
	virtual uint32_t encode_occupy()const;
	virtual uint32_t encode(uint8_t* buf)const;
	virtual void decode()throw(URtmfpException);
};

URTMFP_NMSP_END

#endif /* URTMFPSESSIONCHUNK_H_ */
