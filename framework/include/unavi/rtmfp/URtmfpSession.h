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
	//在BUFFERED时，表示发送缓冲区已满，消息放在等待队列中。
	//上层应该降低发送频率，或者暂停发送进度
	SEND_BUFFERED,
	SEND_SUCC,
	//pending msg缓冲区过大，会清理onflybuf中的未经确认的消息
	//在返回overwhelmed时，表示本端不确保早先的消息对端是收到的。
	SEND_OVERWHELMED,
	SEND_DENYED,//一般是flow已经结束，或者对端已经flow excp该flow
	SEND_ON_BROKEN_SESSION,
	SEND_INFRA_ERR//底层基础设施有错误
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
	PENDING_SEND_CLOSING, //todo: 有pending数据已发出，且未经确认，但是上层以发起关闭。
	CLOSE_ACK_WAITING, //主动关闭端的状态，最多等待ack 20s钟
	CLOSE_TIMEOUT, //被动关闭端的状态，等待对端重发的close chunk，维持20s钟
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

	URtmfpPeerReady* frame; //frame对象可能先于session对象被创建，frame对象肯定先于session对象被销毁

	//设置session无交互时间限，超过指定限制，session自动关闭。
	//交互不包括本端自动发起的sys ping。
	void set_idle_limit(uint64_t timeout_mc);
	//在没有发送数据时，没有收到对端任何flow消息超过时限时，被调用
	void idle_timedout();
	//在只有出向包，没有来向包多长时间之后，被调用。默认采用30s的限制。
	//或者对端接收到了非法的packet
	void broken(URtmfpError broken_reason);

	bool send_would_overwhelm(uint32_t oflowid);
	void send_timedout(uint32_t flowid); //有数据发出，但是无对端ack超时后，被调用
	//在oflow从写缓冲区满重新变为可写时，被调用。
	//在writeable中调用的第一个semd_msg肯定能被接受
	void writable(URtmfpOflow* flow);
	void outflow_rejected(URtmfpOflow* flow, uint64_t code);
	void outflow_related(URtmfpOflow* oflow, URtmfpIflow* iflow);

	void readable(URtmfpIflow* flow);//在in flow有消息可用时，被调用。所有session的readable被公平地调用。
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

	const uint32_t far_ssid; //对端维护
	const uint32_t near_ssid; //本端维护
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
	 *如果不设置to_limit，那么以默认的指数退避算法进行重传，如果超过重传
	 *退避次数，认为超时。如果设置了超时，那么该消息以设置值作为超时时刻
	 *如果设置的超时时刻小于当前估算的rtt，那么采用当前rtt值。
	 *如果is_last为真，那么data chunk将会携带finish标记
	 *
	 *对返回值为BUFFERED时，建议降低消息发送的频率。
	 *如果返回值是OVERWHELMED时，说明为了本消息的发送，最早在发送缓冲中的消息
	 *被丢弃，不可能再有重传的机会，也就是说不能保证部分消息被对端接收。
	 *一般在流式推送模式下，在buffered时，适当降低发送频率，但是在推送进度
	 *落后过多时，可以适当强制发送
	 *****************/
	SendStatus send_msg(uint32_t flowid,const uint8_t* raw,uint32_t size, int32_t to_limit = -1,
		bool is_last=false, const FlowOptsArg* opts=NULL);

	//清理flow/ping语境，清理frame对象
	void cleanup();

	friend class URtmfpPeerReady;
	union {
		sockaddr sa_peer_addr;
		sockaddr_in in_peer_addr;
		sockaddr_in6 in6_peer_addr;
	};

	URtmfpIflow* cur_readable; //加速inflow的查找过程 上层每次切换flow进行read msg时，会进行一次二分查找
	URtmfpOflow* cur_writable; //加速outflow的查找过程 上层每次切换flow进行send msg时，会进行一次二分查找

	friend class URtmfpOflow;
	friend class URtmfpIflow;
	//chunk是触发rtmfppacket输出的chunk，检查chunk是否
	//能让入一个udp包，不能的话，会自动建立多个fragment packet
	//不能使data chunk和next data chunk
	//该方法用于发送ping,ack,probe_buf等非data/next data的chunk
	void send_sys_chunk(URtmfpChunk& chunk);
	//该方法用于msg chunk的重发，与send_msg的逻辑有一定的差别，复杂逻辑主要包括
	//退避算法以及fsn的前移
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
	uint64_t last_active_tm; //无论出入向，有数据负载的包的发生时刻，或者ack接收的时刻。或者有出向的用户ping/来向ping reply的时刻。
	uint64_t last_in_tm;//无论什么类型，有来向包的最近时刻。
	uint64_t last_out_tm;//最近一次的data/next data/buffer probe/ping包的时刻，这些包均期望对端的反馈

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
	 * 主动发起的ping。包括无
	 * 	1： 数据交互时自动的ping
	 * 	2： 应用层主动发起的ping
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

		int32_t id; //user ping使用正直，sys ping使用负值
		uint8_t* ping_raw;
		uint32_t sz;
		URtmfpSession* target;
	};

	typedef std::map<int32_t, Ping*>::iterator PingIter;
	std::map<int32_t, Ping*> user_pings;

	int32_t sys_pingid;
	int32_t user_pingid;

	/*********************************
	 * RTT计算相关
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
	 *	session接收的缓冲区管理及ack管理
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

	//定时的ack计算和ack发送分开的目的：
	//在定时计算ack之时，可能有新的udp flow msg的输入
	//这些发生在同一次cycle循环中，可能使得计算的ack不能
	//准确反映接收缓冲区的状态
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

	//如果出向包可以携带ack，且空间足够包含所有ack内容，则发送ack，不要发送不完整
	//反映本地缓冲空间情况的ack，否则会激起对端重发已发送的包。
	//在每次收到新的包之后，触发idle_ack_timer
	//在每次收到重发的包之后，触发emerge_ack_timer, 因为本端的ack晚于重发包超时时刻或者ack丢失
	//尽快发出ack，避免对端仍旧重复发送
	//在每次发送ack时，取消这两个timer。

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

	//在每次发送rtmfp packet时，看是否能piggy back ack内容
	//piggy back时，不对pending_ack进行重新计算，因为发送
	//可能在任何时刻发生，而ack的计算不能频繁的进行
	//piggy_back的ack内容因此会有一定的状态延迟，但是在可接受范围
	void try_piggy_ack(URtmfpPacket& out);
	UNaviList pending_ack;//URangeAckChunk或者UBitmapAckChunk

	AckPrepareTimer emerg_ack_timer;//收到重发包，以及其他需要尽快发出ack的定时器。 10 milli sec
	AckPrepareTimer idle_ack_timer;//没有出向包时的ack定时器. 200milli sec
	AckSmartSender ack_smart_sender;//单独发出ack。如果有其他出向的rtmfpPacket时，看空间是否足够，若足够，则携带发送ack

	static const int Idle_ack_wait = 99000;//micro sec
	static const int Emerg_ack_wait = 4000;
	static const int Send_ack_wait = 1000;

	/******************************************
	 * 发送端的管理以及定时重传机制,ack的接收处理
	 *****************************************/

	//发送时复用rtmfp packet的辅助信息
	struct LastDataInfo {
		LastDataInfo():
			flow_id(-1),
			fsn(0),
			resend_seqid(0)
		{}
		int64_t flow_id; //为-1时，表示前一个chunk不是data chunk或者next data chunk
		uint64_t fsn;
		uint64_t resend_seqid;
	};
	LastDataInfo last_data_chunk;
	std::vector<uint32_t> cur_packet_flows; //当前packet容纳的oflow的id

	//主要用于多个chunk复用packet进行发送
	URtmfpPacket out_packet;
	bool send_pending;

