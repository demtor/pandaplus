#include <umps/libumps.h>
#include <umps/cp0.h>
#include <umps/arch.h>
#include "pandos_types.h"
#include "pandos_const.h"
#include "utils.h"

#include "phase2/syscalls.h"
#include "phase2/scheduler.h"
#include "phase2/exceptions.h"
#include "phase2/helpers.h"
#include "phase2/variables.h"
#include "phase1/asl.h"
#include "phase1/pcb.h"

/* --- prototypes --- */

HIDDEN void sysContextSwitch();
HIDDEN void returnFromSysException();
HIDDEN void setSysReturnValue(unsigned int v);

HIDDEN void createProcess(state_t* pstate, int prio, support_t* psupport);
HIDDEN void terminateProcess(pid_t pid);
HIDDEN void passeren(sem_t* semAddr);
HIDDEN void verhogen(sem_t* semAddr);
HIDDEN void doIoDevice(devregf_t* commandAddr, devregf_t commandValue);
HIDDEN void getCpuTime();
HIDDEN void waitForClock();
HIDDEN void getSupportData();
HIDDEN void getProcessId(int parent);
HIDDEN void yield();

extern cpu_t startingTime;

/* --- main handler --- */

/*
 * Kernel Syscall Exception Handler
 * occurs when the SYSCALL instruction is executed
 */
void syscallExceptionHandler(state_t* pstate)
{
#ifdef DEBUG
    static int bpSyscallNo;
    /* make debug of (negative) kernel syscall easier */
    bpSyscallNo = (int)pstate->reg_a0 < 0 ? -pstate->reg_a0 + 0xF0 : pstate->reg_a0;
    (void) bpSyscallNo;
#endif

    int syscallNo = pstate->reg_a0;

    /*
     * processes in user-mode cant perform kernel syscalls
     * kernel syscalls are identified by a negative syscall number
     */
    if ((pstate->status & USERPON) == USERPON && syscallNo < 0) {
        generateException(EXC_RI);
    }

    /*
     * try to pass up if a non-kernel (user) syscall is requested
     * non-kernel syscalls are identified by a positive syscall number
     */
    if (syscallNo >= 1) {
        passUpOrDie(pstate, GENERALEXCEPT);
    }

    int arg1 = pstate->reg_a1;
    int arg2 = pstate->reg_a2;
    int arg3 = pstate->reg_a3;

    switch (syscallNo) {
        case CREATEPROCESS: /* NSYS1 */
            createProcess((state_t*) arg1, arg2, (support_t*) arg3);
            break;
        case TERMPROCESS: /* NSYS2 */
            terminateProcess(arg1);
            break;
        case PASSEREN: /* NSYS3 (sem_wait) */
            passeren((sem_t*) arg1);
            break;
        case VERHOGEN: /* NSYS4 (sem_post) */
            verhogen((sem_t*) arg1);
            break;
        case DOIO: /* NSYS5 */
            doIoDevice((devregf_t*) arg1, (devregf_t) arg2);
            break;
        case GETTIME: /* NSYS6 */
            getCpuTime();
            break;
        case CLOCKWAIT: /* NSYS7 */
            waitForClock();
            break;
        case GETSUPPORTPTR: /* NSYS8 */
            getSupportData();
            break;
        case GETPROCESSID: /* NSYS9 */
            getProcessId(arg1);
            break;
        case YIELD: /* NSYS10 */
            yield();
            break;
        default:
            generateException(EXC_RI); /* non-existent kernel syscall */
            break;
    }
}

/* --- support functions --- */

HIDDEN void sysContextSwitch()
{
    /*
     * update the processor state of the current executing process
     * before descheduling it
     */
    memcpy(&currentProcess->p_s, (state_t*) PROCESSORSTATE0, sizeof(state_t));
    /*
     * make sure that when the process is scheduled again
     * it doesnt start from the syscall already handled
     * (avoid infinite syscall loops)
     */
    currentProcess->p_s.pc_epc += WORDLEN;
    /* update accumulated processor time by the current process before scheduling */
    updateCpuTime();

    scheduler();
}

HIDDEN void returnFromSysException()
{
    /* avoid infinite syscall loops */
    ((state_t*) PROCESSORSTATE0)->pc_epc += WORDLEN;
    LDST((state_t*) PROCESSORSTATE0);
}

HIDDEN void setSysReturnValue(unsigned int v)
{
    ((state_t*) PROCESSORSTATE0)->reg_v0 = v;
}

/* --- syscalls --- */

/*
 * NSYS1
 */
HIDDEN inline pid_t allocPid() {
    static pid_t lastpid = 1;

    /* get a pid in the allowed range */
    pid_t pid = PID_MIN + (++lastpid - PID_MIN) % (PID_MAX - PID_MIN + 1);

    /* make sure the pid is unique */
    size_t i = 1;
    while (i <= PID_MAX - PID_MIN && findPcb(pid) != NULL) {
        pid = PID_MIN + (++lastpid - PID_MIN) % (PID_MAX - PID_MIN + 1);
        ++i;
    }

    if (i == PID_MAX - PID_MIN && findPcb(pid) != NULL) {
        return -1; /* can't find a valid pid in the allowed range */
    }

    return pid;
}

