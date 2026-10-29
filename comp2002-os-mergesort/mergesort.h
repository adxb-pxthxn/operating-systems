#define TRUE 1
#define FALSE 0

#include <pthread.h>

// function signatures
void my_mergesort(int left, int right);
void merge(int leftstart, int leftend, int rightstart, int rightend);

// each instance of this struct stores the argument to the parallel_mergesort() function
struct argument {
	int left;
	int right;
	int level;
};

/* we dont want arg to be a void type pointer,
   it's just that pthread_create() assumes the thread
   function takes a void type pointer as its parameter. */
void * parallel_mergesort(void *arg);
struct argument * buildArgs(int left, int right, int level);

// the number of levels of threads, specified by input
extern int cutoff;

// the array to sort
extern int *A;

// when merge, we need a temp array (named B) to temporarily store elements of the original array
extern int *B;
