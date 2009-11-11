/******************************************************************************
 *
 *   Copyright © International Business Machines  Corp., 2009
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
 *      futex.h
 *
 * DESCRIPTION
 *      Glibc independent futex library for testing kernel functionality.
 *
 * AUTHOR
 *      Darren Hart <dvhltc@us.ibm.com>
 *
 * HISTORY
 *      2009-Nov-6: Initial version by Darren Hart <dvhltc@us.ibm.com>
 *
 *****************************************************************************/

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/futex.h>

/*
 * Define PASS, ERROR, and FAIL strings with and without color escape
 * sequences, default to no color.
 */
#define ESC 0x1B, '['
#define BRIGHT '1'
#define GREEN '3', '2'
#define YELLOW '3', '3'
#define RED '3', '1'
#define ESCEND 'm'
#define BRIGHT_GREEN ESC, BRIGHT, ';', GREEN, ESCEND
#define BRIGHT_YELLOW ESC, BRIGHT, ';', YELLOW, ESCEND
#define BRIGHT_RED ESC, BRIGHT, ';', RED, ESCEND
#define RESET_COLOR ESC, '0', 'm'
char PASS_COLOR[] = {BRIGHT_GREEN, ' ', 'P', 'A', 'S', 'S', RESET_COLOR, 0};
char ERROR_COLOR[] = {BRIGHT_YELLOW, 'E', 'R', 'R', 'O', 'R', RESET_COLOR, 0};
char FAIL_COLOR[] = {BRIGHT_RED, ' ', 'F', 'A', 'I', 'L', RESET_COLOR, 0};
char PASS_NORMAL[] = " PASS";
char ERROR_NORMAL[] = "ERROR";
char FAIL_NORMAL[] = " FAIL";
char *PASS = PASS_NORMAL;
char *ERROR = ERROR_NORMAL;
char *FAIL = FAIL_NORMAL;

typedef volatile __uint32_t futex_t;
#define FUTEX_INITIALIZER 0

/* Define the newer op codes if the system header file is not up to date. */
#ifndef FUTEX_WAIT_REQUEUE_PI
#define FUTEX_WAIT_REQUEUE_PI		11
#endif
#ifndef FUTEX_CMP_REQUEUE_PI
#define FUTEX_CMP_REQUEUE_PI		12
#endif
#ifndef FUTEX_WAIT_REQUEUE_PI_PRIVATE
#define FUTEX_WAIT_REQUEUE_PI_PRIVATE	(FUTEX_WAIT_REQUEUE_PI | \
					 FUTEX_PRIVATE_FLAG)
#endif
#ifndef FUTEX_REQUEUE_PI_PRIVATE
#define FUTEX_CMP_REQUEUE_PI_PRIVATE	(FUTEX_CMP_REQUEUE_PI | \
					 FUTEX_PRIVATE_FLAG)
#endif

/** 
 * futex() - SYS_futex syscall wrapper
 * @uaddr:	address of first futex
 * @op:		futex op code
 * @val:	typically expected value of uaddr, but varies by op
 * @timeout:	typically an absolute struct timespec (except where noted
 * 		otherwise). Overloaded by some ops
 * @uaddr2:	address of second futex for some ops\
 * @val3:	varies by op
 * @opflags:	flags to be bitwise OR'd with op, such as FUTEX_PRIVATE_FLAG
 *
 * futex() is used by all the following futex op wrappers. It can also be
 * used for misuse and abuse testing. Generally, the specific op wrappers
 * should be used instead.
 *
 * These argument descriptions are the defaults for all
 * like-named arguments in the following wrappers except where noted below.
 */
#define futex(uaddr, op, val, timeout, uaddr2, val3, opflags) \
	syscall(SYS_futex, uaddr, op | opflags, val, timeout, uaddr2, val3);

/**
 * futex_wait() - block on uaddr with optional timeout
 * @timeout:	relative timeout
 */
#define futex_wait(uaddr, val, timeout, opflags) \
	futex(uaddr, FUTEX_WAIT, val, timeout, NULL, 0, opflags)

/**
 * futex_wake() - wake one or more tasks blocked on uaddr
 * @nr_wake:	wake up to this many tasks
 */
#define futex_wake(uaddr, val, nr_wake, opflags) \
	futex(uaddr, FUTEX_WAKE, val, NULL, NULL, nr_wake, opflags)

/**
 * futex_wait_bitset() - block on uaddr with bitset
 * @bitset:	bitset to be used with futex_wake_bitset
 */
#define futex_wait_bitset(uaddr, val, timeout, bitset, opflags) \
	futex(uaddr, FUTEX_WAIT_BITSET, val, timeout, NULL, bitset, opflags)

