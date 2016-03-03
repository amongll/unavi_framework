/*
 * UNaviPacket.h
 *
 *  Created on: 2014-9-22
 *      Author: li.lei
 */

#ifndef UNAVIUDPPACKET_H_
#define UNAVIUDPPACKET_H_

#include "unavi/UNaviCommon.h"
#include "unavi/util/UNaviUtil.h"
#include "unavi/core/UNaviProtoDeclare.h"
#include "unavi/core/UNaviProto.h"
#include "unavi/util/UNaviListable.h"
#include <exception>
#include <ostream>

UNAVI_NMSP_BEGIN

class UNaviPacketExp : public std::exception
{
public:
	enum PacketOpErr {
		READ_NOT_COMPLETE,
		ADDR_NOT_ALLOWED,
		SEEK_NOT_ALLOWED,
		OPERR_DUMMY
	};

	UNaviPacketExp(PacketOpErr e):
		type(e)
	{}

	virtual ~UNaviPacketExp()throw() {};
	virtual const char* what()const throw() {
		return err_str[type];
	}
private:
	static const char* err_str[OPERR_DUMMY+1];
	PacketOpErr type;
};

class UNaviUDPPipe;
class UNaviIOSvc;
class UNaviUDPPacket : public UNaviListable
{
public:
	friend class UNaviUDPPipe;

	enum SeekType {
		SEEK_ABS,
		SEEK_REL
	};

	typedef UNaviProto::DispatchInfo DispatchInfo;
	typedef UNaviProto::DispatchType DispatchType;

	DispatchInfo dispatch(uint8_t proto_typeid) {
		static DispatchInfo invalid_ret = {UNaviProto::DISP_DROP, 0};
		UNaviProto* proto =  UNaviProtoTypes::proto(proto_typeid);
		if (!proto) {
			return invalid_ret;
		}
		return proto->dispatch(*this);
	}

	void reset() {
		pos = used = 0;
		//memset(&in6_peer_addr,0x00,sizeof(sockaddr_in6));
	}

	void reset(uint16_t mtu){
		pos = used = 0;
		if (mtu < this->mtu) {
			delete []buf;
			buf = new uint8_t[mtu];
			uint16_t* pmtu = const_cast<uint16_t*>(&this->mtu);
			*pmtu = mtu;
		}
	}

	void set_peer_addr(const sockaddr& addr) {
		switch(addr.sa_family) {
		case AF_INET:
			memcpy(&in_peer_addr,&addr,sizeof(sockaddr_in));
			break;
		case AF_INET6:
			memcpy(&in6_peer_addr,&addr,sizeof(sockaddr_in6));
			break;
		default:
			throw UNaviPacketExp(UNaviPacketExp::ADDR_NOT_ALLOWED);
		}
	}

	const sockaddr& peer_addr() {
		return sa_addr;
	}

	uint16_t size() {
		return used;
	}

	uint16_t cur_pos() {
		return pos;
	}

	uint8_t* cur()
	{
		return buf + pos;
	}

	const uint8_t* cur()const
	{
		return buf + pos;
	}

	void seek(int32_t offset, SeekType st = SEEK_ABS) {
		int32_t abs = offset + (int32_t)(st==SEEK_ABS?0:(int32_t)pos);
		if ( abs > 0 && abs < mtu) {
			pos = abs;
			if (pos > used) used = pos;
		}
		else {
			throw UNaviPacketExp(UNaviPacketExp::SEEK_NOT_ALLOWED);
		}
	}

	bool fillin_u64(uint64_t v) {
		if ( sizeof(v) > mtu - pos )
			return false;
		UNaviUtil::hton_u64(v,buf+pos);
		pos += sizeof(v);
		if (pos > used) used = pos;
		return true;
	}

	bool fillin_u32(uint32_t v) {
		if ( sizeof(v) > mtu - pos )
			return false;
		UNaviUtil::hton_u32(v,buf+pos);
		pos += sizeof(v);
		if (pos > used) used = pos;
		return true;
	}

	bool fillin_u16(uint16_t v) {
		if ( sizeof(v) > mtu - pos )
			return false;
		UNaviUtil::hton_u16(v,buf+pos);
		pos += sizeof(v);
		if (pos > used) used = pos;
		return true;
	}

	bool fillin_u8(uint8_t v) {
		if ( mtu == pos )
			return false;
		*(buf + pos++) = v;
		if (pos > used) used = pos;
		return true;
	}

	bool fillin_i64(int64_t v) {
		if ( sizeof(v) > mtu - pos )
			return false;
		UNaviUtil::hton_u64(v,buf+pos);
		pos += sizeof(v);
		if (pos > used) used = pos;
		return true;
	}

	bool fillin_i32(int32_t v) {
		if ( sizeof(v) > mtu - pos )
			return false;
		UNaviUtil::hton_u32(v,buf+pos);
		pos += sizeof(v);
		if (pos > used) used = pos;
		return true;
	}

	bool fillin_i16(int16_t v) {
		if ( sizeof(v) > mtu - pos )
			return false;
		UNaviUtil::hton_u16(v,buf+pos);
		pos += sizeof(v);
		if (pos > used) used = pos;
		return true;
	}

