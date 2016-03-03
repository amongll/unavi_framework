/*
 * UNaviProto.h
 *
 *  Created on: 2014-9-23
 *      Author: li.lei
 */

#ifndef UNAVIPROTO_H_
#define UNAVIPROTO_H_

#include "unavi/UNaviCommon.h"
#include "unavi/core/UNaviProtoDeclare.h"

UNAVI_NMSP_BEGIN

class UNaviUDPPacket;

class UNaviProto
{
public:
	enum DispatchType {
		DISP_PRE_PROC,
		DISP_OK,
		DISP_DROP
	};

	struct DispatchInfo {
		DispatchType type;
		uint32_t session_disting;
	};

public:
	UNaviProto(){}
	virtual ~UNaviProto() {}
	UNaviProto(const UNaviProto& r){}


	virtual void run_init() = 0;

	//preproc��process�϶��ڲ�ͬ��proto�����ϱ�����
	//preproc���ܺ�dispatch��ͬһ��proto�ϱ����ã�Ҳ���ܲ���ͬһ����
	virtual void preproc(UNaviUDPPacket& udp); //Ԥ����
	//dispatch��process�϶�����ͬһ��proto�����ϱ�����
	virtual DispatchInfo dispatch(UNaviUDPPacket& udp, bool pre_proced = false) = 0;

	virtual void process(UNaviUDPPacket& udp) = 0 ;
	virtual void cleanup() = 0;
	virtual UNaviProto* copy() = 0;
	static UNaviProto* get_worker_proto(uint8_t protoid);
private:
	UNaviProto& operator=(const UNaviProto&);
};

UNAVI_NMSP_END

#endif /* UNAVIPROTO_H_ */
