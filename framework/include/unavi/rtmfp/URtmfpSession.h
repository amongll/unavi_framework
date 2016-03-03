/*
 * URtmfpSession.h
 *
 *  Created on: 2014-10-20
 *      Author: dell
 */

#ifndef URTMFPSESSION_H_
#define URTMFPSESSION_H_
#include "unavi/rtmfp/URtmfpMsg.h"
#include "unavi/rtmfp/URtmfpIflow.h"
#include "unavi/rtmfp/URtmfpOflow.h"
#include "unavi/core/UNaviEvent.h"
#include "unavi/rtmfp/URtmfpPacket.h"
#include "unavi/rtmfp/URtmfpPeerId.h"

URTMFP_NMSP_BEGIN

class URtmfpProto;
class URtmfpDebugProto;
class URtmfpSession;

enum SendStatus
{
	//��BUFFEREDʱ����ʾ���ͻ�������������Ϣ���ڵȴ������С�
	//�ϲ�Ӧ�ý��ͷ���Ƶ�ʣ�������ͣ���ͽ���
	SEND_BUFFERED,
	SEND_SUCC,
	//pending msg���������󣬻�����onflybuf�е�δ��ȷ�ϵ���Ϣ
	//�ڷ���overwhelmedʱ����ʾ���˲�ȷ�����ȵ���Ϣ�Զ����յ��ġ�
	SEND_OVERWHELMED,
	SEND_DENYED,//һ����flow�Ѿ����������߶Զ��Ѿ�flow excp��flow
	SEND_ON_BROKEN_SESSION,
	SEND_INFRA_ERR//�ײ������ʩ�д���
};

enum ReadStatus
{
	READ_OK,
	PEER_FINISHED,
	READ_WOULD_BLOCK,
	READ_NONEXIST_FLOW
};

enum SessionStage
{
	SESSION_ALIVE,
	PENDING_SEND_CLOSING, //todo: ��pending�����ѷ�������δ��ȷ�ϣ������ϲ��Է���رա�
	CLOSE_ACK_WAITING, //�����رն˵�״̬�����ȴ�ack 20s��
	CLOSE_TIMEOUT, //�����رն˵�״̬���ȴ��Զ��ط���close chunk��ά��20s��
	SESSION_BROKEN
};

struct ReadGotten
{
	ReadGotten(ReadStatus arg=READ_OK):
	code(arg),
	msg(NULL),
	flow_meta(NULL),
	flow_meta_size(0)
	{}

	ReadStatus code;
	URtmfpMsg *msg;
	const uint8_t* flow_meta;
	uint32_t flow_meta_size;
};

struct FlowOptsArg
{
	FlowOptsArg():
		user_meta(NULL),
		meta_size(0),
		relating_iflow(-1)
	{}
	const uint8_t* user_meta;
	int meta_size;
	int64_t relating_iflow;
};

struct URtmfpPeerReady;
class URtmfpProto;
class URtmfpDebugProto;

class URtmfpSession
{
public:
	typedef UNaviRef<URtmfpSession> Ref;
	friend class UNaviRef<URtmfpSession>;

	//static void new_session(URtmfpPeerReady* frame, uint32_t pipe_id, uint32_t n_ssid, uint32_t f_ssid, SessionMod mod);

	URtmfpPeerReady* frame; //frame�����������session���󱻴�����frame����϶�����session��������

	//����session�޽���ʱ���ޣ�����ָ�����ƣ�session�Զ��رա�
	//���������������Զ������sys ping��
	void set_idle_limit(uint64_t timeout_mc);
	//��û�з�������ʱ��û���յ��Զ��κ�flow��Ϣ����ʱ��ʱ��������
	void idle_timedout();
	//��ֻ�г������û��������೤ʱ��֮�󣬱����á�Ĭ�ϲ���30s�����ơ�
	//���߶Զ˽��յ��˷Ƿ���packet
	void broken(URtmfpError broken_reason);

