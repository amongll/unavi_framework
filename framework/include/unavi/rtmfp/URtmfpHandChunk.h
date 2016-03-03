/*
 * URtmfpHandChunk.h
 *
 *  Created on: 2014-10-21
 *      Author: dell
 */

#ifndef URTMFPHANDCHUNK_H_
#define URTMFPHANDCHUNK_H_
#include "unavi/rtmfp/URtmfpChunk.h"
#include "unavi/rtmfp/URtmfpHandshake.h"

URTMFP_NMSP_BEGIN

class URedirectChunk : public URtmfpChunk
{
public:
	URedirectChunk(const uint8_t* in=NULL, uint32_t sz=0):
		URtmfpChunk(REDIRECT_CHUNK,in,sz)
	{}
	virtual ~URedirectChunk(){}
	virtual uint32_t encode(uint8_t* buf)const {return 0;}
	virtual uint32_t encode_occupy()const {return 0;}
	virtual void decode()throw(URtmfpException){}
};

class UIHelloChunk : public URtmfpChunk
{
public:
	UIHelloChunk(const uint8_t* in, uint32_t sz):
		URtmfpChunk(REDIRECT_CHUNK,in,sz)
	{}
	virtual ~UIHelloChunk(){}

	virtual uint32_t encode(uint8_t* buf)const {return 0;}
	virtual uint32_t encode_occupy()const {return 0;}
	virtual void decode()throw(URtmfpException){}
};

class URHelloChunk : public URtmfpChunk
{
public:
	URHelloChunk(const uint8_t* in=NULL, uint32_t sz=0):
		URtmfpChunk(REDIRECT_CHUNK,in,sz)
	{}
	~URHelloChunk(){}
	virtual uint32_t encode(uint8_t* buf)const {return 0;}
	virtual uint32_t encode_occupy()const {return 0;}
	virtual void decode()throw(URtmfpException){}
};

class UIIKeyingChunk : public URtmfpChunk
{
public:
	UIIKeyingChunk(const uint8_t* in=NULL, uint32_t sz=0):
		URtmfpChunk(REDIRECT_CHUNK,in,sz)
	{}
	~UIIKeyingChunk(){}
	virtual uint32_t encode(uint8_t* buf)const {return 0;}
	virtual uint32_t encode_occupy()const {return 0;}
	virtual void decode()throw(URtmfpException){}
};

class URIKeyingChunk : public URtmfpChunk
{
public:
	URIKeyingChunk(const uint8_t* in=NULL, uint32_t sz=0):
		URtmfpChunk(REDIRECT_CHUNK,in,sz)
	{}
	~URIKeyingChunk(){}
	virtual uint32_t encode(uint8_t* buf)const {return 0;}
	virtual uint32_t encode_occupy()const {return 0;}
	virtual void decode()throw(URtmfpException){}
};

class UCookieChangeChunk : public URtmfpChunk
{
public:
	UCookieChangeChunk(const uint8_t* in=NULL, uint32_t sz=0):
		URtmfpChunk(REDIRECT_CHUNK,in,sz)
	{}
	~UCookieChangeChunk(){}
	virtual uint32_t encode(uint8_t* buf)const {return 0;}
	virtual uint32_t encode_occupy()const {return 0;}
	virtual void decode()throw(URtmfpException){}
};

URTMFP_NMSP_END

#endif /* URTMFPHANDCHUNK_H_ */
