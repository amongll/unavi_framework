/*
 * UNaviHandler.h
 *
 *  Created on: 2014-9-25
 *      Author: dell
 */

#ifndef UNAVIHANDLER_H_
#define UNAVIHANDLER_H_

#include "unavi/UNaviCommon.h"
#include "unavi/core/UNaviCycle.h"

UNAVI_NMSP_BEGIN
class UNaviHandlerPipe;
class UNaviHandlerSPipe;
class UNaviUDPPipe;

class UNaviHandler
{
	friend class UNaviCycle;
public:
	void event_regist(UNaviEvent& ev);
	void event_quit(UNaviEvent& ev, uint32_t quit_mask=0xffffffff);

protected:
	UNaviHandler(UNaviCycle* cycle);
	virtual ~UNaviHandler();
	friend class UNaviHandlerPipe;
	friend class UNaviHandlerSPipe;
	UNaviCycle* cycle;
	uint16_t handler_id;
private:
	UNaviList ready_events;

	void process_events(); //子类调用该函数获得fired event的处理

	virtual void run_init() {};
	virtual void cleanup() {};

	virtual void process(UNaviUDPPipe& pipe){};
	virtual void process(UNaviHandlerPipe& pipe){};
	virtual void process(UNaviHandlerSPipe& pipe){};
};

UNAVI_NMSP_END

#endif /* UNAVIHANDLER_H_ */
