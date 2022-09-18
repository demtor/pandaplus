#ifndef PHASE1_ASL_H_INCLUDED
#define PHASE1_ASL_H_INCLUDED

#include "pandos_types.h"

/* Gestione della ASL (Active Semaphore List) */
void         initASL();
list_head_t* getSemdHead();
int          insertBlocked(const int* semAdd, pcb_t* p);
pcb_t*       removeBlocked(const int* semAdd);
pcb_t*       outBlocked(pcb_t* p);
pcb_t*       headBlocked(const int* semAdd);

#endif