	bool send_would_overwhelm(uint32_t oflowid);
	void send_timedout(uint32_t flowid); //�����ݷ����������޶Զ�ack��ʱ�󣬱�����
	//��oflow��д�����������±�Ϊ��дʱ�������á�
	//��writeable�е��õĵ�һ��semd_msg�϶��ܱ�����
	void writable(URtmfpOflow* flow);
	void outflow_rejected(URtmfpOflow* flow, uint64_t code);
	void outflow_related(URtmfpOflow* oflow, URtmfpIflow* iflow);

	void readable(URtmfpIflow* flow);//��in flow����Ϣ����ʱ�������á�����session��readable����ƽ�ص��á�
	ReadGotten read_msg(uint32_t iflow);
	void release_input_msg(URtmfpMsg* msg);
	void reject_input_flow(URtmfpIflow* flow,uint64_t code);

	void user_ping_relayed(const uint8_t* relay, uint32_t sz);
	void user_ping_timedout(const uint8_t* ping_raw, uint32_t sz);
	void user_ping(const uint8_t* ping_raw, uint32_t sz, uint64_t timeout=0);

	void peer_closed();
	void close();

	uint32_t get_rtt()
	{
		return effective_rtt > 250? effective_rtt : 250;
	}

	const uint32_t far_ssid; //�Զ�ά��
	const uint32_t near_ssid; //����ά��
	const SessionMod mod;

	const uint32_t pipe_id;
	const sockaddr* peer_addr;

	static URtmfpIflow* get_curssn_iflow(uint32_t flowid);
private:
	bool had_msg_ever;

	friend class URtmfpProto;
	friend class URtmfpDebugProto;
#ifndef RTMFP_SESSION_DEBUG
	URtmfpSession(URtmfpProto& mgr, const uint32_t n_ssid, const uint32_t f_ssid, SessionMod ssmod,
		const uint32_t ppid, URtmfpPeerId* peerid);
#else
	URtmfpSession(URtmfpDebugProto& mgr, const uint32_t n_ssid, const uint32_t f_ssid, SessionMod ssmod,
			const uint32_t ppid, URtmfpPeerId* peerid);
#endif
	~URtmfpSession()
	{
		cleanup();
	}

	/************
	 *���������to_limit����ô��Ĭ�ϵ�ָ���˱��㷨�����ش�����������ش�
	 *�˱ܴ�������Ϊ��ʱ����������˳�ʱ����ô����Ϣ������ֵ��Ϊ��ʱʱ��
	 *������õĳ�ʱʱ��С�ڵ�ǰ�����rtt����ô���õ�ǰrttֵ��
	 *���is_lastΪ�棬��ôdata chunk����Я��finish���
	 *
	 *�Է���ֵΪBUFFEREDʱ�����齵����Ϣ���͵�Ƶ�ʡ�
	 *�������ֵ��OVERWHELMEDʱ��˵��Ϊ�˱���Ϣ�ķ��ͣ������ڷ��ͻ����е���Ϣ
	 *�������������������ش��Ļ��ᣬҲ����˵���ܱ�֤������Ϣ���Զ˽��ա�
	 *һ������ʽ����ģʽ�£���bufferedʱ���ʵ����ͷ���Ƶ�ʣ����������ͽ���
	 *������ʱ�������ʵ�ǿ�Ʒ���
	 *****************/
	SendStatus send_msg(uint32_t flowid,const uint8_t* raw,uint32_t size, int32_t to_limit = -1,
		bool is_last=false, const FlowOptsArg* opts=NULL);

	//����flow/ping�ﾳ������frame����
	void cleanup();

	friend class URtmfpPeerReady;
	union {
		sockaddr sa_peer_addr;
		sockaddr_in in_peer_addr;
		sockaddr_in6 in6_peer_addr;
	};

