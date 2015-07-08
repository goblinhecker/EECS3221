/*
==================================================================================================================
This C program creates two groups of Pthreads, an IN group and an OUT group, to create an exact copy of a source 
file passed as a command-line argument. It also creates a LOG file to keep track of how the program was run.
==================================================================================================================
*/

#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <semaphore.h>

// #define for easy read
#define FOR(i,n) for(i = 0 ; i < (n) ; i++)
#define TEN_MILLIS_IN_NANOS 10000000

//global variables
int buff = 0; //buffer size
FILE *input; //input file
FILE *output; //output file 
FILE *activity_log; //log file
int incount, outcount; //number of in threads, out threads
int in_thread_count; //variable for out_thread_func
int i; //to traverse over buffer
int *can_in_join, *can_out_join; //to join threads in main()

//mutexes for file
pthread_mutex_t global_lock; //lock for mutual exclusion to thread 
pthread_mutex_t incr_tid; //increment in thread in lock
pthread_mutex_t incr_tid_out; //increment out thread in lock
pthread_mutex_t decr_tid; //decrement running in threads
pthread_mutex_t thread_finished; //check if thread finished (assign it 1)

//modified BufferItem for file
typedef struct{
	char string[7]; //to ascribe the state of buffer cell
					//could have also used flag = 1 or 0
					//but "string" clearer
	char data; //hold byte
	off_t offset; //hold offset of file
} AwareBufferItem;

//declare buffer
AwareBufferItem *buffer;

//functions
int pos_empty_cell();
int pos_full_cell();
void go_to_sleep();
void *in_thread_func(void *param); 
void *out_thread_func(void *param);
void close_release();

//main execution
int main(int argc, char *argv[]){
	
	//exit if argc < 7
	if (argc > 7 || argc < 7) {
		fprintf(stderr, "\tProvide Valid Arguments!\n\tExiting Now!\n");
		exit(1);
	}
	
	//initiate mutex locks
	pthread_mutex_init(&decr_tid, NULL);	
	pthread_mutex_init(&incr_tid, NULL);
	pthread_mutex_init(&global_lock, NULL);
	pthread_mutex_init(&incr_tid_out, NULL);
	pthread_mutex_init(&thread_finished, NULL);
	
	//assign global and local variables to appropriate system arguments
	int travel = 0;
	buff = atoi(argv[5]);
	char *log_file = argv[6];
	char *filename_in = argv[3];
	char *filename_out = argv[4];
	int in_count = atoi(argv[1]);
	int out_count = atoi(argv[2]);
	in_thread_count = in_count;
	
	//allocate memory
	can_in_join = malloc(in_count * sizeof(int));
	can_out_join = malloc(out_count * sizeof(int));
	
	/** "Exit" under these cases... */
	if (can_in_join == NULL || can_out_join == NULL) {
		fprintf(stderr, "\tCan't Assign Memory to Threads!\n\tExiting Now!\n");
		exit(1);	
	}
	
	if (in_count < 1 || out_count < 1 || buff < 1)  {
		fprintf(stderr, "\tInvalid Values!\n\tExiting Now!\n");
		exit(1);	
	} 
	
	activity_log = fopen(log_file, "w");
	if (activity_log == NULL) {
		fprintf(stderr, "\tInvalid Log File!\n\tExiting Now!\n");	
		exit(1);
	}
	
	input = fopen(filename_in, "r");
	if (input == NULL) {
		fprintf(stderr, "\tInvalid Input File!\n\tExiting Now!\n");	
		exit(1);
	}
	
	output = fopen(filename_out, "w");
	if (output == NULL) {
		fprintf(stderr, "\tInvalid Output File!\n\tExiting Now!\n");	
		exit(1);
	}

	//assign number of threads
	pthread_t in_thread[in_count];
	pthread_t out_thread[out_count];
	
	printf("\tCreating Buffer...\n\t");
	
	//assign memory and initialize to "empty"
	buffer = (AwareBufferItem*) malloc(buff * sizeof(AwareBufferItem));
	FOR(travel, buff)
		strcpy(buffer[travel].string, "empty");
	
	printf("Creating in threads...\n\t");
	
	/** for loops to create, wait for, and join "in" and "out" threads */
	FOR(travel, in_count)
		pthread_create(&in_thread[travel], NULL, (void *) in_thread_func, input);

	printf("Creating out threads...\n\t");
	
	FOR(travel, out_count)
		pthread_create(&out_thread[travel], NULL, (void *) out_thread_func, output);	

	printf("Waiting for in threads...\n\t");
	
	/**The next four "FOR" loops are for "Extra Credit". It makes sure
	 * main() doesn't proceed if any of the threads are active.
	 */
	FOR(travel, in_count)
		while(can_in_join[travel] == 0);	
	
	printf("Waiting for out threads...\n\t");
	
	FOR(travel, out_count)
		while(can_out_join[travel] == 0);
		
	FOR(travel, in_count)
		pthread_join(in_thread[travel], NULL);
	
	FOR(travel, out_count)
		pthread_join(out_thread[travel], NULL);
	
	printf("Finished Successfully.\n");

	//relase memory
	close_release();

	exit(0);
}

//return index of empty cell
int pos_empty_cell(){
    FOR(i, buff) {
		if(strcmp(buffer[i].string, "empty") == 0)
			return i;
	}
	return -1;
}

