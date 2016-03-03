/*
 * UJOMPeer.h
 *
 * UNavi Object Json Msg Peer
 *
 *  Created on: 2014-11-28
 *      Author: dell
 */

#ifndef UJOMPEER_H_
#define UJOMPEER_H_

#include "unavi/ujom/UJOMCommon.h"
#include "unavi/frame/UOMBPeerApp.h"
#include "unavi/rtmfp/URtmfpSession.h"
#include "unavi/rtmfp/URtmfpDebugHand.h"

UJOM_NMSP_BEGIN

class UJOMPeer : public unavi::UOMBPeer, public urtmfp::URtmfpPeerReady, public urtmfp::URtmfpDebugPeerHand
{
protected:
	virtual void connected();
	virtual void connect_failed();

	virtual UOMBPeer::SendStatus send_msg_proto(const uint8_t* raw, uint32_t sz, UOChannel channel/*object request channel*/,
		uint64_t to_ml, bool is_last);
	virtual UOChannel new_out_channel_proto(const UOMBSign* or_sign);
	virtual void set_idle_timeout_proto(uint64_t to_ml);
	virtual void connect_peer_proto(UOMBPeerIdntfr* idntr, uint64_t to_ml);
	virtual void close_proto();

	virtual void idle_timedout();
	virtual void broken(urtmfp::URtmfpError broken_reason);
	virtual void send_timedout(uint32_t flowid);
	virtual void writable(uint32_t oflow);
	virtual void outflow_rejected(uint32_t oflow, uint64_t code);
	virtual void outflow_related(uint32_t oflowid, uint32_t iflowid);

	virtual void readable(uint32_t iflow);
	virtual void peer_closed();
};

class UJOMPeerApp : public unavi::UOMBPeerApp
{
protected:
	UJOMPeerApp(UOMBPeerApp::AppType type):
		UOMBPeerApp(type)
	{}
	virtual void proxy_runned(UOMBProxy& start){};
	virtual void servant_arrived(UOMBServant& svt){};

	virtual void peer_exception(PeerErrCode err,void* detail);
	virtual void peer_unknown_servant_arrived(const UOMBSign& sign);

};

UJOM_NMSP_END

#endif /* UJOMPEER_H_ */
