/*
 * URtmfpFlowChunk.cpp
 *
 *  Created on: 2014-11-5
 *      Author: li.lei
 */

#include "unavi/rtmfp/URtmfpFlowChunk.h"
#include "unavi/rtmfp/URtmfpSession.h"
#include "unavi/rtmfp/URtmfpUtil.h"

using namespace unavi;
using namespace urtmfp;
using namespace std;

UDataChunk::UDataChunk(const uint8_t* in, uint32_t sz)
	:URtmfpChunk(DATA_CHUNK, in, sz),
	 seq_id(0),
	 data(NULL),
	 size(0),
	 frag_flag(0),
	 input_flow(NULL),
	 relating_oflowid(-1),
	 flow_id(0xffffffff)
{
}

UDataChunk::UDataChunk(URtmfpOflow* oflow,uint64_t seqid, const uint8_t* content,
	uint32_t sz, uint8_t flag)
	:URtmfpChunk(DATA_CHUNK),
	seq_id(seqid),
	fsn_off(seqid - oflow->fsn),
	data(content),
	size(sz),
	frag_flag(flag),
	out_flow(oflow),
	relating_oflowid(-1)
{
}

UDataChunk::~UDataChunk()
{
}

uint32_t UDataChunk::will_occupy_for_datachunk(URtmfpOflow* oflow, uint64_t seqid, uint32_t msg_sz,bool has_opt)
{
	uint32_t ret = 4;
	uint64_t fsnOff = seqid - oflow->fsn; //待重发的包的seqid肯定大于fsn
	ret += URtmfpUtil::vln_length(oflow->flow_id);
	ret += URtmfpUtil::vln_length(seqid);
	ret += URtmfpUtil::vln_length(fsnOff);
	if (has_opt) {
		ret += oflow->opts.encode_occupy();
	}
	ret += msg_sz;
	return ret;
}

uint32_t UDataChunk::will_accept_for_datachunk(URtmfpOflow* oflow, uint32_t available,bool has_opt)
{
	uint32_t ret = 4;
	uint64_t seqid = oflow->seq_gen + 1;
	uint64_t fsnOff = seqid - oflow->fsn;
	ret += URtmfpUtil::vln_length(oflow->flow_id);
	ret += URtmfpUtil::vln_length(seqid);
	ret += URtmfpUtil::vln_length(fsnOff);
	if (has_opt) {
		ret += oflow->opts.encode_occupy();
	}

	if ( available > ret )
		return available - ret;
	else
		return 0;
}

uint32_t UDataChunk::encode_occupy() const
{
	uint32_t ret = 4;
	uint64_t fsnOff = seq_id - out_flow->fsn;
	ret += URtmfpUtil::vln_length(out_flow->flow_id);
	ret += URtmfpUtil::vln_length(seq_id);
	ret += URtmfpUtil::vln_length(fsnOff);
	if ( frag_flag & Flow_frag_opt_bit ) {
		ret += out_flow->opts.encode_occupy();
	}
	ret += size;
	return ret;
}

uint32_t UDataChunk::encode(uint8_t* buf) const
{
	int off = 0;
	if (direct != ENCODE_CHUNK) return 0;
	uint8_t* h = buf;
	*(buf++) = DATA_CHUNK;
	//UNaviUtil::hton_u16(size,buf);
	buf+=2;
	*(buf++) = frag_flag;

	off = URtmfpUtil::vln_encode(out_flow->flow_id, buf);
	buf += off;
	off = URtmfpUtil::vln_encode(seq_id, buf);
	buf += off;
	off = URtmfpUtil::vln_encode(fsn_off, buf);
	buf += off;

	if ( (frag_flag&Flow_frag_opt_bit) ) {
		off = out_flow->opts.encode(buf);
		buf += off;
	}
	::memcpy(buf, data, size);

	const uint8_t** p_raw = const_cast<const uint8_t**>(&value);
	uint32_t* p_len = const_cast<uint32_t*>(&length);
	*p_raw = h + 3;
	*p_len = buf - h + size;

	UNaviUtil::hton_u16(length - 3, h+1);
	return length;
}

