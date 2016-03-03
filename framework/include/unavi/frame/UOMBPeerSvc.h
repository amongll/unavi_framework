/*
 * UNaviService.h
 *
 *  Created on: 2014-11-26
 *      Author: li.lei
 */

#ifndef UNAVIORBSERVICE_H_
#define UNAVIORBSERVICE_H_

#include "unavi/frame/UOMBCommon.h"
#include "unavi/util/UNaviListable.h"
#include "unavi/core/UNaviProtoDeclare.h"
#include "unavi/util/UNaviRef.h"
#include "unavi/core/UNaviLog.h"
#include "unavi/frame/UOMBSign.h"

#include <typeinfo>
#include <list>

UNAVI_NMSP_BEGIN
/*
 * OMB: Object Message Broker。 对象消息中间层框架。远程对象间通过单向OMB Cmd消息进行交互。
 *
 * OMB service:
 *
 * OMB Peer
 * 	某UDP可靠传输协议的网络连接。有消息的收发相关接口。隔离真实可靠传输底层协议的实现。
 * OMB Peer App:
 * 	在某个OMB Peer两端，有client<-->server 应用语境对象。 此处的client只是说它是初始发起交互的一端
 * 	Peer App的子类实例管理它自己以及它所拥有的OMB object的生存期。
 * 	客户端应用，主动发起某个类型的peer app， 框架根据其发起时使用的类型签名，为其创建相对应的peer传输实体。
 * 	服务端应用，根据peer上的初始消息的签名，创建对应类型的peer app。
 * OMB Object
 *  远程对象抽象。分为proxy/servant两类。 属于某个PeerApp。PeerApp下，可以主动发起多个proxy，也会应对端proxy的请求在本段创建
 *  响应的servant。 两端发出的任何完整消息，都看作是对对端对象的一次方法调用。
 * OMB servant object
 *  远程消息交互的初始被动接收端。
 * OMB proxy object
 *  远程消息交互的对象对儿的初始发起端。
 * OMB peer Channel
 *  peer连接之下的子传输通道， 单向， 某个proxy<-->servant对象对儿的专属传输通道。
 *
 * OMB peer service的责任：
 * * client端应用发起的PeerApp， 需要OMBService负责peer的建立等工作
 * * 初始被动接受的peer连接，为其建立对应的server端PeerApp对象。
 * * 是应用的proxy/servant对象的工厂类
 * * 是peer的工厂类。peer根据service使用的不同的底层传输协议而有不同的类型。、
 *
 * * 通过继承UOMBSvc，可以创建如下服务： UFlashSvc，UJSORSvc（基于JSON交互的远程对象调用）, UFlashP2P服务
 * * unavi svc子类对象在每个UOMBWorker上有一个实例，该实例还有在该worker上建立的peer的管理维护的职责。
 *
 * * 每个unavi svc子类，需要声明它所基于的udp 传输协议的协议id，例如rtmfp proto的id。还需要：
 * * *　声明service的签名，proto底层根据签名来访问对应的service，再根据签名来要求service创建不同的servant对象。
 * * * service/servant/proxy均有自己的签名，作为类型标记。以rtmfp协议之上的flash交互为例，以rtmfp flow上的user metadata
 * * * 选项存储签名，每个flow的第一条cmd到达时，根据metadata中的签名，即可知道是NetConnection/NetStream/NetGroup下的cmd，
 * * * 再根据命令名成调用其处理函数。
 *
 * * peer有通讯子通道的概念，channel，借鉴于rtmfp flow，初始经过签名确定service/proxy时，其使用的channel就固定服务于对应的
 * servant/proxy。
 *
 * * 各service子类的servant/proxy之间的object request如何转化为消息内容，以及需要什么其他的安排(例如rtmfp flash需要对flow
 * * 的选项进行安排)，是需要子类具体实现的。
 */

class UOMBServant;
class UOMBProxy;
class UOMBPeerShadow;
class UOMBPeerApp;
class UOMBPeer;
class UOMBPeerIdntfr;

class UOMBPeerSvc
{
public:
	//UOMBSvcDeclare模版类，强制要求UOMBSvc的子类声明定义该静态函数。一般地，使用UOMBObj::regist_cmd模版静态成员
	//函数声明各命令的处理成员函数。
	static void declare_cmds() {}

	UOMBPeerSvc():
		proto_id(0xff)
	{}

	uint8_t get_proto_id()const
	{
		return proto_id;
	}

	virtual UOMBPeerSvc* dup()const = 0;
	virtual void run_init() = 0;
	virtual void clean_up() = 0;

