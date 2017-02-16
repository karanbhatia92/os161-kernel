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
 * Driver code is in kern/tests/synchprobs.c We will replace that file. This
 * file is yours to modify as you see fit.
 *
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is, of
 * course, stable under rotation)
 *
 *   |0 |
 * -     --
 *    01  1
 * 3  32
 * --    --
 *   | 2|
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X first.
 * The semantics of the problem are that once a car enters any quadrant it has
 * to be somewhere in the intersection until it call leaveIntersection(),
 * which it should call while in the final quadrant.
 *
 * As an example, let's say a car approaches the intersection and needs to
 * pass through quadrants 0, 3 and 2. Once you call inQuadrant(0), the car is
 * considered in quadrant 0 until you call inQuadrant(3). After you call
 * inQuadrant(2), the car is considered in quadrant 2 until you call
 * leaveIntersection().
 *
 * You will probably want to write some helper functions to assist with the
 * mappings. Modular arithmetic can help, e.g. a car passing straight through
 * the intersection entering from direction X will leave to direction (X + 2)
 * % 4 and pass through quadrants X and (X + 3) % 4.  Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in synchprobs.c to record their progress.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * Called by the driver during initialization.
 */
struct lock *lock0;
struct lock *lock1;
struct lock *lock2;
struct lock *lock3;
struct lock *counterlock;
struct cv *sharedcv;
static volatile int counter = 0;

struct lock *give_lock(uint32_t direction) {
	struct lock *lockgen = NULL;
	switch(direction){
		case 0:
		lockgen = lock0;
		break;

		case 1:
		lockgen = lock1;
		break;

		case 2:
		lockgen = lock2;
		break;

		case 3:
		lockgen = lock3;
		break;
	}
	return lockgen;
}

void
stoplight_init() {
	lock0 = lock_create("lock0");
        if (lock0 == NULL) {
                panic("lock_create for direction 0 failed\n");
        }
	lock1 = lock_create("lock1");
        if (lock1 == NULL) {
                panic("lock_create for direction 1 failed\n");
        }
	lock2 = lock_create("lock2");
        if (lock2 == NULL) {
                panic("lock_create for direction 2 failed\n");
        }
	lock3 = lock_create("lock3");
        if (lock3 == NULL) {
                panic("lock_create for direction 3 failed\n");
        }
	counterlock = lock_create("counterlock");
        if (counterlock == NULL) {
                panic("lock_create for counterlock failed\n");
        }
	sharedcv = cv_create("sharedcv");
	if (sharedcv == NULL) {
		panic("cv_create failed for sharedcv");
	}
	return;
}

/*
 * Called by the driver during teardown.
 */

void stoplight_cleanup() {
	lock_destroy(lock0);
	lock_destroy(lock1);
	lock_destroy(lock2);
	lock_destroy(lock3);
	cv_destroy(sharedcv);
	return;
}

void
turnright(uint32_t direction, uint32_t index)
{
	//(void)direction;
	//(void)index;
	struct lock *lockright = give_lock(direction);
	KASSERT(lockright != NULL);
	lock_acquire(counterlock);
	while (counter >= 3) {
		cv_wait(sharedcv, counterlock);
	}	
	lock_acquire(lockright);
	counter++;
	lock_release(counterlock);
	inQuadrant((int)direction, index);
	leaveIntersection(index);
	lock_release(lockright);
	lock_acquire(counterlock);
	counter--;
//	if(counter < 3) {
		cv_signal(sharedcv, counterlock);
//	}
	lock_release(counterlock);

	
	/*
	 * Implement this function.
	 */
	return;
}
void
gostraight(uint32_t direction, uint32_t index)
{
	//(void)direction;
	//(void)index;
	struct lock *lockcur = give_lock(direction);
	KASSERT(lockcur != NULL);
	struct lock *locknext = NULL;
        int next = -1;
	

	if(direction == 0) {
		next = 3;
	} else {
		next = (int)direction - 1;
	}
	
	lock_acquire(counterlock);
	while (counter >= 3) {
		cv_wait(sharedcv, counterlock);
	}	
	lock_acquire(lockcur);
	counter++;
	lock_release(counterlock);
	inQuadrant((int)direction, index);
        locknext = give_lock((uint32_t)next);
	KASSERT(locknext != NULL);
	lock_acquire(locknext);
	inQuadrant(next, index);
	lock_release(lockcur);

	leaveIntersection(index);
	
	lock_release(locknext);
	lock_acquire(counterlock);
	counter--;
	cv_signal(sharedcv, counterlock);
	lock_release(counterlock);
	/*
	 * Implement this function.
	 */
	return;
}
void
turnleft(uint32_t direction, uint32_t index)
{
	//(void)direction;
	//(void)index;
	/*
	 * Implement this function.
	 */	
	struct lock *lockcurrent = give_lock(direction);
	KASSERT(lockcurrent != NULL);
	struct lock *lockforward = NULL;
	struct lock *lockleft = NULL;
        int forward = -1;
	int left = -1;

	if(direction == 0) {
		forward = 3;
		left = 2;
	} else if (direction == 1) {
		forward = 0;
		left = 3;
	} else {
		forward = (int)direction - 1;
		left = forward - 1;
	}
	
	KASSERT(forward != -1);
	KASSERT(left != -1);

	lock_acquire(counterlock);
	while (counter >= 3) {
		cv_wait(sharedcv, counterlock);
	}	
	lock_acquire(lockcurrent);
	counter++;
	lock_release(counterlock);
	inQuadrant((int)direction, index);
	lockforward = give_lock((uint32_t)forward);	
	KASSERT(lockforward != NULL);
	lock_acquire(lockforward);
	inQuadrant(forward, index);
	lock_release(lockcurrent);
	lockleft = give_lock((uint32_t)left);
	lock_acquire(lockleft);
	inQuadrant(left, index);
	lock_release(lockforward);
	leaveIntersection(index);
	lock_release(lockleft);
	
	lock_acquire(counterlock);
	counter--;
	cv_signal(sharedcv, counterlock);
	lock_release(counterlock);
	
	return;
}
