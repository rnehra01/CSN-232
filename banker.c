#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <pthread.h>
#define TRUE 1 
#define FALSE 0 
#define MAX 80 //buffer for reads
#define MAX_RANDOM_WAIT 2 //in seconds
#define RAND_REQUEST 20 // 20% chance to not request, 80% to request a resource
#define DEBUG FALSE

int bankers(); 					//tabular algorithm to check need <= available
void printResources(); 			
int init( char * ); 			//loads configuration from file (string) in arg1
void updateNeed( int ); 
void *requestResource( int );		//request a resource in a normal way
int checkCompletion( int ); 
int resources, processes; 
int *Available; 
int **Allocation; 
int **Max; 
int **Need; 
int *Finish; 

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;;

int *is_running;

int main(int argc, char *argv[]) {

	//pthread_mutex_init(&mutex, NULL);
	if( argc < 2 ) { 
		printf("usage: %s config_file\n", argv[0]); 
	 		return 1; 
	} 

// set up variables using config file 
	init(argv[1]);  

// initialize running to true for all processes  
	int i;
	for(i=0; i<processes; i++){ 
		Finish[i] = FALSE; //if not finished, then it is running
		is_running[i] = FALSE;
	}

	printf("Initialized...\n");
	printResources();	

// start the main loop 
	int count;			//counts how many processes have yet to finish
 	do {  
 		count = 0; 		//initialize every loop
					// check the number of running processes 
 		pthread_t threads[processes];
 		int i;
		for(i=0; i<processes; i++) {
			if( Finish[i] == FALSE ) {
				count++;
				//requests a resource at random
				//this could certainly be multithreaded starting here
				//using createThread in the pthreads library
				//requestResource(i); 
				if (!is_running[i]){
					int return_code;
					printf("Creating thread for process %d.\n", i);
					//int *proc = &i;
					return_code = pthread_create(&threads[i], NULL, requestResource, (void *)i);
					is_running[i] = TRUE;
					if (return_code) {
						printf("ERROR; return code from pthread_create() is %d\n", return_code);
						exit(-1);
					}	
				}
				//if threaded starting here, you may wait for all 
				//threads to return here before continuing
			} 
		}
		if (DEBUG){
			printf("Loop complete, Running:%d processes of %d\n",count, processes);  
			//printResources();
		}
 	} while( count != 0 ); 
 
 	// out of the while loop, so all of the processes are finished 
	printf("\n = Success! =\n"); 
 	printf("All processes finished without deadlock.\n");  
 	return 0; 
} 
 
/** 
 * Creates a random request and requests it 
 * 
 * @param process number 
*/
void *requestResource( int process ) { 
	// sleep for a random period 
	//sleep(rand() % MAX_RANDOM_WAIT);

	int Requests[resources];
	// generate a random request vector
	int it;
	for(it=0; it<resources; it++) {
		Requests[it] = 0;
		if (Need[process][it]) Requests[it] = rand() % Need[process][it] + 1;
		// this will always be less than what we need, so we dont have to check
		printf("Process %d is requesting %d unit(s) from R%d.\n",process,Requests[it],it);
	}
	
	if (bankers(process, Requests)){
		printf(" -GRANTED process %d allocated resources\n", process);

		// check if the process completed with this resource request round
		if( checkCompletion( process ) == TRUE ) { 
			printf("Process %d has completed!\n",process); 
			//simulate return of resource
			//sleep(rand() % MAX_RANDOM_WAIT); 
		}
	}else{
		printf(" -DENIED process %d allocated resources\n", process);
	}

	is_running[process] = FALSE;
	pthread_exit(NULL);
} 

int bankers(int process, int *Requests) { 
	// loop variables
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
	pthread_mutex_unlock(&mutex); 

	// set up temp Available array 
	int *temp_avail; 
	if( !(temp_avail = malloc( resources * sizeof( int ) ))) 
		return -1; 
 	// copy the Available array into the temporary array 
 	for( i=0; i<resources; i++ ) { 
 		temp_avail[i] = Available[i]; 
 	} 
	// set up temp Finish array 
	int *temp_finish; 
	if( !(temp_finish = malloc( processes * sizeof( int ) ))) 
		return -1; 

 	// copy the Available array into the temporary array 
 	for( i=0; i<processes; i++ ) { 
 		temp_finish[i] = Finish[i]; 
 	}
	// Find an index i such that... (the thread is not finished, but available is greater than need) 
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
				// this process can finish from the provisional state
				temp_finish[i] = TRUE;
				if (DEBUG){
					// this adds to the safe sequence so far
					printf("P%d->",i);
				}
				// with new available resources
 				for( j=0; j < resources; j++ ) { 
					temp_avail[j] += Allocation[i][j]; 
				}
				// so have to recheck all
				i = -1;  
			} 
			// One iteration - the resources are returned
			// Next iterations
			//	-look again through all processes for one that can finish
			// 	-continue until all can finish

		} 
	} 
	if (DEBUG){
		printf("\n");
	}
	//if one could not finish, then UNSAFE
	for( i=0; i<processes; i++ ) { 
 		if (temp_finish[i] == FALSE) {
			printf(" -Unsafe state-");
			//undo provisional allocation
			pthread_mutex_lock(&mutex);
			for( i=0; i<resources; i++){
				Available[i] += Requests[i]; 
				Allocation[process][i] -= Requests[i]; 
				Need[process][i] += Requests[i];	
			}
			pthread_mutex_unlock(&mutex); 
			return FALSE; //UNSAFE
		}
 	}
	
	return TRUE;//SAFE; 
} 


