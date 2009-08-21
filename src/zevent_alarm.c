#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>

#include <sys/time.h>
#include <time.h>

#include "zevent_alarm.h"

#define ZEVENT_MALLOC_STRUCT(s)   (struct s *) calloc(1, sizeof(struct s))

static struct zevent_alarm *thealarms = NULL;
static int start_alarms = 0;
static unsigned int regnum = 1;

int init_alarm_post_config()
{
	start_alarms = 1;
	set_an_alarm();
	return 0;
}

void sa_update_entry(struct zevent_alarm *a)
{
	if(a->t_last.tv_sec == 0 && a->t_last.tv_usec == 0) {
		struct timeval t_now;

		/*
		 *          * Never been called yet, call time `t' from now.  
		 *                   */
		gettimeofday(&t_now, NULL);

		a->t_last.tv_sec = t_now.tv_sec;
		a->t_last.tv_usec = t_now.tv_usec;

		a->t_next.tv_sec = t_now.tv_sec + a->t.tv_sec;
		a->t_next.tv_usec = t_now.tv_usec + a->t.tv_usec;

		while(a->t_next.tv_usec >= 1000000) {
			a->t_next.tv_usec -= 1000000;
			a->t_next.tv_sec += 1;
		}
	}
	else if(a->t_next.tv_sec == 0 && a->t_next.tv_usec == 0) {
		/*
		 *          * We've been called but not reset for the next call.  
		 *                   */
		if(a->flags & SA_REPEAT) {
			if(a->t.tv_sec == 0 && a->t.tv_usec == 0) {
				printf("update_entry: illegal interval specified\n");
				zevent_alarm_unregister(a->clientreg);
				return;
			}

			a->t_next.tv_sec = a->t_last.tv_sec + a->t.tv_sec;
			a->t_next.tv_usec = a->t_last.tv_usec + a->t.tv_usec;

			while(a->t_next.tv_usec >= 1000000) {
				a->t_next.tv_usec -= 1000000;
				a->t_next.tv_sec += 1;
			}
		}
		else {
			/*
			 *              * Single time call, remove it.  
			 *                           */
			zevent_alarm_unregister(a->clientreg);
		}
	}
}

/**
 * * This function removes the callback function from a list of registered
 * * alarms, unregistering the alarm.
 * *
 * * @param clientreg is a unique unsigned integer representing a registered
 * *        alarm which the client wants to unregister.
 * *
 * * @return void
 * *
 * * @see zevent_alarm_register
 * * @see zevent_alarm_register_hr
 * * @see zevent_alarm_unregister_all
 * */
void zevent_alarm_unregister(unsigned int clientreg)
{
	struct zevent_alarm *sa_ptr, **prevNext = &thealarms;

	for(sa_ptr = thealarms;
			sa_ptr != NULL && sa_ptr->clientreg != clientreg;
			sa_ptr = sa_ptr->next) {
		prevNext = &(sa_ptr->next);
	}

	if(sa_ptr != NULL) {
		*prevNext = sa_ptr->next;
		printf("unregistered alarm %d\n", sa_ptr->clientreg);
		/*
		 *          * Note:  do not free the clientarg, its the clients responsibility
		 *                   */
		free(sa_ptr);
	}
	else {
		printf("no alarm %d to unregister\n", clientreg);
	}
}

/**
 * * This function unregisters all alarms currently stored.
 * *
 * * @return void
 * *
 * * @see zevent_alarm_register
 * * @see zevent_alarm_register_hr
 * * @see zevent_alarm_unregister
 * */
void zevent_alarm_unregister_all(void)
{
	struct zevent_alarm *sa_ptr, *sa_tmp;

	for(sa_ptr = thealarms; sa_ptr != NULL; sa_ptr = sa_tmp) {
		sa_tmp = sa_ptr->next;
		free(sa_ptr);
	}
	printf("ALL alarms unregistered\n");
	thealarms = NULL;
}

struct zevent_alarm *sa_find_next(void)
{
	struct zevent_alarm *a, *lowest = NULL;

	for(a = thealarms; a != NULL; a = a->next) {
		if(lowest == NULL) {
			lowest = a;
		}
		else if(a->t_next.tv_sec == lowest->t_next.tv_sec) {
			if(a->t_next.tv_usec < lowest->t_next.tv_usec) {
				lowest = a;
			}
		}
		else if(a->t_next.tv_sec < lowest->t_next.tv_sec) {
			lowest = a;
		}
	}
	return lowest;
}

struct zevent_alarm *sa_find_specific(unsigned int clientreg)
{
	struct zevent_alarm *sa_ptr;

	for(sa_ptr = thealarms; sa_ptr != NULL; sa_ptr = sa_ptr->next) {
		if(sa_ptr->clientreg == clientreg) {
			return sa_ptr;
		}
	}
	return NULL;
}

