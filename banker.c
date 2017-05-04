#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <pthread.h>
#define TRUE 1 
#define FALSE 0 
#define MAX 80 //buffer for reads

int bankers(); 					//tabular algorithm to check need <= available
void printResources(); 			
void init( char * ); 			//loads configuration from file (string) in arg1
void updateNeed( int );
void updateAvailable(); 
void *requestResource( int );		//request a resource in a normal way
int checkCompletion( int ); 
int is_deadlock();
int resources, processes; 
int *Available; 
int **Allocation; 
int **Max; 
int **Need; 
int *Finish; 
int *is_request_pending;
int random_request_generation  = TRUE;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) {

	if( argc < 2 ) { 
		printf("usage: %s config_file random_request_generation\n", argv[0]); 
	 		return 1; 
	}

// set up variables using config file 
	init(argv[1]);  

// initialize running to true for all processes  
	int i;
	for(i=0; i<processes; i++){ 
		Finish[i] = FALSE;
		is_request_pending[i] = FALSE;
	}

	if (argc == 3) random_request_generation = atoi(argv[2]);

	printf("Initialized...\n");
	printResources();

	if(is_deadlock()){
		printf("DEADLOCK!!!\n");
		return 0;
	}

    //counts how many processes have yet to finish
	int count;			
 	do {  
 		count = 0; 
 		pthread_t threads[processes];
 		int i;
		for(i=0; i<processes; i++) {
			if( Finish[i] == FALSE ) {
				count++;
				//requests a resource at random if process has no pending requests
				if(!is_request_pending[i]){
					int return_code;
					//printf("Creating request thread for process %d.\n", i);
					return_code = pthread_create(&threads[i], NULL, requestResource, (void *)i);
					is_request_pending[i] = TRUE;
					if (return_code) {
						printf("ERROR; return code from pthread_create() is %d\n", return_code);
						exit(-1);
					}	
				}
			} 
		}
 	} while( count != 0 ); 
 
 	//all of the processes are finished 
	printf("\n = Success! =\n"); 
 	printf("All processes finished without deadlock.\n");  
 	return 0; 
}

//Creates a random request and requests it 
void *requestResource( int process ) {
	int Requests[resources];
	// generate a random request vector
	int it;
	for(it=0; it<resources; it++) {
		if (random_request_generation){
			Requests[it] = 0;
			if (Need[process][it]) Requests[it] = rand() % Need[process][it] + 1;
		}else{
			printf("Units of Resource %d Process %d requests", it, process);
			scanf("%d", &Requests[it]);
		}

		printf("Process %d is requesting %d unit(s) from R%d.\n", process, Requests[it], it);
	}
	
	if (bankers(process, Requests)){
		printf(" -GRANTED process %d allocated resources\n", process);
		// check if the process completed with this resource request round
		if( checkCompletion( process ) == TRUE ) { 
			printf("Process %d has completed!\n",process);  
		}
	}else{
		printf(" -DENIED process %d NOT allocated resources\n", process);
	}

	is_request_pending[process] = FALSE;
	pthread_exit(NULL);
} 

