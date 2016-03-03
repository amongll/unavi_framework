/*
 * UFlashPeer.h
 *
 *  Created on: 2014-11-26
 *      Author: dell
 */

#ifndef UFLASHPEER_H_
#define UFLASHPEER_H_

#include "unavi/frame/UOMBPeer.h"
#include "unavi/rtmfp/URtmfpSession.h"
#include "unavi/rtmfp/URtmfpHandshake.h"

UNAVI_NMSP_BEGIN
class UFlashPeer : public UOMBPeer, protected urtmfp::URtmfpPeerReady, protected urtmfp::URtmfpPeerHand
{

};

UNAVI_NMSP_END

#endif /* UFLASHPEER_H_ */
