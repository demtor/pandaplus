#include <umps/libumps.h>
#include <umps/cp0.h>
#include <umps/arch.h>
#include "pandos_types.h"
#include "pandos_const.h"

#include "phase3/vmSupport.h"
#include "phase3/sysSupport.h"
#include "phase2/variables.h"

/* --- prototypes --- */

HIDDEN void flashInit(unsigned int flashNo, devregf_t command, devregf_t data0);
HIDDEN void flashRead(unsigned int flashNo, memaddr srcAddr, unsigned int blockNumber);
HIDDEN void flashWrite(unsigned int flashNo, memaddr destAddr, unsigned int blockNumber);

HIDDEN void pageFaultHandler(support_t* psupport);

/* --- variables --- */

HIDDEN swap_t swapPoolTable[POOLSIZE];
HIDDEN sem_t swapPoolSem;
HIDDEN sem_t flashSems[DEVPERINT];

void initVmStructs()
{
    for (size_t i = 0; i < DEVPERINT; ++i) {
        flashSems[i] = 1;
    }

    swapPoolSem = 1;

    for (size_t i = 0; i < POOLSIZE; ++i) {
        /* frames at the start are unoccupied (obv) */
        swapPoolTable[i].sw_asid = -1;
        swapPoolTable[i].sw_pte = NULL;
	}
}

/* --- handlers --- */

/*
 * TLB-Refill Handler
 * Note: it is part of the kernel phase2 code (it's a kernel function)
 *       so this function runs in single threaded mode with interrupts masked
 */
void uTLB_RefillHandler()
{
#ifdef DEBUG
    static unsigned int bpTLBRefillVPN;
    bpTLBRefillVPN = ENTRYHI_GET_VPN2(((state_t*) PROCESSORSTATE0)->entry_hi);
    (void) bpTLBRefillVPN;
#endif

    /*
     * get processor state at the time of the exception
     * (stored at the start of the BIOS Data Page)
     */
    state_t* processorState = (state_t*) PROCESSORSTATE0;

	/*
     * get virtual page number
     * of the virtual address that failed translation (TLB Miss)
     */
	unsigned int vpn = ENTRYHI_GET_VPN2(processorState->entry_hi);

    /* get the page table entry containing the translation */
    pteEntry_t* pte = &currentProcess->p_supportStruct->sup_privatePgTbl[vpn];

    /* update the TLB with the (hopefully correct) translation */
    setENTRYHI(pte->pte_entryHI);
	setENTRYLO(pte->pte_entryLO);
	TLBWR();

	/* return control and let the hardware retry the instruction */
	LDST(processorState);
}

/*
 * TLB Exception Handler (also called the Pager)
 * all TLB exceptions (except TLB-Refill) triggered by a process
 * with a correctly initialized support struct
 * starts from here
 */
void tlbExceptionHandler()
{
    /* get current process support struct */
	support_t* psupport = (support_t*) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

#ifdef DEBUG
    static unsigned int bpTLBExcCode;
    bpTLBExcCode = CAUSE_GET_EXCCODE(psupport->sup_exceptState[PGFAULTEXCEPT].cause);
    (void) bpTLBExcCode;
#endif

    switch (CAUSE_GET_EXCCODE(psupport->sup_exceptState[PGFAULTEXCEPT].cause)) {
        case EXC_MOD:
            /* this should never happen (pte are all rw) */
            generalExceptionHandler();
			break;
		case EXC_TLBL:
		case EXC_TLBS:
			pageFaultHandler(psupport);
			break;
		default:
            break;
	}
}

/*
 * Support function for pageFaultHandler
 * it returns the "first-in" page frame number
 * to support FIFO page-replacement algorithm
 */
HIDDEN unsigned int getFirstInFrame()
{
    static int lastFrameNo = -1;
    return ++lastFrameNo % POOLSIZE;
}

/*
 * Support function for pageFaultHandler
 * it kicks out the given page frame, freeing it for use
 */
