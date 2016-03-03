/*
 * URtmfpExp.h
 *
 *  Created on: 2014-11-12
 *      Author: li.lei
 */

#ifndef URTMFPEXP_H_
#define URTMFPEXP_H_

#include "unavi/rtmfp/URtmfpCommon.h"
#include "unavi/util/UNaviException.h"

URTMFP_NMSP_BEGIN

enum URtmfpError
{
	URTMFP_INCHUNK_TYPE_ERROR,
	URTMFP_OCHUNK_TYPE_ERROR,
	URTMFP_ICHUNK_DECODE_ERROR,
	URTMFP_OCHUNK_ENCODE_ERROR,
	URTMFP_IDATACHUNK_ERROR,
	URTMFP_ABUSED_ERROR,
	URTMFP_IMPL_ERROR,
	URTMFP_PEER_COMM_ERROR,
	URTMFP_PEER_PROTO_ERROR,
	URTMFP_FLOW_RELATE_ERROR,
	URTMFP_PING_REPLY_ERROR,
	URTMFP_UNKNOWN_ERROR
};

struct URtmfpException : public UNaviException
{
	URtmfpException(URtmfpError rtmfp):
		UNaviException(PROTO_ERROR),
		e(rtmfp)
	{}
	virtual ~URtmfpException()throw(){};
	const char* what()const throw()
	{
		return err_desc[e];
	}
	URtmfpError e;
	static const char* err_desc[URTMFP_UNKNOWN_ERROR+1];
};

URTMFP_NMSP_END

#endif /* URTMFPEXP_H_ */
