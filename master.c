#include "library.h" // Include il file di intestazione che contiene le dichiarazioni e le definizioni necessarie.

void set_sem_values(); // Dichiarazione della funzione per impostare i valori dei semafori.
void print_stats(); // Dichiarazione della funzione per stampare le statistiche.
void stat_total_value(); // Dichiarazione della funzione per aggiornare i valori totali delle statistiche.

int shmid, semid, available_en, msgid; // Identificatori per la memoria condivisa, i semafori e la coda di messaggi.
data_buffer *shmem_ptr; // Puntatore alla struttura di memoria condivisa.
pid_t pid_alimentazione, pid_attivatore, pid_inibitore; // PID dei processi figli.

void signal_handler(int sig) { // Gestore dei segnali.
  switch(sig) { // Switch per gestire diversi segnali.
    case SIGALRM:
      shmem_ptr->termination = 1; // Imposta la variabile di terminazione a 1.
      shmem_ptr->message = "timeout."; // Imposta il messaggio di terminazione a "timeout".
    break;

    case SIGUSR2:
      write(0, "Simulation terminated due to meltdown.\n", 39); // Scrive un messaggio di terminazione per fusione.
      kill(pid_alimentazione, SIGTERM); // Termina il processo alimentazione.
      kill(pid_attivatore, SIGTERM); // Termina il processo attivatore.
      kill(pid_inibitore, SIGTERM); // Termina il processo inibitore.
      semctl(semid, 0, IPC_RMID); // Rimuove l'array di semafori.
      shmdt(shmem_ptr); // Disconnette la memoria condivisa.
      shmctl(shmid, IPC_RMID, NULL); // Rimuove la memoria condivisa.
      msgctl(msgid, IPC_RMID, NULL); // Rimuove la coda di messaggi.
      killpg(getpgid(getpid()), SIGTERM); // Termina tutti i processi nel gruppo di processo.
    break;

    case SIGUSR1:
      if (shmem_ptr->inib_on == 1) {
        kill(pid_inibitore, SIGQUIT); // Termina il processo inibitore se è attivo.
        TEST_ERROR;
      } else if(shmem_ptr->inib_on == 0){
        shmem_ptr->inib_on = 1; // Attiva l'inibitore.
        
        sem.sem_num = INIBSEM; // Seleziona il semaforo dell'inibitore.
        sem.sem_op = 1; // Imposta l'operazione del semaforo a 1.
        semop(semid, &sem, 1); // Esegue l'operazione sul semaforo.

        write(0, "Inibitore ON. Turn off anytime with ctrl + backslash \n", 55); // Scrive un messaggio per indicare che l'inibitore è attivo.
      }
    break;

    case SIGINT:
      shmem_ptr->termination = 1; // Imposta la variabile di terminazione a 1.
      shmem_ptr->message = "user."; // Imposta il messaggio di terminazione a "user".
    break;

    case SIGQUIT: // non fa nulla perche deve arrivare all'inibitore
    break;

    case SIGSEGV:
      shmem_ptr->termination = 1; // Imposta la variabile di terminazione a 1.
      shmem_ptr->message = "segmentation fault."; // Imposta il messaggio di terminazione a "segmentation fault".
    break;
  }
}