void run_alarms(void)
{
	int done = 0;
	struct zevent_alarm *a = NULL;
	unsigned int clientreg;
	struct timeval t_now;

	/*
	 *      * Loop through everything we have repeatedly looking for the next thing to
	 *           * call until all events are finally in the future again.  
	 *                */

	while(!done) {
		if((a = sa_find_next()) == NULL) {
			return;
		}

		gettimeofday(&t_now, NULL);

		if((a->t_next.tv_sec < t_now.tv_sec) ||
				((a->t_next.tv_sec == t_now.tv_sec) &&
				 (a->t_next.tv_usec < t_now.tv_usec))) {
			clientreg = a->clientreg;
			printf("run alarm %d\n", clientreg);
			(*(a->thecallback)) (clientreg, a->clientarg);
			printf("alarm %d completed\n", clientreg);

			if((a = sa_find_specific(clientreg)) != NULL) {
				a->t_last.tv_sec = t_now.tv_sec;
				a->t_last.tv_usec = t_now.tv_usec;
				a->t_next.tv_sec = 0;
				a->t_next.tv_usec = 0;
				sa_update_entry(a);
			}
			else {
				printf("alarm %d deleted itself\n", clientreg);
			}
		}
		else {
			done = 1;
		}
	}
}



RETSIGTYPE alarm_handler(int a)
{
	run_alarms();
	set_an_alarm();
}



int get_next_alarm_delay_time(struct timeval *delta)
{
	struct zevent_alarm *sa_ptr;
	struct timeval t_diff, t_now;

	sa_ptr = sa_find_next();

	if(sa_ptr) {
		gettimeofday(&t_now, 0);

		if((t_now.tv_sec > sa_ptr->t_next.tv_sec) ||
				((t_now.tv_sec == sa_ptr->t_next.tv_sec) &&
				 (t_now.tv_usec > sa_ptr->t_next.tv_usec))) {
			/*
			 *              * Time has already passed.  Return the smallest possible amount of
			 *                           * time.  
			 *                                        */
			delta->tv_sec = 0;
			delta->tv_usec = 1;
			return sa_ptr->clientreg;
		}
		else {
			/*
			 *              * Time is still in the future.  
			 *                           */
			t_diff.tv_sec = sa_ptr->t_next.tv_sec - t_now.tv_sec;
			t_diff.tv_usec = sa_ptr->t_next.tv_usec - t_now.tv_usec;

			while(t_diff.tv_usec < 0) {
				t_diff.tv_sec -= 1;
				t_diff.tv_usec += 1000000;
			}

			delta->tv_sec = t_diff.tv_sec;
			delta->tv_usec = t_diff.tv_usec;
			return sa_ptr->clientreg;
		}
	}

	/*
	 *      * Nothing Left.  
	 *           */
	return 0;
}


void set_an_alarm(void)
{
	struct timeval delta;
	int nextalarm = get_next_alarm_delay_time(&delta);

	/*
	 *      * We don't use signals if they asked us nicely not to.  It's expected
	 *           * they'll check the next alarm time and do their own calling of
	 *                * run_alarms().  
	 *                     */

	if(nextalarm) {
		struct itimerval it;

		it.it_value.tv_sec = delta.tv_sec;
		it.it_value.tv_usec = delta.tv_usec;
		it.it_interval.tv_sec = 0;
		it.it_interval.tv_usec = 0;

		signal(SIGALRM, alarm_handler);
		setitimer(ITIMER_REAL, &it, NULL);
		printf("schedule alarm %d in %d.%03d seconds\n",
				nextalarm, (int) delta.tv_sec,
				(int) (delta.tv_usec / 1000));

	}
	else {
		printf("no alarms found to schedule\n");
	}
}


/**
 * * This function registers function callbacks to occur at a speciifc time
 * * in the future.
 * *
 * * @param when is an unsigned integer specifying when the callback function
 * *             will be called in seconds.
 * *
 * * @param flags is an unsigned integer that specifies how frequent the callback
 * *        function is called in seconds.  Should be SA_REPEAT or 0.  If  
 * *        flags  is  set with SA_REPEAT, then the registered callback function
 * *        will be called every SA_REPEAT seconds.  If flags is 0 then the
 * *        function will only be called once and then removed from the
 * *        registered alarm list.
 * *
 * * @param thecallback is a pointer ZEVENTAlarmCallback which is the callback
 * *        function being stored and registered.
 * *
 * * @param clientarg is a void pointer used by the callback function.  This
 * *        pointer is assigned to zevent_alarm->clientarg and passed into the
 * *        callback function for the client's specifc needs.
 * *
 * * @return Returns a unique unsigned integer(which is also passed as the first
 * *        argument of each callback), which can then be used to remove the
 * *        callback from the list at a later point in the future using the
 * *        zevent_alarm_unregister() function.  If memory could not be allocated
 * *        for the zevent_alarm struct 0 is returned.
 * *
 * * @see zevent_alarm_unregister
 * * @see zevent_alarm_register_hr
 * * @see zevent_alarm_unregister_all
 * */
	unsigned int
