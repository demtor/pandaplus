#ifndef PHASE2_VARIABLES_H_INCLUDED
#define PHASE2_VARIABLES_H_INCLUDED

#include "pandos_types.h"
#include "pandos_const.h"

/* global variables defined in phase2/initial.c */

/* number of started processes */
extern unsigned int processCount;
/* number of blocked processes (due to an I/O or timer request) */
extern unsigned int softBlockCount;
/* current executing process */
extern pcb_t*       currentProcess;
/* queue of low priority (and ready) processes */
extern list_head_t* procQLow;
/* queue of high priority (and ready) processes */
extern list_head_t* procQHigh;

/* pseudo-clock sync semaphore (used in NSYS7) */
extern sem_t pseudoClockSem;
/* device sync semaphores (used to sync I/O operations) */
extern sem_t devSems[(DEVINTNUM-1)*DEVPERINT];
/* terminal sync semaphores, 0 = transmitter / 1 = receiver */
extern sem_t termSems[2][DEVPERINT];

/*
 * boolean used to flag the scheduler to try to skip for the current scheduling
 * the queue of high priority processes
 * and indeed to force the queue of low priority processes
 * used primarily in the yield syscall
 */
extern int forceLowQ;

#endif
