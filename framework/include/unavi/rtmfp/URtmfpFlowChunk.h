/*
 * URtmfpFlowChunk.h
 *
 *  Created on: 2014-10-21
 *      Author: dell
 */

#ifndef URTMFPFLOWCHUNK_H_
#define URTMFPFLOWCHUNK_H_
#include "unavi/rtmfp/URtmfpChunk.h"
#include "unavi/rtmfp/URtmfpUtil.h"

URTMFP_NMSP_BEGIN

class URtmfpOflow;
class URtmfpIflow;
class UDataChunk : public URtmfpChunk
{
public:
	UDataChunk(const uint8_t* in, uint32_t sz);
	virtual ~UDataChunk();

	UDataChunk(URtmfpOflow* oflow,uint64_t seqid, const uint8_t* content, uint32_t sz, uint8_t flag);

	static uint32_t will_accept_for_datachunk(URtmfpOflow* oflow, uint32_t availabe,bool has_opt=false);
	//用于计算重发的msg fragment将占用的空间
	static uint32_t will_occupy_for_datachunk(URtmfpOflow* oflow, uint64_t seqid, uint32_t msg_sz,bool has_opt=false);

	virtual uint32_t encode_occupy()const;
	virtual uint32_t encode(uint8_t* buf)const;
	virtual void decode()throw(URtmfpException);

#ifdef RTMFP_DEBUG
	virtual void print(std::ostream& os)const;
#endif

	uint64_t seq_id;
	uint64_t fsn_off; //仅解码时使用
	const uint8_t* data;
	uint32_t size;
	uint8_t frag_flag;

	int64_t relating_oflowid;

	union {
		URtmfpOflow* out_flow;
		URtmfpIflow* input_flow;
	};

	uint32_t flow_id;
};

class UNextDataChunk : public URtmfpChunk
{
public:
	UNextDataChunk(const uint8_t* in, uint32_t sz);
	virtual ~UNextDataChunk();
	UNextDataChunk(URtmfpOflow* oflow, uint64_t seqid, const uint8_t* content, uint32_t sz, uint8_t flag);

	static uint32_t will_accept_for_ndatachunk(URtmfpOflow* oflow, uint32_t availabe);
	//用于计算重发的msg fragment将占用的空间
	static uint32_t will_occupy_for_ndatachunk(URtmfpOflow* oflow, uint64_t seqid, uint32_t msg_sz);

	virtual uint32_t encode_occupy()const;
	virtual uint32_t encode(uint8_t* buf)const;
	virtual void decode()throw(URtmfpException);

#ifdef RTMFP_DEBUG
	virtual void print(std::ostream& os)const;
#endif

	const uint8_t* data;
	uint32_t size;
	uint8_t frag_flag;
};

typedef std::map<uint64_t, uint64_t>::iterator RangeIter;

class UBitmapAckChunk : public URtmfpChunk
{
public:
	UBitmapAckChunk(const uint8_t* in, uint32_t sz);
	UBitmapAckChunk(const uint8_t* header, uint32_t h_sz, const uint8_t* content, uint32_t c_sz);
	virtual ~UBitmapAckChunk();

	virtual uint32_t encode_occupy()const;
	virtual uint32_t encode(uint8_t* buf)const;
	virtual void decode()throw(URtmfpException);
#ifdef RTMFP_DEBUG
	virtual void print(std::ostream& os)const;
#endif

	struct EncodePart {
		const uint8_t* head;
		int head_size;
		const uint8_t* content;
		int content_size;
	};

	struct DecodeInfo {
		uint32_t flow_id;
		uint32_t buf_available;
		uint64_t cum_ack_seq;
		std::map<uint64_t,uint64_t> ranges;
	};

	union {
		EncodePart* part;
		DecodeInfo* info;
	};
};

class URangeAckChunk : public URtmfpChunk
{
public:
	URangeAckChunk(const uint8_t* in, uint32_t sz);
	URangeAckChunk(const uint8_t* header, uint32_t h_sz, const uint8_t* content, uint32_t c_sz);
	virtual ~URangeAckChunk();
	virtual uint32_t encode_occupy()const;
	virtual uint32_t encode(uint8_t* buf)const;
	virtual void decode()throw(URtmfpException);
#ifdef RTMFP_DEBUG
	virtual void print(std::ostream& os)const;
#endif

	struct EncodePart {
		const uint8_t* head;
		int head_size;
		const uint8_t* content;
		int content_size;
	};

	struct DecodeInfo {
		uint32_t flow_id;
		uint32_t buf_available;
		uint64_t cum_ack_seq;
		std::map<uint64_t,uint64_t> ranges;
	};

	union {
		EncodePart* part;
		DecodeInfo* info;
	};
};

class UBufferProbeChunk : public URtmfpChunk
{
public:
	UBufferProbeChunk(const uint8_t* in, uint32_t sz):
		URtmfpChunk(BUFFER_PROBE_CHUNK, in, sz)
	{}

	UBufferProbeChunk(uint32_t fid):
		URtmfpChunk(BUFFER_PROBE_CHUNK),
		flowid(fid)
	{}
	virtual ~UBufferProbeChunk();
	virtual uint32_t encode_occupy()const;
	virtual uint32_t encode(uint8_t* buf)const;
	virtual void decode()throw(URtmfpException);
#ifdef RTMFP_DEBUG
	virtual void print(std::ostream& os)const;
#endif

	uint32_t flowid;
};

class UFlowExcpChunk : public URtmfpChunk
{
public:
	UFlowExcpChunk(const uint8_t* in, uint32_t sz):
		URtmfpChunk(FLOW_EXP_CHUNK, in ,sz)
	{}

	UFlowExcpChunk(uint32_t fid, uint64_t excpCode):
		URtmfpChunk(FLOW_EXP_CHUNK),
		flowid(fid),
		excp_code(excpCode)
	{}
	virtual ~UFlowExcpChunk(){};

	virtual uint32_t encode_occupy()const;
	virtual uint32_t encode(uint8_t* buf)const;
	virtual void decode()throw(URtmfpException);
#ifdef RTMFP_DEBUG
	virtual void print(std::ostream& os)const;
#endif

	uint32_t flowid;
	uint64_t excp_code;
};

URTMFP_NMSP_END

#endif /* URTMFPFLOWCHUNK_H_ */
