#define MAX(a, b) ((a > b) ? (a) : (b))
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <ctype.h>

struct sembuf sem;

typedef struct message {
    long mtype;
    char mymessage[10];
} message_buffer;

typedef struct data_buffer {
  int waste_rel;
  int prod_en_rel;
  int div_rel;
  int act_rel;
  int cons_en_rel;
  int absorbed_en_rel;
  int undiv_rel;

  int waste_tot;
  int prod_en_tot;
  int div_tot;
  int act_tot;
  int cons_en_tot;
  int absorbed_en_tot;
  int undiv_tot;
  
  char * message;
  int simulation_start;
  int inib_on;
  int termination;
  pid_t pid_master;
} data_buffer;

#define TEST_ERROR   if (errno) {fprintf(stderr, \
					   "%s:%d: PID=%5d: Error %d (%s)\n",\
					   __FILE__,\
					   __LINE__,\
					   getpid(),\
					   errno,\
					   strerror(errno));}

int send_atom_pid(int msgid, int pid) {
  message_buffer message;
  message.mtype = PID_TYPE;
  sprintf(message.mymessage, "%d", pid);
  return msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
}

int receive_pid(int msgid) {
  message_buffer message;
  if(msgrcv(msgid, &message, (sizeof(message) - sizeof(long)), 0, 0) == -1) {
    return -1;
  }
  return atoi(message.mymessage);
}
