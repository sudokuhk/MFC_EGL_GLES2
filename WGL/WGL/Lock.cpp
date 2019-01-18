#include "stdafx.h"

#include "Lock.h"
#include <assert.h>

/*** Static mutex and condition variable ***/
static mutex_t super_mutex;
static cond_t  super_variable;

/*** Mutexes ***/
void mutex_init(mutex_t *p_mutex)
{
	/* This creates a recursive mutex. This is OK as fast mutexes have
	 * no defined behavior in case of recursive locking. */
	InitializeCriticalSection(&p_mutex->mutex);
	p_mutex->dynamic = true;
}

void mutex_init_recursive(mutex_t *p_mutex)
{
	InitializeCriticalSection(&p_mutex->mutex);
	p_mutex->dynamic = true;
}


void mutex_destroy(mutex_t *p_mutex)
{
	assert(p_mutex->dynamic);
	DeleteCriticalSection(&p_mutex->mutex);
}

void mutex_lock(mutex_t *p_mutex)
{
	if (!p_mutex->dynamic)
	{   /* static mutexes */
		int canc = vlc_savecancel();
		assert(p_mutex != &super_mutex); /* this one cannot be static */

		vlc_mutex_lock(&super_mutex);
		while (p_mutex->locked)
		{
			p_mutex->contention++;
			vlc_cond_wait(&super_variable, &super_mutex);
			p_mutex->contention--;
		}
		p_mutex->locked = true;
		vlc_mutex_unlock(&super_mutex);
		vlc_restorecancel(canc);
		return;
	}

	EnterCriticalSection(&p_mutex->mutex);
}

int vlc_mutex_trylock(mutex_t *p_mutex)
{
	if (!p_mutex->dynamic)
	{   /* static mutexes */
		int ret = EBUSY;

		assert(p_mutex != &super_mutex); /* this one cannot be static */
		vlc_mutex_lock(&super_mutex);
		if (!p_mutex->locked)
		{
			p_mutex->locked = true;
			ret = 0;
		}
		vlc_mutex_unlock(&super_mutex);
		return ret;
	}

	return TryEnterCriticalSection(&p_mutex->mutex) ? 0 : EBUSY;
}

void vlc_mutex_unlock(mutex_t *p_mutex)
{
	if (!p_mutex->dynamic)
	{   /* static mutexes */
		assert(p_mutex != &super_mutex); /* this one cannot be static */

		vlc_mutex_lock(&super_mutex);
		assert(p_mutex->locked);
		p_mutex->locked = false;
		if (p_mutex->contention)
			vlc_cond_broadcast(&super_variable);
		vlc_mutex_unlock(&super_mutex);
		return;
	}

	LeaveCriticalSection(&p_mutex->mutex);
}

/*** Condition variables ***/
enum
{
	VLC_CLOCK_STATIC = 0, /* must be zero for VLC_STATIC_COND */
	VLC_CLOCK_MONOTONIC,
	VLC_CLOCK_REALTIME,
};

static void vlc_cond_init_common(vlc_cond_t *p_condvar, unsigned clock)
{
	/* Create a manual-reset event (manual reset is needed for broadcast). */
	p_condvar->handle = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!p_condvar->handle)
		abort();
	p_condvar->clock = clock;
}

void vlc_cond_init(vlc_cond_t *p_condvar)
{
	vlc_cond_init_common(p_condvar, VLC_CLOCK_MONOTONIC);
}

void vlc_cond_init_daytime(vlc_cond_t *p_condvar)
{
	vlc_cond_init_common(p_condvar, VLC_CLOCK_REALTIME);
}

void vlc_cond_destroy(vlc_cond_t *p_condvar)
{
	CloseHandle(p_condvar->handle);
}

void vlc_cond_signal(vlc_cond_t *p_condvar)
{
	if (!p_condvar->clock)
		return;

	/* This is suboptimal but works. */
	vlc_cond_broadcast(p_condvar);
}

void vlc_cond_broadcast(vlc_cond_t *p_condvar)
{
	if (!p_condvar->clock)
		return;

	/* Wake all threads up (as the event HANDLE has manual reset) */
	SetEvent(p_condvar->handle);
}

void vlc_cond_wait(vlc_cond_t *p_condvar, mutex_t *p_mutex)
{
	DWORD result;

	if (!p_condvar->clock)
	{   /* FIXME FIXME FIXME */
		msleep(50000);
		return;
	}

	do
	{
		vlc_testcancel();
		vlc_mutex_unlock(p_mutex);
		result = vlc_WaitForSingleObject(p_condvar->handle, INFINITE);
		vlc_mutex_lock(p_mutex);
	} while (result == WAIT_IO_COMPLETION);

	ResetEvent(p_condvar->handle);
}

int vlc_cond_timedwait(vlc_cond_t *p_condvar, mutex_t *p_mutex,
	mtime_t deadline)
{
	DWORD result;

	do
	{
		vlc_testcancel();

		mtime_t total;
		switch (p_condvar->clock)
		{
		case VLC_CLOCK_MONOTONIC:
			total = mdate();
			break;
		case VLC_CLOCK_REALTIME: /* FIXME? sub-second precision */
			total = CLOCK_FREQ * time(NULL);
			break;
		default:
			assert(!p_condvar->clock);
			/* FIXME FIXME FIXME */
			msleep(50000);
			return 0;
		}
		total = (deadline - total) / 1000;
		if (total < 0)
			total = 0;

		DWORD delay = (total > 0x7fffffff) ? 0x7fffffff : total;
		vlc_mutex_unlock(p_mutex);
		result = vlc_WaitForSingleObject(p_condvar->handle, delay);
		vlc_mutex_lock(p_mutex);
	} while (result == WAIT_IO_COMPLETION);

	ResetEvent(p_condvar->handle);

	return (result == WAIT_OBJECT_0) ? 0 : ETIMEDOUT;
}
