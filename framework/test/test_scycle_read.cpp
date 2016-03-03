/*
 * test_scycle_read.cpp
 *
 *  Created on: 2014-9-29
 *      Author: dell
 */




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
		//timer = new MyTimer(*this);
		//UNaviWorker::regist_event(*timer);
	}

	virtual DispatchInfo dispatch(UNaviUDPPacket& udp, bool) {
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

			for (int i=0 ; i<2000; i++ ) {
				pkt =UNaviUDPPacket::new_outpacket(proto.pipe_id);
				pkt->used = 1024;
				pkt->output(*g_dst);
			}
			set_expire(1000);
		}

		MyProto& proto;
	};
	MyTimer* timer;
};

int main(int arg,char* argv[])
{
	UNaviProtoDeclare<MyProto>::declare();
	UNaviCycle::Ref cycle = UNaviCycle::new_cycle();
	sockaddr_in binded;
	binded.sin_family = AF_INET;
	binded.sin_port = htons(atoi(argv[1]));
	binded.sin_addr.s_addr = INADDR_ANY;
	g_svc = UNaviIOSvc::new_svc(cycle,cycle,1400,UNaviProtoDeclare<MyProto>::proto_id,
		(const sockaddr*)&binded);

	UNaviWorker* worker = UNaviWorker::new_worker(cycle);

	cycle->run(NULL);

	UNaviWorker::recycle_all_workers();
	UNaviIOSvc::recycle_all_svc();
}
