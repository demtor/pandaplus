#include <umps/libumps.h>
#include <umps/cp0.h>
#include <umps/arch.h>
#include "pandos_types.h"
#include "pandos_const.h"

#include "phase3/sysSupport.h"

#define TERM_STATUS_MASK 0xFF
#define PRINTCHR 2

/* --- prototypes --- */

HIDDEN void trapExceptionHandler(support_t* psupport);
HIDDEN void syscallExceptionHandler(support_t* psupport);

HIDDEN void getTod(support_t* psupport);
HIDDEN void terminate(support_t* psupport);
HIDDEN void writeToPrinter(support_t* psupport, char* strVirtAddr, int len);
HIDDEN void writeToTerminal(support_t* psupport, char* strVirtAddr, int len);
HIDDEN void readFromTerminal(support_t* psupport, char* strVirtAddr);

/* --- variables --- */

/* printer semaphores (mutex) */
HIDDEN sem_t printerSems[DEVPERINT];
/* terminal semaphores (mutex), 0 = transmitter / 1 = receiver */
HIDDEN sem_t termSems[2][DEVPERINT];

sem_t masterSem;

void initSysStructs()
{
    masterSem = 0;

	for (size_t i = 0; i < DEVPERINT; ++i) {
        printerSems[i] = 1;
    }
    for (size_t i = 0; i < 2*DEVPERINT; ++i) {
        *((sem_t*)termSems + i) = 1;
    }
}

/* --- handlers --- */

/*
 * Support General Exception Handler
 * all non-TLB related exceptions triggered by a process
 * with a correctly initialized support structure
 * starts from here
 */
void generalExceptionHandler()
{
    /* get current process support struct */
	support_t* psupport = (support_t*) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    /* retrieve the cause of the exception */
	unsigned int excCode = CAUSE_GET_EXCCODE(psupport->sup_exceptState[GENERALEXCEPT].cause);

    if (excCode == EXC_SYS) {
        /* non-kernel syscalls */
        syscallExceptionHandler(psupport);
    } else {
        /* all other exceptions */
        trapExceptionHandler(psupport);
    }
}

/*
 * Support Trap Exception Handler
 */
HIDDEN void trapExceptionHandler(support_t* psupport)
{
#ifdef DEBUG
    static unsigned int bpSupTrapExcCode;
    bpSupTrapExcCode = CAUSE_GET_EXCCODE(psupport->sup_exceptState[GENERALEXCEPT].cause);
    (void) bpSupTrapExcCode;
#endif

    terminate(psupport);
}

/*
 * Support Syscall Exception Handler
 */
HIDDEN void syscallExceptionHandler(support_t* psupport)
{
#ifdef DEBUG
    static int bpSupSyscallNo;
    bpSupSyscallNo = psupport->sup_exceptState[GENERALEXCEPT].reg_a0;
    (void) bpSupSyscallNo;
#endif

    state_t* processorState = &psupport->sup_exceptState[GENERALEXCEPT];

    int syscallNo = processorState->reg_a0;
    int arg1 = processorState->reg_a1;
    int arg2 = processorState->reg_a2;
    //int arg3 = processorState->reg_a3;

    switch (syscallNo) {
        case GETTOD: /* SYS1 */
            getTod(psupport);
        case TERMINATE: /* SYS2 */
            terminate(psupport);
        case WRITEPRINTER: /* SYS3 */
            writeToPrinter(psupport, (char*) arg1, (int) arg2);
            break;
        case WRITETERMINAL: /* SYS4 */
            writeToTerminal(psupport, (char*) arg1, (int) arg2);
            break;
        case READTERMINAL: /* SYS5 */
            readFromTerminal(psupport, (char*) arg1);
            break;
        default:
            /* non-existent user syscall */
            break;
    }
}

/* --- support functions --- */

HIDDEN void returnFromSysException(support_t* psupport)
{
    /* avoid infinite syscall loops */
    psupport->sup_exceptState[GENERALEXCEPT].pc_epc += WORDLEN;
    LDST(&psupport->sup_exceptState[GENERALEXCEPT]);
}

/* --- syscalls --- */

/*
 * SYS1
 */
HIDDEN void getTod(support_t* psupport)
{
    STCK(psupport->sup_exceptState[GENERALEXCEPT].reg_v0);
    returnFromSysException(psupport);
}

/*
 * SYS2
 */
