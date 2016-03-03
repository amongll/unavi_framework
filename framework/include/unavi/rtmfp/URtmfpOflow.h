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
	uint16_t used; //不可能大于64K
	uint16_t size; //不可能大于64K
	uint16_t resend_rtt; //不可能大于64秒
	uint8_t resend_count; //不可能大于256次backoff
	uint64_t user_set_to;//用户设置的最大超时时刻
	URtmfpOflow& target;
};

struct URtmfpOflow : public URtmfpFlow
{
	friend class UNaviSession;

	//FlowRelateDir relate_dir;
	//主动关联的对端的iflow
	URtmfpIflow* relating;
	//该oflow被对端的多个iflow关联
	std::vector<URtmfpIflow*> related;

	static const uint16_t Max_backoff_rtt = 10000; //最大的backoff超时是10s，如果backoff已到10s，那么认为超时
	static const uint16_t Max_backoff_count = 7; //以最小200ms rtt计，在6次退避后，总的超时大概是22秒。

	URtmfpOflow(URtmfpSession& ssn, uint32_t flowid);
	virtual ~URtmfpOflow();

	//set_user_meta和relate_inflow必须在send_msg之前设置。
	void set_user_meta(const uint8_t* meta, uint32_t size);
	//无法预知对端的flowid，所以必须是对端发送过数据的已经存在的flow

	void relate_inflow(URtmfpIflow* flow, FlowRelateDir relate_dir);

	void rejected(uint64_t reason)
	{
		is_rejected = true;
		reject_reason = reason;
	}

	void cum_ack(uint64_t cum_seq, int notify_buf);
	void ack_range(uint64_t start, uint64_t end);
	// user_to是上层设置的超时时间。如果小于当前rtt，使用当前rtt值。
	// 在重发退避发生时，检查退避rtt值和用户设置超时时刻，使用更小的超时时刻作为超时定时器的值
	void push_pending(const uint8_t* content, uint32_t sz, int user_to, uint8_t flag);
	//需要去除flag中的opt标记
	void push_onfly(uint64_t seq, const uint8_t* content, uint32_t sz, int user_to,uint8_t flag);
	void cleanup_onfly(uint64_t seq);

	void forword_onfly();

	void trigger_nack_resend(uint64_t max_acked);

	//返回buf可用字节数，如果onfly_bufsz已经大于限制，则返回0
	int available();

	//如果大于10s，则使用10s作为rtt值。
	static uint64_t rtt_backoff(uint64_t last_rtt);

	//如果有user metadata或者relate选项，从开始到收到ack之前，user meta和relate opt必须一直发送
	bool acked;
	bool finished;
	bool is_rejected;
	uint64_t reject_reason;
	uint64_t fsn;
	uint64_t seq_gen; //已经发出的包的最大seq id。发送小于该id的data chunk，是重发的包

	UNaviList pending_msgs;
	uint32_t pending_count;
	uint32_t pending_size;

private:
	int notified_buf_sz;
	int onfly_sz; //suggest send buf 和notified_buf_sz取小者，减去onfly_sz就是可以使用的空间
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

