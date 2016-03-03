/** \brief 
 * UNaviUtil.h
 *  Created on: 2014-9-3
 *      Author: li.lei
 *  brief: 
 */

#ifndef UNAVIUTIL_H_
#define UNAVIUTIL_H_

#include "unavi/UNaviCommon.h"

UNAVI_NMSP_BEGIN

struct UNaviUtil {
	static uint64_t get_time_ml();
	static uint64_t get_time_mc();
	static bool big_ending;
	static bool check_big_ending();

	static void hton_i16(int16_t v,uint8_t* dst)
	{
		hton_u16(v,dst);
	}

	static void hton_i16(int16_t &v)
	{
		hton_u16(reinterpret_cast<uint16_t&>(v));
	}

	static void hton_i32(int32_t v,uint8_t* dst)
	{
		hton_u32(v,dst);
	}

	static void hton_i32(int32_t &v)
	{
		hton_u32(reinterpret_cast<uint32_t&>(v));
	}

	static void hton_i64(int64_t v,uint8_t* dst)
	{
		hton_u64(v,dst);
	}

	static void hton_i64(int64_t &v)
	{
		hton_u64(reinterpret_cast<uint64_t&>(v));
	}

	static void hton_u16(uint16_t v,uint8_t* dst)
	{
		if ( !big_ending ) {
			*dst++ = (uint8_t)(v>>8);
			*dst = (uint8_t)(v & 0x00FF);
		}
		else {
			memcpy(dst,(void*)&v,sizeof(uint16_t));
		}
	}

	static void hton_u16(uint16_t &v)
	{
		if (big_ending) return;
		uint16_t t = v;
		v = (t>>8) | ((t&0x00FF)<<8);
	}

	static void hton_u32(uint32_t v, uint8_t* dst)
	{
		if ( !big_ending ) {
			*dst++ = (uint8_t)(v>>24);
			*dst++ = (uint8_t)(v>>16 & 0xFF);
			*dst++ = (uint8_t)(v>>8 & 0xFF);
			*dst = (uint8_t)(v & 0xFF);
		}
		else {
			memcpy(dst,(void *)&v, sizeof(uint32_t));
		}
	}

	static void hton_u32(uint32_t &v)
	{
		if (big_ending) return;
		uint32_t t = v;
		v = (t>>24) | ( (t&0xff) << 24) | ((t&0xff00) <<8) | ((t&0xff0000) >> 8);
	}

	static void hton_u64(uint64_t v, uint8_t* dst)
	{
		if ( !big_ending ) {
			*dst++ = (uint8_t)(v>>56);
			*dst++ = (uint8_t)(v>>48 & 0xFF);
			*dst++ = (uint8_t)(v>>40 & 0xFF);
			*dst++ = (uint8_t)(v>>32 & 0xFF);
			*dst++ = (uint8_t)(v>>24 & 0xFF);
			*dst++ = (uint8_t)(v>>16 & 0xFF);
			*dst++ = (uint8_t)(v>>8 & 0xFF);
			*dst = (uint8_t)(v & 0xFF);
		}
		else {
			memcpy(dst,(void *)&v, sizeof(uint64_t));
		}
	}

	static void hton_u64(uint64_t& v)
	{
		if (big_ending) return;
		uint8_t* pa = (uint8_t*)&v + 7;
		uint8_t* pb = (uint8_t*)&v;
		for (int i=0; i<4; i++) {
			uint8_t swap = *pa;
			*(pa++) = *pb;
			*(pb++) = swap;
		}
	}

	static uint16_t ntoh_u16(const uint8_t* src)
	{
		return (uint16_t)( *src<<8 | *(src+1));
	}

	static uint32_t ntoh_u32(const uint8_t* src)
	{
		return (uint32_t) ( *(src)<<24 | (*(src+1)<<16) | (*(src+2)<<8) | *(src+3) );
	}

	static uint64_t ntoh_u64(const uint8_t* src)
	{
		return (uint64_t) ( ((uint64_t)*(src))<<56 | ((uint64_t)*(src+1))<<48|
			((uint64_t)*(src+2))<<40 | ((uint64_t)*(src+3))<<32 |
			((uint64_t)*(src+4))<<24 | ((uint64_t)*(src+5))<<16 |
			((uint64_t)*(src+6))<<8 | ((uint64_t)*(src+7)) );
	}