/**
 * futex_wake_bitset() - wake one or more tasks blocked on uaddr with bitset
 * @bitset:	bitset to compare with that used in futex_wait_bitset
 */
#define futex_wake_bitset(uaddr, val, nr_wake, bitset, opflags) \
	futex(uaddr, FUTEX_WAKE_BITSET, val, NULL, NULL, bitset, opflags)

/**
 * futex_lock_pi() - block on uaddr as a PI mutex
 * @detect:	whether (1) or not (0) to perform deadlock detection
 */
#define futex_lock_pi(uaddr, timeout, detect, opflags) \
	futex(uaddr, FUTEX_LOCK_PI, detect, timeout, NULL, 0, opflags)

/**
 * futex_unlock_pi() - release uaddr as a PI mutex, waking the top waiter
 */
#define futex_unlock_pi(uaddr, opflags) \
	futex(uaddr, FUTEX_UNLOCK_PI, 0, NULL, NULL, 0, opflags)

/**
 * futex_wake_op() - FIXME: COME UP WITH A GOOD ONE LINE DESCRIPTION
 */
#define futex_wake_op(uaddr, uaddr2, nr_wake, nr_wake2, wake_op, opflags) \
	futex(uaddr, FUTEX_WAKE_OP, nr_wake, nr_wake2, uaddr2, wake_op, opflags)

/**
 * futex_requeue() - requeue without expected value comparison, deprecated
 * @nr_wake:	wake up to this many tasks
 * @nr_requeue:	requeue up to this many tasks
 *
 * Due to its inherently racy implementation, futex_requeue() is deprecated in
 * favor of futex_cmp_requeue().
 */
#define futex_requeue(uaddr, uaddr2, nr_wake, nr_requeue, opflags) \
	futex(uaddr, FUTEX_REQUEUE, nr_wake, nr_requeue, uaddr2, 0, opflags)

/**
 * futex_cmp_requeue() - requeue tasks from uaddr to uaddr2
 * @nr_wake:	wake up to this many tasks
 * @nr_requeue:	requeue up to this many tasks
 */
#define futex_cmp_requeue(uaddr, val, uaddr2, nr_wake, nr_requeue, opflags) \
	futex(uaddr, FUTEX_CMP_REQUEUE, nr_wake, nr_requeue, uaddr2, val, \
	      opflags)

/**
 * futex_wait_requeue_pi() - block on uaddr and prepare to requeue to uaddr2
 * @uaddr:	non-PI futex source
 * @uaddr2:	PI futex target
 *
 * This is the first half of the requeue_pi mechanism. It shall always be
 * paired with futex_cmp_requeue_pi().
 */
#define futex_wait_requeue_pi(uaddr, val, uaddr2, timeout, opflags) \
	futex(uaddr, FUTEX_WAIT_REQUEUE_PI, val, timeout, uaddr2, 0, opflags)

/**
 * futex_cmp_requeue_pi() - requeue tasks from uaddr to uaddr2 (PI aware)
 * @uaddr:	non-PI futex source
 * @uaddr2:	PI futex target
 * @nr_wake:	wake up to this many tasks
 * @nr_requeue:	requeue up to this many tasks
 */
#define futex_cmp_requeue_pi(uaddr, val, uaddr2, nr_wake, nr_requeue, opflags) \
	futex(uaddr, FUTEX_CMP_REQUEUE_PI, nr_wake, nr_requeue, uaddr2, val, \
	      opflags)

/**
 * futex_cmpxchg() - Atomic compare and exchange
 * @uaddr:	The address of the futex to be modified
 * @oldval:	The expected value of the futex
 * @newval:	The new value to try and assign the futex
 *
 * Implement cmpxchg using gcc atomic builtins.
 * http://gcc.gnu.org/onlinedocs/gcc-4.1.0/gcc/Atomic-Builtins.html
 */
int futex_cmpxchg(futex_t *uaddr, u_int32_t oldval, u_int32_t newval)
{
	return __sync_val_compare_and_swap(uaddr, oldval, newval);
}

/**
 * futextest_use_color() - Use colored output for PASS, ERROR, and FAIL strings
 * @use_color:	use color (1) or not (0)
 */
void futextest_use_color(int use_color)
{
	if (use_color) {
		PASS = PASS_COLOR;
		ERROR = ERROR_COLOR;
		FAIL = FAIL_COLOR;
	} else {
		PASS = PASS_NORMAL;
		ERROR = ERROR_NORMAL;
		FAIL = FAIL_NORMAL;
	}
}