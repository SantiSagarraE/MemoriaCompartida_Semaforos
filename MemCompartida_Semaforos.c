/*
Este programa distingue dos procesos tipo A o B. Mediante
el uso de semaforos y de una memoria compartida, el proceso A 
almacena numeros random pares en la memoria compartida que luego
lee el proceso B y el proceso B almacena numeros impares en la memoria 
compartida que luego lee el proceso A.
El programa finaliza enviando la señal SIGTERM a cualquiera de los
dos procesos. 
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <signal.h>

#define ENTRAR(OP) ((OP).sem_op = -1)
#define SALIR(OP) ((OP).sem_op = +1)

int salir=0;

void trapper1(int sig); 

// Esta union, es necesaria para inicializar los semaforos usando semctl()
union semun {
	int              val;    /* Value for SETVAL */
	struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
	unsigned short  *array;  /* Array for GETALL, SETALL */
	struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux-specific) */
};


int main(){
	key_t ClaveMem,ClaveSem;
	int Id_Memoria,Id_Semaforo;
	int *Memoria = NULL;
	int pidb, pida,i=0,j=0, flagA=0, flagB=0;
	struct sembuf Operacion;
	union semun arg;
	char nombre[10];

	signal(SIGTERM, trapper1);

/*					MEMORIA COMPARTIDA 					*/
	ClaveMem = ftok ("/bin/ls", 33);
	if (ClaveMem == -1){
		printf("No consigo clave para memoria compartida\n");
		exit(0);
	}
	
	Id_Memoria = shmget (ClaveMem, sizeof(int)*12, 0660 | IPC_CREAT);
	if (Id_Memoria == -1){
		printf("No consigo Id para memoria compartida\n");
		exit (0);
	}
	
	Memoria = (int *)shmat (Id_Memoria, (char *)0, 0);
	if (Memoria == NULL){
		printf("No consigo memoria compartida\n");
		exit (0);
	}
	
	/*					SEMAFOROS 					*/

	ClaveSem = ftok ("/bin/ls", 33);
	if (ClaveSem == (key_t)-1){
		printf("No puedo conseguir clave de semáforo\n");
		exit(0);
	}       

	Id_Semaforo = semget (ClaveSem, 2, 0660 | IPC_CREAT);	// Array de semaforo
	if (Id_Semaforo == -1){
		printf("No puedo crear semáforo\n");
		exit (0);
	}

	Operacion.sem_num = 0;								
	Operacion.sem_flg = 0;								

/*					PROCESOS					*/
	printf("Identifique el proceso a utilizar \n"); 	//Identifico que proceso deseo activar
	scanf("%[^\n]" ,nombre);

	if(strcmp(nombre, "proceso a") == 0){
		printf("Soy el proceso A\n");
		Memoria[10]=getpid();
		flagA=1;										// Activo el flag del proceso A
		if (Memoria[11]==0){							// Pongo en rojo el semaforo que escribe
			arg.val =0;									// la seccion de la memoria del proceso B
			semctl (Id_Semaforo, 1, SETVAL, arg);
		}
		arg.val =1;										// Pongo en verde el semaforo que escribe
		semctl (Id_Semaforo, 0, SETVAL, arg);			// la seccion de la memoria del proceso A

	}else if(strcmp(nombre, "proceso b") == 0){			// Hago lo mismo si el proceso es B
		printf("Soy el proceso B\n");
		Memoria[11]=getpid();
		flagB=1;
		if (Memoria[10]==0){
			arg.val =0;
			semctl (Id_Semaforo, 0, SETVAL, arg);
		}
		arg.val =1;
		semctl (Id_Semaforo, 1, SETVAL, arg);
	}else{											
		printf("No se indentifico el proceso\n");
		return -1;
	}
	
	while(1){														
		if(flagA){										// Caso proceso A inicializado
			Operacion.sem_num = 0;						// Selecciono el semaforo del proceso A		
			ENTRAR(Operacion);						
			semop (Id_Semaforo, &Operacion, 1);	
				for (j=0; j<=4; j++)					// Escribo los 5 numeros pares y luego
				{										// salgo del semaforo
					Memoria[j] = (rand()%500)*2;
					printf(".");
					i++;
					sleep(1);
				}
			SALIR(Operacion);
			semop (Id_Semaforo, &Operacion, 1);
			printf("\n");

			Operacion.sem_num = 1;						// Selecciono el semaforo del proceso B					
			ENTRAR(Operacion);							
			semop (Id_Semaforo, &Operacion, 1);			
				for (j=5; j<=9; j++){					// Leo los numeros escritos por el proceso B
					printf("Soy el proceso A, PID=%d :Memoria[%d] = %d \n", getpid(), j, Memoria[j]);
				}
			SALIR(Operacion);
			semop (Id_Semaforo, &Operacion, 1);
		}
		if(flagB){
			Operacion.sem_num = 1;						// Ahora en caso de que sea proceso B el inicializado			
			ENTRAR(Operacion);						
			semop (Id_Semaforo, &Operacion, 1);	
				for (j=5; j<=9; j++)
				{	Memoria[j] = (rand()%500)*2+1;
					printf(".");
					i++;
					sleep(1);
				}
			SALIR(Operacion);
			semop (Id_Semaforo, &Operacion, 1);
			printf("\n");
			Operacion.sem_num = 0;											
			ENTRAR(Operacion);						
			semop (Id_Semaforo, &Operacion, 1);
				for (j=0; j<=4; j++){	
					printf("Soy el proceso B, PID= %d :Memoria[%d] = %d \n", getpid(), j, Memoria[j]);
				}
			SALIR(Operacion);
			semop (Id_Semaforo, &Operacion, 1);
		}
	
	
		if(salir==1 && flagA){										// Finaliza con la señal SIGTERM
			printf("Soy el proceso A y procedo a matar al proceso B en caso de que este activo\n");	
			kill(Memoria[11],SIGTERM);
			printf("Soy el proceso A y voy a morir\n");
			semctl(Id_Semaforo, 0, IPC_RMID);
			shmdt ((char *)Memoria);
			shmctl (Id_Memoria, IPC_RMID, (struct shmid_ds *)NULL);
			return 0;
		}
		if(salir==1 && flagB){										
			printf("Soy el proceso B y procedo a matar al proceso A en caso de que este activo\n");
			kill(Memoria[10],SIGTERM);
			printf("Soy el proceso B y voy a morir\n");
			semctl(Id_Semaforo, 0, IPC_RMID);
			shmdt ((char *)Memoria);
			shmctl (Id_Memoria, IPC_RMID, (struct shmid_ds *)NULL);
			return 0;
		}
	}
}

void trapper1(int sig){
	salir=1;	 		
}
