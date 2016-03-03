/*
 * URtmfpProto.h
 *
 *  Created on: 2014-10-20
 *      Author: dell
 */

#ifndef URTMFPPROTO_H_
#define URTMFPPROTO_H_

#include "unavi/core/UNaviProto.h"
#include "unavi/core/UNaviProtoDeclare.h"
#include "unavi/rtmfp/URtmfpSession.h"
#include "unavi/rtmfp/URtmfpHandshake.h"
#include "unavi/core/UNaviEvent.h"

URTMFP_NMSP_BEGIN
//TODO: fragment packet的处理
struct URtmfpProto : public UNaviProto
{
	typedef hash_map<uint32_t, URtmfpSession::Ref>::iterator SSNIter;
	typedef hash_map<uint32_t, URtmfpSession::Ref>::const_iterator SSNCIter;
	hash_map<uint32_t, URtmfpSession::Ref> sessions; //已建立session

	typedef hash_map<uint32_t, URtmfpHandshake::Ref>::iterator HANDIter;
	typedef hash_map<uint32_t, URtmfpHandshake::Ref>::const_iterator HANDCIter;
	hash_map<uint32_t, URtmfpHandshake::Ref> pending_inits; //处于握手阶段的发起方
	hash_map<uint32_t, URtmfpHandshake::Ref> pending_resps; //处于握手阶段的响应方

	URtmfpProto();
	virtual ~URtmfpProto();

	virtual void run_init();

	virtual void preproc(UNaviUDPPacket& udp);
	virtual DispatchInfo dispatch(UNaviUDPPacket& udp, bool pre_proced);
	virtual void process(UNaviUDPPacket& udp);
	virtual void cleanup();
	virtual UNaviProto* copy();

	/*
	 * 拥塞控制相关。采用基于congest window的拥塞控制
	 * 以512KB为最大接收缓冲，以4K为最小缓冲区，以设定的流量限制，
	 * 输出流量大于限定值时，平滑调整输出缓冲区大小，以4K为最小调整单位。
	 * 输入流量大于限定值时，调整给对端建议的flow buffer大小，并且通过
	 * 限制ack的发送，来达到限制对端发送频率的目的。
	 *
	 * 默认以64KB为发送拥塞控制窗口大小，以及接收窗口大小。
	 * 以1,2,4,8,16个4K逐渐增加窗口
	 * 以1,2,4,8,16个4K逐渐递减窗口
	 */
	uint32_t cur_suggest_sendbuf(uint32_t pipeid)const;
	uint32_t cur_suggest_recvbuf(uint32_t pipeid)const;

	static const uint32_t Max_sendbuf = 524288;
	static const uint32_t Max_recvbuf = 524288;
	static const uint32_t Default_sendbuf = 65536;
	static const uint32_t Default_recvbuf = 65536;
	static const uint32_t Min_sendbuf = 4096;
	static const uint32_t Min_recvbuf = 4096;
private:

	uint32_t session_id_gen;
	unsigned int rand_seed;

	struct BandCheckTimer : public UNaviEvent
	{
		BandCheckTimer(URtmfpProto& proto,uint32_t arg_ppid);

		virtual void process_event();
		URtmfpProto& target;
		int in_tune_direct; //0: 未调整, 1: 增加， -1，减少
		int in_step; //1,2,4,8 最大一次调整32K
		int out_tune_direct;
		int out_step;
		uint32_t suggest_sendbuf;
		uint32_t suggest_recvbuf;
		uint32_t pipe_id;
	};

	void init_band_checker(uint32_t ppid);

	friend class BandCheckTimer;
	BandCheckTimer* pipes_band_monitor[UNAVI_MAX_IOSVC];

	friend class URtmfpSession;

	static URtmfpProto* get_worker_proto()
	{
		return dynamic_cast<URtmfpProto*>(UNaviProto::get_worker_proto(UNaviProtoDeclare<URtmfpProto>::proto_id));
	}

	//有可读完整消息的session。只有用户读取了消息之后，
	//接收缓冲区才会释放空间。
	//在ready链表中的session的readable会递次被调用，使得各
	//session公平地得到处理
	UNaviList read_ready_sessions;

	void read_ready(URtmfpSession& ss);
	void read_empty(URtmfpSession& ss);

	struct SmartDisper: public UNaviEvent
	{
		SmartDisper(URtmfpProto& proto);
		virtual ~SmartDisper(){};
		virtual void process_event();
		URtmfpProto& target;
	};
	friend class SmartDisper;
	SmartDisper *dispatcher;

	URtmfpSession* cur_decode_session;

	//void _new_session(URtmfpPeerReady* frame, uint32_t pipe_id, uint32_t n_ssid, uint32_t f_ssid, SessionMod mod);
};

URTMFP_NMSP_END


#endif /* URTMFPPROTO_H_ */
