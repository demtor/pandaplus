#include <umps/libumps.h>
#include <umps3/umps/cp0.h>
#include "pandos_types.h"
#include "pandos_const.h"

#include "phase3/vmSupport.h"
#include "phase3/sysSupport.h"

extern sem_t masterSem;

/*
 * InstantiatorProcess
 */
void test() {
    static support_t psupports[UPROCMAX];

    initVmStructs();
    initSysStructs();

    /* U-proc processor state */
    /* note: it's not static because is gonna be copied by CREATEPROCESS */
    state_t pstate;

    unsigned int asid = 1;
    while (asid < UPROCMAX + 1) {
        pstate.pc_epc = pstate.reg_t9 = (memaddr) UPROCSTARTADDR;
        pstate.reg_sp = (memaddr) USERSTACKTOP;
        pstate.status = TEBITON | IEPON | USERPON; /* PLT, INTERRUPTS, USER MODE */
        //~ pstate.status = USERPON | IEPON | IMON | TEBITON;
        ENTRYHI_SET_ASID(pstate.entry_hi, asid);

        /* U-proc support structure */
        support_t* psupport = &psupports[asid - 1];

        context_t* psupportContext = psupport->sup_exceptContext;
        pteEntry_t* psupportPgTbl = psupport->sup_privatePgTbl;

        psupport->sup_asid = asid;

        psupportContext[PGFAULTEXCEPT].pc = (memaddr) tlbExceptionHandler;
        psupportContext[PGFAULTEXCEPT].stackPtr = (memaddr) &psupport->sup_stackTLB[499];
        psupportContext[PGFAULTEXCEPT].status = TEBITON | IEPON; /* PLT, INTERRUPTS, KERNEL MODE */
        //~ psupportContext[PGFAULTEXCEPT].status = IEPON | IMON | TEBITON;

        psupportContext[GENERALEXCEPT].pc = (memaddr) generalExceptionHandler;
        psupportContext[GENERALEXCEPT].stackPtr = (memaddr) &psupport->sup_stackGen[499];
        psupportContext[GENERALEXCEPT].status = TEBITON | IEPON; /* PLT, INTERRUPTS, KERNEL MODE */
        //~ psupportContext[GENERALEXCEPT].status = IEPON | IMON | TEBITON;

        /* Initialize U-proc page tables */
        for (int i = 0; i < USERPGTBLSIZE; ++i) {
            ENTRYHI_SET_VPN(psupportPgTbl[i].pte_entryHI, i);
            ENTRYHI_SET_ASID(psupportPgTbl[i].pte_entryHI, asid);
            psupportPgTbl[i].pte_entryLO = DIRTYON;
        }

        ENTRYHI_SET_VPN(psupportPgTbl[USERPGTBLSIZE-1].pte_entryHI, USERPGTBLSIZE-1);

        SYSCALL(CREATEPROCESS, (int) &pstate, PROCESS_PRIO_LOW, (int) psupport);

        ++asid;
    }

    for (int i = 0; i < UPROCMAX; ++i) {
        SYSCALL(PASSEREN, (int) &masterSem, 0, 0);
    }

    SYSCALL(TERMPROCESS, 0, 0, 0);
}