	//传输层连接建立后，收到初始消息时，通过其中的签名找到svc，然后调用该方法，
	//获得连接对应于应用层的peer实体。
	//svc子类可以在该接口内部完成其对于连接的管理行工作。
	virtual UOMBPeer* new_server_peer(const UOMBPeerIdntfr& id) = 0;
	//主动连接对象工厂方法。svc子类可以用此来完成连接管理性的工作。
	virtual UOMBPeer* new_client_peer(const UOMBPeerIdntfr& idfr) = 0;

	virtual void quit_peer(const UOMBPeerIdntfr& id) = 0;

	//用于创建连接应用服务端对象
	virtual UOMBPeerApp* get_new_app(bool server) = 0;

	typedef void (UOMBPeerSvc::*AppNotifier)(const UOMBPeerShadow&);

	template<class SubClass>
	static void regist_app_arrived_notifier(void (SubClass::*method)(const UOMBPeerShadow&));

	template<class SubClass>
	static void regist_app_destroy_notifier(void (SubClass::*method)(const UOMBPeerShadow&));

	void call_destroy_notifier(UOMBPeerApp* app);
	void call_arrive_notifier(UOMBPeerApp* app);

	static UOMBPeerApp* start_up_server(uint8_t proto_id,const UOMBPeerIdntfr& id);

	static void init_svc_map();
	static void clean_svc_map();

	typedef UNaviRef<UOMBPeerSvc> Ref;
	friend class UNaviRef<UOMBPeerSvc>;
	static UOMBPeerSvc* service_declares[256];

	//从线程专有存储中获得svc实例。
	static UOMBPeerSvc* get_svc(uint8_t proto_id, bool need_thr=true);

	UOMBPeerSvc(const UOMBPeerSvc& r){}
	UOMBPeerSvc& operator=(const UOMBPeerSvc& r)
	{
		return *this;
	}

	void regist_peer(UOMBPeerApp* peer_app);
	uint8_t proto_id;
protected:
	//经由某个SVC实例创建的UOMBPeerApp实例的链表。
	UNaviList peers;
	virtual ~UOMBPeerSvc();

private:
	static pthread_once_t svc_map_once;
	static pthread_key_t svc_map_key;
	static void key_once(void);

	static hash_map<const char*, AppNotifier> server_app_arrive_notifiers;
	static hash_map<const char*, AppNotifier> app_destroy_notifiers;
};

template<class SubClass>
void UOMBPeerSvc::regist_app_arrived_notifier(void (SubClass::*method)(const UOMBPeerShadow&))
{
	server_app_arrive_notifiers[typeid(SubClass).name()] = static_cast<AppNotifier>(method);
}

template<class SubClass>
void UOMBPeerSvc::regist_app_destroy_notifier(void (SubClass::*method)(const UOMBPeerShadow&))
{
	app_destroy_notifiers[typeid(SubClass).name()] = static_cast<AppNotifier>(method);
}

//线程专有存储，存储线程下各proto下的各svc实例。
//采用智能指针，使得各svc在线程退出时自动回收。
struct UOMBSvcMap
{
	UOMBPeerSvc::Ref proto_map[256];
	void clean_up_svcs();
};

template<class SvcClass>
class UOMBSvcDeclare
{
public:
	typedef SvcClass SvcType;;
	static bool declare(uint8_t proto);
	/*
	 * 使用该接口获得线程专有的SvcClass实例。
	 * 在没有建立过连接的对端上主动发起初始的proxy时，第一个参数可以用该use()调用获得。
	 */
	static UOMBPeerSvc* use(bool thr_spec=true);
	static uint8_t proto_id;
	static SvcClass obj;
};

template<class SvcClass>
uint8_t UOMBSvcDeclare<SvcClass>::proto_id = 255;

template<class SvcClass>
SvcClass UOMBSvcDeclare<SvcClass>::obj;

template<class SvcClass>
UOMBPeerSvc* UOMBSvcDeclare<SvcClass>::use(bool thr_spec)
{
	return UOMBPeerSvc::get_svc(proto_id,  thr_spec);
}

template<class SvcClass>
bool UOMBSvcDeclare<SvcClass>::declare(uint8_t proto)
{
	UOMBPeerSvc*& pglobal = UOMBPeerSvc::service_declares[proto];
	if (pglobal)
		return false;
	proto_id = proto;
	obj.proto_id = proto_id;
	SvcClass::declare_cmds(); //强制SvcClass必须实现declare_cmds静态函数。
	pglobal = &obj;
}

UNAVI_NMSP_END

#endif /* UNAVISERVICE_H_ */
