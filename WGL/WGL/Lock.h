#ifndef __LOCK_H__
#define __LOCK_H__

/*** Condition variables ***/
enum
{
	CLOCK_STATIC = 0, /* must be zero for VLC_STATIC_COND */
	CLOCK_MONOTONIC,
	CLOCK_REALTIME,
};

typedef struct
{
	bool dynamic;
	union
	{
		struct
		{
			bool locked;
			unsigned long contention;
		};
		CRITICAL_SECTION mutex;
	};
} mutex_t;

#define VLC_STATIC_MUTEX { false, { { false, 0 } } }

typedef struct
{
	HANDLE   handle;
	unsigned clock;
} cond_t;

#define STATIC_COND { 0, 0 }
#endif