	bool fillin_i8(int8_t v) {
		if ( mtu == pos )
			return false;
		*(buf + pos++) = v;
		if (pos > used) used = pos;
		return true;
	}

	bool fillin(const uint8_t* raw_piece, uint32_t sz) {
		if ( sz > mtu - pos )
			return false;
		for (int i=0; i<sz; i++) {
			*(buf + pos++) = *(raw_piece+i);
		}
		if (pos > used) used = pos;
		return true;
	}

	uint64_t read_u64() {
		if ( pos > used || used - pos < sizeof(uint64_t) )
			throw UNaviPacketExp(UNaviPacketExp::READ_NOT_COMPLETE);
		uint16_t tp = pos;
		pos += sizeof(uint64_t);
		return UNaviUtil::ntoh_u64(buf+tp);
	}

	uint32_t read_u32() {
		if ( pos > used || used - pos < sizeof(uint32_t) )
			throw UNaviPacketExp(UNaviPacketExp::READ_NOT_COMPLETE);
		uint16_t tp = pos;
		pos += sizeof(uint32_t);
		return UNaviUtil::ntoh_u32(buf+tp);
	}

	uint16_t read_u16() {
		if ( pos > used || used - pos < sizeof(uint16_t) )
			throw UNaviPacketExp(UNaviPacketExp::READ_NOT_COMPLETE);
		uint16_t tp = pos;
		pos += sizeof(uint16_t);
		return UNaviUtil::ntoh_u16(buf+tp);
	}

	uint8_t read_u8() {
		if ( pos >= used )
			throw UNaviPacketExp(UNaviPacketExp::READ_NOT_COMPLETE);
		return *( buf + pos++ );
	}

	uint64_t read_i64() {
		if ( pos > used || used - pos < sizeof(int64_t) )
			throw UNaviPacketExp(UNaviPacketExp::READ_NOT_COMPLETE);
		uint16_t tp = pos;
		pos += sizeof(int64_t);
		return UNaviUtil::ntoh_i64(buf+tp);
	}

	int32_t read_i32() {
		if ( pos > used ||used - pos < sizeof(int32_t) )
			throw UNaviPacketExp(UNaviPacketExp::READ_NOT_COMPLETE);
		uint16_t tp = pos;
		pos += sizeof(int32_t);
		return UNaviUtil::ntoh_i32(buf+tp);
	}

	int16_t read_i16() {
		if ( pos > used || used - pos < sizeof(int16_t) )
			throw UNaviPacketExp(UNaviPacketExp::READ_NOT_COMPLETE);
		uint16_t tp = pos;
		pos += sizeof(int16_t);
		return UNaviUtil::ntoh_i16(buf+tp);
	}

	int8_t read_i8() {
		if ( used <= pos )
			throw UNaviPacketExp(UNaviPacketExp::READ_NOT_COMPLETE);
		return (int8_t)*( buf + pos++ );
	}

	const uint8_t* read(uint32_t sz) {
		if ( pos > used || used - pos < sz )
			throw UNaviPacketExp(UNaviPacketExp::READ_NOT_COMPLETE);
		int16_t tp = pos;
		pos += sz;
		return (const uint8_t*)(buf + tp);
	}

	const uint16_t mtu;
	uint8_t* buf;
	uint16_t used;
	uint16_t pos;

	union  {
		sockaddr sa_addr;
		sockaddr_in in_peer_addr;
		sockaddr_in6 in6_peer_addr;
	};

	//sz小于等于svc设置的mtu时，给出mtu大小的packet，否则给出sz大小的packet
	static UNaviUDPPacket* new_outpacket(uint32_t pipe_id,size_t sz=0);
	static UNaviUDPPacket* new_outpacket(UNaviIOSvc* svc,size_t sz=0); //较pipe_id版本，可能使用线程专有存储效率稍差
	static UNaviUDPPacket* new_proto_outpacket(uint8_t proto_id,size_t sz=0);
	void output(const sockaddr& peer);

	void release_in_packet();
	void cancel_out_packet();

	uint32_t get_pipe_id()const {
		return pipe_id;
	}

#ifdef DEBUG
	void dump(std::ostream& os);
#endif

private:

	friend class UNaviIOSvc;
	UNaviUDPPacket(uint16_t arg_mtu,uint32_t _pid=0xffffffff):
		mtu(arg_mtu),
		used(0),
		pos(0),
		buf(new uint8_t[mtu]),
		pipe_id(_pid)
	{}
	uint32_t pipe_id;
	~UNaviUDPPacket() {
		delete []buf;
	}

	void set_packet_len(size_t sz) {
		if ( sz <= mtu ) return;
		uint16_t* pmtu = const_cast<uint16_t*>(&mtu);
		*pmtu = sz;
		uint8_t* new_buf = new uint8_t[sz];
		if ( used > 0) {
			::memcpy(new_buf,buf,used);
		}
		delete []buf;
		buf = new_buf;
	}

};

UNAVI_NMSP_END

#endif /* UNAVIPACKET_H_ */
