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
 * OMB: Object Message Broker�� ������Ϣ�м���ܡ�Զ�̶����ͨ������OMB Cmd��Ϣ���н�����
 *
 * OMB service:
 *
 * OMB Peer
 * 	ĳUDP�ɿ�����Э����������ӡ�����Ϣ���շ���ؽӿڡ�������ʵ�ɿ�����ײ�Э���ʵ�֡�
 * OMB Peer App:
 * 	��ĳ��OMB Peer���ˣ���client<-->server Ӧ���ﾳ���� �˴���clientֻ��˵���ǳ�ʼ���𽻻���һ��
 * 	Peer App������ʵ���������Լ��Լ�����ӵ�е�OMB object�������ڡ�
 * 	�ͻ���Ӧ�ã���������ĳ�����͵�peer app�� ��ܸ����䷢��ʱʹ�õ�����ǩ����Ϊ�䴴�����Ӧ��peer����ʵ�塣
 * 	�����Ӧ�ã�����peer�ϵĳ�ʼ��Ϣ��ǩ����������Ӧ���͵�peer app��
 * OMB Object
 *  Զ�̶�����󡣷�Ϊproxy/servant���ࡣ ����ĳ��PeerApp��PeerApp�£���������������proxy��Ҳ��Ӧ�Զ�proxy�������ڱ��δ���
 *  ��Ӧ��servant�� ���˷������κ�������Ϣ���������ǶԶԶ˶����һ�η������á�
 * OMB servant object
 *  Զ����Ϣ�����ĳ�ʼ�������նˡ�
 * OMB proxy object
 *  Զ����Ϣ�����Ķ���Զ��ĳ�ʼ����ˡ�
 * OMB peer Channel
 *  peer����֮�µ��Ӵ���ͨ���� ���� ĳ��proxy<-->servant����Զ���ר������ͨ����
 *
 * OMB peer service�����Σ�
 * * client��Ӧ�÷����PeerApp�� ��ҪOMBService����peer�Ľ����ȹ���
 * * ��ʼ�������ܵ�peer���ӣ�Ϊ�佨����Ӧ��server��PeerApp����
 * * ��Ӧ�õ�proxy/servant����Ĺ�����
 * * ��peer�Ĺ����ࡣpeer����serviceʹ�õĲ�ͬ�ĵײ㴫��Э����в�ͬ�����͡���
 *
 * * ͨ���̳�UOMBSvc�����Դ������·��� UFlashSvc��UJSORSvc������JSON������Զ�̶�����ã�, UFlashP2P����
 * * unavi svc���������ÿ��UOMBWorker����һ��ʵ������ʵ�������ڸ�worker�Ͻ�����peer�Ĺ���ά����ְ��
 *
 * * ÿ��unavi svc���࣬��Ҫ�����������ڵ�udp ����Э���Э��id������rtmfp proto��id������Ҫ��
 * * *������service��ǩ����proto�ײ����ǩ�������ʶ�Ӧ��service���ٸ���ǩ����Ҫ��service������ͬ��servant����
 * * * service/servant/proxy�����Լ���ǩ������Ϊ���ͱ�ǡ���rtmfpЭ��֮�ϵ�flash����Ϊ������rtmfp flow�ϵ�user metadata
 * * * ѡ��洢ǩ����ÿ��flow�ĵ�һ��cmd����ʱ������metadata�е�ǩ��������֪����NetConnection/NetStream/NetGroup�µ�cmd��
 * * * �ٸ����������ɵ����䴦������
 *
 * * peer��ͨѶ��ͨ���ĸ��channel�������rtmfp flow����ʼ����ǩ��ȷ��service/proxyʱ����ʹ�õ�channel�͹̶������ڶ�Ӧ��
 * servant/proxy��
 *
 * * ��service�����servant/proxy֮���object request���ת��Ϊ��Ϣ���ݣ��Լ���Ҫʲô�����İ���(����rtmfp flash��Ҫ��flow
 * * ��ѡ����а���)������Ҫ�������ʵ�ֵġ�
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
	//UOMBSvcDeclareģ���࣬ǿ��Ҫ��UOMBSvc��������������þ�̬������һ��أ�ʹ��UOMBObj::regist_cmdģ�澲̬��Ա
	//��������������Ĵ����Ա������
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

	//��������ӽ������յ���ʼ��Ϣʱ��ͨ�����е�ǩ���ҵ�svc��Ȼ����ø÷�����
	//������Ӷ�Ӧ��Ӧ�ò��peerʵ�塣
	//svc��������ڸýӿ��ڲ������������ӵĹ����й�����
	virtual UOMBPeer* new_server_peer(const UOMBPeerIdntfr& id) = 0;
	//�������Ӷ��󹤳�������svc��������ô���������ӹ����ԵĹ�����
	virtual UOMBPeer* new_client_peer(const UOMBPeerIdntfr& idfr) = 0;

	virtual void quit_peer(const UOMBPeerIdntfr& id) = 0;

	//���ڴ�������Ӧ�÷���˶���
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

	//���߳�ר�д洢�л��svcʵ����
	static UOMBPeerSvc* get_svc(uint8_t proto_id, bool need_thr=true);

	UOMBPeerSvc(const UOMBPeerSvc& r){}
	UOMBPeerSvc& operator=(const UOMBPeerSvc& r)
	{
		return *this;
	}

	void regist_peer(UOMBPeerApp* peer_app);
	uint8_t proto_id;
protected:
	//����ĳ��SVCʵ��������UOMBPeerAppʵ��������
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

//�߳�ר�д洢���洢�߳��¸�proto�µĸ�svcʵ����
//��������ָ�룬ʹ�ø�svc���߳��˳�ʱ�Զ����ա�
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
	 * ʹ�øýӿڻ���߳�ר�е�SvcClassʵ����
	 * ��û�н��������ӵĶԶ������������ʼ��proxyʱ����һ�����������ø�use()���û�á�
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
	SvcClass::declare_cmds(); //ǿ��SvcClass����ʵ��declare_cmds��̬������
	pglobal = &obj;
}

UNAVI_NMSP_END

#endif /* UNAVISERVICE_H_ */
