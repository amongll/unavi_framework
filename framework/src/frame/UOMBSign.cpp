/*
 * UOMBSign.cpp
 *
 *  Created on: 2014-12-2
 *      Author: li.lei
 */

#include "unavi/frame/UOMBSign.h"

using namespace unavi;
using namespace std;

bool UOMBSign::operator <(const UOMBSign& rh) const
{
	int cmp_sz = rh.size < size ? rh.size : size;
	for ( int i=0; i<cmp_sz; i++) {
		if ( sign[i] == rh.sign[i] )
			continue;
		else if ( sign[i] < rh.sign[i] )
			return true;
		else
			return false;
	}

	return rh.size > size ? true: false;
}

bool unavi::operator ==(const UOMBSign& lh,const UOMBSign& rh)
{
	if (rh.size != lh.size) return false;
	for ( int i=0; i<lh.size; i++) {
		if ( lh.sign[i] == rh.sign[i] )
			continue;
		else
			return false;
	}
	return true;
}

std::ostream& unavi::operator<<(std::ostream& os,const UOMBSign& s)
{
	static char hch[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
	string a;
	for(int i=0; i<s.size; i++) {
		if ( ::isprint(s.sign[i])) {
			a.push_back((char)s.sign[i]);
		}
		else {
			a += "\\x";
			a.push_back( hch[(s.sign[i]>>4)] );
			a.push_back( hch[(s.sign[i]&0x0f)]);
		}
	}
	os<<a;
	return os;
}

bool UOMBSign::is_prefix_of(const UOMBSign& rh)const
{
	if ( rh.size < size) return false;
	return 0 == ::memcmp(rh.sign, sign, size);
}

size_t UOMBSign::hash() const
{
    unsigned long __h = 0;
    const uint8_t* tail = sign + size;
    const uint8_t* pc = sign;
    for ( ; pc != tail; ++pc)
      __h = 5*__h + *pc;
    return size_t(__h);
}

void UOMBSign::giveto(UOMBSign& rh)const
{
	if (rh.sign && rh.heap) {
		delete []rh.sign;
	}
	rh.sign = sign;
	rh.heap = heap;
	sign = NULL;
	heap = false;
	rh.size = size;
	size = 0;
}

UOMBSign& UOMBSign::operator =(const UOMBSign& rh)
{
	if( sign && heap ) {
		 if ( rh.size > size) {
			delete []sign;
			sign = new uint8_t[rh.size];
		}
	}
	else {
		sign = new uint8_t[rh.size];
		heap = true;
	}
	::memcpy((void*)sign,rh.sign,rh.size);
	size = rh.size;
	return *this;
}

std::ostream & UOMBSign::operator<<(std::ostream& os)const
{
	std::ostringstream ss;
	const uint8_t* p = sign;
	const uint8_t* e = sign+size;
	char buf[5];
	uint8_t c;
	while ( p != e) {
		c = *(p++);
		if (::isprint(*p)) {
			ss << (char)c;
		}
		else {
			sprintf(buf,"\\x%X", c);
			ss << buf;
		}
	}
	os << ss.str();
	return os;
}