zevent_alarm_register(unsigned int when, unsigned int flags,
		ZEVENTAlarmCallback * thecallback, void *clientarg)
{
	struct zevent_alarm **sa_pptr;

	if(thealarms != NULL) {
		for(sa_pptr = &thealarms; (*sa_pptr) != NULL;
				sa_pptr = &((*sa_pptr)->next)) ;
	}
	else {
		sa_pptr = &thealarms;
	}

	*sa_pptr = ZEVENT_MALLOC_STRUCT(zevent_alarm);
	if(*sa_pptr == NULL)
		return 0;

	if(0 == when) {
		(*sa_pptr)->t.tv_sec = 0;
		(*sa_pptr)->t.tv_usec = 1;
	}
	else {
		(*sa_pptr)->t.tv_sec = when;
		(*sa_pptr)->t.tv_usec = 0;
	}
	(*sa_pptr)->flags = flags;
	(*sa_pptr)->clientarg = clientarg;
	(*sa_pptr)->thecallback = thecallback;
	(*sa_pptr)->clientreg = regnum++;
	(*sa_pptr)->next = NULL;
	sa_update_entry(*sa_pptr);

	printf("registered alarm %d, t = %d.%03d, flags=0x%02x\n",
			(*sa_pptr)->clientreg, (int) (*sa_pptr)->t.tv_sec,
			(int) ((*sa_pptr)->t.tv_usec / 1000), (*sa_pptr)->flags);

	if(start_alarms){
		set_an_alarm();
	}

	return (*sa_pptr)->clientreg;
}


/**
 * * This function offers finer granularity as to when the callback
 * * function is called by making use of t->tv_usec value forming the
 * * "when" aspect of zevent_alarm_register().
 * *
 * * @param t is a timeval structure used to specify when the callback
 * *        function(alarm) will be called.  Adds the ability to specify
 * *        microseconds.  t.tv_sec and t.tv_usec are assigned
 * *        to zevent_alarm->tv_sec and zevent_alarm->tv_usec respectively internally.
 * *        The zevent_alarm_register function only assigns seconds(it's when
 * *        argument).
 * *
 * * @param flags is an unsigned integer that specifies how frequent the callback
 * *        function is called in seconds.  Should be SA_REPEAT or NULL.  If  
 * *        flags  is  set with SA_REPEAT, then the registered callback function
 * *        will be called every SA_REPEAT seconds.  If flags is NULL then the
 * *        function will only be called once and then removed from the
 * *        registered alarm list.
 * *
 * * @param cb is a pointer ZEVENTAlarmCallback which is the callback
 * *        function being stored and registered.
 * *
 * * @param cd is a void pointer used by the callback function.  This
 * *        pointer is assigned to zevent_alarm->clientarg and passed into the
 * *        callback function for the client's specifc needs.
 * *
 * * @return Returns a unique unsigned integer(which is also passed as the first
 * *        argument of each callback), which can then be used to remove the
 * *        callback from the list at a later point in the future using the
 * *        zevent_alarm_unregister() function.  If memory could not be allocated
 * *        for the zevent_alarm struct 0 is returned.
 * *
 * * @see zevent_alarm_register
 * * @see zevent_alarm_unregister
 * * @see zevent_alarm_unregister_all
 * */
	unsigned int
zevent_alarm_register_hr(struct timeval t, unsigned int flags,
		ZEVENTAlarmCallback * cb, void *cd)
{
	struct zevent_alarm **s = NULL;

	for(s = &(thealarms); *s != NULL; s = &((*s)->next)) ;

	*s = ZEVENT_MALLOC_STRUCT(zevent_alarm);
	if(*s == NULL) {
		return 0;
	}

	(*s)->t.tv_sec = t.tv_sec;
	(*s)->t.tv_usec = t.tv_usec;
	(*s)->flags = flags;
	(*s)->clientarg = cd;
	(*s)->thecallback = cb;
	(*s)->clientreg = regnum++;
	(*s)->next = NULL;

	sa_update_entry(*s);

	printf("registered alarm %d, t = %d.%03d, flags=0x%02x\n",
			(*s)->clientreg, (int) (*s)->t.tv_sec,
			(int) ((*s)->t.tv_usec / 1000), (*s)->flags);

	if(start_alarms) {
		set_an_alarm();
	}

	return (*s)->clientreg;
}


