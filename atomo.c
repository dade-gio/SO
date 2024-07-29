#include "library.h"

void generate_n_atom(int *, int *); // Dichiarazione della funzione che genera il numero di atomi figli.
void operate_in_sem(int, int); // Dichiarazione della funzione che opera sui semafori.
int energy(int, int); // Dichiarazione della funzione che calcola l'energia prodotta.

int semid, shmid, msgid; // Identificatori per i semafori, la memoria condivisa e la coda di messaggi.
data_buffer *shmem_ptr; // Puntatore alla struttura di memoria condivisa.

void signal_handler(int sig) {
	switch(sig) {
		case SIGUSR2:
		break;

		case SIGSEGV:
    	kill(shmem_ptr->pid_master, SIGSEGV); // Invia il segnale SIGSEGV al processo master in caso di errore di segmentazione.
    break;
	}
}

int main(int argc, char* argv[]) {

	int child_atom_num, en_lib, parent_atom_num;
	char division_atom_num[3], division_parent_num[4];

	parent_atom_num = atoi(argv[1]); // Converte l'argomento 1 in numero intero.
	shmid = atoi(argv[2]); // Converte l'argomento 2 in numero intero.
	semid = atoi(argv[3]); // Converte l'argomento 3 in numero intero.
	msgid = atoi(argv[4]); // Converte l'argomento 4 in numero intero.

	shmem_ptr = (data_buffer *) shmat(shmid, NULL, 0); // Attacca la memoria condivisa.
	TEST_ERROR;

	struct sigaction sa;
	sigset_t mymask;

	bzero(&sa, sizeof(sa)); // Inizializza la struttura sigaction a zero.
	sa.sa_handler = &signal_handler; // Imposta il gestore dei segnali.
	sa.sa_mask = mymask;

	sigemptyset(&mymask); // Inizializza il set di segnali a vuoto.
	sigaddset(&mymask, SIGQUIT); // Aggiunge SIGQUIT al set di segnali.
	sigprocmask(SIG_BLOCK, &mymask, NULL); // Blocca il segnale SIGQUIT.
	sigaction(SIGUSR2, &sa, NULL); // Imposta il gestore per SIGUSR2.
	sigaction(SIGSEGV, &sa, NULL); // Imposta il gestore per SIGSEGV.

	srand(getpid()); // Inizializza il generatore di numeri casuali con il PID del processo.

	if (shmem_ptr->simulation_start == 1) { operate_in_sem(STARTSEM, 0); } // Se la simulazione è iniziata, esegue un'operazione sul semaforo STARTSEM.

	if (parent_atom_num <= MIN_N_ATOMICO) {
		operate_in_sem(WASTESEM, 0); // Se il numero atomico del genitore è minore o uguale al minimo, opera sul semaforo WASTESEM.
		exit(0); // Termina il processo.
	}

	send_atom_pid(msgid, getpid()); // Invia il PID dell'atomo alla coda di messaggi.

	pause(); // Attende un segnale.

	generate_n_atom(&parent_atom_num, &child_atom_num); // Genera i numeri atomici per il genitore e il figlio.
	sprintf(division_atom_num, "%d", child_atom_num); // Converte il numero atomico del figlio in stringa.
	char *vec_atomo[] = {"atomo", division_atom_num, argv[2], argv[3], NULL}; // Prepara gli argomenti per il processo figlio.

	switch (fork()) {
		case -1:
			sem.sem_num = MELTDOWNSEM; // Seleziona il semaforo di fusione.
			sem.sem_op = -1; // Imposta l'operazione del semaforo a -1.
			semop(semid, &sem, 1); // Esegue l'operazione sul semaforo.
			kill(shmem_ptr->pid_master, SIGUSR2); // Invia il segnale SIGUSR2 al processo master.
		break;

		case 0:
			operate_in_sem(DIVISIONSEM, 0); // Opera sul semaforo di divisione.
			execve("./atomo", vec_atomo, NULL); // Esegue il programma atomo nel processo figlio.
			TEST_ERROR;
		break;

		default:
			en_lib = energy(child_atom_num, parent_atom_num); // Calcola l'energia liberata.

			operate_in_sem(PROD_ENERGYSEM, en_lib); // Opera sul semaforo di produzione energia.

			sprintf(division_parent_num, "%d", parent_atom_num); // Converte il numero atomico del genitore in stringa.
			char *new_vec_atomo[] = {"./atomo", division_parent_num, argv[2], argv[3], NULL}; // Prepara gli argomenti per il nuovo processo genitore.
			execve("atomo", new_vec_atomo, NULL); // Esegue il programma atomo nel processo genitore.
			TEST_ERROR;
		break;
	}
}


