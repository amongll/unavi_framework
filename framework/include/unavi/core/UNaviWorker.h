/*
 * UNaviWorker.h
 *
 *  Created on: 2014-9-24
 *      Author: dell
 */

#ifndef UNAVIWORKER_H_
#define UNAVIWORKER_H_

#include "unavi/core/UNaviUDPPipe.h"
#include "unavi/core/UNaviHandler.h"
#include "unavi/core/UNaviHandlerSPipe.h"
#include "unavi/core/UNaviHandlerPipe.h"
#include "unavi/core/UNaviProtoDeclare.h"

UNAVI_NMSP_BEGIN

class UOMBPeerShadow;
class UOMBPeerBusiness;
struct UOMBPeerBusiEntry: public UHandlerPipeEntry
{
	UOMBPeerBusiEntry();
	virtual ~UOMBPeerBusiEntry()
	{}
	UOMBPeerShadow* obj;
	UOMBPeerBusiness* busi;
	virtual void recycle();
};

class UNaviProto;
class UNaviIOSvc;
class UNaviWorker : public UNaviHandler
{
	friend class UNaviIOSvc;
public:
	static UNaviWorker* new_worker(UNaviCycle::Ref cycle);
	static void regist_event(UNaviEvent& timer);
	static void quit_event(UNaviEvent& timer, uint32_t quit_mask=0xffffffff);
	static uint32_t rand_proto_pipeid(uint8_t proto_id);
	static void current_bandwidth(uint32_t pipe_id, double& inband,double& outband,
		uint32_t &inband_limit_KBPS, uint32_t &outband_limit_KBPS);
	static int preset_mtu(uint32_t pipe_id);

	static int worker_count();
	static UNaviWorker* get_worker(uint16_t worker_id);
	static void recycle_all_workers();

	uint16_t id()const
	{
		return worker_id;
	}

	UNaviProto* proto(uint8_t protoid)
	{
		if ( protoid >= UNaviProtoTypes::max_protos)
			return NULL;
		return protos[protoid];
	}

	void push_peer_business(UOMBPeerShadow* obj, UOMBPeerBusiness* busi);

private:
	UNaviWorker(UNaviCycle::Ref cycle);
	~UNaviWorker();
	static std::vector<UNaviWorker*> get_all();

	friend class UNaviUDPPipe;
	virtual void run_init();
	virtual void process(UNaviUDPPipe& pipe);
	virtual void cleanup();
	//处理unavi运行时结构外部发起的OMBProxy对象。
	virtual void process(UNaviHandlerSPipe& pipe);

	uint16_t worker_id;
	UNaviUDPPipe* iosvc_pipes[UNAVI_MAX_IOSVC];
	UNaviProto* protos[UNaviProtoTypes::max_protos];
	std::vector<uint32_t> proto_pipes[UNaviProtoTypes::max_protos];

	UNaviHandlerSPipe *omb_peer_busi_pipe;

	static uint16_t s_worker_id;
	static UNaviWorker* s_workers[UNAVI_MAX_WORKER];

	UNaviWorker(const UNaviWorker&);
	UNaviWorker& operator=(const UNaviWorker&);
};

UNAVI_NMSP_END

#endif /* UNAVIWORKER_H_ */
