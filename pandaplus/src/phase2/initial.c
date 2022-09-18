#include "pandos_types.h"
#include "pandos_const.h"

#include "phase2/exceptions.h"
#include "phase2/scheduler.h"
#include "phase2/helpers.h"
#include "phase1/pcb.h"
#include "phase1/asl.h"

extern void test();
extern void uTLB_RefillHandler();

HIDDEN list_head_t procQLow_sentinel, procQHigh_sentinel;

/* Kernel global variables (check phase2/variables.h) */
unsigned int processCount, softBlockCount;
pcb_t*       currentProcess;
list_head_t* procQLow;
list_head_t* procQHigh;
sem_t        pseudoClockSem;
sem_t        devSems[(DEVINTNUM-1)*DEVPERINT];
sem_t        termSems[2][DEVPERINT];
int          forceLowQ;

void main()
{
    /* Populate the Processor 0 Pass Up Vector */
    passupvector_t* passUpVector = (passupvector_t*) PASSUPVECTOR;
    passUpVector->tlb_refill_handler = (memaddr) uTLB_RefillHandler;
    passUpVector->tlb_refill_stackPtr = (memaddr) KERNELSTACK;
    passUpVector->exception_handler = (memaddr) exceptionHandler;
    passUpVector->exception_stackPtr= (memaddr) KERNELSTACK;

    /* Initialize phase1 data structures */
    initPcbs();
	initASL();

    /* Initialize all kernel maintained variables */
    processCount = 0;
    softBlockCount = 0;
    currentProcess = NULL;

    procQLow = &procQLow_sentinel;
    mkEmptyProcQ(procQLow);
    procQHigh = &procQHigh_sentinel;
    mkEmptyProcQ(procQHigh);

    forceLowQ = 0; /* false */

    pseudoClockSem = 0;
	for (size_t i = 0; i < (DEVINTNUM-1)*DEVPERINT; ++i) {
        devSems[i] = 0;
    }
    for (size_t i = 0; i < 2*DEVPERINT; ++i) {
        *((sem_t*)termSems + i) = 0;
    }

    /* Load the system-wide Interval Timer (used in NSYS7) */
    LDIT(PSECOND);

    /* First process setup */
    pcb_t* proc = allocPcb();
    proc->p_pid = 1;
    proc->p_prio = PROCESS_PRIO_LOW;
    proc->p_s.status = TEBITON | IMON | IEPON; /* PLT, INTERRUPTS, KERNEL MODE */
    proc->p_s.pc_epc = proc->p_s.reg_t9 = (memaddr) test;
    RAMTOP(proc->p_s.reg_sp); /* SP set to last RAM frame */

    insertPrioProcQ(proc);
    ++processCount;

    /* Call the Scheduler */
    scheduler();
}