#ifdef RTMFP_DEBUG
void UDataChunk::print(std::ostream& os)const
{
	if ( direct != DECODE_CHUNK ) return;
	os << " flowid:" << flow_id << " seq:" << seq_id
		<< " fsn:" << seq_id - fsn_off << " data_size:" << size;
	os << " frag_type:" << URtmfpUtil::decode_frag_ctrl(frag_flag);
	os << " fin:" << (frag_flag & Flow_frag_fin_bit);
	os << " abn:" << (frag_flag & Flow_frag_abn_bit);
	os << " opt:" << (frag_flag & Flow_frag_opt_bit);
}
#endif

void UDataChunk::decode()throw(URtmfpException)
{
	if ( length <= 1) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}
	const uint8_t* pd = value;
	const uint8_t* pe = value + length;
	int32_t off;
	frag_flag = *(pd++);

	uint64_t flow_id;
	off = URtmfpUtil::vln_decode(pd,flow_id);
	pd += off;
	if (off == -1 || pd >= pe){
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}
	off = URtmfpUtil::vln_decode(pd,seq_id);
	pd += off;
	if (off == -1 || pd >= pe) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}

	off = URtmfpUtil::vln_decode(pd, fsn_off);
	pd += off;
	if (off == -1 || pd > pe ) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}

	if ( seq_id == 0) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}
	else if (seq_id == 1) {
		FlowFragmentCtrl ctrl = URtmfpUtil::decode_frag_ctrl(frag_flag);
		if ( ctrl == Flow_frag_ctrl_bits[MSG_LAST_FRAG] ||
			ctrl == Flow_frag_ctrl_bits[MSG_MIDDLE_FRAG]) {

			URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
			uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
		}
	}

	input_flow = URtmfpSession::get_curssn_iflow(flow_id);
	this->flow_id = flow_id;

	if ( pd == pe ) {
		if (!(frag_flag&Flow_frag_abn_bit)) {
			URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
			uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
		}
		//seqid abandon
		data = NULL;
		size = 0;
		return;
	}

	if (  frag_flag & Flow_frag_opt_bit ) {
		uint32_t off;
		if (input_flow && input_flow->has_opts()==false) {
			input_flow->opts = URtmfpOptList::decode(pd,off);
			input_flow->opts.dup();

			input_flow->get_relating_opt(relating_oflowid);

			if (input_flow->opts.invalid) {
				URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
				uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
			}
		}
		else {
			URtmfpOptList dummy = URtmfpOptList::decode(pd,off);
			if (dummy.invalid) {
				URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
				uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
			}
		}
		pd += off;

		if ( pd == pe ) {
			if (!(frag_flag&Flow_frag_abn_bit)) {
				URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
				uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
			}
			//seqid abandon
			data = NULL;
			size = 0;
			return;
		}
	}

	data = pd;
	size = length - (pd - value);
}

UNextDataChunk::UNextDataChunk(const uint8_t* in, uint32_t sz)
	:URtmfpChunk(NEXT_DATA_CHUNK, in, sz),
	 data(NULL),
	 size(0),
	 frag_flag(0)
{
}

UNextDataChunk::UNextDataChunk(URtmfpOflow* oflow, uint64_t seqid,
	const uint8_t* content, uint32_t sz, uint8_t flag)
	:URtmfpChunk(NEXT_DATA_CHUNK),
	data(content),
	size(sz),
	frag_flag(flag & ~Flow_frag_opt_bit)
{
}

UNextDataChunk::~UNextDataChunk()
{
}

uint32_t UNextDataChunk::will_occupy_for_ndatachunk(URtmfpOflow* oflow, uint64_t seqid, uint32_t msg_sz)
{
	uint32_t ret = 4;
	/**
	if (oflow->acked == false) {
		ret += oflow->opts.encode_occupy();
	}**/
	ret += msg_sz;
	return ret;
}
//根据rtmfp packet可用空间，计算可以接受的data chunk的负载大小。
//中间flow的opt list可能未经确认，如果是，则需要在packet中放入option list
uint32_t UNextDataChunk::will_accept_for_ndatachunk(URtmfpOflow* oflow, uint32_t available)
{
	uint32_t ret = 4;
	if ( available > ret)
		return available - ret;
	return 0;
}

