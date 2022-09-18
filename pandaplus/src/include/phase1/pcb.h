#ifndef PHASE1_PCB_H_INCLUDED
#define PHASE1_PCB_H_INCLUDED

#include "pandos_types.h"

/* Allocazione dei PCB */
void   initPcbs();
void   freePcb(pcb_t* p);
pcb_t* allocPcb();

/* Gestione delle code di processi (liste di PCB) */
void   mkEmptyProcQ(list_head_t* head);
int    emptyProcQ(const list_head_t* head);
void   insertProcQ(list_head_t* head, pcb_t* p);
pcb_t* headProcQ(const list_head_t* head);
pcb_t* removeProcQ(list_head_t* head);
pcb_t* outProcQ(list_head_t* head, pcb_t* p);

/* Gestione degli alberi di processi */
int    emptyChild(const pcb_t* p);
void   insertChild(pcb_t* prnt, pcb_t* p);
pcb_t* removeChild(pcb_t* p);
pcb_t* outChild(pcb_t* p);

#endif
