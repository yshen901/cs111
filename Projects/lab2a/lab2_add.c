#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>

pthread_mutex_t m_lock;
volatile int s_lock = 0;

int num_iters;
int y_flag;
char sync_opt;
long long counter;

/* THREAD ARGUMENTS */
/*struct args {
  int num_iters;
  long long* pointer;
  int y_flag;
  char sync_opt;
  };*/

/* HELPER FUNCTIONS */
void printCSV(int num_threads, long num_ops,
	      long long total_time, long long avg_optime
	      ) {

  char name [50] = "add";
  if (y_flag == 1)
    strcat(name, "-yield");

  if (sync_opt == 'm')
    strcat(name, "-m");
  else if (sync_opt == 'c')
    strcat(name, "-c");
  else if (sync_opt == 's')
    strcat(name, "-s");
  else
    strcat(name, "-none");

  printf("%s,%d,%d,%ld,%lld,%lld,%lld\n",
	 name, num_threads, num_iters, num_ops, total_time, avg_optime, counter);
}

/* THREAD FUNCTIONS */
void add_none(long long *pointer, long long value) {
  long long sum = *pointer + value;
  if (y_flag)
    sched_yield();
  *pointer = sum;
}

void add_mutex(long long *pointer, long long value) {
  pthread_mutex_lock(&m_lock);
  long long sum = *pointer + value;
  if (y_flag)
    sched_yield();
  *pointer = sum;
  pthread_mutex_unlock(&m_lock);
}

void add_spin(long long *pointer, long long value) {
  while(__sync_lock_test_and_set(&s_lock, 1));
  long long sum = *pointer + value;
  if (y_flag)
    sched_yield();
  *pointer = sum;
  __sync_lock_release(&s_lock);
}

void add_cas(long long *pointer, long long value) {
  long long oldval;
  do {
    oldval = counter;
    if (y_flag)
      sched_yield();
  } while (__sync_val_compare_and_swap(pointer, oldval, oldval + value) != oldval);
}

void* thread_func(void *args) {
  /*int num_iters = ((struct args*)args)->num_iters;
  long long * pointer = ((struct args*)args)->pointer;
  int y_flag = ((struct args*)args)->y_flag;
  char sync_opt = ((struct args*)args)->sync_opt;*/

  switch(sync_opt) {
  case 'm':
    for (int i = 0; i < num_iters; i++) {
      add_mutex(&counter, 1);
      add_mutex(&counter, -1);
    }
    break;
  case 's':
    for (int i = 0; i < num_iters; i++) {
      add_spin(&counter, 1);
      add_spin(&counter, -1);
    }
    break;
  case 'c':
    for (int i = 0; i < num_iters; i++) {
      add_cas(&counter, 1);
      add_cas(&counter, -1);
    }
    break;
  default:
    for (int i = 0; i < num_iters; i++) {
      add_none(&counter, 1);
      add_none(&counter, -1);
    }
  }
  return args;
}

int main(int argc, char ** argv) {
  char c;
  int option_index = 0;
  
  int t_flag, i_flag;
  sync_opt = 'n';
  
  int num_threads = 1;
  num_iters = 1;
  pthread_t *tid;
  counter = 0;
  
  struct timespec start_time, end_time;
  
  static struct option long_options[] = {
    {"threads", required_argument, 0, 't'},
    {"iterations", required_argument, 0, 'i'},
    {"sync", required_argument, 0, 's'},
    {"yield", no_argument, 0, 'y'},
    {0,0,0,0}
  };

  while((c = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
    switch(c) {
    case 't':
      t_flag = 1;
      num_threads = atoi(optarg);
      break;
    case 'i':
      i_flag = 1;
      num_iters = atoi(optarg);
      break;
    case 'y':
      y_flag = 1;
      break;
    case 's':
      sync_opt = optarg[0]; // mutex, spin, or CAS locks
      break;
    case '?':
    default:
      fprintf(stderr, "ERROR processing arguments, please use --threads={num_threads} or --iterations={num_iterations}\n");
      exit(1);
    }
  }

  // Checks for missing required arguments
  if (t_flag != 1 || i_flag != 1) {
    fprintf(stderr, "ERROR missing arguments, --threads and --iterations are mandatory arguments\n");
    exit(1);
  }

  // Checks for invalid synchronization arguments
  if (sync_opt != 'n' && sync_opt != 'c' && sync_opt != 'm' && sync_opt != 's') {
    fprintf(stderr, "ERROR invalid synchronization options, please use s, m, or c");
    exit(1);
  }

  // Set aside memory for thread ids
  tid = malloc(num_threads * sizeof(pthread_t));
  if (tid == NULL) {
    fprintf(stderr, "ERROR memory allocation failed (tid): %s\n", strerror(errno));
    exit(1);
  }

  // NOTE: Using this for some reason greatly increases runtime...using global variables is faster
  // Structure for holding thread function arguments
  /*struct args *targv = (struct args *)malloc(sizeof(struct args));
  if (targv == NULL) {
    fprintf(stderr, "ERROR memory allocation failed (targv): %s\n", strerror(errno));
    exit(1);
  }
  targv->num_iters = num_iters;
  targv->pointer = &counter;
  targv->y_flag = y_flag;
  targv->sync_opt = sync_opt;*/
  
  // Setup synchronization options
  if (sync_opt == 'm') {
    if (pthread_mutex_init(&m_lock, NULL) != 0) {
      printf("ERROR failed to initialize mutex\n");
      exit(1);
    }
  }

  // Get start-time
  if(clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
    fprintf(stderr, "ERROR gettime failed: %s\n", strerror(errno));
    exit(1);
  }

  // Run the threads with the given options
  for (int i = 0; i < num_threads; i++) {
    if(pthread_create(&(tid[i]), NULL, &thread_func, NULL) != 0) {
      fprintf(stderr, "ERROR failed to create thread: %s\n", strerror(errno));
      exit(1);
    }
  }

  for (int i = 0; i < num_threads; i++) {
    if(pthread_join(tid[i], NULL) != 0) {
      fprintf(stderr, "ERROR failed to join thread: %s\n", strerror(errno));
      exit(1);
    }
  }

  // Get end-time
  if(clock_gettime(CLOCK_MONOTONIC, &end_time) != 0) {
    fprintf(stderr, "ERROR gettime failed: %s\n", strerror(errno));
    exit(1);
  }

  // Cleanup synchronization options
  if (sync_opt == 'm') {
    if (pthread_mutex_destroy(&m_lock) != 0) {
      printf("ERROR failed to initialize mutex\n");
      exit(1);
    }
  }

  // Calculate the relevant values and print a CSV row
  long long num_ops = num_threads * num_iters * 2;
  long long total_time = 1000000000L * (end_time.tv_sec - start_time.tv_sec) + end_time.tv_nsec - start_time.tv_nsec;
  long long avg_optime = total_time / num_ops;
  printCSV(num_threads, num_ops, total_time, avg_optime);

  // Free relevant values and exit
  // free(targv);
  free(tid);
  exit(0);
}
