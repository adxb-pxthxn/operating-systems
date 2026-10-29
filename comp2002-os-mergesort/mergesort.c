/**
 * mergesort.c - serial and pthread-parallel mergesort for COMP2002.
 *
 * The array to sort (A) and the scratch buffer (B) are globals defined
 * by the test driver. parallel_mergesort() splits the range in two and,
 * until it reaches the user-supplied cutoff level, creates two threads
 * per level (one per half), joins both, then merges. At the cutoff level
 * the remaining range is sorted serially with my_mergesort().
 *
 * Thread safety: sibling threads always work on disjoint index ranges
 * of A and B, and a parent only merges after pthread_join() has returned
 * for both children, so no locks are needed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "mergesort.h"

/**
 * midpoint - overflow-safe midpoint of [left, right].
 * @left:  first index (left <= right)
 * @right: last index
 *
 * Uses left + (right - left) / 2 instead of (left + right) / 2, which
 * could overflow int when both indices are close to INT_MAX.
 */
static int midpoint(int left, int right)
{
	return left + (right - left) / 2;
}

/**
 * merge - merge two adjacent sorted runs of A into one sorted run.
 * @leftstart:  first index of the left run
 * @leftend:    last index of the left run
 * @rightstart: first index of the right run (must be leftend + 1)
 * @rightend:   last index of the right run
 *
 * Copies A[leftstart..rightend] into the same slots of B, then merges
 * back into A. Only touches indices in [leftstart, rightend], so calls
 * on disjoint ranges may run concurrently. Stable (ties take the left).
 */
void merge(int leftstart, int leftend, int rightstart, int rightend)
{
	int i, j, k;

	if (A == NULL || B == NULL || leftstart > rightend)
		return;

	/* size_t arithmetic so the byte count cannot overflow int */
	memcpy(B + leftstart, A + leftstart,
	       sizeof(int) * ((size_t) rightend - (size_t) leftstart + 1));

	i = leftstart;
	j = rightstart;
	k = leftstart;

	while (i <= leftend && j <= rightend) {
		if (B[i] <= B[j])
			A[k++] = B[i++];
		else
			A[k++] = B[j++];
	}
	while (i <= leftend)
		A[k++] = B[i++];
	while (j <= rightend)
		A[k++] = B[j++];
}

/**
 * my_mergesort - recursive serial mergesort of A[left..right].
 * @left:  first index
 * @right: last index (inclusive)
 *
 * Base case: a range of 0 or 1 elements is already sorted.
 */
void my_mergesort(int left, int right)
{
	int mid;

	if (left >= right)
		return;

	mid = midpoint(left, right);
	my_mergesort(left, mid);
	my_mergesort(mid + 1, right);
	merge(left, mid, mid + 1, right);
}

/**
 * spawn_half - start a thread that sorts one half, or sort it inline.
 * @tid:   where to store the new thread id
 * @arg:   argument built by buildArgs() (may be NULL if malloc failed)
 *
 * Returns TRUE if a thread was created and must be joined, FALSE if the
 * work was done in the calling thread (malloc or pthread_create failed).
 * Falling back to serial work keeps the result correct under resource
 * exhaustion instead of aborting.
 */
static int spawn_half(pthread_t *tid, struct argument *arg)
{
	int rc;

	if (arg == NULL)
		return FALSE;

	rc = pthread_create(tid, NULL, parallel_mergesort, arg);
	if (rc != 0) {
		fprintf(stderr, "parallel_mergesort: pthread_create: %s\n",
			strerror(rc));
		parallel_mergesort(arg);	/* do the work ourselves */
		return FALSE;
	}
	return TRUE;
}

/**
 * join_half - wait for a thread started by spawn_half().
 * @tid: thread id to join
 *
 * pthread_join() is the happens-before edge that makes the child's
 * writes to A/B visible to this thread before it merges.
 */
static void join_half(pthread_t tid)
{
	int rc = pthread_join(tid, NULL);

	if (rc != 0) {
		fprintf(stderr, "parallel_mergesort: pthread_join: %s\n",
			strerror(rc));
		exit(EXIT_FAILURE);	/* cannot safely merge unfinished data */
	}
}

/**
 * parallel_mergesort - thread entry point; sorts A[arg->left..arg->right].
 * @arg: struct argument * (left, right, level), owned by the caller
 *
 * If level >= cutoff, sorts serially. Otherwise builds arguments for the
 * two halves, creates one thread for each (level + 1), joins both, frees
 * their arguments and merges. Always returns NULL. The caller keeps
 * ownership of @arg; this function never frees it.
 */
void *parallel_mergesort(void *arg)
{
	struct argument *in = (struct argument *) arg;
	struct argument *leftArg, *rightArg;
	pthread_t leftTid, rightTid;
	int leftRunning, rightRunning;
	int left, right, level, mid;

	if (in == NULL)
		return NULL;

	left = in->left;
	right = in->right;
	level = in->level;

	if (left >= right)
		return NULL;

	if (level >= cutoff) {
		my_mergesort(left, right);
		return NULL;
	}

	mid = midpoint(left, right);
	leftArg = buildArgs(left, mid, level + 1);
	rightArg = buildArgs(mid + 1, right, level + 1);

	leftRunning = spawn_half(&leftTid, leftArg);
	if (leftArg == NULL)
		my_mergesort(left, mid);
	rightRunning = spawn_half(&rightTid, rightArg);
	if (rightArg == NULL)
		my_mergesort(mid + 1, right);

	if (leftRunning)
		join_half(leftTid);
	if (rightRunning)
		join_half(rightTid);

	/* both children are finished: their args are no longer in use */
	free(leftArg);
	free(rightArg);

	merge(left, mid, mid + 1, right);
	return NULL;
}

/**
 * buildArgs - allocate and fill a struct argument.
 * @left:  first index
 * @right: last index
 * @level: thread level (main thread is 0)
 *
 * Returns a heap pointer the caller must free(), or NULL on failure.
 * Heap allocation (not a stack local) guarantees the argument outlives
 * the creating stack frame for as long as the thread uses it.
 */
struct argument *buildArgs(int left, int right, int level)
{
	struct argument *arg = malloc(sizeof(*arg));

	if (arg == NULL) {
		perror("buildArgs: malloc");
		return NULL;
	}
	arg->left = left;
	arg->right = right;
	arg->level = level;
	return arg;
}
