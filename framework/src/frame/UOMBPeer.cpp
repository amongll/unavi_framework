/*
 * UOMBPeer.cpp
 *
 *  Created on: 2014-12-26
 *      Author: li.lei
 */

#include "unavi/frame/UOMBPeer.h"
#include "unavi/frame/UOMBPeerApp.h"
#include "unavi/core/UNaviLog.h"
#include "unavi/frame/UOMBPeerSvc.h"
#include "unavi/frame/UOMBObj.h"
#include "unavi/frame/UOMBServant.h"

using namespace std;
using namespace unavi;

UOMBPeer::UOMBPeer():
	last_send_obj(NULL),
	last_send_ch(UOMBPeerApp::NULL_CHANNEL),
	conn_to_ml(0),
	idle_to_ml(0),
	app(NULL)
{
}

void UOMBPeer::bind_app(UOMBPeerApp* app_obj)
{
	app = app_obj;
	app->conn = this;
}

void UOMBPeer::_peer_ich_idle_exception(UIChannel ich)
{
	app->peer_exception(UOMBPeerApp::PEER_EXPECTED_ICHANNLE_TIMEDOUT, &ich);
}

void UOMBPeer::connect_peer(UOMBPeerIdntfr* id)
{
	app->status = UOMBPeerApp::CONNECTING;
	connect_peer_proto(id, conn_to_ml);
}

void UOMBPeer::peer_connected(const UOMBPeerIdntfr* id)
{
	if ( app->status == UOMBPeerApp::CONNECTING ) {
		app->status = UOMBPeerApp::CONNECTED;
		app->set_peer_id(*id);
	}
	notify_peer_connected();
}

UOMBPeer::SendStatus UOMBPeer::send_msg(UOMBObj& obj, const uint8_t* raw,
    uint32_t sz, UOChannel ch,  uint64_t timeout, bool is_last)
{
	hash_map<UOChannel, OChannelInfo>::iterator it = active_chn_check.find(ch);
	if ( last_send_ch!=ch || last_send_obj != &obj) {
		if (it == active_chn_check.end()) {
			UOMBException e(UOMB_IMPL_ERROR,"channel do not related to UOMBObj");
			uexception_throw_v(e,LOG_ERROR,frame_logid);
		}
		if (it->second.object != &obj) {
			UOMBException e(UOMB_IMPL_ERROR,"channel do not related to UOMBObj");
			uexception_throw_v(e,LOG_ERROR,frame_logid);
		}
	}

	if ( it->second.status != 0) {
		if ( (it->second.status & UOMBObj::OCHANNEL_REJECTED) ||
			(it->second.status & UOMBObj::OCHANNEL_FINISHED)) {
			ostringstream oss;
			oss<<obj.sign;
			UOMBException e(UOMB_IMPL_ERROR, "out channel rejected or finished. obj:%s, channel:%d",
				oss.str().c_str(), ch);
			uexception_throw_v(e,LOG_ERROR,frame_logid);
		}
		else if ( it->second.status & UOMBObj::OCHANNEL_OBJ_REMOVED) {
			ostringstream oss;
			oss<<obj.sign;
			UOMBException e(UOMB_IMPL_ERROR, "out channel is related by pre-existing object. channel:%d,"
				"send object:%s",
				ch,oss.str().c_str());
			uexception_throw_v(e,LOG_ERROR,frame_logid);
		}
	}

	last_send_ch = ch;
	last_send_obj = &obj;
	UOMBPeer::SendStatus ret = send_msg_proto(raw,sz,ch, timeout, is_last);
	return ret;
}

std::pair<UOChannel, UOMBPeer::SendStatus> UOMBPeer::send_msg_with_newchannel(
    UOMBObj& obj, const uint8_t* raw, uint32_t sz, uint64_t send_to, bool is_last)
{
	UOChannel new_ch = new_out_channel_proto(&obj.sign);
	last_send_ch = new_ch;
	last_send_obj = &obj;

	OChannelInfo& info = active_chn_check[new_ch];

	if (info.status != 0) {
		if ( info.status & UOMBObj::OCHANNEL_OBJ_REMOVED) {
			ostringstream oss;
			oss<<obj.sign;
			UOMBException e(UOMB_IMPL_ERROR, "out channel is related by pre-existing object. channel:%d,"
				"send object:%s",
				new_ch,oss.str().c_str());
			uexception_throw_v(e,LOG_ERROR,frame_logid);
		}
	}

	info.object = &obj;
	info.status = is_last ? UOMBObj::OCHANNEL_FINISHED : 0;

	UOMBPeer::SendStatus status = send_msg_proto(raw,sz,new_ch,send_to, is_last);
	return pair<UOChannel,UOMBPeer::SendStatus>(new_ch, status);
}