HIDDEN void terminate(support_t* psupport)
{
    SYSCALL(VERHOGEN, (int) &masterSem, 0, 0);
    SYSCALL(TERMPROCESS, 0, 0, 0);
}


/*
 * SYS3
 * transmit a line of output to the printer
 */
HIDDEN void writeToPrinter(support_t* psupport, char* strVirtAddr, int len)
{
    if ((memaddr) strVirtAddr < KUSEG || len < 0 || len > 128) {
        terminate(psupport);
    }

    unsigned int printerNo = psupport->sup_asid - 1;
    dtpreg_t* printerReg = (dtpreg_t*) DEV_REG_ADDR(PRNTINT, printerNo);

    int i = 0;
    unsigned int status;

    SYSCALL(PASSEREN, (int) &printerSems[printerNo], 0, 0);

    for (; i < len; ++i) {
        setSTATUS(getSTATUS() & (~IECON)); /* atomic on */
        
        printerReg->data0 = ((unsigned int) *(strVirtAddr + i));
        status = SYSCALL(DOIO, (int) &printerReg->command, PRINTCHR, 0);

        setSTATUS(getSTATUS() | IECON); /* atomic off */

        /* error during printing :( */ 
        if (status != READY) {
            i = -status;
            break;
        }
    }

    SYSCALL(VERHOGEN, (int) &printerSems[printerNo], 0, 0);

    /* return the number of characters transmitted */
    psupport->sup_exceptState[GENERALEXCEPT].reg_v0 = i;
    returnFromSysException(psupport);
}

/*
 * SYS4
 * transmit a line of output to the terminal
 */
HIDDEN void writeToTerminal(support_t* psupport, char* strVirtAddr, int len)
{
    if ((memaddr) strVirtAddr < KUSEG || len < 0 || len > 128) {
        terminate(psupport);
    }

    unsigned int terminalNo = psupport->sup_asid - 1;
    termreg_t* terminalReg = (termreg_t*) DEV_REG_ADDR(TERMINT, terminalNo);

    int i = 0;
    unsigned int status;

    SYSCALL(PASSEREN, (int) &termSems[0][terminalNo], 0, 0);

    for (; i < len; ++i) {
        setSTATUS(getSTATUS() & (~IECON)); /* atomic on */
        
        status = SYSCALL(DOIO, (int) &terminalReg->transm_command, TRANSMITCHAR | (((unsigned int) *(strVirtAddr + i)) << BYTELENGTH), 0);

        setSTATUS(getSTATUS() | IECON); /* atomic off */

        /* error during writing :( */ 
        if ((status & TERM_STATUS_MASK) != OKCHARTRANS) {
            i = -(status & TERM_STATUS_MASK);
            break;
        }
    }

    SYSCALL(VERHOGEN, (int) &termSems[0][terminalNo], 0, 0);

    /* return the number of characters transmitted */
    psupport->sup_exceptState[GENERALEXCEPT].reg_v0 = i;
    returnFromSysException(psupport);
}

/*
 * SYS5
 * transmit a line of input from the terminal (receiving)
 */
HIDDEN void readFromTerminal(support_t* psupport, char* strVirtAddr)
{
    if ((memaddr) strVirtAddr < KUSEG) {
        terminate(psupport);
    }

    unsigned int terminalNo = psupport->sup_asid - 1;
    termreg_t* terminalReg = (termreg_t*) DEV_REG_ADDR(TERMINT, terminalNo);

    int i = 0;
    char recvVal = ' '; /* null */
    unsigned int status;

    SYSCALL(PASSEREN, (int) &termSems[1][terminalNo], 0, 0);

    while (recvVal != '\n') {
        setSTATUS(getSTATUS() & (~IECON)); /* atomic on */

        status = SYSCALL(DOIO, (int) &terminalReg->recv_command, RECEIVECHAR, 0);

        setSTATUS(getSTATUS() | IECON); /* atomic off */

        if ((status & TERM_STATUS_MASK) != OKCHARTRANS) {
            i = -(status & TERM_STATUS_MASK);
            break;
        }

        recvVal = status >> BYTELENGTH;
        //diosanto = (char) recvVal;
        *strVirtAddr = recvVal;
        strVirtAddr++;
        ++i;
    }

    SYSCALL(VERHOGEN, (int) &termSems[1][terminalNo], 0, 0);

    *strVirtAddr = '\0';

    psupport->sup_exceptState[GENERALEXCEPT].reg_v0 = i;
    returnFromSysException(psupport);
}