/*
 * UNaviLock.cpp
 *
 *  Created on: 2014-10-20
 *      Author: li.lei
 */

#include "unavi/util/UNaviLock.h"

using namespace unavi;

UNaviLock::UNaviLock()
{
	pthread_mutex_init(&impl,NULL);
}

UNaviLock::~UNaviLock()
{
	pthread_mutex_destroy(&impl);
}

void UNaviLock::lock()
{
	pthread_mutex_lock(&impl);
}

void UNaviLock::unlock()
{
	pthread_mutex_unlock(&impl);
}




