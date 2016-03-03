/*
 * UJOMSvc.h
 *
 * Unavi object json msg svc
 *
 *  Created on: 2014-11-28
 *      Author: dell
 */

#ifndef UJOMBASESVC_H_
#define UJOMBASESVC_H_
#include "unavi/ujom/UJOMCommon.h"
#include "unavi/ujom/UJOMPeer.h"
#include "unavi/frame/UOMBProxy.h"
#include "unavi/frame/UOMBServant.h"
#include "unavi/util/UNaviUtil.h"

UJOM_NMSP_BEGIN

class UJOMCmd : public unavi::UOMBCmd
{
public:
	UJOMCmd(const char* cmd_name, json_t* content):
		cmd_content(json_incref(content))
	{
		json_object_set_new(cmd_content, "cmd", json_string(cmd_name));
	}

	virtual const char* cmd_name()const
	{
		json_t* nm = json_object_get(cmd_content,"cmd");
		if ( !nm || !json_is_string(nm) || 0==strlen(json_string_value(nm)))
			return NULL;
		else
			return json_string_value(nm);
	}

	virtual void encode(UOMBCmdRaw& raw)
	{
		char* r = json_dumps(cmd_content, 0);
		raw.raw = new uint8_t[strlen(r)];
		raw.size = strlen(r);
		::memcpy(raw.raw, r, raw.size);
		::free(r);
	}

	json_t* cmd_content;
};

class UJOMBaseSvc : public unavi::UOMBPeerSvc
{
public:
	virtual UJOMPeer* new_server_peer(const UOMBPeerIdntfr& id);
	//主动连接对象工厂方法。svc子类可以用此来完成连接管理性的工作。
	virtual UJOMPeer* new_client_peer(const UOMBPeerIdntfr& idfr);
	virtual void quit_peer(const UOMBPeerIdntfr& id) ;
};

class UJOMBaseProxy : public unavi::UOMBProxy
{
public:
	virtual void ochannel_exception(UOChannel och, OChannelErrCode code, void* detail);
	virtual void ichannel_exception(UIChannel ich, IChannelErrCode code, void* detail);
	virtual void input_idle_timedout();

	char obj_id[256];

	void proc_close(UOMBCmd& cmd, UIChannel ich);
};

class UJOMBaseServant : public unavi::UOMBServant
{
public:
	virtual void ochannel_exception(UOChannel och, OChannelErrCode code, void* detail);
	virtual void ichannel_exception(UIChannel ich, IChannelErrCode code, void* detail);
	virtual void input_idle_timedout();

	void proc_close(UOMBCmd& cmd, UIChannel ich);

	char obj_id[256];
};

UJOM_NMSP_END
#endif /* UJOMSVC_H_ */
