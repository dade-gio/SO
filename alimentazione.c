#include "library.h"

void signal_handler(int sig); // Dichiarazione della funzione di gestione dei segnali.
int semid, shmid, msgid; // Identificatori per i semafori, la memoria condivisa e la coda di messaggi.
data_buffer *shmem_ptr; // Puntatore alla struttura di memoria condivisa.

void signal_handler(int sig) {
    switch (sig) {
        case SIGSEGV:
            kill(shmem_ptr->pid_master, SIGSEGV); // Invia il segnale SIGSEGV al processo master in caso di errore di segmentazione.
        break;
    }
}

int main(int argc, char *argv[]) {

    int atomic_num;
    char n_atom[4];
    int sleep_error;
    srand(getpid()); // Inizializza il generatore di numeri casuali con il PID del processo.

    shmid = atoi(argv[1]); 
    semid = atoi(argv[2]); 
    msgid = atoi(argv[3]); 

    shmem_ptr = (data_buffer *) shmat(shmid, NULL, 0); // attachment della memoria condivisa
    TEST_ERROR;

    struct timespec step_nanosec, step_remaining;
    step_nanosec.tv_sec = 0;
    step_nanosec.tv_nsec = STEP_ALIMENTAZIONE; // Imposta il tempo di alimentazione in nanosecondi.

    struct sigaction sa;
    sigset_t mymask;

    bzero(&sa, sizeof(sa)); // Inizializza la struttura sigaction a zero.
    sa.sa_handler = &signal_handler; // Imposta il gestore dei segnali.
    sa.sa_mask = mymask;

    sigemptyset(&mymask); 
    sigaddset(&mymask, SIGQUIT); 
    sigprocmask(SIG_BLOCK, &mymask, NULL);
    sigaction(SIGSEGV, &sa, NULL); 

    sem.sem_num = STARTSEM; // Seleziona il semaforo di inizio.
    sem.sem_op = -1; // Imposta l'operazione del semaforo a -1.
    semop(semid, &sem, 1); // Esegue l'operazione sul semaforo.

    while (shmem_ptr->termination != 1) {
        while ((sleep_error = nanosleep(&step_nanosec, &step_remaining)) == -1 && errno == EINTR) {
            step_nanosec = step_remaining; // Ripristina il tempo rimanente in caso di interruzione.
        }

        for (int i = 0; i < N_NUOVI_ATOMI; i++) {
            atomic_num = rand() % N_ATOM_MAX + 1; // Genera un numero atomico casuale.
            sprintf(n_atom, "%d", atomic_num); // Converte il numero atomico in stringa.
            char *vec_atomo[] = {"./atomo", n_atom, argv[1], argv[2], argv[3], NULL}; // Prepara gli argomenti per il nuovo processo.

            switch (fork()) {
                case -1:
                    sem.sem_num = MELTDOWNSEM; // Seleziona il semaforo di fusione.
                    sem.sem_op = -1; // Imposta l'operazione del semaforo a -1.
                    semop(semid, &sem, 1); // Esegue l'operazione sul semaforo.
                    kill(shmem_ptr->pid_master, SIGUSR2); // Invia il segnale SIGUSR2 al processo master.
                break;

                case 0:
                    execve("./atomo", vec_atomo, NULL); // Esegue il programma atomo nel processo figlio.
                    TEST_ERROR;
                break;
            }
        }
    }

    return 0; // Termina il processo principale.
}
