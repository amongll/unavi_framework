/*
 * UOMBPeerApp.h
 *
 *  Created on: 2014-12-25
 *      Author: li.lei
 *      Desc: 应用语境对象。该对象和某个peer远端对应。
 *      	管理在peer上的proxy,servant对象的生存期。
 *      	以及衔接同一个peer上的不同的OMBObj之间的交互逻辑
 */

#ifndef UOMBPEERAPP_H_
#define UOMBPEERAPP_H_

#include "unavi/frame/UOMBCommon.h"
#include "unavi/core/UNaviLog.h"
#include "unavi/frame/UOMBException.h"
#include "unavi/frame/UOMBPeer.h"
#include "unavi/frame/UOMBPeerSvc.h"

UNAVI_NMSP_BEGIN

class UNaviWorker;
class OMBAppRegBucket;
class UOMBProxy;
class UOMBServant;
class UOMBPeerSvc;
class UOMBPeerShadow;
class UOMBPeerApp;

struct UOMBPeerBusiness
{
protected:
	friend class UOMBPeerApp;
	UOMBPeerBusiness(){};
	virtual ~UOMBPeerBusiness(){};
	virtual UOMBPeerBusiness* dup()const = 0;
	virtual const char* name()const = 0;
};

struct UOMBPeerBusiClosure
{
	UOMBPeerShadow* shadow;
	UOMBPeerBusiness* business;
};

class UOMBPeerApp : public UNaviListable
{
public:
	static const uint32_t NULL_CHANNEL = 0xffffffff;

	template<class SvcClass>
	static void run_peer_business(const UOMBPeerIdntfr& peer, const UOMBPeerBusiness& busi);

	template<class SvcClass>
	static void run_peer_business(const UOMBPeerIdntfr& peer, UOMBPeerBusiness* busi);

	template<class SvcClass>
	static bool run_peer_business(const UOMBPeerShadow& peer, const UOMBPeerBusiness& busi);

	template<class SvcClass>
	static bool run_peer_business(const UOMBPeerShadow& peer, UOMBPeerBusiness* busi);

	template<class SubClass>
	static void regist_business_method(const char* busi_name, void (SubClass::*busi_method)(const UOMBPeerBusiness&));

	enum AppType
	{
		PEER_APP_UNKNOWN,
		PEER_CLIENT,
		PEER_SERVER,
		APP_SHADOW
	};

	enum PeerState
	{
		CREATED,
		CONNECTING,
		CONNECTED,
		ACCEPTED,
		BROKEN,
		CLOSED
	};

	enum PeerErrCode
	{
		PEER_CONNECT_FAILED,
		PEER_CONNECT_TIMEDOUT,
		PEER_BROKEN,
		PEER_IDLE_TIMEDOUT,
		PEER_CLOSED,
		PEER_SERVANT_UNKNOWN_ARRIVED,
		PEER_EXPECTED_ICHANNLE_TIMEDOUT,
		PEER_OTHER_ERROR
	};

	const UOMBPeerIdntfr* get_peer_id()const
	{
		return _id;
	}

	PeerState get_peer_status()const
	{
		return status;
	}

	UOMBPeer* get_connection()
	{
		return conn;
	}

protected:
	UOMBPeerApp(AppType tp);
	virtual ~UOMBPeerApp();

	friend class UOMBObj;
	friend class UOMBPeerSvc;
	friend class UOMBPeer;
	friend class OMBAppRegBucket;
	friend class UOMBPeerShadow;

	UOMBProxy* run_proxy(const UOMBSign& proxy_type, const UOMBCmd& start_cmd)throw(UOMBException);

	/*
	 * 设置主动连接的连接超时。超时时，应用层会得到通知。
	 */
	void set_connect_timeout(uint64_t to_ml);
	/*
	 * 在连接上设置全局的idle时限，如果连接上没有任何实际的出入向完整消息，
	 * 超过该时限时，应用层会得到通知。
	 */
	void set_idle_timeout(uint64_t to_ml);

	bool set_peer_id(const UOMBPeerIdntfr& id);
	void set_peer_id(UOMBPeerIdntfr& id);

	const UOMBPeerSvc* get_svc()const
	{
		return svc;
	}

	UOMBPeerSvc* get_svc()
	{
		return svc;
	}

	UOMBObj* get_object(const UOMBSign& sign);
	const UOMBObj* get_object(const UOMBSign& sign)const;

	void close();

private:

	void close_impl(bool recycle);
	AppType type;
	PeerState status;
	uint32_t worker_id;
	int64_t generation;

	void regist_obj(UOMBObj& obj)throw(UOMBException);
	void unregist_obj(UOMBObj& obj);

	UNaviList object_list;

	typedef hash_map<const UOMBSign*, UOMBObj*>::iterator ObjIter;
	typedef hash_map<const UOMBSign*, UOMBObj*>::const_iterator ObjCIter;
	hash_map<const UOMBSign*, UOMBObj*> object_map;

	void process_business(const UOMBPeerBusiness& busi);
	virtual void peer_exception(PeerErrCode err,void* detail) = 0;

