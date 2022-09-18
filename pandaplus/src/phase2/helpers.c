#include <umps/libumps.h>
#include <umps/cp0.h>
#include "pandos_types.h"
#include "pandos_const.h"
#include "utils.h"

#include "phase2/exceptions.h"
#include "phase2/scheduler.h"
#include "phase2/helpers.h"
#include "phase2/variables.h"
#include "phase1/pcb.h"
#include "phase1/asl.h"

extern cpu_t schedulingTime;

/*
 * Insert the process in the priority queue
 */
void insertPrioProcQ(pcb_t* p)
{
    if (p->p_prio == PROCESS_PRIO_LOW) {
        insertProcQ(procQLow, p);
    } else if (p->p_prio == PROCESS_PRIO_HIGH) {
        insertProcQ(procQHigh, p);
    } else {
        /* this should never happen */
        ;
    }
}

/*
 * Remove the process from its priority queue
 */
void outPrioProcQ(pcb_t* p)
{
    if (p->p_prio == PROCESS_PRIO_LOW) {
        outProcQ(procQLow, p);
    } else if (p->p_prio == PROCESS_PRIO_HIGH) {
        outProcQ(procQHigh, p);
    } else {
        /* this should never happen */
        ;
    }
}

/*
 * Checks if the given semAddr is part of the allocated device semaphores
 */
int isDeviceSemaphore(sem_t* semAddr)
{
    return
        (semAddr >= &devSems[0] && semAddr <= &devSems[(DEVINTNUM-1)*DEVPERINT]) ||
        (semAddr >= &termSems[0][0] && semAddr <= &termSems[2][DEVPERINT]) ||
        semAddr == &pseudoClockSem;
}

/*
 * Find the pcb corresponding to the given pid
 * in all available process queues.
 * It seems inefficient but it's actually O(processCount)
 */
pcb_t* findPcb(pid_t pid)
{
    pcb_t* currentPcb;
    semd_t* currentSemd;

    /* search the pcb in the queue of low priority processes */
    list_for_each_entry(currentPcb, procQLow, p_list) {
        if (currentPcb->p_pid == pid) {
            return currentPcb;
        }
    }

    /* search the pcb in the queue of high priority processes */
    list_for_each_entry(currentPcb, procQHigh, p_list) {
        if (currentPcb->p_pid == pid) {
            return currentPcb;
        }
    }

    /* search the pcb in the queue of active SEMDs */
    list_for_each_entry(currentSemd, getSemdHead(), s_link) {
        list_for_each_entry(currentPcb, &currentSemd->s_procq, p_list) {
            if (currentPcb->p_pid == pid) {
                return currentPcb;
            }
        }
    }

    return NULL;
}

/*
 * Suspend the current process (blocking it) on the given semd
 */
void semSuspend(sem_t* semAddr) {
    /* block the process on the semd */
    int failed = insertBlocked(semAddr, currentProcess) == 1;

    /* PANIC if we run out of semaphores */
    if (failed == 1) {
        PANIC();
    }
}

/*
 * Wake up a blocked processs on the given semd
 */
pcb_t* semWakeup(sem_t* semAddr)
{
    pcb_t* proc = removeBlocked(semAddr);

    if (proc != NULL) {
        insertPrioProcQ(proc);
    }

    return proc;
}

/*
 * Terminate the given process and its progeny
 * a kill operation is frequent in a OS, because many times
 * the OS may prefer to kill a process instead
 * of trying to report an error
 */
void kill(pcb_t* proc)
{
    if (proc == NULL) {
        return;
    }

    /* remove proc from its parent (if available) */
    outChild(proc);

    /* if the process is blocked on a semaphore... */
    if (proc->p_semAdd != NULL) {
        /* remove it from the semd proc queue */
        outBlocked(proc);

        /* if we are handling a device semaphore */
        if (isDeviceSemaphore(proc->p_semAdd)) {
            --softBlockCount;
        }
    } else {
        /* remove proc from the ready queue */
        outPrioProcQ(proc);
    }

    --processCount;

    /* terminate every proc child */
    while (!emptyChild(proc)) {
        kill(removeChild(proc));
    }

    /* flag that current process ceased to exist */
    if (currentProcess == proc) {
        currentProcess = NULL;
    }

    freePcb(proc);
}

void generateException(unsigned int excCode)
{
    ((state_t*) PROCESSORSTATE0)->status &=
        ~CAUSE_EXCCODE_MASK | (excCode << CAUSE_EXCCODE_BIT);
    exceptionHandler();
}

/*
 * Update accumulated processor time by the current process
 * and make sure to update scheduling time for the next update
 */
void updateCpuTime()
{
    cpu_t currentTime; 
    STCK(currentTime); 

    currentProcess->p_time += currentTime - schedulingTime;

    STCK(schedulingTime); 
}
