#ifndef PHASE2_HELPERS_H_INCLUDED
#define PHASE2_HELPERS_H_INCLUDED

#include "pandos_types.h"

void     contextSwitch();
void     semSuspend(sem_t* semAddr);
pcb_t*   semWakeup(sem_t* semAddr);
void     insertPrioProcQ(pcb_t* p);
void     outPrioProcQ(pcb_t* p);
int      isDeviceSemaphore(sem_t* semAddr);
pcb_t*   findPcb(pid_t pid);
void     kill(pcb_t* proc);
void     generateException(unsigned int excCode);
void     updateCpuTime();

#endif
