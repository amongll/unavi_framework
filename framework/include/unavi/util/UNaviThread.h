/** \brief 
 * UNaviThread.h
 *  Created on: 2014-9-11
 *      Author: li.lei
 *  brief: 
 */

#ifndef UNAVITHREAD_H_
#define UNAVITHREAD_H_

#include "unavi/UNaviCommon.h"

UNAVI_NMSP_BEGIN

class UNaviThread
{
public:
	friend class UNaviServer;
	enum Status
	{
		INIT,
		RUNNING,
		STOPPED
	};
public:
	bool thread_start();
	void stop();
	bool thread_wait(int msec=100);

	virtual void run(void*) = 0;

	void* run_arg;

	Status thread_get_status();
protected:
	UNaviThread();
	virtual ~UNaviThread();
	bool canceled();
private:
	pthread_mutex_t status_lk;
	pthread_cond_t status_cond;
	Status _status;

	pthread_rwlock_t control_lk;
	bool ctrl_cancel;

	pthread_t tid;
	static void* thr_run(void* arg);

	UNaviThread(const UNaviThread&);
	UNaviThread& operator=(const UNaviThread&);
};

UNAVI_NMSP_END

#endif /* UNAVITHREAD_H_ */
