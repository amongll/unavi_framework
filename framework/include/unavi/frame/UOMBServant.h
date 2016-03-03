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
 * UOMBServant和UOMBProxy的大部分共性在父类UOMBObj中。
 * 不同点在于他们被初始创建的方式。
 */
class UOMBServant: public UOMBObj
{
public:
	UOMBServant():
		UOMBObj(UOMBObj::OMB_SERVANT)
	{}
	//子类可以从初始签名中获得一些信息，比如去除类型前缀之后的对象标记
	virtual void parse_signature(const UOMBSign& sign){}
};

UNAVI_NMSP_END


#endif /* UNAVIORSERVANT_H_ */