uint32_t UNextDataChunk::encode_occupy() const
{
	uint32_t ret = 4;
	ret += size;
	return ret;
}

uint32_t UNextDataChunk::encode(uint8_t* buf) const
{
	int off = 0;
	if (direct != ENCODE_CHUNK) return 0;
	uint8_t* h = buf;
	*(buf++) = NEXT_DATA_CHUNK;
	//UNaviUtil::hton_u16(size,buf);
	buf+=2;
	*(buf++) = frag_flag;

	::memcpy(buf, data, size);

	const uint8_t** p_raw = const_cast<const uint8_t**>(&value);
	uint32_t* p_len = const_cast<uint32_t*>(&length);
	*p_raw = h + 3;
	*p_len = buf - h + size;
	UNaviUtil::hton_u16(length-3,buf-3);
	return length;
}

void UNextDataChunk::decode()throw(URtmfpException)
{
	if ( length <= 1) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}
	const uint8_t* pd = value;
	const uint8_t* pe = value + length;
	uint32_t off;
	frag_flag = *(pd++);

	//它的前序data chunk肯定携带有opt list
	if ( frag_flag & Flow_frag_opt_bit ) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}

	if ( pd == pe ) {
		if (!(frag_flag&Flow_frag_abn_bit)) {
			URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
			uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
		}
		//seqid abandon
		data = NULL;
		size = 0;
		return;
	}

	data = pd;
	size = length - (pd - value);
}

#ifdef RTMFP_DEBUG
void UNextDataChunk::print(ostream& os)const
{
	if ( direct != DECODE_CHUNK ) return;
	os << " data_size:" << size;
	os << " frag_type:" << URtmfpUtil::decode_frag_ctrl(frag_flag);
	os << " fin:" << (frag_flag & Flow_frag_fin_bit);
	os << " abn:" << (frag_flag & Flow_frag_abn_bit);
	os << " opt:" << (frag_flag & Flow_frag_opt_bit);
}
#endif

UBitmapAckChunk::UBitmapAckChunk(const uint8_t* in, uint32_t sz):
	URtmfpChunk(BITMAP_ACK_CHUNK, in, sz)
{
}

UBitmapAckChunk::UBitmapAckChunk(const uint8_t* header, uint32_t h_sz,
    const uint8_t* content, uint32_t c_sz):
    URtmfpChunk(BITMAP_ACK_CHUNK),
    part(new EncodePart)
{
	part->head = header;
	part->head_size = h_sz;
	part->content = content;
	part->content_size = c_sz;
}

UBitmapAckChunk::~UBitmapAckChunk()
{
	switch(direct) {
	case ENCODE_CHUNK:
		if (part) delete part;
		break;
	case DECODE_CHUNK:
		if (info) delete info;
		break;
	default:
		break;
	}
}

uint32_t UBitmapAckChunk::encode_occupy() const
{
	if ( direct == ENCODE_CHUNK ) {
		return 3 + part->head_size + part->content_size;
	}
	return 0;
}

uint32_t UBitmapAckChunk::encode(uint8_t* buf) const
{
	if ( direct == DECODE_CHUNK)
		return 0;

	buf[0] = BITMAP_ACK_CHUNK;
	UNaviUtil::hton_u16((uint16_t)(part->head_size + part->content_size), buf + 1);
	::memcpy(buf + 3, part->head, part->head_size);
	::memcpy(buf + 3 + part->head_size, part->content, part->content_size);

	const uint8_t** p_raw = const_cast<const uint8_t**>(&value);
	uint32_t* p_len = const_cast<uint32_t*>(&length);

	*p_raw = buf + 3;
	*p_len = 3 + part->head_size + part->content_size;
	return length;
}

#define CHECK_BIT(byte,bit) ((byte) & (0x1<<(7-(bit))))