	virtual void proxy_runned(UOMBProxy& start){};
	virtual void servant_arrived(UOMBServant& svt){};
	virtual void peer_unknown_servant_arrived(const UOMBSign& sign){};

	virtual UOMBProxy* get_new_proxy(const UOMBSign& obj_type) = 0;
	virtual UOMBServant* get_new_servant(const UOMBSign& obj_id) = 0;

	friend class UNaviWorker;

	static void push_peer_business(UOMBPeerShadow& shadow, UOMBPeerBusiness& busi);
	static void run_peer_business(UOMBPeerShadow& shadow, UOMBPeerBusiness& busi);

	static UOMBPeerApp* start_up_client(UOMBPeerShadow& shadow);

	void run_peer_business(UOMBPeerBusiness* busi) {
		process_business(*busi);
		delete busi;
	}

	void start_on_connected();
	UOMBPeer* conn;
	UOMBPeerIdntfr* _id;
	UOMBPeerSvc* svc;

	bool is_in_my_worker()const;

	static bool is_in_worker();
	static bool is_in_worker(uint32_t workerid);

	static UOMBPeerApp* get_client_or_shadow(const UOMBPeerIdntfr& peer_id);
	static UOMBPeerApp* get_client_or_shadow(const UOMBPeerShadow& shadow);

	typedef void (UOMBPeerApp::*BusiMethod)(const UOMBPeerBusiness&);
	typedef hash_map<std::string,BusiMethod> AppBusiMethods;
	static hash_map<const char*, AppBusiMethods> business_procs;

	UOMBPeerApp(const UOMBPeerApp&);
	UOMBPeerApp& operator=(const UOMBPeerApp&);
};

struct UOMBPeerShadow : public UOMBPeerApp
{
	virtual ~UOMBPeerShadow();
	UOMBPeerShadow(const UOMBPeerIdntfr& id, int64_t gene=-1);
	UOMBPeerShadow(const UOMBPeerShadow& r);
	UOMBPeerShadow& operator=(const UOMBPeerShadow& r);
	void set_worker_id(uint32_t wk)
	{
		worker_id = wk;
	}
	uint8_t proto_id;
private:
	virtual void peer_exception(PeerErrCode err,void* detail) {}
	virtual void peer_unknown_servant_arrived(const UOMBSign& sign) {}

	virtual UOMBProxy* get_new_proxy(const UOMBSign& sign){return NULL;}
	virtual UOMBServant* get_new_servant(const UOMBSign& sign){return NULL;}
};

template<class SvcClass>
void UOMBPeerApp::run_peer_business(const UOMBPeerIdntfr& peer, const UOMBPeerBusiness& busi)
{
	UOMBPeerBusiness* dupobj = busi.dup();
	run_peer_business<SvcClass>(peer,dupobj);
}

template<class SvcClass>
void UOMBPeerApp::run_peer_business(const UOMBPeerIdntfr& peer, UOMBPeerBusiness* busi)
{
	int64_t gen;
	uint32_t wkid;
	UOMBPeerApp* app_or_shadow = get_client_or_shadow(peer);
	if ( app_or_shadow->type == APP_SHADOW ) {
		UOMBPeerShadow* shadow = dynamic_cast<UOMBPeerShadow*>(app_or_shadow);
		shadow->proto_id = UOMBSvcDeclare<SvcClass>::proto_id;
		push_peer_business(dynamic_cast<UOMBPeerShadow&>(*app_or_shadow), *busi);
	}
	else {
		app_or_shadow->run_peer_business(busi);
		delete busi;
	}
}

template<class SvcClass>
bool UOMBPeerApp::run_peer_business(const UOMBPeerShadow& peer, const UOMBPeerBusiness& busi)
{
	UOMBPeerBusiness* dupobj = busi.dup();
	return run_peer_business<SvcClass>(peer, busi);
}

template<class SvcClass>
bool UOMBPeerApp::run_peer_business(const UOMBPeerShadow& peer, UOMBPeerBusiness* busi)
{
	UOMBPeerApp* app_or_shadow = get_client_or_shadow(peer);
	if (app_or_shadow == NULL)
		return false; //外部拿到的peer shadow对应的peer实体，已经有新的相同Peerid的实体取代。
	if ( app_or_shadow->type == APP_SHADOW ) {
		UOMBPeerShadow* shadow = dynamic_cast<UOMBPeerShadow*>(app_or_shadow);
		shadow->proto_id = UOMBSvcDeclare<SvcClass>::proto_id;
		push_peer_business(dynamic_cast<UOMBPeerShadow&>(*app_or_shadow), *busi);
	}
	else {
		app_or_shadow->run_peer_business(busi);
		delete busi;
	}
}

template<class SubClass>
void UOMBPeerApp::regist_business_method(const char* busi_name, void (SubClass::*busi_method)(const UOMBPeerBusiness&))
{
	business_procs[typeid(SubClass).name()].insert(make_pair(std::string(busi_name),
		static_cast<BusiMethod>(busi_method)));
}

UNAVI_NMSP_END

#endif /* UOMBPEERAPP_H_ */
