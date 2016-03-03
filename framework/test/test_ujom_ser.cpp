/*
 * test_cycle_single.cpp
 *
 *  Created on: 2014-9-28
 *      Author: dell
 */

#include "core/UNaviIOSvc.h"
#include "core/UNaviWorker.h"
#include "unavi/ujom/UJOMExmpleA.h"

using namespace unavi;

UNaviIOSvc* g_svc = NULL;
sockaddr* g_dst = NULL;
UNaviCycle::Ref g_cycle;

void my_sig_handler(int)
{
	g_cycle->stop();
}

int main(int arg,char* argv[])
{
	if ( arg < 2 ) return 0;
	struct sigaction _act;
	ujom::UJOMExmpleA::declare();

	_act.sa_handler=my_sig_handler;
	sigemptyset(&_act.sa_mask);
	sigaddset(&_act.sa_mask,SIGINT);
	sigaddset(&_act.sa_mask,SIGTERM);
	sigaddset(&_act.sa_mask,SIGQUIT);
	sigaddset(&_act.sa_mask,SIGPIPE);
	_act.sa_flags=SA_SIGINFO;

	if(-1==sigaction(SIGINT,&_act,NULL))
	{
		return 0;
	}
	if(-1==sigaction(SIGQUIT,&_act,NULL))
	{
		return 0;
	}
	if(-1==sigaction(SIGTERM,&_act,NULL))
	{
		return 0;
	}
	_act.sa_handler=SIG_IGN;
	if(-1==sigaction(SIGPIPE,&_act,NULL))
	{
		return 0;
	}

	UNaviLogInfra::set_universal_min_level(unavi::LOG_DEBUG);

	UNaviCycle::Ref cycle = UNaviCycle::new_cycle();
	g_cycle = cycle;
	sockaddr_in binded;
	binded.sin_family = AF_INET;
	binded.sin_port = htons(atoi(argv[1]));
	binded.sin_addr.s_addr = INADDR_ANY;
	uint8_t proto_id = UOMBSvcDeclare<ujom::UJOMExmpleA>::proto_id;
	g_svc = UNaviIOSvc::new_svc(cycle,cycle,1400,proto_id,
		(const sockaddr*)&binded);
	UNaviWorker* worker = UNaviWorker::new_worker(cycle);
	cycle->thread_start();
	cycle->thread_wait(0);
	UNaviWorker::recycle_all_workers();
	UNaviIOSvc::recycle_all_svc();
}



