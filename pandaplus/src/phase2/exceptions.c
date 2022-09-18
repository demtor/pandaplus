#include <umps/libumps.h>
#include <umps/cp0.h>
#include "pandos_types.h"
#include "pandos_const.h"
#include "utils.h"

#include "phase2/exceptions.h"
#include "phase2/scheduler.h"
#include "phase2/interrupts.h"
#include "phase2/syscalls.h"
#include "phase2/helpers.h"
#include "phase2/variables.h"

/*
 * Kernel Exception Handler
 * all exceptions (except TLB-Refill events) starts from here
 * indeed it acts more like a dispatcher
 */
void exceptionHandler()
{
#ifdef DEBUG
    static unsigned int bpExcCode;
    bpExcCode = CAUSE_GET_EXCCODE(((state_t*) PROCESSORSTATE0)->cause);
    (void) bpExcCode;
#endif

    /*
     * get processor state at the time of the exception
     * (stored at the start of the BIOS Data Page)
     */
    state_t* processorState = (state_t*) PROCESSORSTATE0;

    switch (CAUSE_GET_EXCCODE(processorState->cause)) {
        case EXC_INT:
            interruptExceptionHandler(processorState);
            break;
        case EXC_SYS:
            syscallExceptionHandler(processorState);
            break;
        case EXC_MOD:
        case EXC_TLBL:
        case EXC_TLBS:
            passUpOrDie(processorState, PGFAULTEXCEPT);
            break;
        case EXC_ADEL:
        case EXC_ADES:
        case EXC_IBE:
        case EXC_DBE:
        case EXC_BP:
        case EXC_RI:
        case EXC_CPU:
        case EXC_OV:
            passUpOrDie(processorState, GENERALEXCEPT);
            break;
        default:
            break;
    }
}

/*
 * Pass Up or Die
 * all other exceptions (
 *  tlb exceptions,
 *  program traps,
 *  positively numbered syscalls
 * )
 * are either passed up to the process support structure
 * or terminated (if no support structure is provided)
 */
void passUpOrDie(state_t* pstate, unsigned int excType)
{
    if (currentProcess->p_supportStruct == NULL) {
        kill(currentProcess);
        scheduler();
    }

    /* save exception state into a location accessible to the support level (phase 3) */
    memcpy(&currentProcess->p_supportStruct->sup_exceptState[excType], pstate, sizeof(state_t));

    /* load new context */
    LDCXT(
        currentProcess->p_supportStruct->sup_exceptContext[excType].stackPtr,
        currentProcess->p_supportStruct->sup_exceptContext[excType].status,
        currentProcess->p_supportStruct->sup_exceptContext[excType].pc
    );
}
