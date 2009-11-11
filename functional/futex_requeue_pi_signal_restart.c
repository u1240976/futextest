/******************************************************************************
 *
 *   Copyright © International Business Machines  Corp., 2006-2008
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * NAME
 *      requeue_pi_sig_restart.c
 *
 * DESCRIPTION
 *      This test exercises the futex_wait_requeue_pi signal restart after a
 *      deliberate wake-up.
 *
 * AUTHORS
 *      Darren Hart <dvhltc@us.ibm.com>
 *
 * HISTORY
 *      2008-May-5: Initial version by Darren Hart <dvhltc@us.ibm.com>
 *
 *****************************************************************************/

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "futextest.h"

futex_t f1 = FUTEX_INITIALIZER;
futex_t f2 = FUTEX_INITIALIZER;

typedef struct struct_waiter_arg {
	long id;
	struct timespec *timeout;
} waiter_arg_t;

void usage(char *prog)
{
	printf("Usage: %s\n", prog);
	printf("  -c	Use color\n");
	printf("  -h	Display this help message\n");
}

int create_rt_thread(pthread_t *pth, void*(*func)(void*), void *arg, int policy, int prio)
{
	int ret;
	struct sched_param schedp;
	pthread_attr_t attr;
	
	pthread_attr_init(&attr);
	memset(&schedp, 0, sizeof(schedp));

	if ((ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) != 0) {
		fprintf(stderr, "\t%s: pthread_attr_setinheritsched: %s\n",
			ERROR, strerror(ret));
		return -1;
	}

	if ((ret = pthread_attr_setschedpolicy(&attr, policy)) != 0) {
		fprintf(stderr, "\t%s: pthread_attr_setschedpolicy: %s\n",
			ERROR, strerror(ret));
		return -1;
	}

	schedp.sched_priority = prio;
	if ((ret = pthread_attr_setschedparam(&attr, &schedp)) != 0) {
		fprintf(stderr, "\t%s: pthread_attr_setschedparam: %s\n",
			ERROR, strerror(ret));
		return -1;
	}

	if ((ret = pthread_create(pth, &attr, func, arg)) != 0) {
		fprintf(stderr, "\t%s: pthread_create: %s\n",
			ERROR, strerror(ret));
		return -1;
	}
	return 0;
}

void handle_signal(int signo)
{
	fprintf(stderr, "\thandled signal: %d\n", signo);
}

void *waiterfn(void *arg)
{
	unsigned int old_val;
	int ret;

	fprintf(stderr, "\tWaiter running\n"); 

	fprintf(stderr, "\tCalling FUTEX_LOCK_PI on f2=%x @ %p\n", f2, &f2);
	/* cond_wait */
	old_val = f1;
	ret = futex_wait_requeue_pi(&f1, old_val, &(f2), NULL, FUTEX_PRIVATE_FLAG);
	if (ret < 0) {
		ret = -errno;
		fprintf(stderr, "\t%s: waiterfn: %s\n", ERROR, strerror(errno));
	}
	fprintf(stderr, "\tFUTEX_WAIT_REQUEUE_PI returned: %d\n", ret); fflush(stdout);
	fprintf(stderr, "\tw1:futex: %x\n", f2); fflush(stdout);
	if (ret)
		futex_lock_pi(&f2, 0, 0, FUTEX_PRIVATE_FLAG);
	futex_unlock_pi(&f2, FUTEX_PRIVATE_FLAG);

	fprintf(stderr, "\tWaiter exiting with %d\n", ret); fflush(stdout);
	fprintf(stderr, "\tw2:futex: %x\n", f2); fflush(stdout);
	return (void*)(long)ret;
}


int main(int argc, char *argv[])
{
	unsigned int old_val;
	struct sigaction sa;
	pthread_t waiter;
	int c, ret = 0;

	while ((c = getopt(argc, argv, "ch")) != -1) {
		switch(c) {
		case 'c':
			futextest_use_color(1);
			break;
		case 'h':
			usage(basename(argv[0]));
			exit(0);
		default:
			usage(basename(argv[0]));
			exit(1);
		}
	}

	printf("%s: Test signal handling during requeue_pi\n", basename(argv[0]));
	printf("\tArguments: <none>\n");

	sa.sa_handler = handle_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGUSR1, &sa, NULL)) {
		fprintf(stderr, "\t%s: sigaction: %s\n", ERROR, strerror(errno));
		exit(1);
	}

	fprintf(stderr, "\tm1:futex: %x\n", f2);
	fprintf(stderr, "\tCreating waiter\n");
	if ((ret = create_rt_thread(&waiter, waiterfn, NULL, SCHED_FIFO, 1))) {
		perror("Creating waiting thread failed");
		exit(1);
	}
	fprintf(stderr, "\tm2:futex: %x\n", f2);

	fprintf(stderr, "\tCalling FUTEX_LOCK_PI on mutex=%x @ %p\n", f2, &f2);
	
	futex_lock_pi(&f2, 0, 0, FUTEX_PRIVATE_FLAG);
	fprintf(stderr, "\tm3:futex: %x\n", f2);

	fprintf(stderr, "\tWaking waiter via FUTEX_CMP_REQUEUE_PI\n");fflush(stdout);
	/* cond_signal */
	old_val = f1;
	ret = futex_cmp_requeue_pi(&f1, old_val, &(f2),
				   1, 0, FUTEX_PRIVATE_FLAG);
	if (ret < 0) {
		ret = -errno;
		fprintf(stderr, "\t%s: FUTEX_CMP_REQUEUE_PI failed: %s\n",
			ERROR, strerror(errno));
		/* FIXME - do something sane.... */
	}
	fprintf(stderr, "\tm4:futex: %x\n", f2); 

	/* give the waiter time to wake and block on the lock */
	sleep(2);
	fprintf(stderr, "\tm5:futex: %x\n", f2);

	/* 
	 * signal the waiter to force a syscall restart to
	 * futex_lock_pi_restart()
	 */
	fprintf(stderr, "\tIssuing SIGUSR1 to waiter\n"); 
	pthread_kill(waiter, SIGUSR1);

	/* give the signal time to get to the waiter */
	sleep(2);
	fprintf(stderr, "\tm6:futex: %x\n", f2);
	fprintf(stderr, "\tCalling FUTEX_UNLOCK_PI on mutex=%x @ %p\n", f2, &f2);
	futex_unlock_pi(&f2, FUTEX_PRIVATE_FLAG);

	/* Wait for waiter to finish */
	fprintf(stderr, "\tWaiting for waiter to return\n");
	pthread_join(waiter, NULL);
	fprintf(stderr, "\tm7:futex: %x\n", f2);

	printf("Result: %s\n", ret ? ERROR : PASS);
	return ret;
}