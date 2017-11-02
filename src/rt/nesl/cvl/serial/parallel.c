#include <assert.h>
#include "parallel.h"

void chunk_range(int * vals, int index, int length, int num_procs) {
	if (length < num_procs) {
		if (index < length) {
			vals[0] = index;
			vals[1] = index + 1;
		} else {
			vals[0] = -1;
			vals[1] = -1;
		}
		return;
	}
	int spill_over = length % num_procs;
	int chunk_size = length / num_procs;
	if (index < spill_over) {
		vals[0] =  (index) + (index * chunk_size);
		vals[1] = vals[0] + chunk_size + 1;
	} else {
		vals[0] = spill_over + (index * chunk_size);
		vals[1] = vals[0] + chunk_size;
	}
	return;
}

/* CHUNK_RANGE TESTS */
int test_len_less_than_num_procs() {

	int NUM_PROCS = 12;
	int length_of_work = NUM_PROCS-1;
	int vals[2];

	int i;
	for (i = 0; i < NUM_PROCS; i ++) {
		chunk_range(vals, i, length_of_work, NUM_PROCS);
		if (i != NUM_PROCS - 1) {
			assert(vals[0] == i);
			assert(vals[1] == i + 1);
		} else {
			assert(vals[0] == -1);
			assert(vals[1] == -1);
		}
	}
	return 0;
}

int test_len_double_num_procs() {

	int NUM_PROCS = 12;
	int length_of_work = 2 * NUM_PROCS;
	int vals[2];

	int i;
	for (i = 0; i < NUM_PROCS; i ++) {
		chunk_range(vals, i, length_of_work, NUM_PROCS);
			assert(vals[0] == 2*i);
			assert(vals[1] == 2*i+2);
	}
	return 0;
}

int test_len_greater_num_proc_prime_spillover() {

	int NUM_PROCS = 13;
	int length_of_work = 2 * NUM_PROCS + 5;
	int vals[2];
	int i;
	for (i = 0; i < NUM_PROCS; i ++) {
		chunk_range(vals, i, length_of_work, NUM_PROCS);
			assert(vals[0] == 2*i + i*(i < 5 ? 1 : 0)+ (i >= 5 ? 5 : 0));
			assert(vals[1] == vals[0] + 2 + (i < 5 ? 1 :0));
	}
	return 0;
}
/*END CHUNK_RANGE TESTS*/

