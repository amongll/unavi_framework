/*
 * test_cycle_single.cpp
 *
 *  Created on: 2014-9-28
 *      Author: dell
 */

#include "core/UNaviIOSvc.h"
#include "core/UNaviWorker.h"
#include <openssl/aes.h>

using namespace unavi;

sockaddr* g_dst = NULL;

int g_send_once = 0;
int g_send_sz = 0;
UNaviIOSvc* g_svc = NULL;

class MyProto : public UNaviProto
{
public:
	MyProto():
		UNaviProto(),
		pipe_id(0xf),
		timer(NULL)
	{
		AES_set_encrypt_key((const unsigned char*)"test", 0x80, &enc_key);
		AES_set_decrypt_key((const unsigned char*)"test", 0x80, &dec_key);
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

	virtual void preproc(UNaviUDPPacket& udp)
	{
		unsigned char iv[32];
		memset(iv,0x00,sizeof(iv));
		unsigned char tmpbuf[1024];
		memset(tmpbuf,0x00,1024);
		AES_cbc_encrypt((const unsigned char*)udp.buf,tmpbuf,1024,&dec_key,iv,0);
		memcpy(udp.buf,tmpbuf,1024);
	}

	virtual DispatchInfo dispatch(UNaviUDPPacket& udp,bool pre_proced) {
		static int i=0;
		static DispatchInfo ret = {DISP_PRE_PROC, 0};
		static DispatchInfo ret2 = {DISP_OK, i++%10};
		static DispatchInfo drop = {DISP_DROP, 0};

		//if ( rand()%1000 == 0)
		//	return drop;
		//memcpy(udp.buf,tmpbuf,1024);
		return pre_proced?ret2:ret;
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
				pkt->used = 1024;
				unsigned char iv[32];
				memset(iv,0x00,sizeof(iv));
				unsigned char tmpbuf[1024];
				AES_cbc_encrypt((const unsigned char*)pkt->buf,tmpbuf,1024,&proto.enc_key,iv,1);
				//memcpy(pkt->buf,tmpbuf,1024);
				pkt->output(*g_dst);
			}
			set_expire(1000);
		}

		MyProto& proto;
	};
	MyTimer* timer;

	AES_KEY enc_key;
	AES_KEY dec_key;
};

UNaviCycle::Ref g_cycle[2];
UNaviCycle::Ref g_wcycle;
UNaviCycle::Ref g_rcycle;

void my_sig_handler(int)
{
	g_cycle[0]->stop();
	g_cycle[1]->stop();
	g_wcycle->stop();
	g_rcycle->stop();
}

int main(int arg,char* argv[])
{
	struct sigaction _act;
	memset(&_act,0x00,sizeof(struct sigaction));

	_act.sa_handler=my_sig_handler;
	sigemptyset(&_act.sa_mask);
	sigaddset(&_act.sa_mask,SIGINT);
	sigaddset(&_act.sa_mask,SIGTERM);
	sigaddset(&_act.sa_mask,SIGQUIT);
	sigaddset(&_act.sa_mask,SIGPIPE);
	//_act.sa_flags=SA_RESTART;

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
	g_wcycle = UNaviCycle::new_cycle();
	g_rcycle = UNaviCycle::new_cycle();
	g_cycle[0] = UNaviCycle::new_cycle();
	g_cycle[1] = UNaviCycle::new_cycle();
	sockaddr_in binded;
	binded.sin_family = AF_INET;
	binded.sin_port = htons(atoi(argv[1]));
	binded.sin_addr.s_addr = INADDR_ANY;
	g_svc = UNaviIOSvc::new_svc(g_rcycle,g_wcycle,1400,UNaviProtoDeclare<MyProto>::proto_id,
		(const sockaddr*)&binded, 4);

	UNaviWorker::new_worker(g_cycle[0]);
	UNaviWorker::new_worker(g_cycle[1]);

	sockaddr_in *dst = new sockaddr_in;
	dst->sin_family = AF_INET;
	dst->sin_port = htons(atoi(argv[3]));
	::inet_pton(AF_INET,argv[2],&dst->sin_addr.s_addr);
	g_dst = (sockaddr*)dst;

	g_send_once = atoi(argv[4]);
	g_send_sz = atoi(argv[5]);

	g_wcycle->thread_start();
	g_rcycle->thread_start();
	g_cycle[1]->thread_start();
	g_cycle[0]->run(NULL);

	bool st=false;
	do {
	 st = g_wcycle->thread_wait(10000);
	} while(st==false);

	do {
	 st = g_rcycle->thread_wait(10000);
	} while(st==false);

	do {
	 st = g_cycle[1]->thread_wait(10000);
	} while(st==false);

	UNaviWorker::recycle_all_workers();
	UNaviIOSvc::recycle_all_svc();
}
