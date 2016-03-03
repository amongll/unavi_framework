/*
 * UOMBPeer.h
 *
 *  Created on: 2014-11-26
 *      Author: li.lei
 */

#ifndef UOMBPEER_H_
#define UOMBPEER_H_

#include "unavi/frame/UOMBCommon.h"
#include "unavi/frame/UOMBException.h"
#include "unavi/frame/UOMBObj.h"
#include "unavi/core/UNaviWorker.h"

UNAVI_NMSP_BEGIN

/*
 * 代表某个基于udp的传输协议的连接。公共虚类。
 * 有消息收发处理虚接口，子类需要实现这些接口，实现的方式是同时多继承
 * 下层传输层Proto的连接相关接口类，实现其中的虚接口部分。
 * UOMBPeer的虚接口调用传输层接口类的非虚函数部分来实现其功能。UOMBPeer的其他提供给上层的接口调用这些
 * UOMBPeer的实现虚接口。
 *
 * 继承自传输层的接口类的虚接口部分，它们的实现主要是调用UOMBPeer提供给传输层触发的非虚接口。而UOMBPeer的
 * 这些被动触发接口，则触发UOMBObj的相关虚接口。
 *
 * Peer下有channel的概念，若干channel专为一个proxy<->servant对儿服务。
 *
 * Peer被创建的两种方式：
 * 1：被动式。 初始时，传输层接受到被动连接并创建传输层的连接对象，在接收到首条消息之后，根据签名
 * 	从对应的service申请新的UOMBPeer子类对象，和传输层连接对象绑定。下一步，进一步根据消息签名
 * 	会，通过该peer，从service中申请对应的servant子类，该servant子类和该peer绑定
 * 2：主动式。初始时，业务开发者从某个service主动申请一个Proxy对象，它和一个空的对应类型的peer
 * 	绑定，该peer还没有和传输层建立联系。当业务开发者发起该proxy对象上的远程对象调用时，在传输层
 * 	创建连接实例与其绑定，待传输层连接建立后， 把pending的消息发送出去。
 *
 * Peer在创建之初，主动发起方肯定有一个proxy，被动接收端肯定有一个servant。
 * 后续，在Peer两端，servant/proxy可以任意发起。
 */


class UOMBPeerIdntfr;
class UOMBPeerApp;
class UOMBPeer : public UNaviListable
{
public:
	enum SendStatus
	{
		SEND_SUCC,
		SEND_SHOULD_SLOW_DOWN, //提示应用层降低发送频率。
		SEND_OVERWHELMED, //发送频率太快，底层传输层协议不能保证消息的完整发送
		SEND_FAILED
	};

	void peer_connected(const UOMBPeerIdntfr* id);

	UOMBObj* ichannel_relating_obj(UIChannel ch, bool &is_closed_obj);
	UOMBObj* ochannel_relating_obj(UOChannel ch, bool &is_closed_obj);

	const UOMBPeerIdntfr* get_peer_id()const;

	void bind_app(UOMBPeerApp* app_obj);
protected:
	friend class UOMBPeerApp;
	UOMBPeerApp* app;
	UOMBPeer();
	virtual ~UOMBPeer();


	/*
	 * 传输层触发调用的接口部分，会触发peer所绑定的ORObj的相关处理。
	 */
	void notify_idle_timedout();
	void notify_connect_failed();
	void notify_broken();
	void notify_peer_closed();
	//传输层触发实际Peer子类的connected接口，在其中封装peer id，然后调用该接口
	//设置peer id。与发起连接的Idntfr不一定相同，比发起连接时的参数更权威，能精确匹配对端。
	void notify_peer_connected();

	/*
	 * 如下的虚函数要求Peer子类实现，调用对应的传输层接口实现。
	 */
	//send_msg根据底层传输层协议的不同，而不同。例如以rtmfp协议为例，channel对应于flow，signature对应于
	//flow的metadata。所以，此处为虚函数。
	virtual SendStatus send_msg_proto(const uint8_t* raw, uint32_t sz, UOChannel channel/*object request channel*/,
		uint64_t to_ml, bool is_last) = 0;

	virtual UOChannel new_out_channel_proto(const UOMBSign* or_sign) = 0;
	virtual void set_idle_timeout_proto(uint64_t to_ml) = 0;
	virtual void connect_peer_proto(UOMBPeerIdntfr* idntr, uint64_t to_ml) = 0;
	virtual void close_proto() = 0;

