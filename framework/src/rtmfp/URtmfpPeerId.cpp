/*
 * URtmfpPeerId.cpp
 *
 *  Created on: 2014-12-4
 *      Author: li.lei
 */

#include "unavi/rtmfp/URtmfpPeerId.h"

using namespace unavi;
using namespace urtmfp;
using namespace std;


#ifndef RTMFP_SESSION_DEBUG
size_t urtmfp::URtmfpPeerId::hash() const
{}

unavi::UOMBPeerIdntfr* urtmfp::URtmfpPeerId::dup() const
{}

bool urtmfp::URtmfpPeerId::equal(const UOMBPeerIdntfr& rh)const;:
#else
size_t urtmfp::URtmfpPeerId::hash() const
{
	int sz = 0;
	if ( sa_addr.sa_family == AF_INET)
		sz = sizeof(sockaddr_in);
	else
		sz = sizeof(sockaddr_in6);

	unsigned char* p = (unsigned char*)&sa_addr;
	unsigned long long v = 0;
	while(sz--) {
		v = 5*v + *p;
		p++;
	}

	return size_t(v);
}

unavi::UOMBPeerIdntfr* urtmfp::URtmfpPeerId::dup() const
{
	return new URtmfpPeerId(&sa_addr);
}

bool URtmfpPeerId::equal(const UOMBPeerIdntfr& rh)const
{
	const URtmfpPeerId* r = dynamic_cast<const URtmfpPeerId*>(&rh);
	if (!r)return false;
	if ( sa_addr.sa_family == r->sa_addr.sa_family) {
		if ( sa_addr.sa_family == AF_INET) {
			return ( 0==memcmp(&in_addr.sin_addr, &r->in_addr.sin_addr, sizeof(in_addr))
				&& in_addr.sin_port == r->in_addr.sin_port);
		}
		else {
			return ( 0 == memcmp(&in6_addr.sin6_addr, &r->in6_addr.sin6_addr, sizeof(in6_addr))
				&& in6_addr.sin6_port == r->in6_addr.sin6_port);
		}
	}
	return false;
}


#endif
