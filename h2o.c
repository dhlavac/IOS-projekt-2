/*************************************************************
** IOS - projekt c. 2
** Autor: Dominik Hlaváč 
** Datum vytvorenia: apríl 2015
**
** Popis programu: Progrsm demonstruje synchronizaciu procesov
** pri probleme Building H20. S vyuzitim semaforov a sdielanej
** pamati a zabranujuc synchronizacnim problemom ako deadlock 
** ci vyhladovenie.
**
** pouzitej ceruzky a papieru :vela   prebdenych noci: 1
** prepadnutie zoufalstvu: 5x
***************************************************************/


#include <sys/types.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define EXIT_OK 0
#define EXIT_ARG 1
#define EXIT_SYST 2
#define EXIT_SEM 3
#define EXIT_SHM 4

/*-----------------------------------premenne------------------------------*/
int *process_id = NULL,
    *count = NULL,
    *H_id = NULL,
    *n_done = NULL,
    *O_id = NULL;

typedef struct param {
	int N;
	int GH;
	int GO;
	int B;
} Tparam;

//zdielana pamat
int shm_H_id = 0,
   pid_mainproc = 0,
   shm_O_id = 0,
   shm_c_id = 0,
   shm_d_id = 0,
   shm_p_id = 0;
//semafory
sem_t
    *hydrogens,
    *mutex,
    *bar_mutex,
    *bar_mutex2,
    *writing,
    *barrier1,
    *barrier2,
    *barrier3,
    *oxygens;

FILE *file;

/*------------------- Prototypi funkcii ------------------------*/

int nacitanie_zdrojov () ;
void uvolnenie_zdrojov(void);
void ukonci() ;
int pars_param(int argc, char *argv[], Tparam *params);
void oxygen_cr(int i,Tparam params);
void hydrogen_cr(int j,Tparam params);




/*------------------- Parsrovanie argumentov ------------------------*/

int pars_param(int argc, char *argv[], Tparam *params) 
{
	int err = EXIT_OK;	
	char *chyba = NULL;
	if (argc!=5){
 		fprintf(stderr, "Bol zadany zly pocet parametrov\n");
    exit (1);
 		return EXIT_ARG;
 	}
 	else {
		params->N = strtol(argv[1], &chyba, 10);
		params->GH = strtol(argv[2], &chyba, 10);
		params->GO = strtol(argv[3], &chyba, 10);
		params->B = strtol(argv[4], &chyba, 10);
		//osetrenie chyboveho vstupu
		if(*chyba != 0 || !(params->N > 0 ) || !(params->GH >= 0 && params->GH < 5001) ||
     !(params->GO >= 0 && params->GO < 5001) || !(params->B >= 0 && params->B < 5001)){
			err = EXIT_ARG;
			fprintf(stderr, "Bol zadany chybny parameter.\n");
      exit (1);
		}
	}
	return err;
}

/*-----------------------------------oxygen------------------------------*/
void oxygen_cr(int i,Tparam params)
{
  int pom =i,
      s_time=(random() % (params.B + 1));
  pom++;
  //sem started
  sem_wait(writing);
    fprintf(file,"%d\t: O %d\t: started\n",*process_id,pom);
    fflush(file);
    (*process_id)++;
  sem_post(writing);
  
  //sem ready alebo waiting
  sem_wait(mutex);
  
  (*O_id)++;
  if(*H_id >=2 ){
    sem_wait(writing);
      fprintf(file,"%d\t: O %d\t: ready\n",*process_id,pom);
      fflush(file);
      (*process_id)++;
    sem_post(writing);
    sem_post(hydrogens);
    sem_post(hydrogens);
    (*H_id)-=2;
    sem_post(oxygens);
    (*O_id)--;
  }
  else{
    sem_wait(writing);
      fprintf(file,"%d\t: O %d\t: waiting\n",*process_id,pom);
      fflush(file);
      (*process_id)++;
    sem_post(writing);
    sem_post(mutex);
  }
  sem_wait(oxygens);

  sem_wait(writing);
    fprintf(file,"%d\t: O %d\t: begin bonding\n",*process_id,pom);
    fflush(file);
    (*process_id)++;
  sem_post(writing);

  sem_wait(bar_mutex);
  (*count)++;
  if (*count == 3){
    sem_wait(barrier2);
    sem_post(barrier1);
  }
  //barrier
  sem_post(bar_mutex);

  sem_wait(barrier1);
  sem_post(barrier1);

  sem_wait(writing);
    usleep(s_time * 1000);
    fprintf(file,"%d\t: O %d\t: bonded\n",*process_id,pom);
    fflush(file);
    (*process_id)++;
  sem_post(writing);
  
  sem_wait(bar_mutex);
  (*count)--;
  if (*count == 0){
    sem_wait(barrier1);
    sem_post(barrier2);
  }

  sem_post(bar_mutex);

  sem_wait(barrier2);
  sem_post(barrier2);
  //odomknutie mutexu pri procese O
  sem_post(mutex);

  //barrier 2, pre finished 
  sem_wait(bar_mutex2);
  (*n_done)++;
  sem_post(bar_mutex2);

  if (*n_done== params.N*3){
    sem_post(barrier3);
  } 

  sem_wait(barrier3);
  sem_post(barrier3);

  sem_wait(writing);
    fprintf(file,"%d\t: O %d\t: finished\n",*process_id,pom);
    fflush(file);
    (*process_id)++;
  sem_post(writing);
 
}



