#include "pandos_types.h"
#include "pandos_const.h"
#include "listx.h"
#include "phase1/asl.h"
#include "phase1/pcb.h"

HIDDEN semd_t* allocSemd(const int* semAdd);
HIDDEN void    freeSemd(semd_t* s);
HIDDEN semd_t* getSemd(const int* semAdd);

/* sentinella della lista dei SEMD liberi */
HIDDEN LIST_HEAD(semdFree_sentinel);
/* head lista dei SEMD liberi */
HIDDEN list_head_t* semdFree_h = &semdFree_sentinel;

/* sentinella della lista dei SEMD attivi (ASL) */
HIDDEN LIST_HEAD(semd_sentinel);
/* head della lista dei SEMD attivi (ASL) */
HIDDEN list_head_t* semd_h = &semd_sentinel;

/* array di SEMD */
HIDDEN semd_t semd_table[MAXPROC];


HIDDEN semd_t* allocSemd(const int* semAdd) {
    /* la lista dei SEMD liberi è vuota? */
    if (list_empty(semdFree_h)) {
        return NULL;
    }

    /* s è il SEMD in testa (arbitrario) alla lista dei SEMD liberi */
    semd_t* s = container_of(semdFree_h->next, semd_t, s_link);

    /* rimuove s dalla lista dei SEMD liberi */
    list_del(&s->s_link);

    /* inserisce s nella lista dei SEMD attivi */
    list_add(&s->s_link, semd_h);

    /* inizializza tutti i campi di s */
    s->s_key = (int*) semAdd;
    mkEmptyProcQ(&s->s_procq);

    return s;
}

HIDDEN void freeSemd(semd_t* s) {
    /* rimuove s dalla lista dei SEMD attivi */
    list_del(&s->s_link);

    /* inserisce s nella lista dei SEMD liberi */
    list_add(&s->s_link, semdFree_h);
}

HIDDEN semd_t* getSemd(const int* semAdd) {
    semd_t* current;

    /* cerca un SEMD con key semAdd nella lista dei SEMD liberi */
    list_for_each_entry(current, semd_h, s_link) {
        if (current->s_key == semAdd) {
            return current;
        }
    }

    return NULL;
}


void initASL() {
    /* aggiunge ogni elemento dell'array di SEMD nella lista dei SEMD liberi */
    for (int i = 0; i < MAXPROC; ++i) {
        list_add(&semd_table[i].s_link, semdFree_h);
    }
}

list_head_t* getSemdHead()
{
    /* restituisce l'head della lista dei SEMD attivi */
    return semd_h;
}

int insertBlocked(const int* semAdd, pcb_t* p) {
    semd_t* s;

    /*
     * se viene trovato s con key semAdd o
     * se è possibile allocare un nuovo s con key semAdd,
     * inserisce p nella coda dei processi bloccati di s
     */
    if ((s = getSemd(semAdd)) != NULL || (s = allocSemd(semAdd)) != NULL) {
        p->p_semAdd = (int*) semAdd;
        insertProcQ(&s->s_procq, p);
        return 0;
    }

    return 1;
}

pcb_t* removeBlocked(const int* semAdd) {
    semd_t* s = getSemd(semAdd);

    /* semAdd valido? */
    if (s == NULL) {
        return NULL;
    }

    /* rimuove il PCB in testa alla coda dei processi bloccati di s */
    pcb_t* p = removeProcQ(&s->s_procq);
    p->p_semAdd = NULL;

    /* se la coda dei processi è vuota dopo la rimozione, libera il SEMD */
    if (emptyProcQ(&s->s_procq)) {
        freeSemd(s);
    }

    return p;
}

pcb_t* outBlocked(pcb_t* p) {
    semd_t* s = getSemd(p->p_semAdd);

    /* semAdd valido? */
    if (s == NULL) {
        return NULL;
    }

    /* rimuove p dalla coda dei processi bloccati di s */
    p = outProcQ(&s->s_procq, p);

    /* p non rimosso */
    if (p == NULL) {
        return NULL;
    }

    p->p_semAdd = NULL;

    /* se la coda dei processi è vuota dopo la rimozione, libera il SEMD */
    if (emptyProcQ(&s->s_procq)) {
        freeSemd(s);
    }

    return p;
}

pcb_t* headBlocked(const int* semAdd) {
    semd_t* s = getSemd(semAdd);

    /* semAdd valido? */
    if (s == NULL) {
        return NULL;
    }

    /* restituisce il PCB in testa alla coda dei processi bloccati di s */
    return headProcQ(&s->s_procq);
}
