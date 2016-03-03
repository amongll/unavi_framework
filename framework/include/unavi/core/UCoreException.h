/*
 * UCoreException.h
 *
 *  Created on: 2014-12-19
 *      Author: dell
 */

#ifndef UCOREEXCEPTION_H_
#define UCOREEXCEPTION_H_

#include "unavi/util/UNaviException.h"

UNAVI_NMSP_BEGIN

enum UCoreError
{
	UCORE_SYS_ERROR,
	UCORE_IMPL_ERROR,
	UCORE_ABUSED_ERROR,
	UCORE_UNKNOWN_ERROR
};

struct UCoreException : public UNaviException
{
	UCoreException(UCoreError code);
	UCoreException(UCoreError code, const char* fmt,...);
	UCoreError e;
	static const char* code_desc[UCORE_UNKNOWN_ERROR+1];
};

UNAVI_NMSP_END

#endif /* UCOREEXCEPTION_H_ */
