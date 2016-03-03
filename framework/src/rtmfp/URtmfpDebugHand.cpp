/*
 * URtmfpDebugHand.cpp
 *
 *  Created on: 2014-12-3
 *      Author: dell
 */

#include "unavi/rtmfp/URtmfpDebugHand.h"
#include "unavi/rtmfp/URtmfpDebugHandChunk.h"
#include "unavi/rtmfp/URtmfpDebugProto.h"

using namespace std;
using namespace unavi;
using namespace urtmfp;

URtmfpDebugPeerHand::URtmfpDebugPeerHand():
	hand(NULL)
{
}

URtmfpDebugPeerHand::~URtmfpDebugPeerHand()
{
}

void URtmfpDebugPeerHand::connect(const sockaddr* peeraddr)
{
	uint8_t proto_id = UNaviProtoDeclare<URtmfpDebugProto>::proto_id;
	URtmfpDebugProto* proto = dynamic_cast<URtmfpDebugProto*>
		(UNaviProto::get_worker_proto(UNaviProtoDeclare<URtmfpDebugProto>::proto_id));
	URtmfpDebugHand* new_hand = proto->new_active_hand(peeraddr);
	set_hand_impl(new_hand);

	URtmfpPacket out_packet(0, STARTUP_MOD);
	UDebugIHelloChunk ichunk(new_hand->near_id);
	UNaviUDPPacket* oudp = UNaviUDPPacket::new_proto_outpacket(proto_id);
	new_hand->pipe_id = oudp->get_pipe_id();
	out_packet.set(*oudp);
	out_packet.push_chunk(ichunk);
	out_packet.send_out(new_hand->sa_addr);
}

void URtmfpDebugPeerHand::cancel_connect()
{
	if ( hand ) {
		uint8_t proto_id = UNaviProtoDeclare<URtmfpDebugProto>::proto_id;
		URtmfpDebugProto* proto = dynamic_cast<URtmfpDebugProto*>
			(UNaviProto::get_worker_proto(UNaviProtoDeclare<URtmfpDebugProto>::proto_id));
		hand->frame = NULL;
		proto->delete_hand(hand->near_id, hand->is_initor);
		hand = NULL;
	}
}

void URtmfpDebugPeerHand::set_hand_impl(URtmfpDebugHand* impl)
{
	hand = impl;
	impl->frame = this;
}

void URtmfpDebugPeerHand::unbind_hand_impl()
{
	hand->frame = NULL;
	hand = NULL;
}

URtmfpDebugHand::TimerCheck::TimerCheck(URtmfpDebugHand& hand):
	UNaviEvent(),
	target(hand),
	dead_time(0)
{
	UNaviWorker::regist_event(*this);
}

void URtmfpDebugHand::TimerCheck::process_event()
{
	if (UNaviCycle::curtime_ml() >= dead_time ) {
		uint8_t proto_id = UNaviProtoDeclare<URtmfpDebugProto>::proto_id;
		URtmfpDebugProto* proto = dynamic_cast<URtmfpDebugProto*>
			(UNaviProto::get_worker_proto(UNaviProtoDeclare<URtmfpDebugProto>::proto_id));
		if (target.is_initor) {
			//֪ͨ�ϲ㡣
			URtmfpDebugPeerHand* frame = target.frame;
			target.frame->unbind_hand_impl();
			frame->connect_failed();

			proto->delete_hand(target.near_id,true);
		}
		else {
			proto->delete_hand(target.near_id,false);
		}
		return;
	}
	set_expire(check_period);
	if (target.is_initor) {
		URtmfpPacket out_packet(0, STARTUP_MOD);
		UDebugIHelloChunk ichunk(target.near_id);
		UNaviUDPPacket* oudp = UNaviUDPPacket::new_outpacket(target.pipe_id);
		out_packet.set(*oudp);
		out_packet.push_chunk(ichunk);
		out_packet.send_out(target.sa_addr);
	}
	else {
		URtmfpPacket out_packet(0, STARTUP_MOD);
		UDebugRHelloChunk rhello(target.far_id, target.near_id);
		UNaviUDPPacket* oudp = UNaviUDPPacket::new_outpacket(target.pipe_id);
		out_packet.set(*oudp);
		out_packet.push_chunk(rhello);
		out_packet.send_out(target.sa_addr);
	}
	return;
}

void URtmfpDebugHand::start_check(uint64_t expire, uint64_t ck_period)
{
	timer.dead_time = UNaviCycle::curtime_ml() + expire;
	timer.set_expire(ck_period*1000);
	timer.check_period = ck_period*1000;
}