/**
 * Checks if a given process has completed. 
 * 
 * @param process number 
 * @return true if the process is complete 
 */
int checkCompletion(int process) {
	int i, j;
	for( i=0; i<resources; i++) { 
		if( !(Allocation[process][i] >= Max[process][i])){
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


/** 
  * Print current system status to the screen. 
  */ 
void printResources() { 
	//loop variables
	int i, j;

	printf("\n    == Current system resources =="); 
 
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

 
/** 
 * Reads a configuration file and sets the global variables as needed. 
 * 
 * @param configuration file filename 
 * @return status 
 */ 
int init( char *filename ) { 
	int i, j;
	// open the config file 
	FILE *file; 
	if( !(file = fopen(filename,"r"))) { 
		printf("error: can't open file %s\n",filename); 
			return 9; 
	} 
 
	char buffer[MAX]; // the line read from the file 
	char *tmp;        // each value on a line as a string 
 
	// parse the first line: number of resource types 
	fgets(buffer, MAX, file); 
	resources = atoi(buffer); 

 	// now we can set up the Available array 
	if( !(Available = malloc( resources * sizeof( int ) ))) 
		return 6; 
 
 	// parse second line: number of instances of each resource type 
 	fgets(buffer, MAX, file); 
 	i=0; 
 	// we'll use a tokenizer to split the string on spaces 
 	tmp = strtok(buffer," "); 
 	while( tmp != NULL) { 
 		Available[i] = atoi(tmp); 
 		tmp = strtok(NULL, " "); 
 		i++; 
 	} 
	// parse third line: number of processes 
	fgets(buffer, MAX, file); 
	processes = atoi(buffer); 
 	// now that we have both processes and resources 
	// we can set up the Max array (2D) 
	if( !(Max = malloc( processes * sizeof( int* ) ))) 
		return 3; 
	for( i=0; i<processes; i++ ) 
		if( !(Max[i] = malloc( resources * sizeof( int ) ))) 
	  	return 4; 
 	// parse rest of lines: max requests by each process 
	i=0; 
	while(fgets(buffer, MAX, file) != NULL) { 
		j=0; 
		// we'll use a tokenizer to split the string on spaces 
		tmp = strtok(buffer," "); 
		while( tmp != NULL) { 
			Max[i][j] = atoi(tmp); 
			tmp = strtok(NULL, " "); 
			j++; 
		} 
		i++; 
	} 
	// now we can finish creating the other arrays 

	// set up Allocation array (2D) 
	if( !(Allocation = malloc( processes * sizeof( int* ) ))) 
		return 1; 
	for( i=0; i<processes; i++ ) 
	 	if(!(Allocation[i] = malloc( resources * sizeof( int ) ))) 
	  	return 2; 

	// set up Need array (2D) 
	if( !(Need = malloc( processes * sizeof( int* ) ))) 
		return 7; 
	for( i=0; i<processes; i++ ) 
		if(!(Need[i] = malloc( resources * sizeof( int ) ))) 
		  return 8; 
	// fill in Need array for each process 
	for( i=0; i<processes; i++ ) { 
		updateNeed(i);
	} 
	// set up Finish array 
	if( !(Finish = malloc( processes * sizeof( int ) ))) 
		return 5;

	if( !(is_running = malloc( processes * sizeof( int ) ))) 
		return 5;

	fclose(file); 

	return 0; 
} 
void updateNeed(int process) { 
	int i = 0;
	for( i=0; i<resources; i++ ){
		pthread_mutex_lock(&mutex);
		Need[process][i] = Max[process][i] - Allocation[process][i];
		pthread_mutex_unlock(&mutex);
	} 

} 


