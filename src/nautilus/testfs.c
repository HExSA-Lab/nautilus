/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2016, Brady Lee and David Williams
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Brady Lee <BradyLee2016@u.northwestern.edu>
 *           David Williams <davidwilliams2016@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifdef INFO
#undef INFO
#endif
#define INFO(fmt, args...) printk("FILESYSTEM TESTING: " fmt "\n", ##args)

int run_all() {
	int passed = 0;
	passed += test_1();
	passed += test_2();
	passed += test_3();
	passed += test_4();
	passed += test_5();
	//passed += test_6();
	INFO("%d tests passed", passed);
	return passed;
}

//open, then read more than length of file
int test_1() {
	int passed = 0;
	int fn = open("/readme",O_RDWR);
	char* buf = malloc(15);
	ssize_t bytes = read(fn, buf, 15);
	if(bytes == 15 && !strcmp(buf,"hello world\n")) {
		INFO("Test 1: PASSED");
		passed = 1;
	}
	else {
		INFO("Test 1: FAILED");
	}
	free(buf);
	return passed;
}

//open, write, seek to beginning, then read more than length of file
int test_2() {
	int passed = 0;
	int fn = open("/readme",O_RDWR);
	char* wr_buf = "adios";
	char* rd_buf = malloc(15);
	int wr_bytes = write(fn, wr_buf, 5);
	lseek(fn,0,0);
	int rd_bytes = read(fn, rd_buf, 15);
	if(rd_bytes == 15 && wr_bytes == 5 && !strcmp(rd_buf,"adios world\n")) {
		INFO("Test 2: PASSED");
		passed = 1;
	}
	else {
		INFO("Test 2: FAILED");
	}
	free(rd_buf);
	return passed;
}

//open, seek to end, write to end, seek to beginning, read all
int test_3() {
	int passed = 0;
	int fn = open("/readme",O_RDWR);
	char* wr_buf = "adios";
	char* rd_buf = malloc(20);
	lseek(fn,0,2);
	int wr_bytes = write(fn, wr_buf, 5);
	lseek(fn,0,0);
	int rd_bytes = read(fn, rd_buf, 20); 
	if(rd_bytes == 20 && wr_bytes == 5 && !strcmp(rd_buf,"adios world\nadios")) {
		INFO("Test 3: PASSED");
		passed = 1;
	}
	else {
		INFO("Test 3: FAILED");
	}
	free(rd_buf);
	return passed;
}
//open a blank file, write to it, read it
int test_4() {
	int passed = 0;
	int fn = open("/null",O_RDWR);
	char* wr_buf = "this used to be empty";
	char* rd_buf = malloc(21);
	int wr_bytes = write(fn, wr_buf, 21);
	lseek(fn,0,0);
	int rd_bytes = read(fn, rd_buf, 21); 
	if(rd_bytes == 21 && wr_bytes == 21 && !strcmp(rd_buf,"this used to be empty")) {
		INFO("Test 4: PASSED");
		passed = 1;
	}
	else {
		INFO("Test 4: FAILED");
	}
	free(rd_buf);
	return passed;
}
//open a file that doesnt exist, write to it, read it
int test_5() {
	int passed = 0;
	int fn = open("/new_file",O_RDWR|O_CREAT);
	char* wr_buf = "this used to not exist";
	char* rd_buf = malloc(22);
	int wr_bytes = write(fn, wr_buf, 22);
	lseek(fn,0,0);
	int rd_bytes = read(fn, rd_buf, 22); 
	if(rd_bytes == 22 && wr_bytes == 22 && !strcmp(rd_buf,"this used to not exist")) {
		INFO("Test 5: PASSED");
		passed = 1;
	}
	else {
		INFO("Test 5: FAILED");
	}
	free(rd_buf);
	return passed;
}
//open a file that doesn't exist, write to it, delete it, create it again, read it
int test_6() {
	int passed = 0;
	int fn = open("/new_file2",O_RDWR|O_CREAT);
	char* wr_buf = "this used to not exist";
	char* rd_buf = malloc(22);
	int wr_bytes = write(fn, wr_buf, 22);
	remove("/new_file2");
	signed int testfn = open("/new_file2",O_RDWR);
	INFO("HERE-- %x", testfn);
	fn = open("/new_file2",O_RDWR|O_CREAT);
	
	int rd_bytes = read(fn, rd_buf, 22); 
	if(rd_bytes == 0 && wr_bytes == 22 && strcmp(rd_buf,"this used to not exist") && testfn == -1) {
		INFO("Test 6: PASSED");
		passed = 1;
	}
	else {
		INFO("Test 6: FAILED");
	}
	free(rd_buf);
	return passed;
}

