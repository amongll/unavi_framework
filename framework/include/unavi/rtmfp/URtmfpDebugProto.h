/*
 * URtmfpProto.h
 *
 *  Created on: 2014-10-20
 *      Author: dell
 */

#ifndef URTMFPDEBUGPROTO_H_
#define URTMFPDEBUGPROTO_H_

#include "unavi/core/UNaviProto.h"
#include "unavi/core/UNaviProtoDeclare.h"
#include "unavi/rtmfp/URtmfpSession.h"
#include "unavi/rtmfp/URtmfpDebugHand.h"
#include "unavi/core/UNaviEvent.h"
#include "unavi/util/UNaviUtil.h"

URTMFP_NMSP_BEGIN

//TODO: fragment packet�Ĵ���
struct URtmfpDebugProto : public UNaviProto
{
	//����ͬ�ĵ�ַ4Ԫ���ϣ���ֹʹ���ظ��ĶԶ�ssid��

	typedef hash_map<uint32_t, URtmfpSession::Ref>::iterator SSNIter;
	typedef hash_map<uint32_t, URtmfpSession::Ref>::const_iterator SSNCIter;
	hash_map<uint32_t, URtmfpSession::Ref> sessions; //�ѽ���session

	typedef hash_map<uint32_t, URtmfpDebugHand::Ref>::iterator HANDIter;
	typedef hash_map<uint32_t, URtmfpDebugHand::Ref>::const_iterator HANDCIter;
	hash_map<uint32_t, URtmfpDebugHand::Ref> pending_inits; //�������ֽضϵķ���
	hash_map<uint32_t, URtmfpDebugHand::Ref> pending_resps; //�������ֽ׶ε���Ӧ��

	typedef hash_multimap<unavi::AddressKey, uint32_t>::iterator AddrIdxIter;
	hash_multimap<unavi::AddressKey, uint32_t> accept_hand_addr_idx;
	hash_multimap<unavi::AddressKey, uint32_t> accept_ssn_addr_idx;

	URtmfpDebugProto();
	virtual ~URtmfpDebugProto();

	virtual void run_init();

	virtual void preproc(UNaviUDPPacket& udp);
	virtual DispatchInfo dispatch(UNaviUDPPacket& udp, bool pre_proced);
	virtual void process(UNaviUDPPacket& udp);
	virtual void cleanup();
	virtual UNaviProto* copy();

	/*
	 * ӵ��������ء����û���congest window��ӵ������
	 * ��512KBΪ�����ջ��壬��4KΪ��С�����������趨���������ƣ�
	 * ������������޶�ֵʱ��ƽ�����������������С����4KΪ��С������λ��
	 * �������������޶�ֵʱ���������Զ˽����flow buffer��С������ͨ��
	 * ����ack�ķ��ͣ����ﵽ���ƶԶ˷���Ƶ�ʵ�Ŀ�ġ�
	 *
	 * Ĭ����64KBΪ����ӵ�����ƴ��ڴ�С���Լ����մ��ڴ�С��
	 * ��1,2,4,8,16��4K�����Ӵ���
	 * ��1,2,4,8,16��4K�𽥵ݼ�����
	 */
	uint32_t cur_suggest_sendbuf(uint32_t pipeid)const;
	uint32_t cur_suggest_recvbuf(uint32_t pipeid)const;

	URtmfpDebugHand* new_active_hand(const sockaddr* peer_addr);
	void delete_hand(uint32_t near_ssid,bool is_initor);
	void delete_session(uint32_t near_ssid);

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
		BandCheckTimer(URtmfpDebugProto& proto,uint32_t arg_ppid);

		virtual void process_event();
		URtmfpDebugProto& target;
		int in_tune_direct; //0: δ����, 1: ���ӣ� -1������
		int in_step; //1,2,4,8 ���һ�ε���32K
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

	static URtmfpDebugProto* get_worker_proto()
	{
		return dynamic_cast<URtmfpDebugProto*>(UNaviProto::get_worker_proto(UNaviProtoDeclare<URtmfpDebugProto>::proto_id));
	}

	//�пɶ�������Ϣ��session��ֻ���û���ȡ����Ϣ֮��
	//���ջ������Ż��ͷſռ䡣
	//��ready�����е�session��readable��ݴα����ã�ʹ�ø�
	//session��ƽ�صõ�����
	UNaviList read_ready_sessions;

	void read_ready(URtmfpSession& ss);
	void read_empty(URtmfpSession& ss);

	struct SmartDisper: public UNaviEvent
	{
		SmartDisper(URtmfpDebugProto& proto);
		virtual ~SmartDisper(){};
		virtual void process_event();
		URtmfpDebugProto& target;
	};
	friend class SmartDisper;
	SmartDisper *dispatcher;

	URtmfpSession* cur_decode_session;
};

URTMFP_NMSP_END


#endif /* URTMFPPROTO_H_ */
