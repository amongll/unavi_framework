/*
 * UNaviRWLock.cpp
 *
 *  Created on: 2014-9-24
 *      Author: dell
 */

#include "unavi/util/UNaviRWLock.h"

using namespace std;
using namespace unavi;

UNaviRWLock::UNaviRWLock()
{
	pthread_rwlock_init(&impl,NULL);
}

UNaviRWLock::~UNaviRWLock()
{
	pthread_rwlock_destroy(&impl);
}

void UNaviRWLock::rlock()
{
	pthread_rwlock_rdlock(&impl);
}

void UNaviRWLock::wlock()
{
	pthread_rwlock_wrlock(&impl);
}

void UNaviRWLock::unlock()
{
	pthread_rwlock_unlock(&impl);
}