HIDDEN void createProcess(state_t* pstate, int prio, support_t* psupport)
{
    pcb_t* proc = allocPcb();
    pid_t pid = allocPid();

    if (proc == NULL || pid == -1) {
        setSysReturnValue(-1);
        returnFromSysException();
    }

    /* new process setup */
    proc->p_pid = pid;
    proc->p_prio = prio;
    proc->p_supportStruct = psupport;
    memcpy(&proc->p_s, pstate, sizeof(state_t));

    /* the new process is part of the progeny of the caller */
    insertChild(currentProcess, proc);

    /* warn the kernel about the new process */
    insertPrioProcQ(proc);
    ++processCount;

    setSysReturnValue(proc->p_pid);
    returnFromSysException();
}

/*
 * NSYS2
 */
HIDDEN void terminateProcess(pid_t pid)
{
    kill(pid == 0 ? currentProcess : findPcb(pid));

    if (currentProcess == NULL) {
        scheduler();
    } else {
        returnFromSysException();
    }
}

/*
 * NSYS3 (sem_wait)
 */
HIDDEN void passeren(sem_t* semAddr)
{
    if (*semAddr == 0) {
        semSuspend(semAddr);
        sysContextSwitch();
    } else {
        /* decrement only if we could not wake up any process */
        if (semWakeup(semAddr) == NULL) {
            --(*semAddr); /* (0) */
        }

        returnFromSysException();
    }
}

/* 
 * NSYS4 (sem_post / signal)
 */
HIDDEN void verhogen(sem_t* semAddr)
{
    if (*semAddr == 1) {
        semSuspend(semAddr);
        sysContextSwitch();
    } else {
        /* increment only if we could not wake up any process */
        if (semWakeup(semAddr) == NULL) {
            ++(*semAddr); /* (1) */
        }

        returnFromSysException();
    }
}

/*
 * Support function for doIoDevice
 * it retrieves the correct device semaphore
 * starting from the commandAddr given to DOIO NSYS5
 */
HIDDEN inline sem_t* getDeviceSemAddr(memaddr commandAddr)
{
    sem_t* semAddr;

    /* terminal device register area */
    const memaddr TERM_REG_START = DEV_REG_ADDR(TERMINT, 0);
    const memaddr TERM_REG_END = DEV_REG_ADDR(TERMINT, N_DEV_PER_IL);

    /* if we are handling a terminal device... */
    if (commandAddr >= TERM_REG_START && commandAddr < TERM_REG_END) {
        /* determine the terminal sub-device semaphore */
        int isTermReceiver = (commandAddr - TERM_REG_START) % DEV_REG_SIZE == RECVCOMMAND * DEV_REG_SIZE_W;
        semAddr = &termSems[isTermReceiver][(commandAddr - TERM_REG_START) / DEV_REG_SIZE];
    } else {
        /* determine the device semaphore */
        semAddr = &devSems[(commandAddr - DEV_REG_START) / DEV_REG_SIZE];
    }

    return semAddr;
}

/*
 * NSYS5
 */
HIDDEN void doIoDevice(devregf_t* commandAddr, devregf_t commandValue)
{
    /* begin I/O operation */
    *commandAddr = commandValue;

    sem_t* semAddr = getDeviceSemAddr((memaddr) commandAddr);

    ++softBlockCount;
    /* should always block since dev semaphores are used for sync */
    passeren(semAddr);
}

/*
 * NSYS6
 */
HIDDEN void getCpuTime()
{
    updateCpuTime();

    setSysReturnValue(currentProcess->p_time); 
    returnFromSysException();
}

/*
 * NSYS7
 */
HIDDEN void waitForClock()
{
    ++softBlockCount;
    /* should always block since dev semaphores are used for sync */
    passeren(&pseudoClockSem);
}

/*
 * NSYS8
 */
HIDDEN void getSupportData()
{
    setSysReturnValue((memaddr) currentProcess->p_supportStruct);
    returnFromSysException();
}

/*
 * NSYS9
 */
HIDDEN void getProcessId(int parent)
{
    if (parent) {
        if (currentProcess->p_parent != NULL) {
            setSysReturnValue(currentProcess->p_parent->p_pid);
        } else {
            setSysReturnValue(0);
        }
    } else {
        setSysReturnValue(currentProcess->p_pid);
    }

    returnFromSysException();
}

/*
 * NSYS10
 */
HIDDEN void yield()
{
    /*
     * if we are handling a high priority proces
     * and the queue of high priority processes is empty
     * make sure to try to force the queue of low priority processes
     * on the context switch
     */
    if (currentProcess->p_prio == PROCESS_PRIO_HIGH && headProcQ(procQHigh)) {
        forceLowQ = 1;
    }

    insertPrioProcQ(currentProcess);
    sysContextSwitch();
}
