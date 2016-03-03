/** \brief 
 * UNaviThread.cpp
 *  Created on: 2014-9-10
 *      Author: li.lei
 *  brief: 
 */

#include "unavi/util/UNaviThread.h"

using namespace std;
using namespace unavi;

UNaviThread::UNaviThread():
	_status(INIT),
	ctrl_cancel(false),
	run_arg(NULL)
{
	pthread_mutex_init(&status_lk,NULL);
	pthread_cond_init(&status_cond,NULL);
	pthread_rwlock_init(&control_lk,NULL);
}

UNaviThread::~UNaviThread()
{
	pthread_mutex_destroy(&status_lk);
	pthread_rwlock_destroy(&control_lk);
	pthread_cond_destroy(&status_cond);
}

bool UNaviThread::thread_start()
{
	if(0 != pthread_create(&tid, NULL, (void* (*)(void*))(UNaviThread::thr_run), this))
	{
		return false;
	}
	pthread_mutex_lock(&status_lk);
	while(_status==INIT)
	{
		pthread_cond_wait(&status_cond,&status_lk);
	}
	pthread_mutex_unlock(&status_lk);
	return true;
}

void UNaviThread::stop()
{
	pthread_rwlock_wrlock(&control_lk);
	ctrl_cancel = true;
	pthread_rwlock_unlock(&control_lk);
}

bool UNaviThread::thread_wait(int msec)
{
	Status ret;
	pthread_mutex_lock(&status_lk);
	struct timeval tv;
	struct timespec ts;
	if (msec<0) msec = 0;

	if ( msec == 0 ) {
		pthread_cond_wait(&status_cond,&status_lk);
		ret = _status;
		pthread_mutex_unlock(&status_lk);
	}
	else {
		while(msec--) {
			gettimeofday(&tv,NULL);
			if (tv.tv_usec==999999) {
				ts.tv_sec = tv.tv_sec+1;
				ts.tv_nsec = 0;
			}
			else {
				ts.tv_sec = tv.tv_sec;
				ts.tv_nsec = (tv.tv_usec+1)*1000;
			}
			pthread_cond_timedwait(&status_cond,&status_lk,&ts);
			ret = _status;
			pthread_mutex_unlock(&status_lk);
			if (ret==STOPPED)
				break;
		};
	}

	if (ret == STOPPED) {
		pthread_join(tid,NULL);
	}
	return ret == STOPPED;
}

bool UNaviThread::canceled()
{
	bool ret = false;
	pthread_rwlock_rdlock(&control_lk);
	ret = ctrl_cancel;
	pthread_rwlock_unlock(&control_lk);
	return ret;
}

UNaviThread::Status UNaviThread::thread_get_status()
{
	Status ret;
	pthread_mutex_lock(&status_lk);
	ret = _status;
	pthread_mutex_unlock(&status_lk);
	return ret;
}

void* unavi::UNaviThread::thr_run(void* arg)
{
	UNaviThread* self = (UNaviThread*)arg;
	self->tid = pthread_self();
	pthread_mutex_lock(&self->status_lk);
	self->_status = UNaviThread::RUNNING;
	pthread_cond_signal(&self->status_cond);
	pthread_mutex_unlock(&self->status_lk);

	self->run(self->run_arg);

	pthread_mutex_lock(&self->status_lk);
	self->_status = UNaviThread::STOPPED;
	pthread_cond_signal(&self->status_cond);
	pthread_mutex_unlock(&self->status_lk);

	return NULL;
}
