/*
 * UOMBCmd.h
 *
 *  Created on: 2014-11-27
 *      Author: dell
 */

#ifndef UOMBCMD_H_
#define UOMBCMD_H_

#include "unavi/frame/UOMBCommon.h"

UNAVI_NMSP_BEGIN
class UOMBCmd;
class UOMBObj;
class UOMBProxy;
class UOMBCmdRaw
{
public:
	UOMBCmdRaw():
		raw(NULL),
		size(0)
	{}
	uint32_t size;
	uint8_t *raw;
private:
	friend class UOMBCmd;
	~UOMBCmdRaw()
	{
		if (raw)
			delete []raw;
	}
};

class UOMBCmd
{
public:
	virtual ~UOMBCmd()
	{
	}

	UOMBCmd() :
		trans_id(0),
		is_last(false),
		send_timeout(0)
	{}

	void set_last()
	{
		is_last = true;
	}

	void set_send_timeout(uint64_t timeout_ml)
	{
		send_timeout = timeout_ml;
	}

	void give(UOMBCmd& dst)
	{
		if (dst.raw.raw)delete []dst.raw.raw;
		dst.raw = raw;
		raw.raw = NULL;
		dst.trans_id = trans_id;
	}

	virtual const char* cmd_name()const = 0;

protected:
	friend class UOMBObj;
	friend class UOMBProxy;
	bool is_last;
	uint64_t send_timeout;
	//相向的前后两个cmd，如果有反馈关系，使用trans_id字段，
	//如果为0，表示不需要对端的反馈cmd
	uint64_t trans_id;
	UOMBCmdRaw raw;
	virtual void encode(UOMBCmdRaw& raw)const = 0;
};

UNAVI_NMSP_END

#endif /* UOMBCALL_H_ */
