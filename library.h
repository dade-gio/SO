#ifndef LIBRARY_H
#define LIBRARY_H

#define _GNU_SOURCE
#include "parameters.txt"
#include "library.c"

#define STARTSEM 0
#define WASTESEM 1
#define PROD_ENERGYSEM 2
#define DIVISIONSEM 3
#define INIBSEM 4
#define MELTDOWNSEM 5

int send_atom_pid(int msgid, int pid);

int receive_pid(int msgid);

#endif /* LIBRARY_H */
