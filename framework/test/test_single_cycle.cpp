/*
 * test_cycle_single.cpp
 *
 *  Created on: 2014-9-28
 *      Author: dell
 */

#include "core/UNaviIOSvc.h"
#include "core/UNaviWorker.h"

using namespace unavi;

UNaviIOSvc* g_svc = NULL;
sockaddr* g_dst = NULL;

int g_send_once = 0;
int g_send_sz = 0;


class MyProto : public UNaviProto
{
public:
	MyProto():
		UNaviProto(),
		pipe_id(0xf)
	{

	}

	class MyTimer;

	virtual ~MyProto()
	{
		delete timer;
	}

	virtual void run_init() {
		timer = new MyTimer(*this);
		UNaviWorker::regist_event(*timer);
	}

	virtual DispatchInfo dispatch(UNaviUDPPacket& udp,bool) {
		static DispatchInfo ret = {DISP_OK, 0};
		return ret;
	}

	virtual void process(UNaviUDPPacket& udp) {
		udp.release_in_packet();
	}


	virtual void cleanup() {

	}
	virtual UNaviProto* copy() {
		return new MyProto;
	}

	uint32_t pipe_id;

	class MyTimer : public UNaviEvent
	{
	public:
		MyTimer(MyProto& _proto):
			UNaviEvent(1000),
			proto(_proto){};
		virtual void process_event() {
			if (proto.pipe_id==0xf) {
				proto.pipe_id = UNaviCycle::pipe_id(g_svc);
			}
			UNaviUDPPacket* pkt ;

			for (int i=0 ; i<g_send_once; i++ ) {
				pkt =UNaviUDPPacket::new_outpacket(proto.pipe_id);
				pkt->used = g_send_sz;
				pkt->output(*g_dst);
			}
			set_expire(1000);
		}

		MyProto& proto;
	};
	MyTimer* timer;
};

UNaviCycle::Ref g_cycle;

void my_sig_handler(int)
{
	g_cycle->stop();
}

int main(int arg,char* argv[])
{
	struct sigaction _act;

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

	UNaviProtoDeclare<MyProto>::declare();
	UNaviCycle::Ref cycle = UNaviCycle::new_cycle();
	g_cycle = cycle;
	sockaddr_in binded;
	binded.sin_family = AF_INET;
	binded.sin_port = htons(atoi(argv[1]));
	binded.sin_addr.s_addr = INADDR_ANY;
	g_svc = UNaviIOSvc::new_svc(cycle,cycle,1400,UNaviProtoDeclare<MyProto>::proto_id,
		(const sockaddr*)&binded);

	UNaviWorker* worker = UNaviWorker::new_worker(cycle);

	sockaddr_in *dst = new sockaddr_in;
	dst->sin_family = AF_INET;
	dst->sin_port = htons(atoi(argv[3]));
	::inet_pton(AF_INET,argv[2],&dst->sin_addr.s_addr);
	g_dst = (sockaddr*)dst;

	g_send_once = atoi(argv[4]);
	g_send_sz = atoi(argv[5]);

	cycle->run(NULL);

	UNaviWorker::recycle_all_workers();
	UNaviIOSvc::recycle_all_svc();
}
