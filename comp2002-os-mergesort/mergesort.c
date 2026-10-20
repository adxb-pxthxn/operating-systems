/**
 * This file implements parallel mergesort.
 */

#include <stdio.h>
#include <string.h> /* for memcpy */
#include <stdlib.h> /* for malloc */
#include "mergesort.h"
#include <pthread.h>

/* this function will be called by mergesort() and also by parallel_mergesort(). */
void merge(int leftstart, int leftend, int rightstart, int rightend){
	/* 1. copy range A to B */
	memcpy(B + leftstart, A + leftstart, sizeof(int) * (rightend - leftstart + 1));

	/* 2. three pointers */
	int i = leftstart; /* pointer for left part */
	int j = rightstart; /* pointer for right part */
	int k = leftstart; /* pointer for A */

	/* 3. merge */
	while (i <= leftend && j <=rightend) {
		if (B[i] <= B[j]) {
			A[k++] = B[i++];
		} else {
			A[k++] = B[j++];
		}
	}
	/* 4. copy the remaining elements */
	while (i <= leftend) {
		A[k++] = B[i++];
	}

	while (j <= rightend) {
		A[k++] = B[j++];
	}
}

/* this function will be called by parallel_mergesort() as its base case. */
void my_mergesort(int left, int right){
	/* base case */
	if (left >= right) return;

	/* recursive case */
	int mid = left + (right - left) / 2;
	my_mergesort(left, mid);
	my_mergesort(mid + 1, right);
	merge(left, mid, mid + 1, right);
}

/* this function will be called by the testing program. */
void * parallel_mergesort(void *arg){
	/* cast the argument */
	struct argument *in = (struct argument *) arg;
	int left = in->left;
	int right = in->right;
	int level = in->level;

	/* base case */
	if (left >= right) return NULL;

	/* if we reach the cutoff level, call my_mergesort */
	if (level >= cutoff) {
		my_mergesort(left, right);
		return NULL;
	}

	/* if the size is small enough, call my_mergesort */
	int mid = left + (right - left) / 2;

	/* build arguments for the two threads */
	struct argument *aL = buildArgs(left, mid, level + 1);
	struct argument *aR = buildArgs(mid + 1, right, level + 1);

	/* create two threads */
	pthread_t tidL, tidR;
	pthread_create(&tidL, NULL, parallel_mergesort, (void *) aL);
	pthread_create(&tidR, NULL, parallel_mergesort, (void *) aR);

	/* wait for the two threads to finish */
	pthread_join(tidL, NULL);
	pthread_join(tidR, NULL);

	/* free the arguments */
	free(aL);
	free(aR);

	/* merge */
	merge(left, mid, mid + 1, right);
	return NULL;
}

/* we build the argument for the parallel_mergesort function. */
struct argument * buildArgs(int left, int right, int level){
	/* allocate memory */
	struct argument *arg = (struct argument *) malloc (sizeof(struct argument));

	/* fill in the fields */
	arg->left = left;
	arg->right = right;
	arg->level = level;

	/* return the argument */
	return arg;
}