	static int16_t ntoh_i16(const uint8_t* src)
	{
		return (int16_t)( *src<<8 | *(src+1));
	}

	static int32_t ntoh_i32(const uint8_t* src)
	{
		return (int32_t) ( *(src)<<24 | (*(src+1)<<16) | (*(src+2)<<8) | *(src+3) );
	}

	static int64_t ntoh_i64(const uint8_t* src)
	{
		return (int64_t) ( ((uint64_t)*(src))<<56 | ((uint64_t)*(src+1))<<48|
			((uint64_t)*(src+2))<<40 | ((uint64_t)*(src+3))<<32 |
			((uint64_t)*(src+4))<<24 | ((uint64_t)*(src+5))<<16 |
			((uint64_t)*(src+6))<<8 | ((uint64_t)*(src+7)) );
	}
};

struct AddressKey
{
	AddressKey(const sockaddr* addr)
	{
		::memset(&sa_addr,0x00,sizeof(sockaddr_in6));
		sa_addr.sa_family = addr->sa_family;

		switch(addr->sa_family) {
		case AF_INET: {
			const sockaddr_in* in_arg = (const sockaddr_in*)addr;
			in_addr.sin_port = in_arg->sin_port;
			in_addr.sin_addr.s_addr = in_arg->sin_addr.s_addr;
			break;
		}
		case AF_INET6: {
			const sockaddr_in6* in_arg = (const sockaddr_in6*)addr;
			in6_addr.sin6_port = in_arg->sin6_port;
			in6_addr.sin6_addr.s6_addr32[0] = in_arg->sin6_addr.s6_addr32[0];
			in6_addr.sin6_addr.s6_addr32[1] = in_arg->sin6_addr.s6_addr32[1];
			in6_addr.sin6_addr.s6_addr32[2] = in_arg->sin6_addr.s6_addr32[2];
			in6_addr.sin6_addr.s6_addr32[3] = in_arg->sin6_addr.s6_addr32[3];
			break;
		}
		default:
			break;
		}
	}
	AddressKey()
	{
		memset(&in6_addr,0x00,sizeof(sockaddr_in6));
	}
	AddressKey(const AddressKey& r)
	{
		::memcpy(&in6_addr,&r.in6_addr,sizeof(sockaddr_in6));
	}
	AddressKey& operator=(const AddressKey& r){
		::memcpy(&in6_addr,&r.in6_addr,sizeof(sockaddr_in6));
		return *this;
	}
	union {
		sockaddr sa_addr;
		sockaddr_in in_addr;
		sockaddr_in6 in6_addr;
	};

	bool operator==(const AddressKey& r) const {
		if ( sa_addr.sa_family != r.sa_addr.sa_family )
			return false;
		return 0 == ::memcmp(&sa_addr,&r.sa_addr,
			sa_addr.sa_family==AF_INET?sizeof(sockaddr_in):sizeof(sockaddr_in6));
	}
};
UNAVI_NMSP_END

namespace __gnu_cxx {
template<>
struct hash<unavi::AddressKey>
{
	size_t operator()(const unavi::AddressKey& obj) const
	{
		size_t v = 0;

		if (obj.sa_addr.sa_family == AF_INET) {
			v = AF_INET;
			v = v*5 + obj.in_addr.sin_port;
			v = v*5 + obj.in_addr.sin_addr.s_addr;
		}
		else {
			v = AF_INET6;
			v = v*5 + obj.in6_addr.sin6_port;
			v = v*5 + obj.in6_addr.sin6_addr.s6_addr32[0];
			v = v*5 + obj.in6_addr.sin6_addr.s6_addr32[1];
			v = v*5 + obj.in6_addr.sin6_addr.s6_addr32[2];
			v = v*5 + obj.in6_addr.sin6_addr.s6_addr32[3];
		}
		return v;
	}
};
}

#endif /* UNAVIUTIL_H_ */