	URtmfpIflow* cur_readable; //����inflow�Ĳ��ҹ��� �ϲ�ÿ���л�flow����read msgʱ�������һ�ζ��ֲ���
	URtmfpOflow* cur_writable; //����outflow�Ĳ��ҹ��� �ϲ�ÿ���л�flow����send msgʱ�������һ�ζ��ֲ���

	friend class URtmfpOflow;
	friend class URtmfpIflow;
	//chunk�Ǵ���rtmfppacket�����chunk�����chunk�Ƿ�
	//������һ��udp�������ܵĻ������Զ��������fragment packet
	//����ʹdata chunk��next data chunk
	//�÷������ڷ���ping,ack,probe_buf�ȷ�data/next data��chunk
	void send_sys_chunk(URtmfpChunk& chunk);
	//�÷�������msg chunk���ط�����send_msg���߼���һ���Ĳ�𣬸����߼���Ҫ����
	//�˱��㷨�Լ�fsn��ǰ��
	void resend_data_chunk(URtmfpOflow& flow, SendBufEntry& resend_entry);

	void send_pending_msg(URtmfpOflow& flow, SendPendingMsg& msg);

	struct SessionChecker : public UNaviEvent
	{
		URtmfpSession& target;
		SessionChecker(URtmfpSession& ssn):
			UNaviEvent(10000),
			target(ssn),
			active_close_deadline(0)
		{
			UNaviWorker::regist_event(*this);
		}

		virtual ~SessionChecker() {};
		virtual void process_event();
		uint64_t active_close_deadline;
	};

	friend class SessionChecker;
	SessionChecker ssn_checker;
	uint64_t idle_limit;
	uint64_t last_active_tm; //���۳����������ݸ��صİ��ķ���ʱ�̣�����ack���յ�ʱ�̡������г�����û�ping/����ping reply��ʱ�̡�
	uint64_t last_in_tm;//����ʲô���ͣ�������������ʱ�̡�
	uint64_t last_out_tm;//���һ�ε�data/next data/buffer probe/ping����ʱ�̣���Щ���������Զ˵ķ���

#ifndef RTMFP_SESSION_DEBUG
	friend class URtmfpProto;
	URtmfpProto& proto;
#else
	URtmfpDebugProto& proto;
#endif

	void process_chunk(URtmfpChunk& chunk);

	typedef std::vector<URtmfpFlow*>::iterator FlowIter;
	typedef std::vector<URtmfpFlow*>::const_iterator FlowCIter;
	std::vector<URtmfpFlow*> input_flows;
	std::vector<URtmfpFlow*> output_flows;

	uint32_t oflow_id_gen;
	URtmfpIflow* get_input_flow(uint32_t flowid);
	URtmfpOflow* get_output_flow(uint32_t flowid);

	URtmfpIflow* have_input_flow(uint32_t flowid);
	URtmfpOflow* have_output_flow(uint32_t flowid);


	/***
	 * ���������ping��������
	 * 	1�� ���ݽ���ʱ�Զ���ping
	 * 	2�� Ӧ�ò����������ping
	 */
	void sys_ping();

	friend class Ping;
	struct Ping : public UNaviEvent
	{
		virtual ~Ping()
		{
			if ( ping_raw) delete []ping_raw;
		}
		virtual void process_event();

		int32_t id; //user pingʹ����ֱ��sys pingʹ�ø�ֵ
		uint8_t* ping_raw;
		uint32_t sz;
		URtmfpSession* target;
	};

	typedef std::map<int32_t, Ping*>::iterator PingIter;
	std::map<int32_t, Ping*> user_pings;

	int32_t sys_pingid;
	int32_t user_pingid;

	/*********************************
	 * RTT�������
	 *********************************/
	uint64_t basetime_ml;
	static const uint32_t min_rtt = 250;
	static const uint32_t max_rtt_backoff = 10000;

	int32_t rd_ts;
	int64_t rd_ts_chgtm;
	int32_t rd_tse;

