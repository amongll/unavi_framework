/*
 * UNaviORProxy.h
 *
 *  Created on: 2014-11-26
 *      Author: dell
 */

#ifndef UNAVIORBPROXY_H_
#define UNAVIORBPROXY_H_

#include "unavi/frame/UOMBCommon.h"
#include "unavi/frame/UOMBObj.h"
#include "unavi/frame/UOMBCmd.h"
#include "unavi/frame/UOMBPeerApp.h"

UNAVI_NMSP_BEGIN

class UOMBProxy : public UOMBObj
{
public:
	UOMBProxy():
		UOMBObj(UOMBObj::OMB_PROXY)
	{

	}
	virtual ~UOMBProxy()
	{
		if(start)
			delete start;
	}

private:
	friend class UOMBPeerApp;

	virtual void init_proxy_id(const UOMBSign& type_sign, const UOMBPeerApp& app, UOMBSign& dst_sign) = 0;

	struct StartCmd: public UOMBCmd
	{
		StartCmd(const UOMBCmd& r):
			name(r.cmd_name())
		{
			raw.raw = new uint8_t[r.raw.size];
			raw.size = r.raw.size;
			::memcpy(raw.raw, r.raw.raw, raw.size);
		}
		StartCmd(UOMBCmd& r):
			name(r.cmd_name())
		{
			r.give(*this);
		}
		virtual const char* cmd_name()const {return NULL;}
		virtual void encode(UOMBCmdRaw& raw)const{}
		std::string name;
	};

	void init_start_cmd(const UOMBCmd& cmd)
	{
		cmd.encode(const_cast<UOMBCmdRaw&>(cmd.raw));
		start = new StartCmd(cmd);
	}

	void start_up()
	{
		call(*start);
		delete start;
		start = NULL;
	}

	StartCmd *start;
};

UNAVI_NMSP_END

#endif /* UNAVIORPROXY_H_ */
