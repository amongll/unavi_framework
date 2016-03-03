/*
 * UOMBException.h
 *
 *  Created on: 2014-11-28
 *      Author: dell
 */

#ifndef UOMBEXCEPTION_H_
#define UOMBEXCEPTION_H_

#include "unavi/frame/UOMBCommon.h"
#include "unavi/util/UNaviException.h"

UNAVI_NMSP_BEGIN

enum UOMBError
{
	UOMB_ABUSED_ERROR,
	UOMB_IMPL_ERROR,
	UOMB_SIGN_ERROR,
	UOMB_APP_ERROR,
	UOMB_CALL_ERROR,
	UOMB_UKNOWN_ERROR
};

struct UOMBException: public UNaviException
{
	UOMBException(UOMBError e);
	UOMBException(UOMBError e, const char* fmt, ...);
	UOMBError e;
	static const char* err_desc[UOMB_UKNOWN_ERROR+1];
};

UNAVI_NMSP_END

#endif /* UOMBEXCEPTION_H_ */
