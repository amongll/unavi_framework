/*
 * UOMBService.cpp
 *
 *  Created on: 2014-12-26
 *      Author: li.lei
 */

#include "unavi/frame/UOMBPeerSvc.h"
#include "unavi/core/UNaviCycle.h"
#include "unavi/core/UNaviWorker.h"
#include "unavi/core/UNaviLog.h"
#include "unavi/frame/UOMBPeerApp.h"
#include "UOMBPeerRunMgr.h"

using namespace std;
using namespace unavi;

pthread_once_t UOMBPeerSvc::svc_map_once = PTHREAD_ONCE_INIT;
pthread_key_t UOMBPeerSvc::svc_map_key ;

UOMBPeerSvc* UOMBPeerSvc::service_declares[];

int unavi::frame_logid = UNaviLogInfra::declare_logger("omb_frame",
	DEFAULT_UNAVI_LOG_ROOT,
#ifdef OMB_FRAME_DEBUG
	unavi::LOG_DEBUG,
#else
	unavi::LOG_NOTICE,
#endif
	30720,
	4096,
	TEE_GREEN);

void UOMBSvcMap::clean_up_svcs()
{
	for(int i=0; i<256; i++) {
		UOMBPeerSvc::Ref& pthr = proto_map[i];
		if ( !pthr ) continue;
		pthr.reset();
	}
}

UOMBPeerApp* UOMBPeerSvc::start_up_server(uint8_t proto_id, const UOMBPeerIdntfr& id)
{
	UOMBPeerSvc* omb_svc = UOMBPeerSvc::get_svc(proto_id,true);
	if (omb_svc==NULL) {
		uwarn_log(unavi::frame_logid,"find omb peer service for proto:%d failed",
			proto_id);
		return NULL;
	}

	UOMBPeerApp* app;

	try {
		app = omb_svc->get_new_app(true);
		if (app == NULL) {
			uwarn_log(unavi::frame_logid, "get new omb_peer_server failed. proto_id:%d, svc:%s",
				proto_id, typeid(*omb_svc).name());
			return NULL;
		}

		if ( !app->set_peer_id(id)) {
			uwarn_log_v(unavi::frame_logid, "init omb_peer_server peer_id failed. type:%s",
				typeid(id).name());
			delete app;
			return NULL;
		}

		UOMBPeer* peer = NULL;
		peer = omb_svc->new_server_peer(id);
		if (peer == NULL) {
			ostringstream oss;
			id.print(oss);
			uwarn_log(unavi::frame_logid, "get nil accepted omb_peer: id:%s, svc:%s",
				oss.str().c_str(), typeid(*omb_svc).name());
			delete app;
			return NULL;
		}
		peer->bind_app(app);
		app->type = UOMBPeerApp::PEER_SERVER;
		app->status = UOMBPeerApp::ACCEPTED;
		app->worker_id = UNaviCycle::worker_id();
		if ( false == OMBSvcAppRegTable::regist_app(*app) ) {
			ostringstream oss;
			id.print(oss);
			uwarn_log(unavi::frame_logid, "get new omb_peer_server failed. peer:%s already exists. proto_id:%d, svc:%s",
				oss.str().c_str(), proto_id, typeid(*omb_svc).name());
			delete app;
			return NULL;
		}
		omb_svc->regist_peer(app);
		omb_svc->call_arrive_notifier(app);

		//todo: 通知应用层
		return app;
	}
	catch(const std::exception& e) {
		ostringstream oss;
		id.print(oss);
		uwarn_log(unavi::frame_logid, "prepare new omb_peer_server failed:%s. proto:%d, svc:%s",
			proto_id, typeid(*omb_svc).name());
	}
	if (app != NULL)
		delete app;
	return NULL;
}

void UOMBPeerSvc::key_once(void)
{
	pthread_key_create(&UOMBPeerSvc::svc_map_key,NULL);
}

void UOMBPeerSvc::init_svc_map()
{
	pthread_once(&UOMBPeerSvc::svc_map_once,UOMBPeerSvc::key_once);

	UOMBSvcMap* svc_map = reinterpret_cast<UOMBSvcMap*>
		(pthread_getspecific(UOMBPeerSvc::svc_map_key));
	if ( svc_map == NULL ) {
		svc_map = new UOMBSvcMap;
		pthread_setspecific(UOMBPeerSvc::svc_map_key, svc_map);
	}
}

void UOMBPeerSvc::clean_svc_map()
{
	UOMBSvcMap* svc_map = reinterpret_cast<UOMBSvcMap*>
			(pthread_getspecific(UOMBPeerSvc::svc_map_key));

	svc_map->clean_up_svcs();
	delete svc_map;
	pthread_setspecific(UOMBPeerSvc::svc_map_key,NULL);
}

//从Svc的类型static对象，复制到线程专有存储的svc实例中。
UOMBPeerSvc* UOMBPeerSvc::get_svc(uint8_t proto_id, bool thr)
{
	pthread_once(&UOMBPeerSvc::svc_map_once,UOMBPeerSvc::key_once);

	if ( proto_id > UNaviProtoTypes::max_protos )
		return NULL;

	UOMBPeerSvc*& declare = service_declares[proto_id];
	if (!declare )
		return NULL;

	if ( thr == false ) {
		return NULL;
	}

	UOMBSvcMap* svc_map = reinterpret_cast<UOMBSvcMap*>
		(pthread_getspecific(UOMBPeerSvc::svc_map_key));
	if ( svc_map == NULL ) {
		svc_map = new UOMBSvcMap;
		pthread_setspecific(UOMBPeerSvc::svc_map_key, svc_map);
	}

	UOMBPeerSvc::Ref& pthr = svc_map->proto_map[proto_id];
	if ( !pthr ) {
		pthr.reset(declare->dup());
	}

	pthr->proto_id = proto_id;
	pthr->run_init();
	return pthr.get();
}

void UOMBPeerSvc::call_destroy_notifier(UOMBPeerApp* app)
{
	hash_map<const char*, AppNotifier>::iterator it = app_destroy_notifiers.find(typeid(*this).name());
	if (it==app_destroy_notifiers.end())
		return;
	UOMBPeerShadow shadow(*app->_id, app->generation);
	(this->*(it->second))(shadow);
}
void UOMBPeerSvc::call_arrive_notifier(UOMBPeerApp* app)
{
	hash_map<const char*, AppNotifier>::iterator it = server_app_arrive_notifiers.find(typeid(*this).name());
	if (it==server_app_arrive_notifiers.end())
		return;
	UOMBPeerShadow shadow(*app->_id, app->generation);
	(this->*(it->second))(shadow);
}

UOMBPeerSvc::~UOMBPeerSvc()
{
	peers.recycle_all_nodes();
}

void UOMBPeerSvc::regist_peer(UOMBPeerApp* peer_app)
{
	peer_app->svc = this;
	peers.insert_tail(*peer_app);
}

hash_map<const char*, UOMBPeerSvc::AppNotifier> UOMBPeerSvc::server_app_arrive_notifiers;
hash_map<const char*, UOMBPeerSvc::AppNotifier> UOMBPeerSvc::app_destroy_notifiers;
