#ifndef PHASE2_EXCEPTIONS_H_INCLUDED
#define PHASE2_EXCEPTIONS_H_INCLUDED

#include "pandos_types.h"

void exceptionHandler();
void passUpOrDie(state_t* pstate, unsigned int excType);

#endif