void UBitmapAckChunk::decode()throw(URtmfpException)
{
	uint64_t v, succ_b = 0;
	int pos = 0, off = 0, bit_cnt=0;
	info = new DecodeInfo;

	off = URtmfpUtil::vln_decode(value,v);
	pos += off;
	if (off==-1 || pos >= length) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}
	info->flow_id = v;

	off = URtmfpUtil::vln_decode(value+pos,v);
	pos += off;
	if (off==-1 || pos >= length) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}
	info->buf_available = v;

	off = URtmfpUtil::vln_decode(value+pos,info->cum_ack_seq);
	pos += off;
	if (off==-1 || pos > length) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}

	//没有空洞
	if ( pos == length )
		return;

	pair<RangeIter,bool> ret ;
	RangeIter cur_range ;

	const uint8_t* byte = value + pos;
	const uint8_t* senti = value + length;

	v = info->cum_ack_seq + 1;

	while( byte != senti ) {
		v++;

		if ( CHECK_BIT(*byte, bit_cnt) ) {
			if ( succ_b == 0 ) {
				succ_b = v;
				ret = info->ranges.insert(make_pair(succ_b,succ_b));
				cur_range = ret.first;
			}
		}
		else {
			if ( succ_b != 0 ) {
				cur_range->second = v - 1;
				succ_b = 0;
			}
		}

		if ( bit_cnt == 7) {
			byte++;
			bit_cnt = 0;
		}
		else {
			bit_cnt++;
		}
	}

	if ( succ_b == 0 ) //对端ack了末尾开放的空洞
	{
		info->ranges.erase(cur_range);
	}
	else
		cur_range->second = v;
}

#ifdef RTMFP_DEBUG
void UBitmapAckChunk::print(std::ostream& os)const
{
	os << " flow_id:"<< info->flow_id << " buf_available:"<< info->buf_available;
	os << " cum_seq:"<< info->cum_ack_seq << " ranges:";
	map<uint64_t,uint64_t>::const_iterator it;
	for ( it = info->ranges.begin(); it != info->ranges.end(); it++) {
		os << " ["<< it->first << ","<<it->second<<"]";
	}
}
#endif

URangeAckChunk::URangeAckChunk(const uint8_t* in, uint32_t sz):
	URtmfpChunk(RANGES_ACK_CHUNK,in,sz)
{
}

URangeAckChunk::URangeAckChunk(const uint8_t* header, uint32_t h_sz,
    const uint8_t* content, uint32_t c_sz):
    URtmfpChunk(RANGES_ACK_CHUNK),
    part(new EncodePart)
{
	part->head = header;
	part->head_size = h_sz;
	part->content = content;
	part->content_size = c_sz;

}

URangeAckChunk::~URangeAckChunk()
{
	switch(direct) {
	case ENCODE_CHUNK:
		if (part) delete part;
		break;
	case DECODE_CHUNK:
		if (info) delete info;
		break;
	default:
		break;
	}
}

uint32_t URangeAckChunk::encode_occupy() const
{
	if ( direct == ENCODE_CHUNK ) {
		return 3 + part->head_size + part->content_size;
	}
	return 0;
}

uint32_t URangeAckChunk::encode(uint8_t* buf) const
{
	if ( direct == DECODE_CHUNK )
		return 0;
	buf[0] = RANGES_ACK_CHUNK;
	UNaviUtil::hton_u16((uint16_t)(part->head_size + part->content_size), buf + 1);
	::memcpy(buf + 3, part->head, part->head_size);
	::memcpy(buf + 3 + part->head_size, part->content, part->content_size);

	const uint8_t** p_raw = const_cast<const uint8_t**>(&value);
	uint32_t* p_len = const_cast<uint32_t*>(&length);

	*p_raw = buf + 3;
	*p_len = 3 + part->head_size + part->content_size;
	return length;
}