/*-----------------------------------hydrogen------------------------------*/
void hydrogen_cr(int j,Tparam params)
{
  int pom =j,
      s_time=(random() % (params.B + 1));
  pom++;
  //sem started
  sem_wait(writing);
    fprintf(file,"%d\t: H %d\t: started\n",*process_id,pom);
    fflush(file);
    (*process_id)++;
  sem_post(writing);
  
  //sem redy alebo waiting
  sem_wait(mutex);
  (*H_id)++;
  if(*H_id >=2 && *O_id >=1){
    sem_wait(writing);
      fprintf(file,"%d\t: H %d\t: ready\n",*process_id,pom);
      fflush(file);
      (*process_id)++;
    sem_post(writing);
    sem_post(hydrogens);
    sem_post(hydrogens);
    (*H_id)-=2;
    sem_post(oxygens);
    (*O_id)--;
  }
  else{
    sem_wait(writing);
      fprintf(file,"%d\t: H %d\t: waiting\n",*process_id,pom);
      fflush(file);
      (*process_id)++;
    sem_post(writing);
    sem_post(mutex);
  }
  sem_wait(hydrogens);

  sem_wait(writing);
  fprintf(file,"%d\t: H %d\t: begin bonding\n",*process_id,pom);
  fflush(file); 
  (*process_id)++;
  sem_post(writing);

  // barrier
  sem_wait(bar_mutex);
  (*count)++;
  if (*count == 3){
    sem_wait(barrier2);
    sem_post(barrier1);
  }
  sem_post(bar_mutex);

  sem_wait(barrier1);
  sem_post(barrier1);

  sem_wait(writing);
    usleep(s_time * 1000);
    fprintf(file,"%d\t: H %d\t: bonded\n",*process_id,pom);
   fflush(file);
    (*process_id)++;
  sem_post(writing);

  sem_wait(bar_mutex);
  (*count)--;
  if (*count == 0){
    sem_wait(barrier1);
    sem_post(barrier2);
  }

  sem_post(bar_mutex);

  sem_wait(barrier2);
  sem_post(barrier2);

  //barrier 2, pre finished 
  sem_wait(bar_mutex2);
  (*n_done)++;
  sem_post(bar_mutex2);

  if (*n_done== params.N*3){
    sem_post(barrier3);
  } 

  sem_wait(barrier3);
  sem_post(barrier3);

  sem_wait(writing);
    fprintf(file,"%d\t: H %d\t: finished\n",*process_id,pom);
    fflush(file);
    (*process_id)++;
  sem_post(writing);
}

/*-----------------------------------nacitanie zdrojov ------------------*/


