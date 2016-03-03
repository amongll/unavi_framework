/*
 * UNaviRWLock.h
 *
 *  Created on: 2014-9-24
 *      Author: li.lei
 */

#ifndef UNAVILOCK_H_
#define UNAVILOCK_H_

#include "unavi/UNaviCommon.h"

UNAVI_NMSP_BEGIN

class UNaviLock
{
public:
	UNaviLock();
	~UNaviLock();

	void lock();
	void unlock();

	//todo: ¿çÆ½Ì¨
	pthread_mutex_t impl;
};

class UNaviScopedLock
{
public:
	UNaviScopedLock(UNaviLock* lk):_lk(lk) {
		if (_lk) {
			_lk->lock();
		}
	}

	void reset() {
		if (_lk) _lk->unlock();
		_lk = NULL;
	}

	~UNaviScopedLock() {
		if (_lk) _lk->unlock();
	}

	UNaviLock* _lk;
};

UNAVI_NMSP_END

#endif /* UNAVIRWLOCK_H_ */
