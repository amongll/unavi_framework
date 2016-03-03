/*
 * URtmfpPeerId.h
 *
 *  Created on: 2014-12-4
 *      Author: dell
 */

#ifndef URTMFPPEERID_H_
#define URTMFPPEERID_H_

#include "unavi/rtmfp/URtmfpCommon.h"
#include "unavi/frame/UOMBPeer.h"

URTMFP_NMSP_BEGIN

struct URtmfpPeerId : public unavi::UOMBPeerIdntfr
{
#ifndef RTMFP_SESSION_DEBUG
	URtmfpPeerId();
	size_t hash()const;
	unavi::UOMBPeerIdntfr* dup()const;
#else
	URtmfpPeerId(const sockaddr* peer_addr)
	{
		memset(&in6_addr,0x00,sizeof(sockaddr_in6));
		switch(peer_addr->sa_family) {
		case AF_INET:
			memcpy(&in_addr,peer_addr,sizeof(sockaddr_in));
			break;
		case AF_INET6:
			memcpy(&in6_addr,peer_addr,sizeof(sockaddr_in6));
			break;
		}
	}

	~URtmfpPeerId(){}

	union {
		sockaddr sa_addr;
		sockaddr_in in_addr;
		sockaddr_in6 in6_addr;
	};

	virtual UOMBPeerIdntfr* dup()const ;
	virtual size_t hash()const ;
	virtual bool equal(const UOMBPeerIdntfr& rh)const ;
	virtual void print(std::ostream& oss)const ;
#endif
};

URTMFP_NMSP_END

#endif /* URTMFPPEERID_H_ */
