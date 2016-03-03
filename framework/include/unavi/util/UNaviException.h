/** \brief 
 * UNaviException.h
 *  Created on: 2014-9-4
 *      Author: li.lei
 *  brief: 
 */

#ifndef UNAVIEXCEPTION_H_
#define UNAVIEXCEPTION_H_

#include "unavi/UNaviCommon.h"
#include <exception>

#define UNAVI_THROW_DECLARE throw(std::exception)

UNAVI_NMSP_BEGIN

enum UNaviError
{
	CORE_ERROR, //基础设施层有问题
	PROTO_ERROR, //传输协议层问题
	FRAME_ERROR, //应用框架层问题
	APP_ERROR, //应用问题
	UNKNOWN_ERROR //未知问题
};

class UNaviException : public std::exception
{
public:
	UNaviException(UNaviError e);
	virtual ~UNaviException()throw() {};
	virtual const char* what()const throw();
	int code;
protected:
	void describe_code(int code, const char* code_desc=NULL);
	void describe_detail(const char* fmt, va_list vl);
private:
	static const char* type_str[UNKNOWN_ERROR + 1];
	UNaviError type;
	std::string reason;
	bool has_code;
};

UNAVI_NMSP_END

#endif /* UNAVIEXCEPTION_H_ */