//return index of full cell
int pos_full_cell(){
	FOR(i, buff) {
		if (strcmp(buffer[i].string, "filled") == 0){
			return i;
		}
	}
	return -1;
}

//let the thread go_to_sleep
void go_to_sleep() {
	struct timespec t;
	t.tv_sec = 0;
	t.tv_nsec = rand()%(TEN_MILLIS_IN_NANOS+1);	
	nanosleep(&t, NULL);
}

/* This IN thread takes in the input file and reads the byte at a specific offset.
 * Then it adds the item from file to the IndexedBuffer buffer. Mutex lock called
 * `global_lock' used for mutual exclusion to a specific thread. 
 */
void *in_thread_func(void *param){
	
	go_to_sleep();
	
	//local variables
	int pos;
	char c;
	int address;
	int local_incount;
	
	//increase thread count
	pthread_mutex_lock(&incr_tid);
	local_incount = incount++;
	pthread_mutex_unlock(&incr_tid);
	
	do {
		//give hold of lock to this thread
		pthread_mutex_lock(&global_lock);
		pos = pos_empty_cell();			

		//produce in any "empty" buffer
		while (pos != -1){
			
			//read from file
			address = ftell(input);
			c = fgetc(input);
			
			//write to log file
			if (fprintf(activity_log, "read_byte PT%d O%d B%d I-1\n", local_incount, address, c) < 0) {
				fprintf(stderr, "\tProvide Valid Log File!\n\tExiting Now!\n");
				exit(1);
			}
			
			if (c == EOF){
				break;
			}

			//produce from file to buffer
			else{ 
				buffer[pos].data = c;
				buffer[pos].offset = address;
				strcpy(buffer[pos].string, "filled");
			}
			
			//write to log file
			if (fprintf(activity_log, "produce PT%d O%d B%d I%d\n", local_incount, address, c, pos) < 0) {
				fprintf(stderr, "\tProvide Valid Log File!\n\tExiting Now!\n");
				exit(1);
			}
			pos = pos_empty_cell();
			
			go_to_sleep();
		}
		
		//release `global_lock'
		pthread_mutex_unlock(&global_lock);

	} while (!feof(input));
	
	//decrement thread count
	pthread_mutex_lock(&decr_tid);
	in_thread_count--;
	pthread_mutex_unlock(&decr_tid);
	
	//let main() know thread has finished
	pthread_mutex_lock(&thread_finished);
	can_in_join[local_incount] = 1;
	pthread_mutex_unlock(&thread_finished);
	
	go_to_sleep();	
	
	pthread_exit(0);
}

/* This OUT thread takes the item from buffer and writes to the output file. 
 * Mutual exclusion is provided with the mutex lock called `global_lock'. 
 */
void *out_thread_func(void *param){
	
	go_to_sleep();	
	
	//local variables
	int in_threads_running;
	int out_count;
	int pos = 0;
	char c;
	int address;
	int val;
	
	//increment thread count
	pthread_mutex_lock(&incr_tid_out);
	out_count = outcount++;
	pthread_mutex_unlock(&incr_tid_out);

	do {
		// critical section to read in the first item to be outputted
		pos = pos_full_cell();

		if (pos != -1){
			
			//obtain lock for consuming
			pthread_mutex_lock(&global_lock);
			pos = pos_full_cell(); 
			
			//consume buffer 
			address = buffer[pos].offset;
			c = buffer[pos].data;
			
			//write to log file
			if (fprintf(activity_log, "consume CT%d O%d B%d I%d\n", out_count, address, c, pos) < 0) {
				fprintf(stderr, "\tProvide Valid Log File!\n\tExiting Now!\n");
				exit(1);
			}
			
			// store empty character to buffer and indicate that it is empty
			strcpy(buffer[pos].string, "empty");
			buffer[pos].offset = 0;
			
			//release lock
			pthread_mutex_unlock(&global_lock);

			//obtain lock for writing
			pthread_mutex_lock(&global_lock);
			
			// critical section for writing to file 
			val = fseek(output, address, SEEK_SET);
			if (val < 0) {
				pthread_mutex_unlock(&global_lock);
    			fprintf(stderr, "\tProvide Valid Seek!\n\tExiting Now!\n");
				exit(1);
			}
			if (fputc(c, output) == EOF) {
				pthread_mutex_unlock(&global_lock);
    			fprintf(stderr, "\tInvalid File Write!\n\tExiting Now!\n");
				exit(1);
			}
			
			//write to log file
			if (fprintf(activity_log, "write_byte CT%d O%d B%d I-1\n", out_count, address, c) < 0) {
				fprintf(stderr, "\tProvide Valid Log File!\n\tExiting Now!\n");
				exit(1);
			}
			
			//release lock
			pthread_mutex_unlock(&global_lock);
		}
		go_to_sleep();
		
		//assign thread count for exiting
		in_threads_running = in_thread_count;
	
	} while (in_threads_running > 0 || pos != -1);
	
	//let main() know thread has finished
	pthread_mutex_lock(&thread_finished);
	can_out_join[out_count] = 1;
	pthread_mutex_unlock(&thread_finished);
	
	pthread_exit(0);
}

/* This thread closes all the opened files and releases buffer.
 */
void close_release() {
	fclose(input);	
	fclose(activity_log);
	fclose(output);
	free(buffer);	
	free(can_in_join);
	free(can_out_join);
}