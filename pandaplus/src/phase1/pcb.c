#include "pandos_types.h"
#include "pandos_const.h"
#include "listx.h"
#include "phase1/pcb.h"

/* sentinella della lista dei PCB liberi */
HIDDEN LIST_HEAD(pcbFree_sentinel);
/* head lista dei PCB liberi */
HIDDEN list_head_t* pcbFree_h = &pcbFree_sentinel;

/* array di PCB */
HIDDEN pcb_t pcbFree_table[MAXPROC];


void initPcbs() {
    /* aggiunge ogni elemento dell'array di PCB nella lista dei PCB liberi */
    for (int i = 0; i < MAXPROC; ++i) {
        list_add(&pcbFree_table[i].p_list, pcbFree_h);
    }
}

void freePcb(pcb_t* p) {
    /* inserisce p nella lista dei PCB liberi */
    list_add(&p->p_list, pcbFree_h);
}

pcb_t* allocPcb() {
    /* la lista dei PCB liberi è vuota? */
    if (list_empty(pcbFree_h)) {
        return NULL;
    }

    /* p è il PCB in testa (arbitrario) alla lista dei PCB liberi */
    pcb_t* p = container_of(pcbFree_h->next, pcb_t, p_list);

    /* rimuove p dalla lista dei PCB liberi */
    list_del(&p->p_list);
 
    /* inizializza tutti i campi di p a NULL/0 */
    p->p_list.next = NULL;
    p->p_list.prev = NULL;
    p->p_parent = NULL;
    INIT_LIST_HEAD(&p->p_child);
    p->p_sib.next = NULL;
    p->p_sib.prev = NULL;
    p->p_time = 0;
    p->p_semAdd = NULL;
    p->p_supportStruct = NULL;

    /* inizializza il campo p_s di p a 0 */
    for (int i = 0; i < STATE_GPR_LEN; ++i) {
        p->p_s.gpr[i] = 0;
    }
    p->p_s.entry_hi = 0;
    p->p_s.cause = 0;
    p->p_s.status = 0;
    p->p_s.pc_epc = 0;
    p->p_s.hi = 0;
    p->p_s.lo = 0;

    return p;
}

void mkEmptyProcQ(list_head_t* head) {
    /* inizializza la coda dei processi */
    INIT_LIST_HEAD(head);
}

int emptyProcQ(const list_head_t* head) {
    /* la coda dei processi è vuota? */
    return list_empty(head);
}

void insertProcQ(list_head_t* head, pcb_t* p) {
    /* inserisce p nella coda dei processi */
    list_add_tail(&p->p_list, head);
}

pcb_t* headProcQ(const list_head_t* head) {
    /* la coda dei processi è vuota? */
    if (emptyProcQ(head)) {
        return NULL;
    }

    /* restituisce il PCB in testa alla coda dei processi */
    return container_of(head->next, pcb_t, p_list);
}

pcb_t* removeProcQ(list_head_t* head) {
    /* la coda dei processi è vuota? */
    if (emptyProcQ(head)) {
        return NULL;
    }

    /* p è il PCB in testa alla coda dei processi  */
    pcb_t* p = container_of(head->next, pcb_t, p_list);

    /* rimuove p dalla coda dei processi */
    list_del(&p->p_list);

    return p;
}

pcb_t* outProcQ(list_head_t* head, pcb_t* p) {
    pcb_t* current;

    /* cerca un p nella coda dei processi */
    list_for_each_entry(current, head, p_list) {
        if (current == p) {
            /* rimuove p dalla coda dei processi */
            list_del(&current->p_list);
            return current;
        }
    }

    /* altrimenti se p non si trova nella coda dei processi */
    return NULL;
}

int emptyChild(const pcb_t* p) {
    /* p ha processi figli? */
    return list_empty(&p->p_child);
}

void insertChild(pcb_t* prnt, pcb_t* p) {
    /* inserisce p nella lista dei figli di prnt */
    list_add(&p->p_sib, &prnt->p_child);
    p->p_parent = prnt;
}

pcb_t* removeChild(pcb_t* p) {
    /* p ha processi figli? */
    if (emptyChild(p)) {
        return NULL;
    }

    /* child è il PCB in testa alla lista dei figli di p */
    pcb_t* child = container_of(p->p_child.next, pcb_t, p_sib);

    /* rimuove child dalla lista dei figli di p */
    list_del(&child->p_sib);
    child->p_parent = NULL;

    return child;
}

pcb_t* outChild(pcb_t* p) {
    /* p è un processo figlio? */
    if (p->p_parent == NULL) {
        return NULL;
    }

    /* rimuove p dalla lista dei figli di p->p_parent */
    list_del(&p->p_sib);
    p->p_parent = NULL;

    return p;
}
