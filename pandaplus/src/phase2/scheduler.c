#include <umps/libumps.h>
#include "pandos_const.h"

#include "phase2/scheduler.h"
#include "phase2/variables.h"
#include "phase1/pcb.h"

cpu_t schedulingTime;

void scheduler()
{
    /*
     * make sure to set forceLowQ
     * just if there are low priority processes available
     */
    forceLowQ = forceLowQ && !emptyProcQ(procQLow);

    if (!emptyProcQ(procQHigh) && !forceLowQ) {
        /* dispatch high priority processes first! */
        currentProcess = removeProcQ(procQHigh);
    } else if (!emptyProcQ(procQLow)) {
        /* dispatch low priority processes */
        currentProcess = removeProcQ(procQLow);
        setTIMER(TIMESLICE * (*((cpu_t*) TIMESCALEADDR)));
        forceLowQ = 0; /* valid just for one call */
    } else {
        /* no processes to dispatch in the ready queues... */
        if (processCount == 0) {
            /*
             * no processes, neither ready nor soft-blocked; halt the system
             * NB: this assumes that softBlockCount is correctly implemented
             */
            HALT();
        } else if (softBlockCount > 0) {
            /* wait for an I/O to complete (or pseudo-clock timer) */
            /* there's no current executing process since we are going to wait */
            currentProcess = NULL;
            /* enable all interrupts except PLT interrupt */
            setTIMER(0xFFFFFFFF); // try to avoid it
            setSTATUS(getSTATUS() | IMON | IECON);
            WAIT();
        } else if (softBlockCount == 0) {
            /* deadlock! */
            PANIC();
        } else {
            /* this should never happen */
            ;
        }
    }

    /*
     * save scheduling time for each scheduled process
     * needed for a correct accumulated processor time field management
     */
    STCK(schedulingTime);
    /* load processor state with the state of the soon-to-be-executing process */
    LDST(&currentProcess->p_s);
}