	int32_t wr_ts;
	int64_t wr_ts_chgtm;
	int32_t wr_tse;

	int32_t smooth_rtt;
	int32_t rtt_va;

	int32_t measured_rtt;
	int32_t effective_rtt;

	uint64_t packet_startup_time_ml;

	void init_rtt();
	void rtt_check(const URtmfpPacket& in);
	void pkt_time_process(URtmfpPacket& out);
	void pkt_time_justify(URtmfpPacket& out);

	/*******************************************
	 *	session���յĻ���������ack����
	 *******************************************/
	mutable UNaviList recycled_ass_nodes;
	int recycled_ass_cnt;

	mutable UNaviList recycled_msg_nodes;
	int recycled_msg_cnt;

	FragAssemble* get_new_assnode()
	{
		if ( recycled_ass_cnt == 0)
		return new FragAssemble;
		else {
			recycled_ass_cnt--;
			FragAssemble* nd = dynamic_cast<FragAssemble*>(recycled_ass_nodes.get_head());
			nd->quit_list();
			return nd;
		}
	}

	void recycle_ass_node(FragAssemble* nd)
	{
		if (!nd) return;
		if (recycled_ass_cnt > 256) delete nd;
		nd->reset();
		recycled_ass_cnt++;
		recycled_ass_nodes.insert_tail(*nd);
	}

	URtmfpMsg* get_new_msgnode()
	{
		if ( recycled_msg_cnt == 0)
			return new URtmfpMsg;
		else {
			recycled_msg_cnt--;
			URtmfpMsg* nd = dynamic_cast<URtmfpMsg*>(recycled_msg_nodes.get_head());
			nd->quit_list();
			return nd;
		}
	}

	void recycle_msgnode(URtmfpMsg* nd)
	{
		if (!nd) return;
		if (recycled_msg_cnt > 36) {
			delete nd;
			return;
		}
		nd->reset();
		recycled_msg_cnt++;
		recycled_msg_nodes.insert_tail(*nd);
	}

	//��ʱ��ack�����ack���ͷֿ���Ŀ�ģ�
	//�ڶ�ʱ����ack֮ʱ���������µ�udp flow msg������
	//��Щ������ͬһ��cycleѭ���У�����ʹ�ü����ack����
	//׼ȷ��ӳ���ջ�������״̬
	struct AckPrepareTimer : public UNaviEvent
	{
		AckPrepareTimer(URtmfpSession& buf):
		UNaviEvent(0),
		target(buf) {
			UNaviWorker::regist_event(*this);
		}

		virtual ~AckPrepareTimer() {};
		virtual void process_event();
		URtmfpSession& target;
	};

	struct AckSmartSender: public UNaviEvent
	{
		AckSmartSender(URtmfpSession& buf):
		UNaviEvent(-1, UNaviEvent::EV_ALREADY),
		target(buf) {
			UNaviWorker::regist_event(*this);
		}

		virtual ~AckSmartSender() {};
		virtual void process_event();
		URtmfpSession& target;
	};

	friend class AckSmartSender;
	friend class AckPrepareTimer;

	//������������Я��ack���ҿռ��㹻��������ack���ݣ�����ack����Ҫ���Ͳ�����
	//��ӳ���ػ���ռ������ack������ἤ��Զ��ط��ѷ��͵İ���
	//��ÿ���յ��µİ�֮�󣬴���idle_ack_timer
	//��ÿ���յ��ط��İ�֮�󣬴���emerge_ack_timer, ��Ϊ���˵�ack�����ط�����ʱʱ�̻���ack��ʧ
	//���췢��ack������Զ��Ծ��ظ�����
	//��ÿ�η���ackʱ��ȡ��������timer��

	void enable_emerge_ack() {
		if ( !emerg_ack_timer.expire_active() ) {
			emerg_ack_timer.set_expire(Emerg_ack_wait);
			if ( idle_ack_timer.expire_active() )
			UNaviWorker::quit_event(idle_ack_timer,UNaviEvent::EV_TIMEOUT);
		}
	}

