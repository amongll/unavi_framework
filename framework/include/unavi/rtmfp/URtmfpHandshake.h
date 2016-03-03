/*
 * URtmfpHandshake.h
 *
 *  Created on: 2014-10-20
 *      Author: dell
 */

#ifndef URTMFPHANDSHAKE_H_
#define URTMFPHANDSHAKE_H_
#include "unavi/rtmfp/URtmfpCommon.h"
#include "unavi/core/UNaviEvent.h"
#include "unavi/util/UNaviRef.h"

URTMFP_NMSP_BEGIN

class URtmfpProto;
class URtmfpHandshake;
class URtmfpChunk;

struct URtmfpIntroducer
{
	virtual ~URtmfpIntroducer();
	virtual void hand_with_other(URtmfpHandshake& hand, const uint8_t* discriminator, uint32_t disc_sz){}
	void redirect(URtmfpHandshake& hand, const sockaddr* redirectto);
	void fwd_to(URtmfpHandshake& hand, URtmfpSession& dst);
};

struct URtmfpPeerHand
{
	URtmfpPeerHand();
	virtual ~URtmfpPeerHand();
	virtual void connected(const sockaddr* peeraddr, const uint8_t* peer_id, uint32_t disc_sz, bool is_initor){}
	void connect(const uint8_t* edp, uint32_t edp_sz, const sockaddr* boot_remote);

	void set_hand_impl(URtmfpHandshake* impl);

private:
	URtmfpHandshake* hand;
	uint8_t* peer_id;
	size_t peer_id_size;
};

class URtmfpHandshake
{
public:
	typedef UNaviRef<URtmfpHandshake> Ref;
	friend class UNaviRef<URtmfpHandshake>;

private:
	URtmfpPeerHand* frame;
	friend class URtmfpProto;
	URtmfpHandshake(){}
	~URtmfpHandshake(){}

	void process_chunk(URtmfpChunk& chunk){}


};

URTMFP_NMSP_END

#endif /* URTMFPSESSION_H_ */
