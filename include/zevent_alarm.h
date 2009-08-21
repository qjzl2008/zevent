#ifndef ZEVENT_ALARM_H
#define ZEVENT_ALARM_H

#include <sys/time.h>
#define RETSIGTYPE void

typedef void (ZEVENTAlarmCallback) (unsigned int clientreg, void *clientarg);

/*
 * * alarm flags
 * */
#define SA_REPEAT 0x01          /* keep repeating every X seconds */

struct zevent_alarm {
	struct timeval t;
	unsigned int flags;
	unsigned int clientreg;
	struct timeval t_last;
	struct timeval t_next;
	void *clientarg;
	ZEVENTAlarmCallback *thecallback;
	struct zevent_alarm *next;
};

/*
 * * the ones you should need
 * */
void zevent_alarm_unregister(unsigned int clientreg);
void zevent_alarm_unregister_all(void);
unsigned int zevent_alarm_register(unsigned int when,
		unsigned int flags,
		ZEVENTAlarmCallback * thecallback,
		void *clientarg);

unsigned int zevent_alarm_register_hr(struct timeval t,
		unsigned int flags,
		ZEVENTAlarmCallback * cb, void *cd);

/*
 * * the ones you shouldn't
 * */
int init_alarm_post_config();
void sa_update_entry(struct zevent_alarm *alrm);
struct zevent_alarm *sa_find_next(void);
void run_alarms(void);
RETSIGTYPE alarm_handler(int a);
void set_an_alarm(void);
int get_next_alarm_delay_time(struct timeval *delta);

#endif