#ifdef RTMFP_SESSION_DEBUG
	void send_packet_trace(URtmfpPacket &opkt,bool piggy_ack,const char* fmt,...);
#endif
	void send_packet(URtmfpPacket &opkt,bool piggy_ack=true);

	void update_peer_addr(const sockaddr& addr);

	//如果有一个接近于mtu的rtmfp包出现，则立即发出，
	//否则借助于该smart event进行发送
	//独立于业务过程、其他定时处理机制，进行实际的包的发送。
	//考虑如下情形：在业务处理过程
	//或者其他定时过程中，发送了大量的小chunk，不需要
	//立即以一个udp包发出，而是复用同一个packet。
	//该过程紧跟其他过程之后发生。
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
	 * session状态管理--其他
	 */
	UNaviListable ready_link;
	UNaviList ready_flows;

	void set_ready_iflow(URtmfpIflow* flow);
	void iflow_msgs_empty(URtmfpIflow* flow);

	//如果发生两端同时close，即在等待close ack期间，收到对端的close，
	//此时发送close ack的同时，进入timeout状态。
	//进入timeout状态时，收到close ，立即反馈close ack。收到close ack不做处理
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

	int64_t get_outflow_relating(uint32_t flowid); //outflow主动关联的inflow的id，-1表示没有
	int64_t get_inflow_relating(uint32_t flowid); //inflow主动关联的outflow的id，-1表示没有
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
	//在iflow的readable之前被调用。如果iflow是oflow的关联的话。
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
