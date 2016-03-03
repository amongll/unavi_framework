/*
 * UNaviORObj.h
 *
 *  Created on: 2014-11-26
 *      Author: dell
 */

#ifndef UOMBOBJ_H_
#define UOMBOBJ_H_

#include "unavi/frame/UOMBCommon.h"
#include "unavi/frame/UOMBException.h"
#include "unavi/util/UNaviListable.h"
#include "unavi/util/UNaviRef.h"
#include "unavi/core/UNaviEvent.h"

#include "unavi/frame/UOMBSign.h"
#include "unavi/frame/UOMBCmd.h"
#include <typeinfo>

UNAVI_NMSP_BEGIN
class UOMBPeerApp;

class UOMBObj : public UNaviListable
{
public:
	typedef void (UOMBObj::*NextStep)(void*);
	typedef void (UOMBObj::*CmdProc)(UOMBCmd& call, UIChannel ch);

	template<class SubClass>
	static void regist_cmd(const char* nm, void (SubClass::*method)(UOMBCmd&, UIChannel));

	enum IChannelErrCode
	{
		ICHANNEL_IDLE_TIMEDOUT = 0x01,
		ICHANNEL_FINISHED = 0x02,
		ICHANNEL_OBJ_REMOVED = 0x08,
		ICHANNEL_CMD_DECODE_FAILED = 0x10,
		ICHANNEL_OTHER_ERROR = 0x20
	};

	enum OChannelErrCode
	{
		OCHANNEL_SEND_TIMEDOUT = 0x01,
		OCHANNEL_REJECTED = 0x02,
		OCHANNEL_FINISHED = 0x04,
		OCHANNEL_OBJ_REMOVED = 0x08,
		OCHANNEL_OTHER_ERROR = 0x10,
	};

	enum ObjType
	{
		OMB_SERVANT,
		OMB_PROXY
	};

	UOMBObj(ObjType tp);
	void close();

	UOMBSign sign; //sign需要service子类填充好内容

	//UOMBObj子类需要实现从原始消息到UOMBCmd子类实例的解码过程。
	virtual UOMBCmd* decode_call(const uint8_t* raw, uint32_t sz)throw(UOMBException) = 0;
	virtual void ochannel_exception(UOChannel och, OChannelErrCode code, void* detail){};
	virtual void ichannel_exception(UIChannel ich, IChannelErrCode code, void* detail){};
	virtual void input_idle_timedout(){};

	void set_ichannel_timeout(UIChannel ich, uint64_t timeout);
	void set_input_timeout(uint64_t timeout);

	/*
	 * 调用对端的接口。
	 * call接口，使用ORBObj上的第一个channel发送。
	 * call_with_channel则指定一个之前存在的channel发送。
	 * call_with_new_channel从peer申请新的channel发送。
	 */
	void call(const UOMBCmd& call);
	void call_with_channel(const UOMBCmd& call, UOChannel och);
	UOChannel call_with_new_channel(const UOMBCmd& call);

	template<class SubClass>
	void set_next_step(void (SubClass::*next)(void*), void* ctx, void (SubClass::*ctx_free_fp)(void*));
	void go_on();

	UOMBPeerApp* get_peer() {return app;};
	const UOMBPeerApp* get_peer()const {return app;};

protected:
	friend class UOMBPeerApp;
	friend class UOMBPeer;
	UOMBPeerApp* app;
	ObjType type;

	struct InputIdleChecker : public UNaviEvent
	{
		UOMBObj& target;
		InputIdleChecker(UOMBObj& obj):
			UNaviEvent(),
			target(obj)
		{}
		virtual void process_event();
	};
	friend class InputIdleChecker;

	UNaviRef<InputIdleChecker> p_idle_checker;

	//返回false时，表示没有找到对应的处理方法
	bool process_cmd(UOMBCmd& call, UIChannel ch);

	virtual ~UOMBObj();
	virtual void clean_up(){};

	UOChannel first_ochannel;
	UIChannel first_ichannel;

	NextStep next_step;
	void* next_ctx;
	NextStep next_ctx_cleanup;

	void set_signature(const UOMBSign& sign)
	{
		this->sign = sign;
	}

	void _close_impl(bool recycle);

	static hash_map<std::string, CmdProc> cmd_map;
};

template<class SubClass>
void UOMBObj::set_next_step(void (SubClass::*next)(void*), void* ctx, void (SubClass::*ctx_free_fp)(void*))
{
	next_step = static_cast<NextStep>(next);
	next_ctx = ctx;
	next_ctx_cleanup = static_cast<NextStep>(ctx_free_fp);
}

template<class SubClass>
void UOMBObj::regist_cmd(const char* nm, void (SubClass::*method)(UOMBCmd&, UIChannel))
{
	std::string name = typeid(SubClass).name();
	name += ".";
	name += nm;
	cmd_map[name] = static_cast<CmdProc>(method);
}

UNAVI_NMSP_END


#endif /* UNAVIOROBJ_H_ */
