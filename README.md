# Panda+

## Requisiti

- [µMPS cross toolchain](https://wiki.virtualsquare.org/education/tutorials/umps/installation.html)
- GNU make

## Compilazione

Effettuare la compilazione attraverso il Makefile incluso:

```bash
$ cd pandaplus
$ make
```

---

*Nota*: il Makefile fa uso del toolchain che ha come prefisso `mipsel-linux-gnu-`. Questo potrebbe variare in base alla distro GNU/Linux con cui si effettua la compilazione, quindi in caso è possibile cambiare la definizione del prefisso attraverso l'opzione `XT_PRG_PREFIX=`. Per esempio, gli utenti di Fedora potrebbero aver bisogno di effettuare la compilazione con:

```bash
$ make XT_PRG_PREFIX=mips64-linux-gnu-
```

## Esecuzione

Di seguito l'esecuzione:

1. Lanciare l'applicazione µMPS3, è possibile farlo anche attraverso il comando `umps3`.
2. Creare una nuova macchina andando su Simulator → New Configuration, selezionare la directory `pandaplus` e inserire un nome a piacere da dare alla macchina.
3. Accendere la macchina per poi avviarla.

Per vedere l'output di `p2test.c` andare su Windows → Terminal 0 (o Alt+0).