int main(int argc, char* argv[]) {

  int atomic_num; // Numero atomico casuale.
  pid_t *pid_atoms; // Puntatore all'array di PID degli atomi.
  key_t shmkey, semkey, msgkey; // Chiavi per la memoria condivisa, i semafori e la coda di messaggi.
  char n_atom[8], id_shmat[8], pointer_shmem[8], id_sem[8], id_message[8]; // Buffer per convertire gli identificatori in stringa.
  
  srand(getpid()); // Inizializza il generatore di numeri casuali con il PID del processo.

  shmkey = ftok("master.c", 'A'); // Genera una chiave per la memoria condivisa.
  semkey = ftok("master.c", 'B'); // Genera una chiave per i semafori.
  msgkey = ftok("master.c", 'C'); // Genera una chiave per la coda di messaggi.

  shmid = shmget(shmkey, SHM_SIZE, IPC_CREAT | 0666); // Crea o ottiene l'ID della memoria condivisa.
  TEST_ERROR;

  semid = semget(semkey, 6, IPC_CREAT | 0666); // Crea o ottiene l'ID dell'array di semafori.
  TEST_ERROR;

  msgid = msgget(msgkey, IPC_CREAT | 0666); // Crea o ottiene l'ID della coda di messaggi.
  TEST_ERROR;

  shmem_ptr = (data_buffer *)shmat(shmid, NULL, 0); // fa l'attachment della memoria condivisa
  TEST_ERROR;

  shmem_ptr->pid_master = getpid(); // Salva il PID del processo master nella memoria condivisa.
  shmem_ptr->termination = 0; // Inizializza la variabile di terminazione a 0.

  set_sem_values(); // Imposta i valori iniziali dei semafori.
  sem.sem_flg = 0; // Imposta il flag dei semafori a 0.

  sprintf(id_shmat, "%d", shmid); // Converte l'ID della memoria condivisa in stringa e lo salva su id_shmat --> necessario per passarlo ai vari figli
  sprintf(id_sem, "%d", semid); // Converte l'ID dei semafori in stringa e lo salva in id_sem
  sprintf(id_message, "%d", msgid); // Converte l'ID della coda di messaggi in stringa e lo salva in id_message

  // setti dei vettori per passarli ai vari processi
  char *vec_alim[] = {"./alimentazione", id_shmat, id_sem, id_message, NULL}; // Argomenti per il processo alimentazione.
  char *vec_attiv[] = {"./attivatore", id_sem, id_shmat, id_message, NULL}; // Argomenti per il processo attivatore.
  char *vec_inib[] = {"./inibitore", id_sem, id_shmat, id_message, NULL}; // Argomenti per il processo inibitore.

  switch(pid_alimentazione = fork()) { // Crea un processo figlio per alimentazione.
    
    case -1: // la fork fallisce, quindi fai terminare tutto
      sem.sem_num = MELTDOWNSEM; // Seleziona il semaforo di fusione.
      sem.sem_op = -1; // Imposta l'operazione del semaforo a -1.
      semop(semid, &sem, 1);

      raise(SIGUSR2); // Genera il segnale SIGUSR2 per terminare.
    break;

    case 0:
      execve("./alimentazione", vec_alim, NULL); // Esegue il programma alimentazione nel processo figlio.
      TEST_ERROR;
    break;

    default: // Processo padre
      switch(pid_attivatore = fork()) { // Crea un processo figlio per attivatore.

        case -1:
          sem.sem_num = MELTDOWNSEM; // Seleziona il semaforo di fusione.
          sem.sem_op = -1; // devi decrementare il semaforo di 1
          semop(semid, &sem, 1); 
          
          raise(SIGUSR2); // Genera il segnale SIGUSR2 per terminare.
        break;

        case 0:
          execve("./attivatore", vec_attiv, NULL); // Esegue il programma attivatore nel processo figlio.
          TEST_ERROR;
        break;
        
        default:
          switch(pid_inibitore = fork()) { // Crea un processo figlio per inibitore.
            case -1:
              sem.sem_num = MELTDOWNSEM; // Seleziona il semaforo di fusione.
              sem.sem_op = -1; // Imposta l'operazione del semaforo a -1.
              semop(semid, &sem, 1);

              raise(SIGUSR2); // Genera il segnale SIGUSR2 per terminare.
            break;
            
            case 0:
              execve("./inibitore", vec_inib, NULL); // Esegue il programma inibitore nel processo figlio.
              TEST_ERROR;
            break;
          }
      }
    break;
  } 
  
  shmem_ptr->simulation_start = 1; // Imposta il flag di inizio simulazione.
  pid_atoms = malloc(sizeof(pid_t) * N_ATOM_INIT); // Alloca memoria per l'array di PID degli atomi.

  for(int i = 0; i < N_ATOM_INIT; i++) { // Crea i processi atomo iniziali.

    pid_atoms[i] = fork(); // Crea un processo figlio.

    srand(getpid()); // Inizializza il generatore di numeri casuali con il PID del processo.
    atomic_num = rand() % N_ATOM_MAX + 1; // Genera un numero atomico casuale.
    sprintf(n_atom, "%d", atomic_num); // Converte il numero atomico in stringa.
    char *vec_atomo[] = {"./atomo", n_atom, id_shmat, id_sem, id_message, NULL}; // Argomenti per il processo atomo.

    switch(pid_atoms[i]) {

      case -1:
        sem.sem_num = MELTDOWNSEM; // Seleziona il semaforo di fusione.
        sem.sem_op = -1; // Imposta l'operazione del semaforo a -1.
        semop(semid, &sem, 1);

        raise(SIGUSR2); // Genera il segnale SIGUSR2 per terminare.
      break;

      case 0: 
        free(pid_atoms); // il figlio non ha bisogno dell'array e si sprecherebbe spazio
        execve("./atomo", vec_atomo, NULL); // Esegue il programma atomo nel processo figlio.
        TEST_ERROR;
      break;

      default:
      break;
    }
  }

  fflush(stdout); // Svuota il buffer di output standard.
  char risposta;
  do {
    printf("Do you want to turn inibitore on? (y for yes, n for no)\n"); // Chiede all'utente se vuole attivare l'inibitore.
    scanf("%s", &risposta); // Legge la risposta dell'utente.
  } while(tolower(risposta) != 'y' && tolower(risposta) != 'n'); // Continua a chiedere fino a quando la risposta non è 'y' o 'n'.

  struct sigaction sa; // Struttura per gestire i segnali.

  bzero(&sa, sizeof(sa)); // Inizializza la struttura a zero.
  sa.sa_handler = &signal_handler; // Imposta il gestore dei segnali.

  // Sigaction modifica il comportamento dei segnali, le devi riscrivere nel signal handler
  sigaction(SIGALRM, &sa, NULL); 
  sigaction(SIGINT, &sa, NULL); 
  sigaction(SIGUSR1, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL); 
  sigaction(SIGUSR2, &sa, NULL); 
  sigaction(SIGSEGV, &sa, NULL);

  free(pid_atoms); // Libera la memoria allocata per l'array di PID degli atomi.
  
  sem.sem_num = STARTSEM; 
  sem.sem_op = N_ATOM_INIT + 3; // permette a 1000 figli + gli altri di fare tutto
  semop(semid, &sem, 1); 

  alarm(SIM_DURATION); // Imposta un allarme per la durata della simulazione.

  if (tolower(risposta) == 'y') {
    raise(SIGUSR1); // Genera il segnale SIGUSR1 se l'utente ha risposto 'y'.
  }

  while(shmem_ptr->termination != 1) { // Loop principale della simulazione.
    sleep(1); // Attende un secondo.

    shmem_ptr->cons_en_rel = ENERGY_DEMAND; // Imposta il consumo di energia relativo.
    
    stat_total_value(); // Aggiorna i valori totali delle statistiche.

    if (shmem_ptr->prod_en_tot >= ENERGY_EXPLODE_THRESHOLD) {
      shmem_ptr->message = "explode.";
      raise(SIGTSTP); // è il segnale di stop da terminale
    }

    print_stats(); // Stampa le statistiche.
    bzero(shmem_ptr, 7 * sizeof(int)); // Inizializza a zero i valori relativi nella memoria condivisa.

    if (ENERGY_DEMAND > shmem_ptr->prod_en_tot) {
      shmem_ptr->message = "blackout."; 
      raise(SIGTSTP); 
    } else {
      shmem_ptr->prod_en_tot = shmem_ptr->prod_en_tot - ENERGY_DEMAND; // Aggiorna l'energia totale prodotta.
    }
  }

  printf("Simulation terminated due to %s\n", shmem_ptr->message); // Stampa il messaggio di terminazione.
  semctl(semid, 0, IPC_RMID); // Rimuove l'array di semafori.
  shmdt(shmem_ptr); // Disconnette la memoria condivisa.
  shmctl(shmid, IPC_RMID, NULL); // Rimuove la memoria condivisa.
  msgctl(msgid, IPC_RMID, NULL); // Rimuove la coda di messaggi.
  killpg(getpgid(getpid()), SIGTERM); // Termina tutti i processi nel gruppo di processo.
}