void generate_n_atom(int *parent_atom_num, int *child_atom_num) {
  int temp = *parent_atom_num;
  *parent_atom_num = (rand() % *parent_atom_num) + 1; // Genera un nuovo numero atomico per il genitore.
  *child_atom_num = temp - *parent_atom_num; // Calcola il numero atomico del figlio.
}

void operate_in_sem(int sem_working, int en_lib) {

	switch (sem_working) {
		case STARTSEM:
			sem.sem_num = STARTSEM; // Seleziona il semaforo di inizio.
			sem.sem_op = -1; // Imposta l'operazione del semaforo a -1.
  			semop(semid, &sem, 1); // Esegue l'operazione sul semaforo.

			shmem_ptr->simulation_start = 0; // Imposta l'inizio della simulazione a 0.
		break;

		case WASTESEM:
			sem.sem_num = WASTESEM; // Seleziona il semaforo di scarto.
			sem.sem_op = -1; // Imposta l'operazione del semaforo a -1.
			semop(semid, &sem, 1); // Esegue l'operazione sul semaforo.

			shmem_ptr->waste_rel = shmem_ptr->waste_rel + 1; // Incrementa il valore di scarto relativo.

			sem.sem_num = WASTESEM; // Seleziona nuovamente il semaforo di scarto.
			sem.sem_op = 1; // Imposta l'operazione del semaforo a 1.
			semop(semid, &sem, 1); // Esegue l'operazione sul semaforo.
		break;

		case PROD_ENERGYSEM:
			sem.sem_num = PROD_ENERGYSEM; // Seleziona il semaforo di produzione energia.
			sem.sem_op = -1; // Imposta l'operazione del semaforo a -1.
			semop(semid, &sem, 1); // Esegue l'operazione sul semaforo.

			shmem_ptr->prod_en_rel = shmem_ptr->prod_en_rel + en_lib; // Incrementa l'energia prodotta relativa.

			sem.sem_num = PROD_ENERGYSEM; // Seleziona nuovamente il semaforo di produzione energia.
			sem.sem_op = 1; // Imposta l'operazione del semaforo a 1.
			semop(semid, &sem, 1); // Esegue l'operazione sul semaforo.
		break;

		case DIVISIONSEM:
			sem.sem_num = DIVISIONSEM; // Seleziona il semaforo di divisione.
			sem.sem_op = -1; // Imposta l'operazione del semaforo a -1.
			semop(semid, &sem, 1); // Esegue l'operazione sul semaforo.

			shmem_ptr->div_rel = shmem_ptr->div_rel + 1; // Incrementa il valore di divisione relativo.

			sem.sem_num = DIVISIONSEM; // Seleziona nuovamente il semaforo di divisione.
			sem.sem_op = 1; // Imposta l'operazione del semaforo a 1.
			semop(semid, &sem, 1); // Esegue l'operazione sul semaforo.
		break;
	}
}

int energy(int child, int parent) {
    return child * parent - MAX(child, parent); // Calcola l'energia prodotta in base ai numeri atomici del genitore e del figlio.
}