	void enable_idle_ack() {
		if ( !emerg_ack_timer.expire_active() ) {
			if ( !idle_ack_timer.expire_active() ) {
				idle_ack_timer.set_expire(Idle_ack_wait);
			}
		}
	}

	//��ÿ�η���rtmfp packetʱ�����Ƿ���piggy back ack����
	//piggy backʱ������pending_ack�������¼��㣬��Ϊ����
	//�������κ�ʱ�̷�������ack�ļ��㲻��Ƶ���Ľ���
	//piggy_back��ack������˻���һ����״̬�ӳ٣������ڿɽ��ܷ�Χ
	void try_piggy_ack(URtmfpPacket& out);
	UNaviList pending_ack;//URangeAckChunk����UBitmapAckChunk

	AckPrepareTimer emerg_ack_timer;//�յ��ط������Լ�������Ҫ���췢��ack�Ķ�ʱ���� 10 milli sec
	AckPrepareTimer idle_ack_timer;//û�г����ʱ��ack��ʱ��. 200milli sec
	AckSmartSender ack_smart_sender;//��������ack����������������rtmfpPacketʱ�����ռ��Ƿ��㹻�����㹻����Я������ack

	static const int Idle_ack_wait = 99000;//micro sec
	static const int Emerg_ack_wait = 4000;
	static const int Send_ack_wait = 1000;

	/******************************************
	 * ���Ͷ˵Ĺ����Լ���ʱ�ش�����,ack�Ľ��մ���
	 *****************************************/

	//����ʱ����rtmfp packet�ĸ�����Ϣ
	struct LastDataInfo {
		LastDataInfo():
			flow_id(-1),
			fsn(0),
			resend_seqid(0)
		{}
		int64_t flow_id; //Ϊ-1ʱ����ʾǰһ��chunk����data chunk����next data chunk
		uint64_t fsn;
		uint64_t resend_seqid;
	};
	LastDataInfo last_data_chunk;
	std::vector<uint32_t> cur_packet_flows; //��ǰpacket���ɵ�oflow��id

	//��Ҫ���ڶ��chunk����packet���з���
	URtmfpPacket out_packet;
	bool send_pending;

#ifdef RTMFP_SESSION_DEBUG
	void send_packet_trace(URtmfpPacket &opkt,bool piggy_ack,const char* fmt,...);
#endif
	void send_packet(URtmfpPacket &opkt,bool piggy_ack=true);

	void update_peer_addr(const sockaddr& addr);

	//�����һ���ӽ���mtu��rtmfp�����֣�������������
	//��������ڸ�smart event���з���
	//������ҵ����̡�������ʱ������ƣ�����ʵ�ʵİ��ķ��͡�
	//�����������Σ���ҵ�������
	//����������ʱ�����У������˴�����Сchunk������Ҫ
	//������һ��udp�����������Ǹ���ͬһ��packet��
	//�ù��̽�����������֮������
	struct SmartSender: public UNaviEvent
	{
		SmartSender(URtmfpSession& ssn):
		UNaviEvent(-1,UNaviEvent::EV_ALREADY),
		target(ssn)
		{
			UNaviWorker::regist_event(*this);
		}

		virtual ~SmartSender() {}
		virtual void process_event();
		URtmfpSession& target;
	};
	friend class SmartSender;
	SmartSender smart;

	/****************************
	 * session״̬����--����
	 */
	UNaviListable ready_link;
	UNaviList ready_flows;

	void set_ready_iflow(URtmfpIflow* flow);
	void iflow_msgs_empty(URtmfpIflow* flow);

	//�����������ͬʱclose�����ڵȴ�close ack�ڼ䣬�յ��Զ˵�close��
	//��ʱ����close ack��ͬʱ������timeout״̬��
	//����timeout״̬ʱ���յ�close ����������close ack���յ�close ack��������
	SessionStage stage;

