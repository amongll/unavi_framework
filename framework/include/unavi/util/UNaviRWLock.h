/*
 * UNaviRWLock.h
 *
 *  Created on: 2014-9-24
 *      Author: li.lei
 */

#ifndef UNAVIRWLOCK_H_
#define UNAVIRWLOCK_H_

#include "unavi/UNaviCommon.h"

UNAVI_NMSP_BEGIN

class UNaviRWLock
{
public:
	UNaviRWLock();
	~UNaviRWLock();

	void rlock();
	void wlock();
	void unlock();

	//todo: ¿çÆ½Ì¨
	pthread_rwlock_t impl;
};

class UNaviScopedRWLock
{
public:
	UNaviScopedRWLock(UNaviRWLock* lk,bool w=false):_lk(lk),is_w(false) {
		if (_lk) {
			if(w) {
				_lk->wlock();
				is_w = true;
			}
			else {
				_lk->rlock();
				is_w = false;
			}
		}
	}

	void reset() {
		if (_lk) _lk->unlock();
		_lk = NULL;
		is_w = false;
	}

	void upgrade() {
		if (_lk && is_w==false) {
			_lk->unlock();
			_lk->wlock();
		}
	}

	~UNaviScopedRWLock() {
		if (_lk) _lk->unlock();
	}

	UNaviRWLock* _lk;
	bool is_w;
};

UNAVI_NMSP_END

#endif /* UNAVIRWLOCK_H_ */