UOMBObj* UOMBPeer::ichannel_relating_obj(UIChannel ch,
    bool& is_closed_obj)
{
	hash_map<UIChannel, IChannelInfo>::iterator it = postive_chn_idx.find(ch);
	if ( it == postive_chn_idx.end() ) {
		is_closed_obj = false;
		return NULL;
	}

	if ( it->second.status & UOMBObj::OCHANNEL_OBJ_REMOVED) {
		is_closed_obj = true;
		return NULL;
	}

	is_closed_obj = false;
	return it->second.object;
}

UOMBObj* UOMBPeer::ochannel_relating_obj(UOChannel ch, bool& is_closed_obj)
{
	hash_map<UIChannel, OChannelInfo>::iterator it = active_chn_check.find(ch);
	if ( it == active_chn_check.end() ) {
		is_closed_obj = false;
		return NULL;
	}

	if ( it->second.status & UOMBObj::OCHANNEL_OBJ_REMOVED) {
		is_closed_obj = true;
		return NULL;
	}

	is_closed_obj = false;
	return it->second.object;
}

const UOMBPeerIdntfr* UOMBPeer::get_peer_id() const
{
	return app->_id;
}

UOMBPeer::~UOMBPeer()
{
	_close_peer(false);
}

void UOMBPeer::notify_idle_timedout()
{
	app->peer_exception(UOMBPeerApp::PEER_IDLE_TIMEDOUT,NULL);
}

void UOMBPeer::notify_connect_failed()
{
	app->peer_exception(UOMBPeerApp::PEER_CONNECT_FAILED,NULL);
}

void UOMBPeer::notify_broken()
{
	app->peer_exception(UOMBPeerApp::PEER_BROKEN,NULL);
}

void UOMBPeer::notify_peer_closed()
{
	app->peer_exception(UOMBPeerApp::PEER_CLOSED,NULL);
}

void UOMBPeer::notify_peer_connected()
{
	app->start_on_connected();
}

void UOMBPeer::process_msg(const uint8_t* raw, uint32_t raw_sz,
    UIChannel channel, const UOMBSign* or_signature)
{
	bool had_exists = false;
	UOMBObj* obj = ichannel_relating_obj(channel, had_exists);
	if ( obj==NULL && !had_exists ) {
		if ( or_signature == NULL) {
			uerror_log(unavi::frame_logid, "implement problem.input channel has no signature.Peer type:%s,"
				"PeerAppType:%s",
				typeid(*this).name(),typeid(*app).name());
			return;
		}

		UOMBObj* obj = app->get_object(*or_signature);

		if ( obj ) {
			try {
			UOMBCmd* call = obj->decode_call(raw,raw_sz);
			if (call == NULL) {
				ostringstream oss;
				oss << "object" << obj->sign << " type:%s" << typeid(*obj).name() <<
					" decode call failed";
				uwarn_log_v(unavi::frame_logid, oss.str().c_str());
				obj->ichannel_exception(channel,UOMBObj::ICHANNEL_CMD_DECODE_FAILED,NULL);
			}
			else
				obj->process_cmd(*call, channel);
			}
			catch(const std::exception& e) {
				ostringstream oss;
				oss << "object" << obj->sign << " type:%s" << typeid(*obj).name() <<
					" decode call failed:%s" << e.what();
				uwarn_log_v(unavi::frame_logid, oss.str().c_str());
				obj->ichannel_exception(channel,UOMBObj::ICHANNEL_CMD_DECODE_FAILED,NULL);
			}
			return;
		}
		else {
			UOMBServant* obj =  app->get_new_servant(*or_signature);
			if ( obj == NULL) {
				app->peer_unknown_servant_arrived(*or_signature);
				return;
			}
			obj->set_signature(*or_signature);
			app->regist_obj(*obj);
			obj->parse_signature(*or_signature);

			app->servant_arrived(*obj);
#ifdef OMB_FRAME_DEBUG
			UNaviLogInfra::declare_logger_tee(frame_logid,TEE_PINK);
			ostringstream oss;
			oss<<" Run new servant of type:"<<typeid(*obj).name() << obj->sign
				<< " on peer of type:"<< typeid(*this).name();
			udebug_log_vv(frame_logid,oss.str().c_str());
			UNaviLogInfra::declare_logger_tee(frame_logid,TEE_NONE);
#endif
			try {
				UOMBCmd* cmd = obj->decode_call(raw,raw_sz);
				if ( cmd ) {
					obj->process_cmd(*cmd, channel);
				}
				else {
					ostringstream oss;
					oss << "object" << obj->sign << " type:%s" << typeid(*obj).name() <<
						" decode call failed";
					uwarn_log_v(unavi::frame_logid, oss.str().c_str());
					obj->ichannel_exception(channel,UOMBObj::ICHANNEL_CMD_DECODE_FAILED,NULL);
				}
			}catch(std::exception& e) {
				ostringstream oss;
				oss << "object" << obj->sign << " type:%s" << typeid(*obj).name() <<
					" decode call failed:%s" << e.what();
				uwarn_log_v(unavi::frame_logid, oss.str().c_str());
				obj->ichannel_exception(channel,UOMBObj::ICHANNEL_CMD_DECODE_FAILED,NULL);
			}
			return;
		}
	}
	else if (obj ){
		try {
			UOMBCmd* cmd = obj->decode_call(raw,raw_sz);
			if ( cmd ) {
				obj->process_cmd(*cmd, channel);
			}
			else {
				ostringstream oss;
				oss << "object" << obj->sign << " type:%s" << typeid(*obj).name() <<
					" decode call failed";
				uwarn_log_v(unavi::frame_logid, oss.str().c_str());
				obj->ichannel_exception(channel,UOMBObj::ICHANNEL_CMD_DECODE_FAILED,NULL);
			}
		}catch(std::exception& e) {
			ostringstream oss;
			oss << "object" << obj->sign << " type:%s" << typeid(*obj).name() <<
				" decode call failed:%s" << e.what();
			uwarn_log_v(unavi::frame_logid, oss.str().c_str());
			obj->ichannel_exception(channel,UOMBObj::ICHANNEL_CMD_DECODE_FAILED,NULL);
		}
		return;
	}
	else {
		//关联的object已经被应用层关闭。
	}
}

