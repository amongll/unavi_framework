/*
 * URtmfpIFlow.h
 *
 *  Created on: 2014-10-20
 *      Author: li.lei
 */

#ifndef URTMFPIFLOW_H_
#define URTMFPIFLOW_H_
#include "unavi/rtmfp/URtmfpCommon.h"
#include "unavi/rtmfp/URtmfpFlow.h"
#include "unavi/rtmfp/URtmfpMsg.h"
#include "unavi/util/UNaviListable.h"
#include "unavi/util/UNaviException.h"

URTMFP_NMSP_BEGIN
class FragAssemble;
class URtmfpOflow;
class URtmfpChunk;
struct URtmfpIflow : public URtmfpFlow
{
	friend class URtmfpSession;

	bool relate_notified;
	//FlowRelateDir relate_dir;
	//���������ı���oflow
	URtmfpOflow* relating;
	//��iflow�����˵Ķ��oflow����
	std::vector<URtmfpOflow*> related;

	uint64_t *finish_seq;
	//bool finished; //�Զ��ѽ���
	bool rejected; //���˾ܾ���flow
	uint64_t reject_reason;

	void relate_outflow(URtmfpOflow* oflow, FlowRelateDir relate_dir)
	{
		if ( relate_dir == ACTIVE_RELATING )
			relating = oflow;
		else if (relate_dir == POSITIVE_RELATED) {
			related.push_back(oflow);
		}
	}

	void reject(uint64_t reason=0);

	bool has_opts() {
		return opts.list.size() > 0;
	}

	bool get_relating_opt(int64_t& oflowid);

	uint32_t get_user_meta(const uint8_t*& meta);

	//void push_data_chunk(uint64_t seqid, uint64_t fsn_offset,
	//	FlowFragmentCtrl frag_type, const uint8_t* content, uint32_t size);

	void push_data_chunk(uint64_t seqid, uint64_t fsn_offset,
		uint8_t frag_opts, const uint8_t* content, uint32_t size);

private:
	URtmfpIflow(URtmfpSession& ssn,uint32_t flowid);
	virtual ~URtmfpIflow();
	/*
	 * ���ջ��弰ack�Ĺ���
	 */

	struct BindPair
	{
		BindPair():prev_node(NULL),next_node(NULL){}
		FragAssemble* prev_node; //seqid���ڵ���Prev_node->seqid
		FragAssemble* next_node; //seqidС��next_node->seqid
	};


	void push_ready(URtmfpMsg& msg);
	UNaviListable ready_link;
public:
	static URtmfpIflow* get_from_ready_link(UNaviListable* link)
	{
		return unavi_list_data(link, URtmfpIflow, ready_link);
	}
private:

	bool set_first_fsn(uint64_t seq);

	//void fill_ass_node(FlowFragmentCtrl type, uint64_t seqid,
	//	const uint8_t* content,
	//	uint32_t sz, bool is_last=false);
	void fill_ass_node(uint8_t opts, uint64_t seqid, const uint8_t* content, uint32_t sz);

	BindPair get_bind(uint64_t seqid);

	//��ackʱ������fsn֮ǰ��δ��ɻ��壬���ҿ�����Ϊ����ʹ���µ���Ϣ
	//���Ե��͸��ϲ㡣 ֻ������֮�󣬲ż���ack����
	void clean_ass_beforeFSN(); //�ڷ���һ��ack chunkʱ������
	void try_msg_afterFSN();

	//����һ��ackʱ�����flowid+bufavailable+cumAck�ֶεı���
	const uint8_t* calc_ack_header(uint32_t& sz, uint32_t &buf_notify);
	void get_ack_content(const uint8_t*& range_raw, uint32_t& range_sz,
		const uint8_t*& bm_raw, uint32_t& bm_sz);

	UNaviList ass_list; //ֻ����δ������msg���м�����пն���Ҳ����û�пն�
	UNaviList ready_msgs; //������������Msg, �����û�����δ������
	//���ڼ�����ջ����window��С��Ӱ��ack�е�bufavialble�ļ���
	int ass_sum; //��assemble�����е��ֽ���
	int ready_sum; //��ready�����е��ֽ���, �������ֽ�����

	//int hole_sum;
	int hole_cnt; //�ն����ܸ���

	//flow seq id Ϊ��������������Ҳ��ܻػ�ʹ��
	//uint32_t flow_id;
	uint64_t filled_to;
	uint64_t *first_fsn;
	uint64_t fsn; //will not recv seq which <= fsn

	uint8_t* range_ack_buf;
	uint32_t range_ack_bufsz;
	uint8_t* bitmap_ack_buf;
	uint32_t bitmap_ack_bufsz;

	URtmfpChunk* pending_ack;
	uint32_t last_notified_bufsz;

	void fill_bm(int bit_pos, int cnt, bool set=true);

	uint8_t ack_header[40];
	bool need_ack;
};

struct FragAssemble : public UNaviListable
{
	FragAssemble(bool pair=false):
		seqid(0),
		serve_pair((uint8_t)(pair?1:0)),
		ass_buf(NULL),
		used(0),
		size(0),
		frag_flags(0)
	{}

	virtual ~FragAssemble()
	{
		if (ass_buf) ::free(ass_buf);
	}

	void reset()
	{
		if (ass_buf==NULL)
			size = 0;
		seqid = 0;
		used = 0;
		frag_flags = 0;
	}

	void fill1(uint64_t argseqid, FlowFragmentCtrl type, const uint8_t* content,
		uint32_t len, bool will_pair=false, bool is_abondon=false);
	void fill2(const uint8_t* content, FlowFragmentCtrl type, uint32_t len, bool is_abondon= false);

	void set_abondon()
	{
		has_abn = 1;
	}

	int64_t seqid;
	uint8_t* ass_buf;
	int32_t used;
	int32_t size;
	union {
		uint32_t frag_flags;
		struct {
			uint8_t has_first:1;
			uint8_t has_last:1;
			uint8_t serve_pair:1;
			uint8_t has_pair:1;
			uint8_t has_abn:1;
		};
	};
};


URTMFP_NMSP_END

#endif /* URTMFPIFLOW_H_ */
