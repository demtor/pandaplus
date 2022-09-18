# Documentazione aggiuntiva

La seguente documentazione va considerata come una integrazione al già presente manuale di Panda+. L'obiettivo, infatti, è quello di dare una panoramica generale del codice sorgente del SO e di documentare più dettagliatamente le implementazioni arbitrarie o aggiuntive.

## Struttura della directory

```
pandaplus
├── Makefile
├── src
│   ├── include
│   │   ├── listx.h
│   │   ├── pandos_const.h
│   │   ├── pandos_types2.h
│   │   ├── pandos_types.h
│   │   ├── phase1
│   │   │   ├── asl.h
│   │   │   └── pcb.h
│   │   ├── phase2
│   │   │   ├── exceptions.h
│   │   │   ├── helpers.h
│   │   │   ├── interrupts.h
│   │   │   ├── scheduler.h
│   │   │   ├── syscalls.h
│   │   │   └── variables.h
│   │   ├── phase3
│   │   │   ├── sysSupport.h
│   │   │   └── vmSupport.h
│   │   └── utils.h
│   ├── phase1
│   │   ├── asl.c
│   │   └── pcb.c
│   ├── phase2
│   │   ├── exceptions.c
│   │   ├── helpers.c
│   │   ├── initial.c
│   │   ├── interrupts.c
│   │   ├── scheduler.c
│   │   └── syscalls.c
│   ├── phase3
│   │   ├── initProc.c
│   │   ├── sysSupport.c
│   │   └── vmSupport.c
│   └── utils.c
├── testers
│   ├── fibEight.c
│   ├── fibEleven.c
│   ├── h
│   │   ├── print.h
│   │   └── tconst.h
│   ├── Makefile
│   ├── print.c
│   ├── printerTest.c
│   ├── strConcat.c
│   ├── terminalReader.c
│   ├── terminalTest2.c
│   ├── terminalTest3.c
│   ├── terminalTest4.c
│   └── terminalTest5.c
└── umps3.json

10 directories, 42 files
```

La progettazione del SO è divisa in più fasi e la struttura della directory esplicita questa divisione. 

### Moduli della fase 1 (Gestione delle code)

- `pcb.c` Inizializzazione, allocazione, deallocazione dei PCB; gestione delle code dei processi; gestione della gerarchia dei processi.
- `asl.c` Gestione dei SEMD (ASL).

### Moduli della fase 2 (Il kernel)

- `initial.c` Implementazione della funzione `main()`, che si può considerare come l'insieme delle operazioni effettuate a boot time dal SO. Definizione delle variabili globali del kernel (e.g. conteggio dei processi, coda dei processi, semafori dei device). Nota: il file `variables.h`, disponibile tra gli include della fase 2, presenta la dichiarazione di tutte le variabili globali.
- `scheduler.c` Implementazione dello scheduler.
- `exceptions.c` Contiene esclusivamente l'entry point delle eccezioni, un multiway branch a tutte le eccezioni che si possono verificare (tranne gli eventi TLB-Refill): gli *interrupts* vengono passati al gestore degli interrupts (`interrupts.c`), le *syscalls* al gestore delle syscalls (`syscalls.c`), mentre le *program traps* e le *eccezioni sul TLB* si prova a farle passare alla struttura di supporto del processo interrotto (se fornita, in caso contrario il processo viene terminato).
- `interrupts.c` Contiene il gestore delle eccezioni causati da interrupts, un multiway branch a tutti gli interrupts che si possono verificare.
- `syscalls.c` Contiene il gestore delle eccezioni causati da syscalls, un multiway branch a tutte le syscall del kernel.
- `helpers.c` Funzioni di supporto alle procedure del kernel (e.g. context switch, l'inserimento di un PCB nella coda dei processi con priorità giusta, kill, etc.)

### Moduli globali

- `utils.c` Funzioni di utilità generali (e.g. memcpy, posizione dei bit più e meno significativi, etc.)

## Tipi di dato

Sono stati introdotti nuovi tipi di dato per rendere il ruolo delle variabili definite più chiare e per migliorare la consistenza del codice:

- `size_t` Riprende il classico typedef definito in `stddef.h`.
- `devregf_t` Campo di un registro di un device (device register field).
- `sem_t` Semaforo.
- `pid_t` Process ID.

## Implementazioni

### Allocazione del PID

Sono state definite due nuove macro:

```c
#define PID_MIN 1
#define PID_MAX 0x7FFF
```

Queste macro permettono di controllare il range (estremi inclusi) dei PID che è possibile associare ai processi creati attraverso la NSYS1. Se si oltrepassa `PID_MAX`, si ricomincia ad assegnare valori partendo da `PID_MIN`. In particolare la macro `PID_MIN` permette di stabilire un inizio in modo da avere uno spazio di PID (< `PID_MIN`) riservati ad altri utilizzi (processi speciali, segnalazioni di errori, etc.)

Di seguito lo pseudocodice del codice usato per allocare un PID:

```pseudocode
procedure allocPid():
	static lastpid = PID_MIN - 1

	/* get a pid in the allowed range */
	pid = PID_MIN + (++lastpid - PID_MIN) % (PID_MAX - PID_MIN + 1)
	i = 1
	/* make sure the pid is unique */
	while (i <= PID_MAX - PID_MIN && findPcb(pid) != NULL):
		pid = PID_MIN + (++lastpid - PID_MIN) % (PID_MAX - PID_MIN + 1)
		++i
	if (i == PID_MAX - PID_MIN && findPcb(pid) != NULL):
		return -1 /* can't find a valid pid in the allowed range */

	return pid
```

Per garantire l'univocità del PID viene utilizzata la funzione `findPcb`. Quest'ultima si occupa di cercare il PCB, attraverso il PID fornito, tra le varie code mantenute dal SO.

### Semafori dei device

I semafori dei device (disk, flash, network, printer, terminal, interval timer), usati per la sincronizzazione delle operazioni richieste, sono stati definiti nel seguente modo:

- `devSems[(DEVINTNUM-1)*DEVPERINT]`, cui dimensione corrisponde al prodotto tra le linee di interrupt riservate ai device di I/O, escluso il terminale, e il numero dei device per ogni linea di interrupt. L'indice di un determinato device è dato da `EXT_IL_INDEX(deviceLineNo)*DEVPERINT+deviceNo` (un po' strano ma è per via della mancanza di bidimensionalità), dove `deviceNo` corrisponde al numero del device con un pending interrupt, mentre `deviceLineNo` alla linea di interrupt riservata al device. Ad esempio, il flash device 0 ha come indice `EXT_IL_INDEX(4)*8+0` che equivale a `(1)*8*0=8`. 
- `termSems[2][DEVPERINT]`, cui dimensione corrisponde al prodotto tra il numero di subdevice di un terminale e il numero di terminali nella sua linea di interrupt. Il primo indice identifica il subdevice, mentre il secondo indice il terminale. L'indice 0 è riservato al *transmitter*, mentre l'indice 1 al receiver.
- `pseudoClockSem`.

### Forzare la coda dei processi a bassa priorità

È stata introdotta una nuova variabile booleana `forceLowQ` tra le variabili globali del kernel. Appena viene definita a 1, permette di forzare il dispatching dalla coda dei processi a bassa priorità alla prossima chiamata dello scheduler, ignorando temporaneamente la coda dei processi ad alta priorità. La variabile viene reimpostata allo stato iniziale (0) non appena viene schedulato il processo (a bassa priorità). La syscall `yield` fa uso di questa funzionalità affinché non venga schedulato lo stesso processo ad alta priorità subito dopo la sua sospensione, ma può tranquillamente essere usata in altri contesti.
