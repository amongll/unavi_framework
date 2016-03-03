/*
 * URtmfpFlow.h
 *
 *  Created on: 2014-10-20
 *      Author: li.lei
 */

#ifndef URTMFPFLOW_H_
#define URTMFPFLOW_H_

#include "unavi/rtmfp/URtmfpCommon.h"
#include "unavi/util/UNaviRef.h"
#include "unavi/rtmfp/URtmfpUtil.h"

URTMFP_NMSP_BEGIN

class URtmfpSession;

enum FlowRelateDir
{
	NO_RELATE,
	ACTIVE_RELATING,
	POSITIVE_RELATED
};

enum FlowDirect
{
	INPUT_FLOW,
	OUTPUT_FLOW
};

struct URtmfpFlow
{
	FlowDirect direct;
	uint32_t flow_id;
	URtmfpSession& session;
	URtmfpOptList opts;

protected:
	friend class UNaviRef<URtmfpFlow>;
	friend class URtmfpSession;
	URtmfpFlow(URtmfpSession& ss, uint32_t flowid, bool input=true):
		direct(input?INPUT_FLOW:OUTPUT_FLOW),
		flow_id(flowid),
		session(ss)
	{}
	virtual ~URtmfpFlow(){}
};

URTMFP_NMSP_END

#endif /* URTMFPFLOW_H_ */
