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
	CORE_ERROR, //������ʩ��������
	PROTO_ERROR, //����Э�������
	FRAME_ERROR, //Ӧ�ÿ�ܲ�����
	APP_ERROR, //Ӧ������
	UNKNOWN_ERROR //δ֪����
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
