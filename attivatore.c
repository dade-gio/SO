#include "library.h"

int semid, shmid, msgid; // Identificatori per i semafori, la memoria condivisa e la coda di messaggi.
data_buffer *shmem_ptr; // Puntatore alla struttura di memoria condivisa.

// Funzione per gestire i segnali
void signal_handler(int sig) {
	switch(sig) {
		case SIGSEGV: // In caso di errore di segmentazione
			kill(shmem_ptr->pid_master, SIGSEGV); // Invia il segnale SIGSEGV al processo master
			break;
	}
}

int main(int argc, char* argv[]) {

	pid_t new_pid;
	int prob_division, sleep_error;

	semid = atoi(argv[1]);
	shmid = atoi(argv[2]);
	msgid = atoi(argv[3]);

	shmem_ptr = (data_buffer *) shmat(shmid, NULL, 0);
	TEST_ERROR;

	struct timespec step_nanosec, step_remaining;
	step_nanosec.tv_sec = 0;
	step_nanosec.tv_nsec = STEP_ATTIVATORE;

	// Inizializza la struttura sigaction per la gestione dei segnali
	struct sigaction sa;
	sigset_t mymask;

	bzero(&sa, sizeof(sa)); // Imposta a zero la struttura sigaction
	sa.sa_handler = &signal_handler; // Imposta il gestore dei segnali
	sa.sa_flags = SA_RESTART; // Imposta il flag per riavviare le system call interrotte
	sa.sa_mask = mymask;

	sigemptyset(&mymask); // Inizializza il set di segnali a vuoto
	sigaddset(&mymask, SIGQUIT); // Aggiunge SIGQUIT al set di segnali
	sigprocmask(SIG_BLOCK, &mymask, NULL); // Blocca il segnale SIGQUIT
	sigaction(SIGSEGV, &sa, NULL); // Imposta il gestore per SIGSEGV

	// Operazione sul semaforo per iniziare
	sem.sem_num = STARTSEM;
	sem.sem_op = -1;
	semop(semid, &sem, 1);

	while (shmem_ptr->termination != 1) { // Loop principale finché non è richiesta la terminazione
		// Pausa di attivazione con gestione dell'interruzione
		while ((sleep_error = nanosleep(&step_nanosec, &step_remaining)) == -1 && errno == EINTR) {
			step_nanosec = step_remaining;
		}

		// Riceve il PID di un nuovo processo
		do {
			new_pid = receive_pid(msgid);
		} while (new_pid == -1 && errno == EINTR);

		// Se l'inibitore è attivo
		if (shmem_ptr->inib_on == 1) {
			prob_division = rand() % 11; // Genera una probabilità casuale

			if (prob_division <= 2) { // Probabilità di attivazione
				shmem_ptr->act_rel = shmem_ptr->act_rel + 1; // Incrementa il contatore di attivazione
				kill(new_pid, SIGUSR2); // Invia il segnale SIGUSR2 al nuovo processo
			} else { // In caso contrario
				kill(new_pid, SIGTERM); // Termina il nuovo processo
				// Operazioni sul semaforo per gestione dei rifiuti
				sem.sem_num = WASTESEM;
				sem.sem_op = -1;
				semop(semid, &sem, 1);

				shmem_ptr->waste_rel = shmem_ptr->waste_rel + 1; // Incrementa il contatore dei rifiuti

				sem.sem_num = WASTESEM;
				sem.sem_op = 1;
				semop(semid, &sem, 1);

				shmem_ptr->undiv_rel = shmem_ptr->undiv_rel + 1; // Incrementa il contatore delle divisioni non riuscite
			}
		} else { // Se l'inibitore non è attivo
			shmem_ptr->act_rel = shmem_ptr->act_rel + 1; // Incrementa il contatore di attivazione
			kill(new_pid, SIGUSR2); // Invia il segnale SIGUSR2 al nuovo processo
		}
	}
	return 0; // Termina il processo principale
}
