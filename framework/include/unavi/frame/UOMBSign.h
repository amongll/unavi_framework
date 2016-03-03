/*
 * UNaviORSign.h
 *
 *  Created on: 2014-11-26
 *      Author: dell
 */

#ifndef UOMBSIGN_H_
#define UOMBSIGN_H_

#include "unavi/frame/UOMBCommon.h"
#include <iostream>
#include <map>

UNAVI_NMSP_BEGIN

struct UOMBSign
{
	UOMBSign():
		sign(NULL),
		size(0),
		heap(false)
	{}
	UOMBSign(const uint8_t* a, uint32_t s, bool dup=false):
		sign(a),
		size(s),
		heap(dup)
	{
		if (heap) {
			sign = (const uint8_t*)(new uint8_t[s]);
			memcpy((uint8_t*)sign, a, s);
		}
	}
	~UOMBSign()
	{
		if (heap && sign) {
			delete []sign;
		}
	}

	UOMBSign(const UOMBSign& r):
		sign(r.sign),
		size(r.size),
		heap(r.heap)
	{
		if ( heap ) {
			sign = new uint8_t[size];
			::memcpy((void*)sign,r.sign,size);
		}
	}

	mutable const uint8_t* sign;
	mutable uint32_t size;
	bool operator<(const UOMBSign& rh)const;
	//bool operator==(const UOMBSign& rh)const;

	bool is_prefix_of(const UOMBSign& rh)const;

	UOMBSign& operator=(const UOMBSign& rh);

	std::ostream &operator<<(std::ostream& os)const;

	size_t hash()const;

	void giveto(UOMBSign& rh)const;
	mutable bool heap;
};

bool operator==(const UOMBSign& l, const UOMBSign& r);

std::ostream &operator<<(std::ostream& os, const UOMBSign& sign);
UNAVI_NMSP_END

namespace std {
	template<>
	struct less<unavi::UOMBSign*>
	{
		bool
		operator()(unavi::UOMBSign* const& __x, unavi::UOMBSign* const& __y) const
		{ return *__x < *__y; }
	};

	template<>
	struct less<const unavi::UOMBSign*>
	{
		bool
		operator()(const unavi::UOMBSign* const& __x, const unavi::UOMBSign* const& __y) const
		{ return *__x < *__y; }
	};

	template <>
	struct equal_to<unavi::UOMBSign*>
	{
		bool
		operator()(unavi::UOMBSign* const __x, unavi::UOMBSign* const __y) const
		{ return *__x == *__y; }
	};

	template <>
	struct equal_to<const unavi::UOMBSign*>
	{
		bool
		operator()(const unavi::UOMBSign* const __x, const unavi::UOMBSign* const __y) const
		{ return *__x == *__y; }
	};
}

namespace __gnu_cxx {
	template<>
	struct hash<const unavi::UOMBSign*>
	{
		size_t operator()(const unavi::UOMBSign* const obj) const
		{
			return obj->hash();
		}
	};

	template<>
	struct hash<unavi::UOMBSign*>
	{
		size_t operator()(unavi::UOMBSign* const obj) const
		{
			return obj->hash();
		}
	};
}


#endif /* UNAVIORSIGN_H_ */
