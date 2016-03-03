/*
 * URtmfpOFlow.h
 *
 *  Created on: 2014-10-20
 *      Author: li.lei
 */
#ifndef _URTMFPOFLOW_H_
#define _URTMFPOFLOW_H_

#include "unavi/rtmfp/URtmfpCommon.h"
#include "unavi/rtmfp/URtmfpFlow.h"
#include "unavi/rtmfp/URtmfpMsg.h"
#include "unavi/core/UNaviEvent.h"
#include "unavi/core/UNaviWorker.h"

URTMFP_NMSP_BEGIN

struct SendPendingMsg : public UNaviListable
{
	SendPendingMsg();
	virtual ~SendPendingMsg();
	uint8_t* content;
	uint64_t user_set_to;
	uint32_t size;
	uint32_t used;
	uint8_t flag;
};

class URtmfpOflow;
struct SendBufEntry : public UNaviEvent
{
	SendBufEntry(URtmfpOflow& flow);
	virtual ~SendBufEntry();
	virtual void process_event();

	uint64_t seq_id;
	uint8_t* chunk_data;
	uint8_t flag;
	uint16_t used; //�����ܴ���64K
	uint16_t size; //�����ܴ���64K
	uint16_t resend_rtt; //�����ܴ���64��
	uint8_t resend_count; //�����ܴ���256��backoff
	uint64_t user_set_to;//�û����õ����ʱʱ��
	URtmfpOflow& target;
};

struct URtmfpOflow : public URtmfpFlow
{
	friend class UNaviSession;

	//FlowRelateDir relate_dir;
	//���������ĶԶ˵�iflow
	URtmfpIflow* relating;
	//��oflow���Զ˵Ķ��iflow����
	std::vector<URtmfpIflow*> related;

	static const uint16_t Max_backoff_rtt = 10000; //����backoff��ʱ��10s�����backoff�ѵ�10s����ô��Ϊ��ʱ
	static const uint16_t Max_backoff_count = 7; //����С200ms rtt�ƣ���6���˱ܺ��ܵĳ�ʱ�����22�롣

	URtmfpOflow(URtmfpSession& ssn, uint32_t flowid);
	virtual ~URtmfpOflow();

	//set_user_meta��relate_inflow������send_msg֮ǰ���á�
	void set_user_meta(const uint8_t* meta, uint32_t size);
	//�޷�Ԥ֪�Զ˵�flowid�����Ա����ǶԶ˷��͹����ݵ��Ѿ����ڵ�flow

	void relate_inflow(URtmfpIflow* flow, FlowRelateDir relate_dir);

	void rejected(uint64_t reason)
	{
		is_rejected = true;
		reject_reason = reason;
	}

	void cum_ack(uint64_t cum_seq, int notify_buf);
	void ack_range(uint64_t start, uint64_t end);
	// user_to���ϲ����õĳ�ʱʱ�䡣���С�ڵ�ǰrtt��ʹ�õ�ǰrttֵ��
	// ���ط��˱ܷ���ʱ������˱�rttֵ���û����ó�ʱʱ�̣�ʹ�ø�С�ĳ�ʱʱ����Ϊ��ʱ��ʱ����ֵ
	void push_pending(const uint8_t* content, uint32_t sz, int user_to, uint8_t flag);
	//��Ҫȥ��flag�е�opt���
	void push_onfly(uint64_t seq, const uint8_t* content, uint32_t sz, int user_to,uint8_t flag);
	void cleanup_onfly(uint64_t seq);

	void forword_onfly();

	void trigger_nack_resend(uint64_t max_acked);

	//����buf�����ֽ��������onfly_bufsz�Ѿ��������ƣ��򷵻�0
	int available();

	//�������10s����ʹ��10s��Ϊrttֵ��
	static uint64_t rtt_backoff(uint64_t last_rtt);

	//�����user metadata����relateѡ��ӿ�ʼ���յ�ack֮ǰ��user meta��relate opt����һֱ����
	bool acked;
	bool finished;
	bool is_rejected;
	uint64_t reject_reason;
	uint64_t fsn;
	uint64_t seq_gen; //�Ѿ������İ������seq id������С�ڸ�id��data chunk�����ط��İ�

	UNaviList pending_msgs;
	uint32_t pending_count;
	uint32_t pending_size;

private:
	int notified_buf_sz;
	int onfly_sz; //suggest send buf ��notified_buf_szȡС�ߣ���ȥonfly_sz���ǿ���ʹ�õĿռ�
	typedef std::map<uint64_t,SendBufEntry*>::iterator FlyIter;
	std::map<uint64_t, SendBufEntry*> onfly_buf;
	friend class SendBufEntry;
	SendBufEntry* get_buf_entry();
	void recycle_buf_entry(SendBufEntry* nd);
	void resend_buf_entry(SendBufEntry& buf);
	UNaviList recycled_onfly_nodes;
	uint32_t recycled_onfly_cnt;

	UNaviList recycled_pending_nodes; //
	uint32_t recycled_pending_cnt;

	SendPendingMsg* get_pending_msg();
	void recycle_pending_msg(SendPendingMsg* nd);

	void cleanup_onfly(SendBufEntry& e);

	struct BufferProber: public UNaviEvent
	{
		BufferProber(URtmfpOflow& flow):
			UNaviEvent(),
			target(flow)
		{
			UNaviWorker::regist_event(*this);
		}
		virtual ~BufferProber(){}
		virtual void process_event();
		URtmfpOflow& target;
	};
	friend class BufferProber;
	BufferProber buf_prober;
};

URTMFP_NMSP_END

#endif

