/*
 * UNaviProtoFlag.cpp
 *
 *  Created on: 2014-9-23
 *      Author: dell
 */

#include "unavi/core/UNaviProtoDeclare.h"
#include "unavi/core/UNaviProto.h"
#include "unavi/core/UNaviCycle.h"
#include "unavi/core/UNaviWorker.h"

using namespace std;
using namespace unavi;

uint8_t UNaviProtoTypes::proto_id = 0;
UNaviProto* UNaviProtoTypes::protos[] = {NULL};

uint8_t UNaviProtoTypes::reg_proto(UNaviProto& single) {
	if (proto_id == max_protos) return 0xFF;
	int ret = proto_id++;
	protos[ret] = &single;
	return ret;
}

UNaviProto* UNaviProtoTypes::proto(uint8_t type_id) {
	if ( type_id >= max_protos) return NULL;
	return protos[type_id];
}

void UNaviProto::preproc(UNaviUDPPacket&)
{

}

UNaviProto* UNaviProto::get_worker_proto(uint8_t protoid)
{
	UNaviWorker* worker = UNaviCycle::worker();
	if (!worker) return NULL;
	return worker->proto(protoid);
}
