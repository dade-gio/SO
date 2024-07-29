#include "library.h"

int semid, shmid, msgid; // Identificatori per i semafori, la memoria condivisa e la coda di messaggi.
data_buffer *shmem_ptr; // Puntatore alla struttura di memoria condivisa.

// Funzione per gestire i segnali
void signal_handler(int sig){
  switch (sig) {
    case SIGQUIT: // Gestione del segnale SIGQUIT
      if (shmem_ptr->inib_on != 0) { // Se l'inibitore è attivo
        write(0, "Inibitore OFF. Turn off anytime with ctrl + backslash \n", 55); // Messaggio informativo
        shmem_ptr->inib_on = 0; // Disattiva l'inibitore
      } else if (shmem_ptr->inib_on == 0) { // Se l'inibitore è disattivo
        kill(shmem_ptr->pid_master, SIGUSR1); // Invia SIGUSR1 al processo master
      }
    break;

    case SIGSEGV: // Gestione del segnale SIGSEGV
      kill(shmem_ptr->pid_master, SIGSEGV); // Invia SIGSEGV al processo master
    break;
  }
}

int main(int argc, char* argv[]) {

  int absorbing_energy, sleep_error;

  // Converte gli argomenti della riga di comando in numeri interi
  semid = atoi(argv[1]);
  shmid = atoi(argv[2]);
  msgid = atoi(argv[3]);

  // Attacca la memoria condivisa
  shmem_ptr = (data_buffer *) shmat(shmid, NULL, 0); 
  TEST_ERROR;

  srand(getpid()); // Inizializza il generatore di numeri casuali con il PID del processo

  // Inizializza la struttura sigaction per la gestione dei segnali
  struct sigaction sa;
  bzero(&sa, sizeof(sa)); // Imposta a zero la struttura sigaction
  sa.sa_handler = &signal_handler; // Imposta il gestore dei segnali
  sa.sa_flags = SA_RESTART; // Imposta il flag per riavviare le system call interrotte
  sigaction(SIGQUIT, &sa, NULL); // Imposta il gestore per SIGQUIT
  sigaction(SIGSEGV, &sa, NULL); // Imposta il gestore per SIGSEGV

  // Inizializza la struttura timespec per il tempo di attivazione
  struct timespec step_nanosec, step_remaining;
  step_nanosec.tv_sec = 0;
  step_nanosec.tv_nsec = STEP_INIBITORE;

  shmem_ptr->inib_on = 0; // Imposta l'inibitore come disattivo

  // Operazione sul semaforo per iniziare
  sem.sem_num = STARTSEM;
  sem.sem_op = -1;
  semop(semid, &sem, 1);

  // Operazione sul semaforo dell'inibitore
  sem.sem_num = INIBSEM;
  sem.sem_op = -1;
  semop(semid, &sem, 1);

  while (shmem_ptr->termination == 0) { // Loop principale finché non è richiesta la terminazione
    // Pausa di attivazione con gestione dell'interruzione
    while((sleep_error = nanosleep(&step_nanosec, &step_remaining)) == -1 && errno == EINTR) {
      step_nanosec = step_remaining;
    } 

    if(shmem_ptr->inib_on == 0) { // Se l'inibitore è disattivo
      // Operazione sul semaforo dell'inibitore
      sem.sem_num = INIBSEM;
      sem.sem_op = -1;
      semop(semid, &sem, 1);
    }
    
    // Calcola l'energia assorbita
    absorbing_energy = shmem_ptr->prod_en_rel / 10;
    shmem_ptr->prod_en_rel -= absorbing_energy;
    shmem_ptr->absorbed_en_rel += absorbing_energy;
  }
  return 0; // Termina il processo principale
}