	ChunkType prev_chunk;
	URtmfpIflow* prev_iflow;
	uint64_t prev_iflow_seq;
	uint64_t prev_fsn_off;

	URtmfpPeerId *peer_id;
};

struct URtmfpPeerReady
{
	URtmfpPeerReady():
		ready_impl(NULL)
	{}
	virtual ~URtmfpPeerReady() {
		if(ready_impl)
			ready_impl->frame = NULL;
	}

	void set_idle_limit(uint64_t timeout_mc)
	{
		ready_impl->set_idle_limit(timeout_mc);
	}

	SendStatus send_msg(uint32_t flowid,const uint8_t* raw,uint32_t size, int32_t to_limit = -1,
		bool is_last=false, const FlowOptsArg* opts=NULL)
	{
		return ready_impl->send_msg(flowid, raw, size ,to_limit,is_last,opts);
	}

	bool send_would_overwhelm(uint32_t oflowid)
	{
		return ready_impl->send_would_overwhelm(oflowid);
	}

	ReadGotten read_msg(uint32_t iflow)
	{
		return ready_impl->read_msg(iflow);
	}

	void release_input_msg(URtmfpMsg* msg)
	{
		ready_impl->release_input_msg(msg);
	}

	void reject_input_flow(uint32_t iflow, uint64_t code)
	{
		URtmfpIflow* flow = ready_impl->have_input_flow(iflow);
		if (flow) ready_impl->reject_input_flow(flow, code);
	}

	void user_ping(const uint8_t* ping_raw, uint32_t sz, uint64_t timeout=0)
	{
		ready_impl->user_ping(ping_raw,sz,timeout);
	}

	void close()
	{
		if ( ready_impl ) {
			if ( ready_impl->stage == SESSION_ALIVE) {
				ready_impl->frame = NULL;
				ready_impl->close();
			}
			ready_impl = NULL;
		}
	}

	int64_t get_outflow_relating(uint32_t flowid); //outflow����������inflow��id��-1��ʾû��
	int64_t get_inflow_relating(uint32_t flowid); //inflow����������outflow��id��-1��ʾû��
	void get_outflow_related(uint32_t flowid,std::vector<uint32_t>& inflows);
	void get_inflow_related(uint32_t flowid, std::vector<uint32_t>& outflows);
	bool have_outflow_related(uint32_t oflowid, uint32_t iflowid);
	bool have_inflow_related(uint32_t iflowid, uint32_t oflowid);

	uint32_t get_new_outflow(const FlowOptsArg* opts);

	URtmfpPeerId* rtmfp_peer_id() {
		if ( ready_impl )
			return ready_impl->peer_id;
		return NULL;
	}

	virtual void idle_timedout(){}
	virtual void broken(URtmfpError broken_reason){}
	virtual void send_timedout(uint32_t flowid){}
	virtual void writable(uint32_t oflow){}
	virtual void outflow_rejected(uint32_t oflow, uint64_t code){}
	//��iflow��readable֮ǰ�����á����iflow��oflow�Ĺ����Ļ���
	virtual void outflow_related(uint32_t oflowid, uint32_t iflowid){}

	virtual void readable(uint32_t iflow){}

	virtual void user_ping_relayed(const uint8_t* relay, uint32_t sz){}
	virtual void user_ping_timedout(const uint8_t* ping_raw, uint32_t sz){}
	virtual void peer_closed(){}

protected:
	friend class URtmfpProto;
	friend class URtmfpDebugProto;
	friend class URtmfpSession;
	void unbind_ready_impl()
	{
		if(ready_impl) {
			ready_impl->frame = NULL;
			ready_impl = NULL;
		}
	}
	void set_ready_impl(URtmfpSession* session)
	{
		ready_impl = session;
		session->frame = this;
	}
	URtmfpSession* ready_impl;
};
URTMFP_NMSP_END

#endif /* URTMFPSESSION_H_ */