void UOMBPeer::close_peer()
{
	_close_peer(true);
}

void UOMBPeer::_close_peer(bool recycle)
{
	ich_idle_checkers.clear();
	postive_chn_idx.clear();
	active_chn_check.clear();
	app->svc->quit_peer(*app->_id);
	close_proto();
	if(recycle)
		delete this;
}

void unavi::UOMBPeer::set_ichannel_idle_limit(UIChannel ich, uint64_t to_ml)
{
	hash_map<UIChannel, IChannelIdleChecker::Ref>::iterator it = ich_idle_checkers.find(ich);
	if ( it == ich_idle_checkers.end() ) {
		IChannelIdleChecker* chkr = new IChannelIdleChecker(ich,*this);
		chkr->set_expire(to_ml*1000);
		ich_idle_checkers[ich].reset(chkr);
	}
	else {
		it->second->set_expire(to_ml*1000);
	}
}

void UOMBPeer::unregist_obj(UOMBObj& obj)
{
	hash_map<UIChannel, IChannelInfo>::iterator it;
	for ( it = postive_chn_idx.begin(); it != postive_chn_idx.end(); it++) {
		if ( it->second.object == &obj) {
			it->second.object = NULL;
			it->second.status |= UOMBObj::ICHANNEL_OBJ_REMOVED;
			ich_idle_checkers.erase(it->first);
		}
	}

	hash_map<UOChannel, OChannelInfo>::iterator it2;
	for ( it2 = active_chn_check.begin(); it2 != active_chn_check.end(); it2++) {
		if ( it->second.object == &obj) {
			it->second.object = NULL;
			it->second.status |= UOMBObj::OCHANNEL_OBJ_REMOVED;
		}
	}
}

UOMBPeer::IChannelInfo::IChannelInfo():
	object(NULL),
	status(0)
{
}

UOMBPeer::OChannelInfo::OChannelInfo():
	object(NULL),
	status(0)
{
}

void unavi::UOMBPeer::IChannelIdleChecker::process_event()
{
	bool closed = false;
	UOMBObj* obj = target.ichannel_relating_obj(id,closed);
	if ( obj && !closed ) {
		obj->ichannel_exception(id, UOMBObj::ICHANNEL_IDLE_TIMEDOUT, NULL);
	}
	else if ( !obj ) {
		target._peer_ich_idle_exception(id);
	}
}
