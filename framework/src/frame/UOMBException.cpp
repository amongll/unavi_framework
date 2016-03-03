/*
 * UOMBException.cpp
 *
 *  Created on: 2014-12-2
 *      Author: li.lei
 */

#include "unavi/frame/UOMBException.h"
#include <cstdarg>
#include "unavi/core/UNaviLog.h"

#include <iostream>

using namespace unavi;
using namespace std;

const char* UOMBException::err_desc[] = {
	"omb_abused_error",
	"omb_impl_error",
	"omb_sign_conflict",
	"omb_app_error",
	"omb_call_error",
	"omb_unknown_error"
};

UOMBException::UOMBException(UOMBError a):
	UNaviException(FRAME_ERROR),
	e(a)
{
	describe_code(e,err_desc[e]);
}
UOMBException::UOMBException(UOMBError a, const char* fmt, ...):
	UNaviException(FRAME_ERROR),
	e(a)
{
	describe_code(e,err_desc[e]);

	va_list vl;
	describe_detail(fmt,vl);
	va_end(vl);
}
