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
 *      logging.h
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

#ifndef _LOGGING_H
#define _LOGGING_H

#include <string.h>
#include <unistd.h>
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
static char PASS_COLOR[] = {BRIGHT_GREEN, ' ', 'P', 'A', 'S', 'S', RESET_COLOR, 0};
static char ERROR_COLOR[] = {BRIGHT_YELLOW, 'E', 'R', 'R', 'O', 'R', RESET_COLOR, 0};
static char FAIL_COLOR[] = {BRIGHT_RED, ' ', 'F', 'A', 'I', 'L', RESET_COLOR, 0};
static char INFO_NORMAL[] = " INFO";
static char PASS_NORMAL[] = " PASS";
static char ERROR_NORMAL[] = "ERROR";
static char FAIL_NORMAL[] = " FAIL";
char *INFO = INFO_NORMAL;
char *PASS = PASS_NORMAL;
char *ERROR = ERROR_NORMAL;
char *FAIL = FAIL_NORMAL;

/* Verbosity setting for INFO messages */
#define VQUIET    0
#define VCRITICAL 1
#define VINFO     2
#define VMAX      VINFO
int _verbose = VCRITICAL;

/* Functional test return codes */
#define RET_PASS   0
#define RET_ERROR -1
#define RET_FAIL  -2

/**
 * log_color() - Use colored output for PASS, ERROR, and FAIL strings
 * @use_color:	use color (1) or not (0)
 */
void log_color(int use_color)
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

/**
 * log_verbosity() - Set verbosity of test output
 * @verbose:	Enable (1) verbose output or not (0)
 *
 * Currently setting verbose=1 will enable INFO messages and 0 will disable
 * them. FAIL and ERROR messages are always displayed.
 */
void log_verbosity(int level)
{
	if (level > VMAX)
		level = VMAX;
	else if (level < 0)
		level = 0;
	_verbose = level;
}

/**
 * print_result() - Print standard PASS | ERROR | FAIL results
 * @ret:	the return value to be considered: 0 | RET_ERROR | RET_FAIL
 *
 * print_result() is primarily intended for functional tests.
 */
void print_result(int ret)
{
	char *result = "Unknown return code";
	switch (ret) {
	case RET_PASS:
		result = PASS;
		break;
	case RET_ERROR:
		result = ERROR;
		break;
	case RET_FAIL:
		result = FAIL;
		break;
	}
	printf("Result: %s\n", result);
}

/* log level macros */
#define info(message, vargs...) \
do { \
	if (_verbose >= VINFO) \
		fprintf(stderr, "\t%s: "message, INFO, ##vargs); \
} while (0)

#define error(message, err, args...) \
do { \
	if (_verbose >= VCRITICAL) {\
		if (err) \
			fprintf(stderr, "\t%s: %s: "message, \
				ERROR, strerror(err), ##args); \
		else \
			fprintf(stderr, "\t%s: "message, ERROR, ##args); \
	} \
} while (0)

#define fail(message, args...) \
do { \
	if (_verbose >= VCRITICAL) \
		fprintf(stderr, "\t%s: "message, FAIL, ##args); \
} while (0)

#endif
