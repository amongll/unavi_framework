/*
 * URtmfpDebugHand.h
 *
 *  Created on: 2014-12-3
 *      Author: dell
 */

#ifndef URTMFPDEBUGHAND_H_
#define URTMFPDEBUGHAND_H_

#include "unavi/rtmfp/URtmfpCommon.h"
#include "unavi/core/UNaviEvent.h"
#include "unavi/util/UNaviRef.h"

URTMFP_NMSP_BEGIN

class URtmfpDebugProto;
class URtmfpDebugHand;
class URtmfpChunk;

//主动发起连接的framePeer对象需要实现的接口。
struct URtmfpDebugPeerHand
{
	URtmfpDebugPeerHand();
	virtual ~URtmfpDebugPeerHand();
	//主动发起连接方的触发回调。
	virtual void connected() = 0;
	virtual void connect_failed() = 0;

protected:
	friend class URtmfpDebugHand;
	friend class URtmfpDebugProto;
	void connect(const sockaddr* peeraddr);
	void cancel_connect();

	void set_hand_impl(URtmfpDebugHand* impl);
	void unbind_hand_impl();

private:
	URtmfpDebugHand* hand;
};

class URtmfpDebugHand
{
public:
	typedef UNaviRef<URtmfpDebugHand> Ref;
	friend class UNaviRef<URtmfpDebugHand>;

	uint32_t near_id;
	uint32_t far_id;
	bool is_initor;
	sockaddr* peer_addr;
	uint32_t pipe_id;
	union{
		sockaddr sa_addr;
		sockaddr_in in_addr;
		sockaddr_in6 in6_addr;
	};

	struct TimerCheck: public UNaviEvent
	{
		TimerCheck(URtmfpDebugHand& hand);
		virtual void process_event();
		URtmfpDebugHand& target;
		uint64_t dead_time;
		uint64_t check_period;
	};
	friend class TimerCheck;

	void start_check(uint64_t expire, uint64_t ck_period);

	TimerCheck timer;

	URtmfpDebugPeerHand* frame;	//在主动发起的连接中，frame非空
	friend class URtmfpDebugProto;
	URtmfpDebugHand():
		frame(NULL),
		near_id(0xffffffff),
		far_id(0xffffffff),
		is_initor(false),
		timer(*this),
		pipe_id(0xffffffff)
	{
		peer_addr = &sa_addr;
		memset(&in6_addr,0x00,sizeof(sockaddr_in6));
	};
	~URtmfpDebugHand(){}
};

URTMFP_NMSP_END




#endif /* URTMFPDEBUGHAND_H_ */
