/*
 * UNaviIOSvc.h
 *
 *  Created on: 2014-9-24
 *      Author: dell
 */

#ifndef UNAVIIOSVC_H_
#define UNAVIIOSVC_H_

#include "unavi/core/UNaviUDPPipe.h"
#include "unavi/core/UNaviHandler.h"
#include "unavi/core/UNaviHandlerPipe.h"
#include "unavi/util/UNaviLock.h"

UNAVI_NMSP_BEGIN

class UNaviWorker;
class UNaviIOSvc;

class UNaviIOReader : public UNaviHandler
{
	friend class UNaviIOSvc;
	friend class UNaviUDPPipe;
private:
	UNaviIOReader(UNaviIOSvc& svc, UNaviCycle::Ref cycle);
	virtual ~UNaviIOReader();
	virtual void run_init();
	virtual void cleanup();

	virtual void process(UNaviHandlerPipe& pipe);

	uint64_t input_size;
	uint32_t input_pkts;
	double bandwidth;
	UNaviLock band_lock;

	UNaviIOSvc& svc;

	class IOSvcStatEv : public UNaviEvent
	{
	public:
		IOSvcStatEv(UNaviIOReader& reader):
			UNaviEvent(6000),
			target(reader),
			last_calc(0)
		{}
		virtual void process_event();
		UNaviIOReader& target;
		uint64_t last_calc;
	};
	friend class IOSvcStatEv;
	IOSvcStatEv* ev_stat;
};

class UNaviIOWriter : public UNaviHandler
{
	friend class UNaviIOSvc;
	friend class UNaviUDPPipe;
private:
	UNaviIOWriter(UNaviIOSvc& svc, UNaviCycle::Ref);
	virtual ~UNaviIOWriter();
	virtual void run_init();
	virtual void process(UNaviUDPPipe& pipe);
	virtual void cleanup();

	uint64_t output_size;
	uint32_t output_pkts;
	double bandwidth;//KBytes/sec
	UNaviLock band_lock;

	UNaviIOSvc& svc;
	class IOSvcStatEv : public UNaviEvent
	{
	public:
		IOSvcStatEv(UNaviIOWriter& writer):
			UNaviEvent(6000),
			target(writer),
			last_calc(0)
		{}
		virtual void process_event();
		UNaviIOWriter& target;
		uint64_t last_calc;
	};
	friend class IOSvcStatEv;
	IOSvcStatEv* ev_stat;
};


class UInputPProcor;

class UNaviIOSvc
{
	friend class UNaviWorker;
public:
	//todo: worker和iosvc在相同的线程中绑定的接口。每个iosvc被对应的worker专用。
	static UNaviIOSvc* new_svc(UNaviCycle::Ref r_cycle, UNaviCycle::Ref w_cycle,
		uint16_t mtu, uint8_t proto_id,const sockaddr* binded, int pp_thr=0);

	static void recycle_all_svc();

	static UNaviIOSvc* get_svc(uint16_t svc_id) {
		if (svc_id >= s_svc_id) return NULL;
		return svcs[svc_id];
	}

	void config_bandwidth_limit(uint32_t in_KBps, uint32_t out_KBps);

private:
	UNaviIOSvc(UNaviCycle::Ref r_cycle, UNaviCycle::Ref w_cycle,
		uint16_t mtu, uint8_t proto_id,const sockaddr* binded, int fd, int pp_thr);
	~UNaviIOSvc();
	static std::vector<UNaviIOSvc*> get_all();

	void process(UNaviUDPPipe& pipe);
	void process(UNaviHandlerPipe& pipe);

	friend class UNaviUDPPipe;
	uint16_t mtu;
	union {
		sockaddr sa_addr;
		sockaddr_in in_addr;
		sockaddr_in6 in6_addr;
	};

	friend class UNaviIOReader;
	friend class UNaviIOWriter;
	UNaviIOReader reader;
	UNaviIOWriter writer;

	UNaviLock band_config_lk;
	uint32_t inband_limit_KBPS;
	uint32_t outband_limit_KBPS;

	friend class UNaviCycle;
	uint16_t svc_id;
	uint8_t proto_id;
	UNaviUDPPipe *worker_pipes[UNAVI_MAX_WORKER];

	int socket_fd;

	friend class IOSvcReadEv;
	class IOSvcReadEv : public UNaviEvent
	{
	public:
		IOSvcReadEv(UNaviIOSvc& _svc):
			UNaviEvent(_svc.socket_fd,UNaviEvent::EV_READ,0),
			svc(_svc)
		{}
		virtual void process_event();
		UNaviIOSvc& svc;
	};

	IOSvcReadEv* ev_read;
	UNaviList free_pkts;
	size_t free_pkts_cnt;

	friend class UInputPProcor;
	uint32_t prev_cnt;
	std::vector<UInputPProcor*> prev_procs;
	std::vector<UNaviHandlerPipe*> prev_pipes;
	std::vector<UNaviCycle::Ref> prev_cycles;
	void init_prev_procs(int cnt);
	void destroy_prev_proces();

	iovec read_v[2];
	msghdr recv_hdr;
	unsigned char read_buf2[65536];

	static uint16_t s_svc_id;
	static UNaviIOSvc* svcs[UNAVI_MAX_IOSVC];

	void close_all_pipenotify();

	UNaviIOSvc(const UNaviIOSvc&);
	UNaviIOSvc& operator=(const UNaviIOSvc&);
};

struct UDPPrevEntry : public UHandlerPipeEntry
{
	UNaviProto::DispatchInfo disp_info;
	UNaviUDPPacket* udp;
	virtual void recycle();
};

class UInputPProcor : public UNaviHandler
{
	friend class UNaviIOSvc;
private:
	UInputPProcor(UNaviCycle::Ref cycle,UNaviIOSvc& svc);
	virtual ~UInputPProcor();
	virtual void run_init();
	virtual void cleanup();
	virtual void process(UNaviHandlerPipe& pipe);
	UNaviIOSvc& svc;
	UNaviProto* proto;
};

UNAVI_NMSP_END

#endif /* UNAVIIOSVC_H_ */
