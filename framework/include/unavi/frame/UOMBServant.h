/*
 * UNaviORServant.h
 *
 *  Created on: 2014-11-26
 *      Author: dell
 */

#ifndef UNAVIORBSERVANT_H_
#define UNAVIORBSERVANT_H_
#include "unavi/frame/UOMBCommon.h"
#include "unavi/frame/UOMBObj.h"
#include "unavi/frame/UOMBCmd.h"
#include "unavi/frame/UOMBException.h"


UNAVI_NMSP_BEGIN

/*
 * UOMBServant��UOMBProxy�Ĵ󲿷ֹ����ڸ���UOMBObj�С�
 * ��ͬ���������Ǳ���ʼ�����ķ�ʽ��
 */
class UOMBServant: public UOMBObj
{
public:
	UOMBServant():
		UOMBObj(UOMBObj::OMB_SERVANT)
	{}
	//������Դӳ�ʼǩ���л��һЩ��Ϣ������ȥ������ǰ׺֮��Ķ�����
	virtual void parse_signature(const UOMBSign& sign){}
};

UNAVI_NMSP_END


#endif /* UNAVIORSERVANT_H_ */
