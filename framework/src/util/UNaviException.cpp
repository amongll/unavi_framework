/** \brief 
 * UNaviException.cpp
 *  Created on: 2014-9-12
 *      Author: li.lei
 *  brief: 
 */

#include "unavi/util/UNaviException.h"
#include <errno.h>

using namespace std;
using namespace unavi;

const char* UNaviException::type_str[] = {
	"core error",
	"proto error",
	"frame error",
	"app error",
	"unknown type error"
};

unavi::UNaviException::UNaviException(UNaviError e):
	type(e),
	has_code(false),
	code(-1)
{
	reason = "!!Exception<";
	reason += type_str[e];
	reason += ">";
}

const char* unavi::UNaviException::what() const  throw()
{
	return reason.c_str();
}

void unavi::UNaviException::describe_code(int code_, const char* desc)
{
	char buf[256];
	if(desc)
		snprintf(buf, sizeof(buf), "<code:%d %s>", code_, desc);
	else
		snprintf(buf, sizeof(buf), "<code:%d>",code_);
	reason += buf;
	code = code_;
	has_code = true;
}

void unavi::UNaviException::describe_detail(const char* fmt, va_list vl)
{
	char buf[1024];
	vsnprintf(buf,sizeof(buf),fmt,vl);

	if(has_code==false){
		reason += "<code:nil>";
	}
	reason += buf;
}
