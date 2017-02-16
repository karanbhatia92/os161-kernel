/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>
/*
 * Called by the driver during initialization.
 */

static volatile int male_num;
static volatile int female_num;
struct lock *wmlock;
struct cv *malecv;
struct cv *femalecv;
struct cv *matchmakercv;

void whalemating_init() {
	male_num = 0;
	female_num = 0;
	malecv = cv_create("malecv");
	if (malecv == NULL) {
		panic("cv_create for malecv failed\n");
	}
 	femalecv = cv_create("femalecv");
	if (femalecv == NULL) {
		panic("cv_create for femalecv failed\n");
	}
	matchmakercv = cv_create("matchmakercv");
	if (matchmakercv == NULL) {
		panic("cv_create for matchmakercv failed\n");
	}
	wmlock = lock_create("wmlock");
	if (wmlock == NULL) {
		panic("lock_create for wmlock failed\n");
	}

	//return;
}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
	cv_destroy(malecv);
	cv_destroy(femalecv);
	cv_destroy(matchmakercv);
	lock_destroy(wmlock);
	return;
}

void
male(uint32_t index)
{
	//(void)index;
	/*
	 * Implement this function by calling male_start and male_end when
	 * appropriate.
	 */
	male_start(index);
	lock_acquire(wmlock);
	male_num++;
	cv_signal(matchmakercv, wmlock);
	cv_wait(malecv, wmlock);
	male_end(index);
	male_num--;
	cv_signal(femalecv, wmlock);
	lock_release(wmlock);
	return;
}

void
female(uint32_t index)
{
	female_start(index);
	lock_acquire(wmlock);
	female_num++;
	cv_signal(matchmakercv, wmlock);
	cv_wait(femalecv, wmlock);
	female_end(index);
	female_num--;
	lock_release(wmlock);	
	//(void)index;
	/*
	 * Implement this function by calling female_start and female_end when
	 * appropriate.
	 */
	return;
}

void
matchmaker(uint32_t index)
{
	matchmaker_start(index);
	lock_acquire(wmlock);
	while (female_num == 0 || male_num == 0) {
		cv_wait(matchmakercv, wmlock);
	}
	matchmaker_end(index);
	cv_signal(malecv, wmlock);
	lock_release(wmlock);
	//(void)index;
	/*
	 * Implement this function by calling matchmaker_start and matchmaker_end
	 * when appropriate.
	 */
	return;
}
