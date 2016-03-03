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
 * ����ĳ������udp�Ĵ���Э������ӡ��������ࡣ
 * ����Ϣ�շ�������ӿڣ�������Ҫʵ����Щ�ӿڣ�ʵ�ֵķ�ʽ��ͬʱ��̳�
 * �²㴫���Proto��������ؽӿ��࣬ʵ�����е���ӿڲ��֡�
 * UOMBPeer����ӿڵ��ô����ӿ���ķ��麯��������ʵ���书�ܡ�UOMBPeer�������ṩ���ϲ�Ľӿڵ�����Щ
 * UOMBPeer��ʵ����ӿڡ�
 *
 * �̳��Դ����Ľӿ������ӿڲ��֣����ǵ�ʵ����Ҫ�ǵ���UOMBPeer�ṩ������㴥���ķ���ӿڡ���UOMBPeer��
 * ��Щ���������ӿڣ��򴥷�UOMBObj�������ӿڡ�
 *
 * Peer����channel�ĸ������channelרΪһ��proxy<->servant�Զ�����
 *
 * Peer�����������ַ�ʽ��
 * 1������ʽ�� ��ʼʱ���������ܵ��������Ӳ��������������Ӷ����ڽ��յ�������Ϣ֮�󣬸���ǩ��
 * 	�Ӷ�Ӧ��service�����µ�UOMBPeer������󣬺ʹ�������Ӷ���󶨡���һ������һ��������Ϣǩ��
 * 	�ᣬͨ����peer����service�������Ӧ��servant���࣬��servant����͸�peer��
 * 2������ʽ����ʼʱ��ҵ�񿪷��ߴ�ĳ��service��������һ��Proxy��������һ���յĶ�Ӧ���͵�peer
 * 	�󶨣���peer��û�кʹ���㽨����ϵ����ҵ�񿪷��߷����proxy�����ϵ�Զ�̶������ʱ���ڴ����
 * 	��������ʵ������󶨣�����������ӽ����� ��pending����Ϣ���ͳ�ȥ��
 *
 * Peer�ڴ���֮�����������𷽿϶���һ��proxy���������ն˿϶���һ��servant��
 * ��������Peer���ˣ�servant/proxy�������ⷢ��
 */


class UOMBPeerIdntfr;
class UOMBPeerApp;
class UOMBPeer : public UNaviListable
{
public:
	enum SendStatus
	{
		SEND_SUCC,
		SEND_SHOULD_SLOW_DOWN, //��ʾӦ�ò㽵�ͷ���Ƶ�ʡ�
		SEND_OVERWHELMED, //����Ƶ��̫�죬�ײ㴫���Э�鲻�ܱ�֤��Ϣ����������
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
	 * ����㴥�����õĽӿڲ��֣��ᴥ��peer���󶨵�ORObj����ش���
	 */
	void notify_idle_timedout();
	void notify_connect_failed();
	void notify_broken();
	void notify_peer_closed();
	//����㴥��ʵ��Peer�����connected�ӿڣ������з�װpeer id��Ȼ����øýӿ�
	//����peer id���뷢�����ӵ�Idntfr��һ����ͬ���ȷ�������ʱ�Ĳ�����Ȩ�����ܾ�ȷƥ��Զˡ�
	void notify_peer_connected();

	/*
	 * ���µ��麯��Ҫ��Peer����ʵ�֣����ö�Ӧ�Ĵ����ӿ�ʵ�֡�
	 */
	//send_msg���ݵײ㴫���Э��Ĳ�ͬ������ͬ��������rtmfpЭ��Ϊ����channel��Ӧ��flow��signature��Ӧ��
	//flow��metadata�����ԣ��˴�Ϊ�麯����
	virtual SendStatus send_msg_proto(const uint8_t* raw, uint32_t sz, UOChannel channel/*object request channel*/,
		uint64_t to_ml, bool is_last) = 0;

	virtual UOChannel new_out_channel_proto(const UOMBSign* or_sign) = 0;
	virtual void set_idle_timeout_proto(uint64_t to_ml) = 0;
	virtual void connect_peer_proto(UOMBPeerIdntfr* idntr, uint64_t to_ml) = 0;
	virtual void close_proto() = 0;

	//��������ֺ����Ŀհ����ӣ���û���κ���Ϣ֮ǰ����Ӧ�ò�û���κ����塣��ʼ��Ϣ��Я����signature
	//��Ϣ�����˴���ʲô���͵�Peer,������peer�ɶ�Ӧ��sign��ǵ�svc��������Ϣ�����ñ�������peer���д���
	// �´�����peer�ĵ�һ��Obj�϶���Servant���󣬴�svc������͸�peer�󶨵�servant����
	// sign�����˴�����servant�����͡�
	// �����Ѿ����ڵ�peer�ϵ���Ϣ�� ���channel֮ǰ�Ѿ����ڣ���ô��channel��Ӧ���Ǹ�ORObj����Ϣ��
	// ���֮ǰchannelδ�����֣���ô��������Ϣ������sign, ���û�ж�Ӧ��ORObj����ô��svc����sign����
	// ��Ӧ���͵�servant�������sign��ӦORObj���ڣ��Ǹ�ORObj�Զ����µ�channel����ORObj������
	// Servant����proxy����
	// ��Щ����õ�ORObj����֪����δ���ԭʼ��Ϣ��peer��һ��֪��ԭʼ��Ϣ�Ľ������(��flash peerΪ����NetStream/NetConnection
	// �Ľ��������NetGroup�Ĺ�����ͬ)
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

	//��service�ﾳ�˳�����ɾ�����󡣴����ײ�proto��ʵ�����Ӷ���Ĺرա�
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

//��ͬ��svc�����ӶԶ˱�ǲ�����ͬ����Ҫ�����Լ������ࡣ
//��flash peerΪ����boot_addr��Э�����ֵķ���˵ĵ�ַ������Ҫ�����EDP���֡�
//��UNavi JSON ORBΪ����
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