int bankers(int process, int *Requests) { 
	int i, j;
	//check if the resource is even available first
	pthread_mutex_lock(&mutex);
	for( i=0; i<resources; i++){
		if (Available[i] < Requests[i]){ 
			printf(" -Unavailable resources-");
			pthread_mutex_unlock(&mutex);
			return FALSE;
		}	
	}
	//otherwise, make a provisional allocation and check that state	
	for( i=0; i<resources; i++){
		Available[i] -= Requests[i]; 
		Allocation[process][i] += Requests[i]; 
		Need[process][i] -= Requests[i];	
	} 

	// set up temp Available array 
	int *temp_avail; 
	if( !(temp_avail = malloc( resources * sizeof( int ) ))) 
		return -1; 
 	
 	for( i=0; i<resources; i++ ) { 
 		temp_avail[i] = Available[i]; 
 	} 
	
	// set up temp Finish array 
	int *temp_finish; 
	if( !(temp_finish = malloc( processes * sizeof( int ) ))) 
		return -1; 

 	for( i=0; i<processes; i++ ) { 
 		temp_finish[i] = Finish[i]; 
 	}
	// Apply safety algorithm to whether all the process can finish after above allocation 
	for( i=0; i<processes; i++ ) {  
		if( temp_finish[i] == FALSE ) { 
			int process_i_can_finish = TRUE;
			for( j=0; j<resources; j++ ) { 
				if( Need[i][j] > temp_avail[j] ) {  
					process_i_can_finish = FALSE;
					break;
 				} 
 			} 
			if (process_i_can_finish){
				temp_finish[i] = TRUE;
 				for( j=0; j < resources; j++ ) { 
					temp_avail[j] += Allocation[i][j]; 
				}
				i = -1;  
			}
		} 
	} 
	
	//if one could not finish, then UNSAFE
	for( i=0; i<processes; i++ ) { 
 		if (temp_finish[i] == FALSE) {
			printf(" -Unsafe state-");
			//undo provisional allocation
			for( i=0; i<resources; i++){
				Available[i] += Requests[i]; 
				Allocation[process][i] -= Requests[i]; 
				Need[process][i] += Requests[i];	
			}
			pthread_mutex_unlock(&mutex); 
			return FALSE; //UNSAFE
		}
 	}
 	pthread_mutex_unlock(&mutex);
	return TRUE;//SAFE
}

//Checks if a given process has completed.
int checkCompletion(int process){
	int i;
	for( i=0; i<resources; i++) { 
		if( Allocation[process][i] < Max[process][i]){
			return FALSE; 
		}
	}	
	pthread_mutex_lock(&mutex); 
	for(i=0; i<resources; i++ ) {
		Available[i] += Allocation[process][i];	  
	}
	pthread_mutex_unlock(&mutex);
	// process is complete! 
	Finish[process] = TRUE;  
	return TRUE;  
} 

int is_deadlock(){
	int i,j;
	int *temp_avail; 
	if( !(temp_avail = malloc( resources * sizeof( int ) ))) 
		return -1; 
 	
 	for( i=0; i<resources; i++ ) { 
 		temp_avail[i] = Available[i]; 
 	} 
	
	// set up temp Finish array 
	int *temp_finish; 
	if( !(temp_finish = malloc( processes * sizeof( int ) ))) 
		return -1; 

 	for( i=0; i<processes; i++ ) { 
 		temp_finish[i] = Finish[i]; 
 	}
	// Apply safety algorithm to whether all the process can finish after above allocation 
	for( i=0; i<processes; i++ ) {  
		if( temp_finish[i] == FALSE ) { 
			int process_i_can_finish = TRUE;
			for( j=0; j<resources; j++ ) { 
				if( Need[i][j] > temp_avail[j] ) {  
					process_i_can_finish = FALSE;
					break;
 				} 
 			} 
			if (process_i_can_finish){
				temp_finish[i] = TRUE;
 				for( j=0; j < resources; j++ ) { 
					temp_avail[j] += Allocation[i][j]; 
				}
				i = -1;  
			}
		} 
	} 
	
	//if one could not finish, then Deadlock
	for( i=0; i<processes; i++ ) { 
 		if (temp_finish[i] == FALSE) {
			printf(" -Unsafe state-"); 
			return TRUE; //Deadlock
		}
 	}
	return FALSE;//SAFE
}

//Print current system status to the screen.  
void printResources(){
	int i, j;

	printf("\n  === Current system resources ===\n"); 
 
	// print status of Available 
 	printf("  === Available ===\n");  
 	for( i=0; i<resources; i++ )  
		printf("\tR%d", i); 

	printf("\n"); 
 
	for( i=0; i<resources; i++ )  
		printf("\t%d", Available[i]); 

	// print status of Allocation 
	printf("\n  === Allocation ===\n");  
	for( i=0; i<resources; i++ )  
		printf("\tR%d",i); 

 	printf("\n"); 

	for( i=0; i<processes; i++) {  
		if( Finish[i] == FALSE ) { 
			printf("P%d\t", i);  
			for( j=0; j<resources; j++)  
				printf("%d\t", Allocation[i][j]);  
 			printf("\n");  
		}  
 	} 
 
	// print status of Max 
	printf("  === Max ===\n");  
	for( i=0; i<resources; i++ )  
		printf("\tR%d", i); 

	printf("\n"); 

	for( i=0; i<processes; i++) {  
		if( Finish[i] == FALSE ) {  
			printf("P%d\t", i);  
			for( j=0; j<resources; j++)  
				printf("%d\t", Max[i][j]);  
			printf("\n");  
		}  
	} 
 
	// print status of Need 
 	printf("  === Need ===\n"); 
 	for( i=0; i<resources; i++ )  
 		printf("\tR%d", i); 

 	printf("\n"); 
 
	for( i=0; i<processes; i++) {  
		if( Finish[i] == FALSE ) {  
 			printf("P%d\t", i);  
 			for( j=0; j<resources; j++)  
 				printf("%d\t", Need[i][j]);  
 			printf("\n");  
 		}  
 	} 
} 