void URangeAckChunk::decode()throw(URtmfpException)
{
	uint64_t v, succ_b = 0;
	int pos = 0, off = 0;
	info = new DecodeInfo;

	off  = URtmfpUtil::vln_decode(value,v);
	pos += off;
	if (off==-1 || pos >= length)
		throw URtmfpException(URTMFP_ICHUNK_DECODE_ERROR);
	info->flow_id = v;

	off  = URtmfpUtil::vln_decode(value + pos,v);
	pos += off;
	if (off==-1 || pos >= length) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}
	info->buf_available = v;

	off  = URtmfpUtil::vln_decode(value + pos,info->cum_ack_seq);
	pos += off;
	if (off==-1 || pos > length) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}

	if (pos == length)
		return;

	succ_b = info->cum_ack_seq;

	pair<RangeIter,bool> ret ;
	RangeIter cur_range ;

	while ( pos < length ) {
		off = URtmfpUtil::vln_decode(value + pos,v);
		pos += off;
		if ( off == -1 || pos >= length) {
			URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
			uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
		}

		succ_b += v + 2;
		if ( pos >= length )
			break;

		off = URtmfpUtil::vln_decode(value + pos,v);
		pos += off;
		if ( off == -1 || pos > length) {
			URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
			uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
		}

		info->ranges.insert(make_pair(succ_b, succ_b + v));
	}
}

#ifdef RTMFP_DEBUG
void URangeAckChunk::print(std::ostream& os)const
{
	os << " flow_id:"<< info->flow_id << " buf_available:"<< info->buf_available;
	os << " cum_seq:"<< info->cum_ack_seq << " ranges:";
	map<uint64_t,uint64_t>::const_iterator it;
	for ( it = info->ranges.begin(); it != info->ranges.end(); it++) {
		os << " ["<< it->first << ","<<it->second<<"]";
	}
}
#endif

UBufferProbeChunk::~UBufferProbeChunk()
{
}

uint32_t UBufferProbeChunk::encode_occupy() const
{
	return 3 + URtmfpUtil::vln_length(flowid);
}

uint32_t UBufferProbeChunk::encode(uint8_t* buf) const
{
	buf[0] = BUFFER_PROBE_CHUNK;
	int sz = URtmfpUtil::vln_encode(flowid, buf+3);
	UNaviUtil::hton_u16(sz, buf + 1);

	const uint8_t** p_raw = const_cast<const uint8_t**>(&value);
	uint32_t* p_len = const_cast<uint32_t*>(&length);

	*p_raw = buf + 3;
	*p_len = sz+3;
	return length;
}

void UBufferProbeChunk::decode()throw(URtmfpException)
{
	uint64_t v;
	int off = URtmfpUtil::vln_decode(value,v);
	if ( -1 == off || off != length ) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}

	flowid = v;
}

#ifdef RTMFP_DEBUG
void UBufferProbeChunk::print(ostream& os)const
{
	os << "flow_id:"<< flowid;
}
#endif

uint32_t UFlowExcpChunk::encode_occupy() const
{
	return 3 + URtmfpUtil::vln_length(flowid) + URtmfpUtil::vln_length(excp_code);
}

uint32_t UFlowExcpChunk::encode(uint8_t* buf) const
{
	buf[0] = FLOW_EXP_CHUNK;
	int sz = URtmfpUtil::vln_encode(flowid, buf+3);
	sz += URtmfpUtil::vln_encode(excp_code, buf+3+sz);
	UNaviUtil::hton_u16(sz, buf+1);

	const uint8_t** p_raw = const_cast<const uint8_t**>(&value);
	uint32_t* p_len = const_cast<uint32_t*>(&length);

	*p_raw = buf + 3;
	*p_len = sz + 3;
	return length;
}

void UFlowExcpChunk::decode()throw(URtmfpException)
{
	uint64_t v1,v2;
	int sz = URtmfpUtil::vln_decode(value, v1);
	if (sz == -1 || sz >= length) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}

	int off = URtmfpUtil::vln_decode(value+sz, v2);

	if ( -1 == off || off + sz != length) {
		URtmfpException e(URTMFP_ICHUNK_DECODE_ERROR);
		uexception_throw_v(e,LOG_NOTICE,urtmfp::log_id);
	}

	flowid = v1;
	excp_code = v2;
}

#ifdef RTMFP_DEBUG
void UFlowExcpChunk::print(ostream& os)const
{
	os << "flow_id:"<< flowid<< "excpCode:"<< excp_code ;
}
#endif
