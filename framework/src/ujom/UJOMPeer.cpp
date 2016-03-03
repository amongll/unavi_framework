/*
 * UJOMPeer.cpp
 *
 *  Created on: 2014-12-4
 *      Author: dell
 */

#include "unavi/ujom/UJOMPeer.h"

using namespace unavi;
using namespace urtmfp;
using namespace ujom;

void UJOMPeer::connected()
{
	unavi::UOMBPeerIdntfr* peer_id = dynamic_cast<UOMBPeerIdntfr*>(rtmfp_peer_id());
	UOMBPeer::peer_connected(peer_id);
}

void UJOMPeer::connect_failed()
{
	notify_connect_failed();
}

UOMBPeer::SendStatus UJOMPeer::send_msg_proto(const uint8_t* raw,
    uint32_t sz, UOChannel channel, uint64_t to_ml, bool is_last)
{
	urtmfp::SendStatus rtmfp_ret = URtmfpPeerReady::send_msg(channel, raw, sz ,to_ml);
	switch(rtmfp_ret) {
	case urtmfp::SEND_SUCC:
		return UOMBPeer::SEND_SUCC;
	case urtmfp::SEND_BUFFERED:
		return UOMBPeer::SEND_SHOULD_SLOW_DOWN;
	case urtmfp::SEND_OVERWHELMED:
		return UOMBPeer::SEND_OVERWHELMED;
	default:
		return UOMBPeer::SEND_FAILED;
	}
}

UOChannel UJOMPeer::new_out_channel_proto(const UOMBSign* or_sign)
{
	urtmfp::FlowOptsArg args;
	args.user_meta = or_sign->sign;
	args.meta_size = or_sign->size;
	uint32_t och = URtmfpPeerReady::get_new_outflow(&args);
	return och;
}

void UJOMPeer::set_idle_timeout_proto(uint64_t to_ml)
{
	URtmfpPeerReady::set_idle_limit(to_ml*1000);
}

void UJOMPeer::connect_peer_proto(UOMBPeerIdntfr* idntr, uint64_t to_ml)
{
	URtmfpPeerId* pid = dynamic_cast<URtmfpPeerId*>(idntr);
	URtmfpDebugPeerHand::connect(&pid->sa_addr);
}

void UJOMPeer::close_proto()
{
    URtmfpDebugPeerHand::cancel_connect();
    URtmfpPeerReady::close();
}

void UJOMPeer::idle_timedout()
{
	notify_idle_timedout();
}

void UJOMPeer::broken(urtmfp::URtmfpError broken_reason)
{
	notify_broken();
}

void UJOMPeer::send_timedout(uint32_t flowid)
{
	bool is_closed;
	UOMBObj* obj = ochannel_relating_obj(flowid, is_closed);
	if ( obj ) {
		obj->ochannel_exception(flowid, UOMBObj::OCHANNEL_SEND_TIMEDOUT, NULL);
	}
}

void UJOMPeer::writable(uint32_t oflow)
{
	bool is_closed;
    UOMBObj* obj = ochannel_relating_obj(oflow, is_closed);
    if (!obj) return;
    obj->go_on();
}

void UJOMPeer::outflow_rejected(uint32_t oflow, uint64_t code)
{
	bool is_closed;
    UOMBObj* obj = ochannel_relating_obj(oflow, is_closed);
    if (!obj) return;

    obj->ochannel_exception(oflow, UOMBObj::OCHANNEL_REJECTED, NULL);
}

void UJOMPeer::outflow_related(uint32_t oflowid, uint32_t iflowid)
{

}

void UJOMPeer::readable(uint32_t iflow)
{
    urtmfp::ReadGotten rget = read_msg(iflow);
    if ( rget.code == urtmfp::READ_OK ) {
        UOMBSign sign(rget.flow_meta,rget.flow_meta_size);
        process_msg(rget.msg->raw, rget.msg->length, iflow, &sign);
        release_input_msg(rget.msg);
    }
}

void UJOMPeer::peer_closed()
{
	notify_peer_closed();
}

void UJOMPeerApp::peer_exception(PeerErrCode err, void* detail)
{
	UOMBPeerApp::close();
}

void UJOMPeerApp::peer_unknown_servant_arrived(const UOMBSign& sign)
{
}

