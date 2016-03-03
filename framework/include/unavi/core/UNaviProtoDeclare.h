/*
 * UNaviProtoFlag.h
 *
 *  Created on: 2014-9-23
 *      Author: dell
 */

#ifndef UNAVIPROTODECLARE_H_
#define UNAVIPROTODECLARE_H_

#include "unavi/UNaviCommon.h"

UNAVI_NMSP_BEGIN

class UNaviProto;

class UNaviProtoTypes
{
public:
	static uint8_t reg_proto(UNaviProto& single);
	static uint8_t proto_count() {return proto_id;}
	static UNaviProto* proto(uint8_t type_id);
	static const uint8_t max_protos = 32;
private:
	static uint8_t proto_id;
	static UNaviProto* protos[max_protos];
};

template <class TProto>
class UNaviProtoDeclare
{
public:
	typedef TProto Proto;
	static void declare(){};
	static const uint8_t proto_id;
	static TProto proto;
};

template <class TProto>
TProto UNaviProtoDeclare<TProto>::proto;

template <class TProto>
const uint8_t UNaviProtoDeclare<TProto>::proto_id =
	UNaviProtoTypes::reg_proto(UNaviProtoDeclare<TProto>::proto);

UNAVI_NMSP_END

#endif /* UNAVIPROTOFLAG_H_ */