	//传输层握手后建立的空白连接，在没有任何消息之前，对应用层没有任何意义。初始消息中携带的signature
	//信息决定了创建什么类型的Peer,该类型peer由对应的sign标记的svc创建。消息交给该被创建的peer进行处理。
	// 新创建的peer的第一个Obj肯定是Servant对象，从svc中申请和该peer绑定的servant对象，
	// sign决定了创建的servant的类型。
	// 对于已经存在的peer上的消息， 如果channel之前已经存在，那么是channel对应的那个ORObj的消息。
	// 如果之前channel未曾出现，那么传输层的消息必须有sign, 如果没有对应的ORObj，那么从svc根据sign申请
	// 对应类型的servant对象。如果sign对应ORObj存在，是该ORObj对儿上新的channel，本ORObj可能是
	// Servant或者proxy对象。
	// 这些被获得的ORObj对象知晓如何处理原始消息。peer不一定知晓原始消息的解码规则(以flash peer为例，NetStream/NetConnection
	// 的解码规则与NetGroup的规则不相同)
	void process_msg(const uint8_t* raw, uint32_t raw_sz, UIChannel channel,
		const UOMBSign* or_signature = NULL);

private:
	uint64_t conn_to_ml;
	uint64_t idle_to_ml;

	friend class UOMBObj;
	void connect_peer(UOMBPeerIdntfr* id);

	SendStatus send_msg(UOMBObj& obj, const uint8_t* raw, uint32_t sz, UOChannel ochannel, uint64_t send_to = 0, bool is_last = false);
	std::pair<UOChannel,SendStatus> send_msg_with_newchannel(UOMBObj& obj, const uint8_t* raw, uint32_t sz,
		uint64_t send_to = 0, bool is_last = false);

	//从service语境退出。并删除对象。触发底层proto的实体连接对象的关闭。
	void close_peer();

	void _close_peer(bool recycle);

	void set_ichannel_idle_limit(UIChannel ich, uint64_t to_ml);

	void unregist_obj(UOMBObj& obj);

	struct IChannelIdleChecker : public UNaviEvent
	{
		typedef UNaviRef<IChannelIdleChecker> Ref;
		UOMBPeer& target;
		UIChannel id;
		IChannelIdleChecker(UIChannel ich, UOMBPeer& peer):
			UNaviEvent(),
			target(peer),
			id(ich)
		{
			UNaviWorker::regist_event(*this);
		}
		virtual void process_event();
	};
	friend class IChannelIdleChecker;
	hash_map<UIChannel, IChannelIdleChecker::Ref> ich_idle_checkers;

	void _peer_ich_idle_exception(UIChannel ich);

	struct IChannelInfo
	{
		IChannelInfo();
		UOMBObj* object;
		uint32_t status; //UOMBPeer::IChannelErrCode bitmask
	};
	hash_map<UIChannel, IChannelInfo> postive_chn_idx;

	UOMBObj* last_send_obj;
	UOChannel last_send_ch;

	struct OChannelInfo
	{
		OChannelInfo();
		UOMBObj* object;
		uint32_t status; //UOMBPeer::OChannelErrCode bitmask
	};
	hash_map<UOChannel, OChannelInfo> active_chn_check;
};

//不同的svc的连接对端标记不尽相同。需要创建自己的子类。
//以flash peer为例，boot_addr是协助握手的服务端的地址，还需要额外的EDP部分。
//以UNavi JSON ORB为例，
struct UOMBPeerIdntfr
{
	UOMBPeerIdntfr(const sockaddr* boot_addr = NULL)
	{
		if(!boot_addr) return;
		if (boot_addr->sa_family == AF_INET)
			memcpy(&boot_in_addr,boot_addr,sizeof(sockaddr_in));
		else if (boot_addr->sa_family == AF_INET6)
			memcpy(&boot_in6_addr,boot_addr,sizeof(sockaddr_in6));
	}
	virtual ~UOMBPeerIdntfr(){};

	union {
		sockaddr boot_sa_addr;
		sockaddr_in boot_in_addr;
		sockaddr_in6 boot_in6_addr;
	};

	virtual UOMBPeerIdntfr* dup()const = 0;
	virtual size_t hash()const = 0;
	virtual bool equal(const UOMBPeerIdntfr& rh)const = 0;
	virtual void print(std::ostream& oss)const = 0;
};

UNAVI_NMSP_END

namespace std {
	template <>
	struct equal_to<unavi::UOMBPeerIdntfr*>
	{
		bool
		operator()(unavi::UOMBPeerIdntfr* const __x, unavi::UOMBPeerIdntfr* const __y) const
		{ return __x->equal(*__y); }
	};

	template <>
	struct equal_to<const unavi::UOMBPeerIdntfr*>
	{
		bool
		operator()(const unavi::UOMBPeerIdntfr* const __x, const unavi::UOMBPeerIdntfr* const __y) const
		{ return __x->equal(*__y); }
	};
}

namespace __gnu_cxx {
	template<>
	struct hash<const unavi::UOMBPeerIdntfr*>
	{
		size_t operator()(const unavi::UOMBPeerIdntfr* const obj) const
		{
			return obj->hash();
		}
	};

	template<>
	struct hash<unavi::UOMBPeerIdntfr*>
	{
		size_t operator()(unavi::UOMBPeerIdntfr* const obj) const
		{
			return obj->hash();
		}
	};
}

#endif /* UNAVIPEER_H_ */
