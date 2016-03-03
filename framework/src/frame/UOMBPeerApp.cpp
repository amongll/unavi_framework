/*
 * UOMBPeerApp.cpp
 *
 *  Created on: 2014-12-25
 *      Author: li.lei
 */

#include "unavi/frame/UOMBPeerApp.h"
#include "unavi/frame/UOMBPeerSvc.h"
#include "unavi/core/UNaviWorker.h"
#include "UOMBPeerRunMgr.h"
#include "unavi/frame/UOMBShadowApp.h"

using namespace std;
using namespace unavi;

UOMBPeerApp::UOMBPeerApp(AppType tp):
	type(tp),
	status(CREATED),
	conn(NULL),
	svc(NULL),
	_id(NULL),
	worker_id(0xffffffff),
	generation(-1)
{
}

bool UOMBPeerApp::is_in_worker()
{
	return  UNaviCycle::worker() != NULL;
}

bool UOMBPeerApp::is_in_worker(uint32_t wk)
{
	UNaviWorker* wker = UNaviCycle::worker();
	if ( wker == NULL)
		return false;

	if ( wk == wker->id() ) {
		return true;
	}
	return false;
}

bool UOMBPeerApp::is_in_my_worker()const
{
	UNaviWorker* worker = UNaviCycle::worker();
	return worker != NULL && worker_id == worker->id();
}

UOMBProxy* UOMBPeerApp::run_proxy(const UOMBSign& proxy_type,
	const UOMBCmd& start_cmd)throw(UOMBException)
{
	if (svc == NULL) {
		UOMBException e(UOMB_ABUSED_ERROR, "app:%s should start with call:init_client()",
			typeid(*this).name());
		uexception_throw(e,LOG_WARN,frame_logid);
	}

	if (!is_in_my_worker()) {
		UOMBException e(UOMB_ABUSED_ERROR, "app:%s should run_proxy in worker thread",
			typeid(*this).name());
		uexception_throw(e,LOG_WARN,frame_logid);
	}

	UOMBProxy* proxy = get_new_proxy(proxy_type);
	if ( proxy == NULL ) {
		ostringstream oss;
		oss<<proxy_type;
		UOMBException e(UOMB_ABUSED_ERROR,"get nil proxy from svc:%s, type:%s",
			typeid(*svc).name(), oss.str().c_str());
		uexception_throw_v(e,LOG_WARN,frame_logid);
	}
	proxy->init_start_cmd(start_cmd);
	proxy->init_proxy_id(proxy_type, *this, proxy->sign);
	try {
		regist_obj(*proxy);
	}
	catch(const std::exception& e)
	{
		delete proxy;
		throw e;
	}

	if ( status == CONNECTED || status == ACCEPTED ) {
		proxy->start_up();
		proxy_runned(*proxy);
	}

	return proxy;
}

const UOMBPeerIdntfr* UOMBPeerApp::get_peer_id() const
{
	return _id;
}

bool UOMBPeerApp::set_peer_id(const UOMBPeerIdntfr& id)
{
	if(_id)
		delete _id;

	_id = id.dup();
	if ( _id == NULL ) {
		return false;
	}
	return true;
}

void UOMBPeerApp::set_peer_id(UOMBPeerIdntfr& id)
{
	if (_id)
		delete _id;
	_id = &id;
}

UOMBPeerApp::PeerState UOMBPeerApp::get_peer_status() const
{
	return status;
}

UOMBObj* UOMBPeerApp::get_object(const UOMBSign& sign)
{
	ObjIter it = object_map.find(&sign);
	if ( it == object_map.end())
		return NULL;
	return it->second;
}

const UOMBObj* UOMBPeerApp::get_object(const UOMBSign& sign)const
{
	ObjCIter it = object_map.find(&sign);
	if ( it == object_map.end())
		return NULL;
	return it->second;
}

void UOMBPeerApp::close()
{
	close_impl(true);
}

UOMBPeerApp::~UOMBPeerApp()
{
	close_impl(false);
}

void UOMBPeerApp::close_impl(bool recycle)
{
	if (conn)conn->close_peer();
	conn = NULL;
	object_map.clear();
	object_list.recycle_all_nodes();

	svc->call_destroy_notifier(this);

	if( _id ) {
		delete _id;
		_id = NULL;
	}
	if (recycle) {
		delete this;
	}
}

void UOMBPeerApp::regist_obj(UOMBObj& obj)throw(UOMBException)
{
	pair<ObjIter,bool> ret = object_map.insert(make_pair(&obj.sign,&obj));
	if (ret.second == false) {
		ostringstream oss;
		oss<<obj.sign;
		UOMBException e(UOMB_SIGN_ERROR, "object sign conflict: %s", oss.str().c_str());
		uexception_throw_v(e,LOG_WARN,frame_logid);
	}

	object_list.insert_tail(obj);
	obj.app = this;
}

void UOMBPeerApp::unregist_obj(UOMBObj& obj)
{
	if ( conn )conn->unregist_obj(obj);

	object_list.quit_node(&obj);
	object_map.erase(&obj.sign);
}