int nacitanie_zdrojov () 
{
  int err = EXIT_OK;
  if((hydrogens = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED ||
     (oxygens= mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED ||
     (mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED ||
     (writing = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED ||
     (barrier1 = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED ||
     (barrier2 = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED ||
     (barrier3 = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED ||
     (bar_mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED ||
     (bar_mutex2 = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED ||
     sem_init(hydrogens, 1, 0) == -1 ||
     sem_init(oxygens, 1, 0) == -1 ||
     sem_init(writing, 1, 1) == -1 ||
     sem_init(barrier1, 1, 0) == -1 ||
     sem_init(barrier2, 1, 1) == -1 ||
     sem_init(barrier3, 1, 0) == -1 ||
     sem_init(bar_mutex, 1,1) == -1 ||
     sem_init(bar_mutex2, 1,1) == -1 ||
     sem_init(mutex, 1,1) == -1 ){
    err = EXIT_SEM;
    fprintf(stderr, "Nastala chyba pri vytvarani semaforov\n");
    exit (EXIT_SYST);
  } else if((shm_H_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | 0666)) == -1 ||
        (shm_O_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | 0666)) == -1 ||
        (shm_c_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | 0666)) == -1 ||
        (shm_d_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | 0666)) == -1 ||
        (shm_p_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | 0666)) == -1 ||
        (process_id = (int *) shmat(shm_p_id, NULL, 0)) == NULL ||
        (H_id = (int *) shmat(shm_H_id, NULL, 0)) == NULL ||
        (O_id = (int *) shmat(shm_O_id, NULL, 0)) == NULL ||
        (count = (int *) shmat(shm_c_id, NULL, 0)) == NULL ||
        (n_done = (int *) shmat(shm_d_id, NULL, 0)) == NULL ){
    err = EXIT_SHM;
    fprintf(stderr, "Nastala chyba pri alokacii zdielanej pamati\n");
    exit (EXIT_SYST);
  }
 if(err != EXIT_OK) {
  uvolnenie_zdrojov();
    err= EXIT_SYST;
 }
 return err;
}
/*-----------------------------------ukonci------------------------------*/

void ukonci() 
{
  uvolnenie_zdrojov();
  kill(pid_mainproc, SIGTERM);
  kill(getpid(), SIGTERM);
  exit (EXIT_SYST);
}


/*-----------------------------------uvolneniezdrojov ------------------*/


void uvolnenie_zdrojov(void)
{
  fclose(file);
  int err = EXIT_OK;
  // uvolnenie semaforov
  if(sem_destroy(hydrogens) == -1 || 
     sem_destroy(mutex) == -1 || 
     sem_destroy(writing) == -1 || 
     sem_destroy(barrier1) == -1 ||
     sem_destroy(barrier2) == -1 ||
     sem_destroy(barrier3) == -1 ||
     sem_destroy(bar_mutex) == -1 ||
     sem_destroy(bar_mutex2) == -1 ||
     sem_destroy(oxygens) == -1) {
   err = EXIT_SEM; 
  }
  //uvolnenie zddielanej pamate
  if(shmctl(shm_H_id, IPC_RMID, NULL) == -1 ||
     shmctl(shm_O_id, IPC_RMID, NULL) == -1 ||
     shmctl(shm_p_id, IPC_RMID, NULL) == -1 ||
     shmctl(shm_c_id, IPC_RMID, NULL) == -1 ||
     shmctl(shm_d_id, IPC_RMID, NULL) == -1 ||
     shmdt(count) == -1 ||
     shmdt(n_done) == -1 ||
     shmdt(process_id) == -1 ||
     shmdt(H_id) == -1 ||
     shmdt(O_id) == -1 ){
    err = EXIT_SHM;
  }
 // kontrola chyb pri uvolnovani pamate
 if(err == EXIT_SEM) {
    fprintf(stderr, "Nepodarilo sa zmamzat semafory.\n");
    exit (EXIT_SYST);
  }

 if(err == EXIT_SHM) {
    fprintf(stderr, "Nepodarilo sa zmazat alokovanu pamat.\n");
    exit (EXIT_SYST);
  }
}



/*-----------------------------------main------------------------------*/

int main (int argc, char *argv[])
{
 Tparam params;
 pars_param(argc,argv,&params);

 
 int i = 0,
     j = 0,
     gen_time = 0;
 pid_t pid_oxygen,pid_mainproc,pid_hydrogen; 
 pid_t potomkovia_O[params.N];
 pid_t potomkovia_H[params.N*2];

 signal(SIGTERM, ukonci);
 signal(SIGINT, ukonci);

 
 // kontrola vystupneho suboru
 if((file = fopen("h2o.out", "w")) == NULL) {
  fprintf(stderr, "Chyba pri praci s vystupnym suborom\n");
  exit(2);
  return EXIT_SYST;
 }
 // random generovanie id procesov
 srand(time(NULL) * getpid());

 // pre spravny vystup do suboru 
 setbuf(file, NULL);

 nacitanie_zdrojov() ;
 //nastavenie pocitadiel 
  *process_id = 1,
  *count = 0,
  *H_id = 0,
  *O_id = 0;

// vytvorenie procesov
 pid_mainproc = fork();
//forkovia
  if(pid_mainproc == 0) {
    for(i = 0; i < params.N; i++) {
      gen_time = (random() % (params.GO + 1));
      usleep(gen_time * 1000);
      pid_oxygen = fork();
      if(pid_oxygen == 0) {
        oxygen_cr(i,params);
        exit(0);
      } else if(pid_oxygen > 0) {
        potomkovia_O[i] = pid_oxygen;
      } else {
        fprintf(stderr, " Chyba pri tvoreni procesu Oxygen\n");
        ukonci();
      }
    }
    // cakanie na procesy potomkov
    for(i = 0; i < params.N; i++) {
      waitpid(potomkovia_O[i], NULL, 0);
    }
  } else if(pid_mainproc > 0) {
    for(j = 0; j < params.N*2; j++) {
      gen_time = (random() % (params.GH + 1));
      usleep(gen_time * 1000);
      pid_hydrogen = fork();
      if(pid_hydrogen == 0) {
        hydrogen_cr(j,params);
        exit(0);
      } else if(pid_hydrogen > 0) {
        potomkovia_H[j] = pid_hydrogen;
      } else {
        fprintf(stderr, "Chyba pri vytvarani procesu hydrogen\n");
        ukonci();
      }
    }
    // cakanie na procesy potomkov
    for(i = 0; i < params.N*2; i++) {
      waitpid(potomkovia_H[j], NULL, 0);
    }
  } else {
    fprintf(stderr,"Chyba pri vytvarani potomkov\n");
    ukonci();
  }

  uvolnenie_zdrojov();

  return 0;
}