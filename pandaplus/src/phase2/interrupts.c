#include <umps/libumps.h>
#include <umps/arch.h>
#include "pandos_types.h"
#include "pandos_const.h"
#include "utils.h"

#include "phase2/interrupts.h"
#include "phase2/scheduler.h"
#include "phase2/helpers.h"
#include "phase2/variables.h"
#include "phase1/pcb.h"

#define TERM_STATUS_MASK 0xFF

HIDDEN void returnFromIntException();

HIDDEN void pltInterruptHandler();
HIDDEN void itInterruptHandler();
HIDDEN void deviceInterruptHandler(unsigned int deviceLineNo);
HIDDEN void terminalInterruptHandler();

extern cpu_t startingTime;


/*
 * Interrupt Exception Handler
 */
void interruptExceptionHandler(state_t* pstate)
{
#ifdef DEBUG
    static unsigned int bpInterruptLine;
    bpInterruptLine = lsb(pstate->cause & IMON) - 8;
    (void) bpInterruptLine;
#endif

    switch (0x1 << lsb(pstate->cause & IMON)) {
        case LOCALTIMERINT:
            pltInterruptHandler();
            break;
        case TIMERINTERRUPT:
            itInterruptHandler();
            break;
        case DISKINTERRUPT:
            deviceInterruptHandler(DISKINT);
            break;
        case FLASHINTERRUPT:
            deviceInterruptHandler(FLASHINT);
            break;
        case PRINTINTERRUPT:
            deviceInterruptHandler(PRNTINT);
            break;
        case TERMINTERRUPT:
            terminalInterruptHandler();
            break;
        default:
            break;
    }
}

HIDDEN void returnFromIntException()
{
    /*
     * if no last executed process is avaiable,
     * and this happens when the OS was in the WAIT state,
     * we need to call the scheduler since no valid LDST can be performed
     */
    if (currentProcess == NULL) {
        scheduler();
    }

    LDST((state_t*) PROCESSORSTATE0);
}

/*
 * used for implementing time sharing of the CPU
 */
HIDDEN void pltInterruptHandler()
{
    setTIMER(0xFFFFFFFF); /* ACK */
    /* preemption is applied only to low priority processes! */
    insertProcQ(procQLow, currentProcess); /* deschedule */

    /* context switch */
    memcpy(&currentProcess->p_s, (state_t*) PROCESSORSTATE0, sizeof(state_t));
    updateCpuTime(); /* update accumulated processor time by the current process */
    scheduler();
}

HIDDEN void itInterruptHandler()
{
    LDIT(PSECOND); /* ACK */

    /* wake up all blocked processes on the pseudo clock semd */
    while (semWakeup(&pseudoClockSem) != NULL) {
        --softBlockCount;
    }

    returnFromIntException();
}

HIDDEN void deviceInterruptHandler(unsigned int deviceLineNo)
{
    /* determine the device with a pending interrupt */
    unsigned int deviceNo = lsb(*((memaddr*) CDEV_BITMAP_ADDR(deviceLineNo)));
    /* device register */
    dtpreg_t* devreg = (dtpreg_t*) DEV_REG_ADDR(deviceLineNo, deviceNo);

    /* save off status field since after ACK it will be overwritten */
    devregf_t status = devreg->status;
    /* ACK the device interrupt */
    devreg->command = ACK;
    /* wake up the blocked process */
    pcb_t* proc = semWakeup(&devSems[EXT_IL_INDEX(deviceLineNo) * DEVPERINT + deviceNo]);

    /*
     * proc should never be NULL, but
     * bad implementations (such as using devices without DOIO)
     * make proc NULL
     * could be NULL also if the process get terminated actually
     */
    if (proc != NULL) {
        --softBlockCount;
        proc->p_s.reg_v0 = status;
    }

    returnFromIntException();
}

HIDDEN void terminalInterruptHandler()
{
    /* determine the terminal with a pending interrupt */
    unsigned int terminalNo = lsb(*((memaddr*) CDEV_BITMAP_ADDR(TERMINT)));
    /* terminal register */
    termreg_t* terminalReg = (termreg_t*) DEV_REG_ADDR(TERMINT, terminalNo);

    devregf_t status;
    pcb_t* proc;

    /*
     * we need to determine the terminal sub-device with a pending interrupt...
     * terminal transmitter (writing) has priority over terminal receiver (reading)
     * to determine which sub-device has a pending interrupt
     * we check that the status of each sub-device is not on READY
     * but be careful...
     * when a sub-device terminal is handling a requested operation
     * the status field is on BUSY
     * so we might have a case where one sub-device have a pending interrupt
     * and the other one is handling a requested operation
     * that's why we also need to make sure that the status of the sub device is not BUSY too
     */
    if (
        (terminalReg->transm_status & TERM_STATUS_MASK) != BUSY &&
        (terminalReg->transm_status & TERM_STATUS_MASK) != READY
    ) {
        /* save off the transmitter status field since after ACK it will be overwritten */
        status = terminalReg->transm_status;
        /* ACK the transmitted character */
        terminalReg->transm_command = ACK;
        /* wake up the blocked process */
        proc = semWakeup(&termSems[0][terminalNo]);
    } else if (
        (terminalReg->recv_status & TERM_STATUS_MASK) != BUSY &&
        (terminalReg->recv_status & TERM_STATUS_MASK) != READY
    ) {
        /* save off the receiver status field since after ACK it will be overwritten */
        status = terminalReg->recv_status;
        /* ACK the received character */
        terminalReg->recv_command = ACK;
        /* wake up the blocked process */
        proc = semWakeup(&termSems[1][terminalNo]);
    } else {
        /* this should never happen */
        ;
    }

    /*
     * proc should never be NULL, but
     * bad implementations (such as using devices without DOIO)
     * make proc NULL
     * could be NULL also if the process get terminated actually
     */
    if (proc != NULL) {
        --softBlockCount;
        proc->p_s.reg_v0 = status;
    }

    returnFromIntException();
}
