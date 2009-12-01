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
 *      This test exercises the futex_wait_requeue_pi() signal handling both
 *      before and after the requeue. The first should be restarted by the
 *      kernel. The latter should return EWOULDBLOCK to the waiter.
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
#include "atomic.h"
#include "futextest.h"
#include "logging.h"

futex_t f1 = FUTEX_INITIALIZER;
futex_t f2 = FUTEX_INITIALIZER;
atomic_t requeued = ATOMIC_INITIALIZER;
atomic_t waiter_running = ATOMIC_INITIALIZER;

typedef struct struct_waiter_arg {
	long id;
	struct timespec *timeout;
} waiter_arg_t;

int waiter_ret = 0;

void usage(char *prog)
{
	printf("Usage: %s\n", prog);
	printf("  -c	Use color\n");
	printf("  -h	Display this help message\n");
	printf("  -v L	Verbosity level: %d=QUIET %d=CRITICAL %d=INFO\n",
	       VQUIET, VCRITICAL, VINFO);
}

int create_rt_thread(pthread_t *pth, void*(*func)(void*), void *arg, int policy, int prio)
{
	int ret;
	struct sched_param schedp;
	pthread_attr_t attr;
	
	pthread_attr_init(&attr);
	memset(&schedp, 0, sizeof(schedp));

	if ((ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) != 0) {
		error("pthread_attr_setinheritsched\n", ret);
		return -1;
	}

	if ((ret = pthread_attr_setschedpolicy(&attr, policy)) != 0) {
		error("pthread_attr_setschedpolicy\n", ret);
		return -1;
	}

	schedp.sched_priority = prio;
	if ((ret = pthread_attr_setschedparam(&attr, &schedp)) != 0) {
		error("pthread_attr_setschedparam\n", ret);
		return -1;
	}

	if ((ret = pthread_create(pth, &attr, func, arg)) != 0) {
		error("pthread_create\n", ret);
		return -1;
	}
	return 0;
}

void handle_signal(int signo)
{
	info("signal received %s requeue\n", 
	     requeued.val ? "after" : "prior to");
}

void *waiterfn(void *arg)
{
	unsigned int old_val;
	int res;
	waiter_ret = RET_PASS;

	info("Waiter running\n");
	info("Calling FUTEX_LOCK_PI on f2=%x @ %p\n", f2, &f2);
	atomic_set(&waiter_running, 1);
	old_val = f1;
	res = futex_wait_requeue_pi(&f1, old_val, &(f2), NULL, FUTEX_PRIVATE_FLAG);
	if (!requeued.val || errno != EWOULDBLOCK) {
		error("unexpected return from futex_wait_requeue_pi\n", errno);
		info("w2:futex: %x\n", f2);
		if (!res)
			futex_unlock_pi(&f2, FUTEX_PRIVATE_FLAG);
		waiter_ret = RET_ERROR;
	}

	info("Waiter exiting with %d\n", waiter_ret);
	pthread_exit(NULL);
}


int main(int argc, char *argv[])
{
	unsigned int old_val;
	struct sigaction sa;
	pthread_t waiter;
	int c, res, ret = RET_PASS;

	while ((c = getopt(argc, argv, "chv:")) != -1) {
		switch(c) {
		case 'c':
			log_color(1);
			break;
		case 'h':
			usage(basename(argv[0]));
			exit(0);
		case 'v':
			log_verbosity(atoi(optarg));
			break;
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
		error("sigaction\n", errno);
		exit(1);
	}

	info("m1:f2: %x\n", f2);
	info("Creating waiter\n");
	if ((res = create_rt_thread(&waiter, waiterfn, NULL, SCHED_FIFO, 1))) {
		error("Creating waiting thread failed", res);
		ret = RET_ERROR;
		goto out;
	}

	info("Calling FUTEX_LOCK_PI on f2=%x @ %p\n", f2, &f2);
	info("m2:f2: %x\n", f2);
	futex_lock_pi(&f2, 0, 0, FUTEX_PRIVATE_FLAG);
	info("m3:f2: %x\n", f2);

	/* wait for the waiter to start running, then give it time to block */
	while (!waiter_running.val)
		usleep(100);
	usleep(100);

	/* 
	 * signal the waiter before requeue, waiter should automatically
	 * restart futex_wait_requeue_pi() in the kernel.
	 */
	info("Issuing SIGUSR1 to waiter\n"); 
	pthread_kill(waiter, SIGUSR1);

	info("Waking waiter via FUTEX_CMP_REQUEUE_PI\n");
	while (1) {
		old_val = f1;
		res = futex_cmp_requeue_pi(&f1, old_val, &(f2), 1, 0,
					   FUTEX_PRIVATE_FLAG);
		if (res)
			break;
		error("Waiter was not ready for requeue. First signal was "
		      "delivered too early\n", 0);
		usleep(100);
	}
	if (res < 0) {
		error("FUTEX_CMP_REQUEUE_PI failed\n", errno);
		ret = RET_ERROR;
	} else {
		atomic_set(&requeued, 1);
	}
	info("m4:f2: %x\n", f2); 

	/* 
	 * signal the waiter after requeue, waiter should return from
	 * futex_wait_requeue_pi() with EWOULDBLOCK.
	 */
	info("Issuing SIGUSR1 to waiter\n"); 
	pthread_kill(waiter, SIGUSR1);

	/* give the signal time to get to the waiter */
	usleep(100);
	info("Calling FUTEX_UNLOCK_PI on mutex=%x @ %p\n", f2, &f2);
	futex_unlock_pi(&f2, FUTEX_PRIVATE_FLAG);

	/* Wait for waiter to finish */
	info("Waiting for waiter to return\n");
	pthread_join(waiter, NULL);
	info("m5:f2: %x\n", f2);

 out:
	if (ret == RET_PASS && waiter_ret)
		ret = waiter_ret;

	print_result(ret);
	return ret;
}
