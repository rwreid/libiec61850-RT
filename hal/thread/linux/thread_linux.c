/*
 *  thread_linux.c
 *
 *  Copyright 2013 Michael Zillgith
 *
 *  This file is part of libIEC61850.
 *
 *  libIEC61850 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  libIEC61850 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */


#include <pthread.h>
#include <limits.h> //RT RWR
//#include <sys/types.h> //BPH
//#include <sched.h>  //BPH
#include <semaphore.h>
#include <unistd.h>
#include "hal_thread.h"

#include "lib_memory.h"

struct sThread {
    ThreadExecutionFunction function;
    void* parameter;
    pthread_t pthread;
    int state;
    bool autodestroy;

    //BPH Extended for RT compaability
    struct sched_param param;
    pthread_attr_t attr;
};

Semaphore
Semaphore_create(int initialValue)
{
    Semaphore self = GLOBAL_MALLOC(sizeof(sem_t));

    sem_init((sem_t*) self, 0, initialValue);

    return self;
}

/* Wait until semaphore value is more than zero. Then decrease the semaphore value. */
void
Semaphore_wait(Semaphore self)
{
    sem_wait((sem_t*) self);
}

void
Semaphore_post(Semaphore self)
{
    sem_post((sem_t*) self);
}

void
Semaphore_destroy(Semaphore self)
{
    sem_destroy((sem_t*) self);
    GLOBAL_FREEMEM(self);
}

Thread
Thread_create(ThreadExecutionFunction function, void* parameter, bool autodestroy)
{
    Thread thread = (Thread) GLOBAL_MALLOC(sizeof(struct sThread));

    if (thread != NULL) {
        thread->parameter = parameter;
        thread->function = function;
        thread->state = 0;
        thread->autodestroy = autodestroy;

        //BPH Set default non RT thread parameters
        pthread_attr_init(&thread->attr);
        pthread_attr_setstacksize(&thread->attr, PTHREAD_STACK_MIN);
        pthread_attr_setschedpolicy(&thread->attr, SCHED_FIFO);

        thread->param.sched_priority = sched_get_priority_min(SCHED_FIFO) + 1  ;  //default prority to set 1 above background linux.
        pthread_attr_setschedparam(&thread->attr, &thread->param);
        pthread_attr_setinheritsched(&thread->attr, PTHREAD_EXPLICIT_SCHED);
    }

    return thread;
}

Thread
Thread_create_RT(ThreadExecutionFunction function, void* parameter, bool autodestroy, int prio)
{
    Thread thread = (Thread) GLOBAL_MALLOC(sizeof(struct sThread));

    if (thread != NULL) {
        thread->parameter = parameter;
        thread->function = function;
        thread->state = 0;
        thread->autodestroy = autodestroy;

        pthread_attr_init(&thread->attr);
        pthread_attr_setstacksize(&thread->attr, PTHREAD_STACK_MIN);
        pthread_attr_setschedpolicy(&thread->attr, SCHED_FIFO);
        thread->param.sched_priority = prio;
        pthread_attr_setschedparam(&thread->attr, &thread->param);
        pthread_attr_setinheritsched(&thread->attr, PTHREAD_EXPLICIT_SCHED);

    }

    return thread;
}

static void*
destroyAutomaticThread(void* parameter)
{
    Thread thread = (Thread) parameter;

    thread->function(thread->parameter);

    GLOBAL_FREEMEM(thread);

    pthread_exit(NULL);
}

void
Thread_start(Thread thread)
{
    if (thread->autodestroy == true) {
        pthread_create(&thread->pthread, &thread->attr, destroyAutomaticThread, thread);  //BPH
        pthread_detach(thread->pthread);
    } else {
        pthread_create(&thread->pthread, &thread->attr, thread->function, thread->parameter);  //BPH
    }

    thread->state = 1;
}

void
Thread_destroy(Thread thread)
{
    if (thread->state == 1) {
        pthread_join(thread->pthread, NULL);
    }

    GLOBAL_FREEMEM(thread);
}

void
Thread_sleep(int millies)
{
    usleep(millies * 1000);
}

void
Thread_sleep_us(int usecs)
{
    usleep(usecs);
}

void
Thread_sleep_deadline(struct timespec *req, struct timespec *rem)
{
    clock_nanosleep( CLOCK_MONOTONIC, TIMER_ABSTIME , req , rem);
}