void UOMBPeerApp::set_connect_timeout(uint64_t to_ml)
{
	conn->conn_to_ml = to_ml;
}

void UOMBPeerApp::set_idle_timeout(uint64_t to_ml)
{
	conn->idle_to_ml = to_ml;
	conn->set_idle_timeout_proto(to_ml);
}

void UOMBPeerApp::start_on_connected()
{
	UNaviListable* nd = object_list.get_head();
	while ( nd ) {
		UOMBProxy* prxy = dynamic_cast<UOMBProxy*>(nd);
		if ( prxy == NULL) {
			nd = object_list.get_next(nd);
			continue;
		}
		nd = object_list.get_next(nd);
		prxy->start_up();
		proxy_runned(*prxy);
	}
}

UOMBPeerApp* UOMBPeerApp::get_client_or_shadow(const UOMBPeerIdntfr& peer_id)
{
	int64_t gen = -1;
	uint32_t wkid = 0xffffffff;
	int64_t should_gen = -1;
	if ( OMBSvcAppRegTable::have_app(peer_id, gen, wkid, should_gen) ) {
		if ( is_in_worker(wkid) ) {
			return const_cast<UOMBPeerApp*>(OMBSvcAppRegTable::get_app(peer_id));
		}
		else {
			UOMBPeerShadow* sha = new UOMBPeerShadow(peer_id,gen);
			sha->set_worker_id(wkid);
			return sha;
		}
	}
	else {
		UOMBPeerShadow *ret = new UOMBPeerShadow(peer_id,should_gen);
		return ret;
	}
}

void UOMBPeerApp::push_peer_business(UOMBPeerShadow& shadow,
    UOMBPeerBusiness& busi)
{
	if ( shadow.worker_id >= UNAVI_MAX_WORKER) {
		size_t wkid = shadow._id->hash() % UNaviWorker::worker_count();
		UNaviWorker::get_worker(wkid)->push_peer_business(&shadow, &busi);
	}
	else {
		UNaviWorker::get_worker(shadow.worker_id)->push_peer_business(&shadow, &busi);
	}
}

void UOMBPeerApp::run_peer_business(UOMBPeerShadow& shadow,
    UOMBPeerBusiness& busi)
{
	if ( shadow.worker_id == 0xffffffff ) {
		UOMBPeerApp* app = start_up_client(shadow);
		OMBSvcAppRegTable::regist_app(*app, shadow.generation);
		app->run_peer_business(&busi);
	}
	else {
		bool gen_match = false;
		if ( false==OMBSvcAppRegTable::have_app(shadow, gen_match)) {
			return;
		}
		if ( gen_match == false )
			return;

		UOMBPeerApp* app = OMBSvcAppRegTable::get_app(*shadow._id);
		app->run_peer_business(&busi);
	}
}

UOMBPeerApp* UOMBPeerApp::start_up_client(UOMBPeerShadow& shadow)
{
	UOMBPeerSvc* svc = UOMBPeerSvc::get_svc(shadow.proto_id,true);
	UOMBPeer* cnn = svc->new_client_peer(*_id);

	if ( cnn == NULL ) {
		return NULL;
	}
	cnn->connect_peer(_id);
	UOMBPeerApp* app = svc->get_new_app(false);
	svc->regist_peer(app);
	cnn->bind_app(app);
	app->worker_id = UNaviCycle::worker_id();
	OMBSvcAppRegTable::regist_app(*app, shadow.generation);
	return app;
}

UOMBPeerApp* UOMBPeerApp::get_client_or_shadow(const UOMBPeerShadow& peer)
{
	bool gen_match = false;
	if ( OMBSvcAppRegTable::have_app(peer, gen_match) ) {
		if ( gen_match == false ) {
			return NULL;
		}
		if ( is_in_worker(peer.worker_id) ) {
			return const_cast<UOMBPeerApp*>(OMBSvcAppRegTable::get_app(*peer.get_peer_id()));
		}
		else {
			UOMBPeerShadow* sha = new UOMBPeerShadow(peer);
			sha->set_worker_id(peer.worker_id);
			return sha;
		}
	}
	else {
		UOMBPeerShadow *ret = new UOMBPeerShadow(peer);
		return ret;
	}
}

UOMBPeerShadow::UOMBPeerShadow(const UOMBPeerIdntfr& id, int64_t gene=-1):
	UOMBPeerApp(UOMBPeerApp::APP_SHADOW),
	proto_id(0xff)
{
	_id = const_cast<UOMBPeerIdntfr*>(&id);
	generation = gene;
}

void UOMBPeerApp::process_business(const UOMBPeerBusiness& busi)
{
	hash_map<const char*, AppBusiMethods>::iterator it1 = business_procs.find(typeid(*this).name());
	if ( it1 == business_procs.end() || busi.name()==NULL)
		return;
	AppBusiMethods::iterator it2 = it1->second.find(busi.name());
	if ( it2 == it1->second.end())
		return;
	(this->*it2->second)(busi);
}

