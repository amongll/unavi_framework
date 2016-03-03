/*
 * UCoreException.cpp
 *
 *  Created on: 2014-12-19
 *      Author: li.lei
 */

#include "unavi/core/UCoreException.h"
#include <cstdarg>

using namespace unavi;
using namespace std;

const char* UCoreException::code_desc[] = {
	"sys_err",
	"impl_err",
	"abused_err",
	"unknown_err"
};

unavi::UCoreException::UCoreException(UCoreError code):
	UNaviException(CORE_ERROR),
	e(code)
{
	describe_code(e, code_desc[e]);
}

unavi::UCoreException::UCoreException(UCoreError code, const char* fmt, ...):
	UNaviException(CORE_ERROR),
	e(code)
{
	describe_code(e, code_desc[e]);
	va_list vl;
	va_start(vl,fmt);
	describe_detail(fmt,vl);
	va_end(vl);
}