void set_sem_values(){
  
  semctl(semid, STARTSEM, SETVAL, 0); // Imposta il valore iniziale del semaforo di inizio a 0.
  TEST_ERROR;

  semctl(semid, WASTESEM, SETVAL, 1); // Imposta il valore iniziale del semaforo di scarto a 1.
  TEST_ERROR;

  semctl(semid, PROD_ENERGYSEM, SETVAL, 1); // Imposta il valore iniziale del semaforo di produzione energia a 1.
  TEST_ERROR;

  semctl(semid, DIVISIONSEM, SETVAL, 1); // Imposta il valore iniziale del semaforo di divisione a 1.
  TEST_ERROR;

  semctl(semid, INIBSEM, SETVAL, 0); // Imposta il valore iniziale del semaforo dell'inibitore a 0.
  TEST_ERROR;

  semctl(semid, MELTDOWNSEM, SETVAL, 1); // Imposta il valore iniziale del semaforo di fusione a 1.
  TEST_ERROR;

}

void print_stats() { // Funzione per stampare le statistiche.
  static int count = 1; // Contatore per il timer della simulazione.

  int col1_width = 35; // Larghezza della prima colonna.
  int col2_width = 15; // Larghezza della seconda colonna.
  printf("\n\n\n\n");
  printf("STATS:\n");
  for (int i = 0; i <= (col1_width + col2_width); i++) {
    printf("-"); // Stampa una linea di separazione.
  }
  printf("\n");

  char *col1[15] = {"Last second activations:","Total activations:","Last second divisions:","Total divisions:",
    "Last second produced energy:","Total produced energy:","Last second consumed energy:","Total consumed energy:",
    "Last second waste:","Total waste:","Last second absorbed energy:","Total absorbed energy:",
    "Last second undivided atoms:","Total undivided atoms:"}; // Array di stringhe per i nomi delle statistiche.
  int col2[15] = {shmem_ptr->act_rel, shmem_ptr->act_tot, shmem_ptr->div_rel, shmem_ptr->div_tot, shmem_ptr->prod_en_rel,
    shmem_ptr->prod_en_tot, shmem_ptr->cons_en_rel, shmem_ptr->cons_en_tot, shmem_ptr->waste_rel, shmem_ptr->waste_tot,
    shmem_ptr->absorbed_en_rel, shmem_ptr->absorbed_en_tot, shmem_ptr->undiv_rel, shmem_ptr->undiv_tot}; // Array di interi per i valori delle statistiche.

  for (long unsigned int i = 0; i < (sizeof(col1)/sizeof(col1[0]))-1; i++) { // Stampa le statistiche.
    printf("%-*s | %-*d\n", col1_width, col1[i], col2_width, col2[i]);
    if (i%2!=0) {
      for (int i = 0; i <= (col1_width + col2_width); i++) {
        printf("-"); // Stampa una linea di separazione.
      }
      printf("\n");
    }
  }

  printf("Simulation timer: %d\n", count++); // Stampa il timer della simulazione.
}

void stat_total_value() { // Funzione per aggiornare i valori totali delle statistiche.
  shmem_ptr->waste_tot = shmem_ptr->waste_tot + shmem_ptr->waste_rel;
  shmem_ptr->act_tot = shmem_ptr->act_tot + shmem_ptr->act_rel;
  shmem_ptr->div_tot = shmem_ptr->div_tot + shmem_ptr->div_rel;
  shmem_ptr->prod_en_tot = shmem_ptr->prod_en_tot + shmem_ptr->prod_en_rel;
  shmem_ptr->cons_en_tot = shmem_ptr->cons_en_tot + shmem_ptr->cons_en_rel;
  shmem_ptr->absorbed_en_tot = shmem_ptr->absorbed_en_tot + shmem_ptr->absorbed_en_rel;
  shmem_ptr->undiv_tot = shmem_ptr->undiv_tot + shmem_ptr->undiv_rel;
}
