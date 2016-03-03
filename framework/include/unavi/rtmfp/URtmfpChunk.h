/*
 * UNaviRtmfpChunk.h
 *
 *  Created on: 2014-10-16
 *      Author: li.lei
 */

#ifndef URTMFPCHUNK_H_
#define URTMFPCHUNK_H_

#include "unavi/rtmfp/URtmfpCommon.h"
#include "unavi/rtmfp/URtmfpException.h"
#include "unavi/util/UNaviListable.h"
#ifdef RTMFP_DEBUG
#include <iostream>
#include <cassert>
#endif

URTMFP_NMSP_BEGIN

class URtmfpPacket;
class URtmfpProto;

enum ChunkDirect
{
	ENCODE_CHUNK,
	DECODE_CHUNK
};

struct URtmfpChunk : public UNaviListable
{
	const ChunkType type;
	const uint8_t* const value; //输入方向的chunk，指向udppacket内存的chunk内容
	const uint32_t length; //输入输出方向上，都不包括chunk的头3字节（type+length)
	const ChunkDirect direct;

	uint32_t occupy()
	{
		if (type == PADING0_CHUNK || type == PADING1_CHUNK)
			return 1;
		return length + 3;
	}

	URtmfpChunk():
		type(INAVLID_CHUNK),
		value(NULL),
		length(0),
		direct(DECODE_CHUNK)
	{}

	URtmfpChunk(const URtmfpChunk& r):
		type(r.type),
		value(r.value),
		length(r.length),
		direct(r.direct)
	{
	}

	URtmfpChunk& operator = (const URtmfpChunk& r)
	{
		ChunkType* ptype = const_cast<ChunkType*>(&type);
		const uint8_t** pvalue = const_cast<const uint8_t**>(&value);
		uint32_t *plen = const_cast<uint32_t*>(&length);
		ChunkDirect* pdir = const_cast<ChunkDirect*>(&direct);

		*ptype = r.type;
		*pvalue = r.value;
		*plen = r.length;
		*pdir = r.direct;
		return *this;
	}

	bool is_padding()
	{
		return (type==PADING0_CHUNK) || (type==PADING1_CHUNK);
	}

	virtual uint32_t encode(uint8_t* buf)const {return 0;}
	virtual uint32_t encode_occupy()const {return 0;}
	virtual void decode()throw(URtmfpException);

#ifdef RTMFP_DEBUG
	virtual void print(std::ostream& os)const {}
#endif
protected:
	friend class URtmfpProto;
	friend class URtmfpDebugProto;
	friend class URtmfpPacket;
	URtmfpChunk(const uint8_t* raw, uint32_t pkt_left); //用于直接构造URtmfpChunk对象
	URtmfpChunk(ChunkType type, const uint8_t* content, uint16_t sz);
	URtmfpChunk(ChunkType type);
	virtual ~URtmfpChunk();
};

URTMFP_NMSP_END

#endif /* UNAVIRTMFPCHUNK_H_ */
