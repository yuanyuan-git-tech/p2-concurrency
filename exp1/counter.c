// No spinlock and CAS code. 
// search for "todo" for hints on where to add code

#include <stdio.h>
#include <getopt.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define ONE_BILLION 1000000000L;

long long the_counter = 0;
int iterations = 1;
int numThreads = 1;

int mutexFlag = 0;
int spinLockFlag = 0;
int c_and_sFlag = 0;

pthread_mutex_t mutex;
// todo: define a spinlock variable 
int spinLock = 0;

void spinlock_init(int* lock) {
    __atomic_store_n(lock, 0, __ATOMIC_SEQ_CST);
}
void spinlock_lock(int* lock) {
    int expected = 0;
    while (!__atomic_compare_exchange_n(lock, &expected, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        expected = 0;
    }
}
void spinlock_unlock(int* lock) {
    __atomic_store_n(lock, 0, __ATOMIC_SEQ_CST);
}

char const * getTestName() {
    if(mutexFlag)
    	return "add-m";
    if(spinLockFlag)
    	return "add-s";
    if(c_and_sFlag)
    	return "add-c";
    return "add-none";
}

void cleanUpLocks(){
    if(mutexFlag)
        pthread_mutex_destroy(&mutex);
}

void print_csv_line(char const * test, int threadNum, int iterations, int numOperation, long long runTime, long long avgTime, long long count){
	fprintf(stdout, "test=%s threadNum=%d iterations=%d numOperation=%d runTime(ns)=%lld avgTime(ns)=%lld count=%lld\n",
			test, threadNum, iterations, numOperation, runTime, avgTime, count);
}

/* ---- thread function --- */
void add (long long *pointer, long long value) {
    long long sum = *pointer + value;
    *pointer = sum;
}

void add_iterate(int val, int iterations) {
    for (int i = 0; i < iterations; i++) {
        if(mutexFlag){
            pthread_mutex_lock(&mutex);
            add(&the_counter, val);
            pthread_mutex_unlock(&mutex);
        }
        else if(spinLockFlag) {
            // todo: lock the spinlock
            /*
            __atomic_test_and_set(&spinLock, __ATOMIC_SEQ_CST) atomically sets spinlock to a non-zero value and 
            returns its previous value. 
            If the returned value is 0, it means the lock was acquired successfully. 
            If the lock is already held (spinlock is non-zero), 
            the thread enters a busy-wait state, continuously checking until the lock becomes available.
            */
            // while (__atomic_test_and_set(&spinLock, __ATOMIC_SEQ_CST)) {
            // }
            spinlock_lock(&spinLock);
            add(&the_counter, val);
            // todo: unlock the spinlock
            /*
            __atomic_clear(&spinlock, __ATOMIC_SEQ_CST) is used to release the lock by setting spinlock back to 0, 
            making it available for other threads to acquire.
            */
            //__atomic_clear(&spinLock, __ATOMIC_SEQ_CST);
            spinlock_unlock(&spinLock);
        }
        else if(c_and_sFlag) {
            long long oldVal, newVal;
			
	        /* todo: change the following, so that it updates @the_counter atomically using CAS */
            
            /*
            __atomic_compare_exchange_n: This function will atomically compare the counter with the expected old value
            and, if they match, swap it with the new value. 
            If not, it will update the old value with the current value of the counter. 
            */
           do {
            oldVal = the_counter;
            newVal = oldVal + val;
           } while (!__atomic_compare_exchange_n(&the_counter, &oldVal, newVal, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
        }
        else
            add(&the_counter, val);
    }
}

void thread_func(int *iterations) {
		add_iterate(1, *iterations);
		add_iterate(-1, *iterations);
}

void usage() {
	fprintf(stderr, "./counter --iterations iterations "
			"--threads numThreads=1 [--sync m/c/s] \n");
}

int main(int argc, char** argv) {
    int opt = 0;
    char input;
    struct timespec start_time, end_time;

    static struct option options [] = {
        {"iterations", 1, 0, 'i'},
        {"threads", 1, 0, 't'},
        {"sync", 1, 0, 's'},
        {0, 0, 0, 0}
    };
    // Check the size of long long type of integer
    // printf("Size of long long: %zu bytes\n", sizeof(long long));
    /* parse cmd args */
    while((opt=getopt_long(argc, argv, "its", options, NULL)) != -1){
        switch(opt){
            case 'i':
                iterations = (int)atoi(optarg);
                break;
            case 't':
                numThreads = (int)atoi(optarg);
                break;
            case 's':
                if(strlen(optarg) != 1) {
                    fprintf(stderr, "Invalid option. Sync type must be one letter\n");
                    exit(1);
                }
                input = *optarg;
                switch(input) {
                    case 'm':
                        if(pthread_mutex_init(&mutex, NULL) < 0)
                            perror("mutex");
                        mutexFlag = 1;
                        break;
                    case 's':
                        spinLockFlag = 1;
                        spinlock_init(&spinLock);
                        break;
                    case 'c':
                        c_and_sFlag = 1;
                        break;
                    default:
                        usage();
                        exit(1);
                        break;
                }
                break;
            default:
                usage();
                exit(1);
                break;
        }
    }

  /* launch threads & exec test */

	pthread_t threads[numThreads];

	if (clock_gettime(CLOCK_MONOTONIC, &start_time) < 0) {
		perror("clock_gettime:");
		exit(1);
	}

	for (int i = 0; i < numThreads; i++) {
		if (pthread_create(&threads[i], NULL,
				(void*) &thread_func, &iterations) < 0) {
					perror("thread_create:");
					exit(1);
		}
	}

	for (int i = 0; i < numThreads; i++) {
		if (pthread_join(threads[i], NULL) < 0) {
				perror("thread_join");
				exit(1);
		}
	}

	if(clock_gettime(CLOCK_MONOTONIC, &end_time) < 0)
			perror("clock_gettime");

	/* output measurement */
	long long diff =  (end_time.tv_sec - start_time.tv_sec) * ONE_BILLION;
	diff += end_time.tv_nsec;
	diff -= start_time.tv_nsec;

	int numOpts = 2 * iterations * numThreads;

	print_csv_line(getTestName(), numThreads, iterations, numOpts, diff,
			diff/numOpts, the_counter);
	atexit(cleanUpLocks);
	exit(0);
}