HIDDEN inline void evictPage(unsigned int pfn, swap_t* spte)
{
    setSTATUS(getSTATUS() & (~IECON)); /* atomic on */

    /* mark the soon-to-be-evicted page as not valid */
    spte->sw_pte->pte_entryLO &= ~VALIDON;

    /* update the TLB (since we just updated a process page table entry) */
    TLBCLR();

    setSTATUS(getSTATUS() | IECON); /* atomic off */

    flashWrite(spte->sw_asid - 1, FLASHPOOLSTART + pfn * PAGESIZE, spte->sw_pageNo);
}

/*
 * Page-Fault Handler
 */
HIDDEN void pageFaultHandler(support_t* psupport)
{
    SYSCALL(PASSEREN, (memaddr) &swapPoolSem, 0, 0);

    /* get processor state at the time of the exception */
    state_t* processorState = &psupport->sup_exceptState[PGFAULTEXCEPT];

    /* get the number of the missed page */
    unsigned int vpn = ENTRYHI_GET_VPN2(processorState->entry_hi);

    /* get the page table entry of the missed page */
    pteEntry_t* pte = &psupport->sup_privatePgTbl[vpn];

    /* find a physical frame for the soon-to-be-faulted-in page to reside within */
    unsigned int pfn = getFirstInFrame();

    /* if no free frame is available */
    if (
        swapPoolTable[pfn].sw_pte != NULL &&
        swapPoolTable[pfn].sw_pte->pte_entryLO & VALIDON
    ) {
        /* run page-replacement algorithm on the found pfn */
        evictPage(pfn, &swapPoolTable[pfn]);
    }

    flashRead(psupport->sup_asid - 1, FLASHPOOLSTART + pfn * PAGESIZE, vpn);

    /* update the swap pool table entry of the new occupied frame */
    swapPoolTable[pfn].sw_asid = psupport->sup_asid;
    swapPoolTable[pfn].sw_pageNo = vpn;
    swapPoolTable[pfn].sw_pte = pte;

    setSTATUS(getSTATUS() & (~IECON)); /* atomic on */

    /* update the page table entry of the missed page */
    pte->pte_entryLO |= VALIDON;
    ENTRYLO_SET_PFN(pte->pte_entryLO, pfn);

    /* update the TLB (since we just updated a process page table entry) */
    TLBCLR();

    setSTATUS(getSTATUS() | IECON); /* atomic off */

    SYSCALL(VERHOGEN, (memaddr) &swapPoolSem, 0, 0);

	/* return control and let the hardware retry the instruction */
    LDST(processorState);
}

/* --- support functions --- */

/*
 * Initiates a R/W operation on the specified flash device
 */
HIDDEN void flashInit(unsigned int flashNo, devregf_t command, devregf_t data0)
{
    dtpreg_t* flashReg = (dtpreg_t*) DEV_REG_ADDR(FLASHINT, flashNo);
    devregf_t status;

    SYSCALL(PASSEREN, (memaddr) &flashSems[flashNo], 0, 0);

    setSTATUS(getSTATUS() & (~IECON)); /* atomic on */

    flashReg->data0 = data0;
    status = SYSCALL(DOIO, (int) &flashReg->command, (int) command, 0);

    setSTATUS(getSTATUS() | IECON); /* atomic off */

    if (status != READY) {
        generalExceptionHandler();
    }

    SYSCALL(VERHOGEN, (memaddr) &flashSems[flashNo], 0, 0);
}

HIDDEN void flashRead(unsigned int flashNo, memaddr srcAddr, unsigned int blockNumber)
{
    flashInit(flashNo, FLASHREAD | blockNumber << BYTELENGTH, srcAddr);
}

HIDDEN void flashWrite(unsigned int flashNo, memaddr destAddr, unsigned int blockNumber)
{
    flashInit(flashNo, FLASHWRITE | blockNumber << BYTELENGTH, destAddr);
}