//Reads a configuration file and sets the global variables as needed.  
void init( char *filename) { 
	int i, j;
	// open the config file 
	FILE *file; 
	if( !(file = fopen(filename,"r"))) { 
		printf("error: can't open file %s\n",filename); 
			return; 
	} 
 
	char buffer[MAX]; // the line read from the file 
	char *tmp;        // each value on a line as a string 
 
	// parse the first line: number of resource types 
	fgets(buffer, MAX, file); 
	resources = atoi(buffer); 

 	// set up the Available array 
	if( !(Available = malloc( resources * sizeof( int ) ))) 
		return; 
 
 	// parse second line: number of instances of each resource type 
 	fgets(buffer, MAX, file); 
 	i=0; 
 
 	tmp = strtok(buffer," "); 
 	while( tmp != NULL) { 
 		Available[i] = atoi(tmp); 
 		tmp = strtok(NULL, " "); 
 		i++; 
 	} 
	// parse third line: number of processes 
	fgets(buffer, MAX, file); 
	processes = atoi(buffer); 
  
	// set up the Max array 
	if( !(Max = malloc( processes * sizeof( int* ) ))) 
		return; 
	for( i=0; i<processes; i++ ) 
		if( !(Max[i] = malloc( resources * sizeof( int ) ))) 
	  	return; 
 	// parse next n(no fof processes) lines: max requests by each process 
	i=0; 
	for (i=0; i<processes; i++) {
		fgets(buffer, MAX, file);
		j=0; 
		tmp = strtok(buffer," "); 
		while( tmp != NULL) { 
			Max[i][j] = atoi(tmp); 
			tmp = strtok(NULL, " "); 
			j++; 
		} 
	}

	// set up Allocation array 
	if( !(Allocation = malloc( processes * sizeof( int* ) ))) 
		return; 
	for( i=0; i<processes; i++ ) 
	 	if(!(Allocation[i] = malloc( resources * sizeof( int ) ))) 
	  	return; 
	// parse rest of lines: allocation for each process 
	i=0; 
	while (fgets(buffer, MAX, file) != NULL) {
		j=0; 
		tmp = strtok(buffer," "); 
		while( tmp != NULL) { 
			Allocation[i][j] = atoi(tmp); 
			tmp = strtok(NULL, " "); 
			j++; 
		}
		i++;
	}

	updateAvailable();

	// set up Need array 
	if( !(Need = malloc( processes * sizeof( int* ) ))) 
		return; 
	for( i=0; i<processes; i++ ) 
		if(!(Need[i] = malloc( resources * sizeof( int ) ))) 
		  return; 
	 
	for( i=0; i<processes; i++ ) { 
		updateNeed(i);
	} 
	// set up Finish array 
	if( !(Finish = malloc( processes * sizeof( int ) ))) 
		return;

	if( !(is_request_pending = malloc( processes * sizeof( int ) ))) 
		return;

	fclose(file); 
 
}

void updateNeed(int process){ 
	int i = 0;
	for( i=0; i<resources; i++ ){
		pthread_mutex_lock(&mutex);
		Need[process][i] = Max[process][i] - Allocation[process][i];
		pthread_mutex_unlock(&mutex);
	} 

} 

void updateAvailable(){ 
	int i = 0;
	int j = 0;
	for( i=0; i<resources; i++ ){
		pthread_mutex_lock(&mutex);
		for (j=0; j<processes; j++)
			Available[i] -= Allocation[j][i];
		pthread_mutex_unlock(&mutex);
	} 

}

