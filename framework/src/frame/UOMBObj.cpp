/*
 * UOMBObj.cpp
 *
 *  Created on: 2014-12-26
 *      Author: li.lei
 */

#include "unavi/frame/UOMBPeerApp.h"
#include "unavi/frame/UOMBObj.h"

using namespace std;
using namespace unavi;

UOMBObj::UOMBObj(ObjType tp):
	type(tp),
	app(NULL),
	first_ichannel(UOMBPeerApp::NULL_CHANNEL),
	first_ochannel(UOMBPeerApp::NULL_CHANNEL),
	next_step(0),
	next_ctx(NULL),
	next_ctx_cleanup(0)
{
}

void UOMBObj::close()
{
	_close_impl(true);
}

void UOMBObj::set_ichannel_timeout(UIChannel ich, uint64_t timeout)
{
	app->conn->set_ichannel_idle_limit(ich, timeout);
}

void UOMBObj::set_input_timeout(uint64_t timeout)
{
	if (!p_idle_checker) {
		if (timeout > 0) {
			InputIdleChecker* ckr = new InputIdleChecker(*this);
			UNaviWorker::regist_event(*ckr);
			ckr->set_expire(timeout);
		}
	}
	else {
		if (timeout > 0) {
			p_idle_checker->set_expire(timeout);
		}
	}
}

void UOMBObj::go_on()
{
	if (next_step) {
		NextStep step = next_step;
		void* ctx = next_ctx;
		next_step = NULL;
		next_ctx = NULL;
		(this->*step)(ctx);
	}
}

bool UOMBObj::process_cmd(UOMBCmd& call, UIChannel ch)
{
	std::string name = typeid(*this).name();
	name += ".";
	name += call.cmd_name();
	hash_map<std::string,CmdProc>::iterator it = cmd_map.find(name);
	if ( it == cmd_map.end()) {
		delete &call;
		return false;
	}
#ifdef OMB_FRAME_DEBUG
	UNaviLogInfra::declare_logger_tee(frame_logid,TEE_PINK);
	ostringstream oss;
	oss<<" recv cmd of name:"<< name << " to obj: " << sign;
	udebug_log_vv(frame_logid, oss.str().c_str());
	UNaviLogInfra::declare_logger_tee(frame_logid,TEE_NONE);
#endif
	(this->*(it->second))(call, ch);
	delete &call;
	return true;
}

UOMBObj::~UOMBObj()
{
	_close_impl(false);
}

void unavi::UOMBObj::call(const UOMBCmd& call)
{
	call.encode(const_cast<UOMBCmdRaw&>(call.raw));
	if ( first_ochannel == UOMBPeerApp::NULL_CHANNEL) {
		pair<UOChannel,UOMBPeer::SendStatus> ret;
		ret = app->conn->send_msg_with_newchannel(*this, call.raw.raw, call.raw.size, call.send_timeout, call.is_last);
		first_ochannel = ret.first;
	}
	else {
		app->conn->send_msg(*this,call.raw.raw, call.raw.size, first_ochannel, call.send_timeout, call.is_last);
	}
#ifdef OMB_FRAME_DEBUG
	UNaviLogInfra::declare_logger_tee(frame_logid,TEE_ORANGE);
	ostringstream oss;
	oss<<" Obj of type: "<<typeid(*this).name()<<" sign:"<<sign<< " send call:"<<call.cmd_name() <<
		" on first channel:"<<first_ochannel;
	udebug_log_vv(frame_logid,oss.str().c_str());
	UNaviLogInfra::declare_logger_tee(frame_logid,TEE_NONE);
#endif
}

void unavi::UOMBObj::call_with_channel(const UOMBCmd& call, UOChannel och)
{
	if ( first_ochannel == UOMBPeerApp::NULL_CHANNEL ) {
		UOMBException e(UOMB_ABUSED_ERROR,"out channel:%d should declared first"); //todo
		uexception_throw_v(e,LOG_NOTICE,frame_logid);
	}

#ifdef OMB_FRAME_DEBUG
	UNaviLogInfra::declare_logger_tee(frame_logid,TEE_ORANGE);
	ostringstream oss;
	oss<<"Obj of type: "<<typeid(*this).name()<<" sign:"<<sign<< " send call:"<<call.cmd_name() <<
		" on channel:";
	udebug_log_vv(frame_logid,oss.str().c_str());
	UNaviLogInfra::declare_logger_tee(frame_logid,TEE_NONE);
#endif
	call.encode(const_cast<UOMBCmdRaw&>(call.raw));
	app->conn->send_msg(*this,call.raw.raw, call.raw.size, och, call.send_timeout, call.is_last);
}

UOChannel unavi::UOMBObj::call_with_new_channel(const UOMBCmd& call)
{
	call.encode(const_cast<UOMBCmdRaw&>(call.raw));
	pair<UOChannel,UOMBPeer::SendStatus> ret;
	ret = app->conn->send_msg_with_newchannel(*this, call.raw.raw, call.raw.size, call.send_timeout, call.is_last );
	UOChannel ch = ret.first;
	if ( first_ochannel == UOMBPeerApp::NULL_CHANNEL)
		first_ochannel = ch;
#ifdef OMB_FRAME_DEBUG
	UNaviLogInfra::declare_logger_tee(frame_logid,TEE_ORANGE);
	ostringstream oss;
	oss<<" Obj of type: "<<typeid(*this).name()<<" sign:"<<sign<< " send call:"<<call.cmd_name() <<
		" on new channel:"<<ch;
	udebug_log_vv(frame_logid,oss.str().c_str());
	UNaviLogInfra::declare_logger_tee(frame_logid,TEE_NONE);
#endif
	return ch;
}

void UOMBObj::_close_impl(bool recycle)
{
	if ( next_ctx && next_ctx_cleanup) {
		(this->*next_ctx_cleanup)(next_ctx);
	}
	app->unregist_obj(*this);
	clean_up();
	if(recycle)
		delete this;
}

void unavi::UOMBObj::InputIdleChecker::process_event()
{
	target.input_idle_timedout();
}

hash_map<std::string, UOMBObj::CmdProc> UOMBObj::cmd_map;

