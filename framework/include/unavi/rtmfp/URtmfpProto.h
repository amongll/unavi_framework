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
//TODO: fragment packet�Ĵ���
struct URtmfpProto : public UNaviProto
{
	typedef hash_map<uint32_t, URtmfpSession::Ref>::iterator SSNIter;
	typedef hash_map<uint32_t, URtmfpSession::Ref>::const_iterator SSNCIter;
	hash_map<uint32_t, URtmfpSession::Ref> sessions; //�ѽ���session

	typedef hash_map<uint32_t, URtmfpHandshake::Ref>::iterator HANDIter;
	typedef hash_map<uint32_t, URtmfpHandshake::Ref>::const_iterator HANDCIter;
	hash_map<uint32_t, URtmfpHandshake::Ref> pending_inits; //�������ֽ׶εķ���
	hash_map<uint32_t, URtmfpHandshake::Ref> pending_resps; //�������ֽ׶ε���Ӧ��

	URtmfpProto();
	virtual ~URtmfpProto();

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

	static URtmfpProto* get_worker_proto()
	{
		return dynamic_cast<URtmfpProto*>(UNaviProto::get_worker_proto(UNaviProtoDeclare<URtmfpProto>::proto_id));
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
