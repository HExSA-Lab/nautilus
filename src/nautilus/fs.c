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

#include <nautilus/fs.h>
#include <nautilus/testfs.h>

//TODO: deal with hard links

#define INFO(fmt, args...) printk("FILESYSTEM: " fmt "\n", ##args)
#define DEBUG(fmt, args...) printk("FILESYSTEM (DEBUG): " fmt "\n", ##args)
#define ERROR(fmt, args...) printk("FILESYSTEM (ERROR): " fmt "\n", ##args)

#ifndef NAUT_CONFIG_DEBUG_FILESYSTEM
#undef DEBUG
#define DEBUG(fmt, args...)
#endif

static void iterate_opened(void (*callback)(struct file*));
static void file_print(struct file* fd);
static void set_file_interface(struct file_int *fi, enum Filesystem fs);
static struct file* get_open_file(uint32_t filenum);
static int file_has_access(struct file *fd, int access);
static uint32_t create_file(char* path);


static int open(char *path, int access);
static int close(uint32_t filenum);
static int remove(char *path);
static int exists(char *path);
static ssize_t read(int filenum, char *buf, size_t num_bytes);
static ssize_t write(int filenum, char *buf, size_t num_bytes);
static ssize_t lseek(int filenum, size_t offset, int whence);
static ssize_t tell(int filenum);

static void __close(struct file *fd);
static ssize_t __lseek(struct file *fd, size_t offset, int whence);

static void directory_list(char *path);

static struct {
	struct list_head head;
	spinlock_t lock;
} open_files;

static inline int file_open(struct file *fd, char *path, int access) {
	return fd->interface.open(&RAMFS_START, path, access);
}
static inline ssize_t file_read(struct file *fd, char *buf, size_t num_bytes) {
	return fd->interface.read(&RAMFS_START, fd->fileid, buf, num_bytes, fd->position);
}
static inline ssize_t file_write(struct file *fd, char *buf, size_t num_bytes) {
	return fd->interface.write(&RAMFS_START, fd->fileid, buf, num_bytes, fd->position);
}
static inline int file_get_size(struct file *fd) {
	return fd->interface.get_size(&RAMFS_START, fd->fileid);
}

void test_fs() {
	char *buf;
	int fn;

	/*
		 DEBUG("Opening files...");
		 fn = open("/readme", O_RDWR);
		 fn = open("/null", O_RDWR);
		 fn = open("/nothing", O_RDWR);
		 fn = open("/nothing", O_RDWR);
		 DEBUG("Done opening");
		 DEBUG("Printing...");
		 iterate_opened(file_print);
		 DEBUG("Done printing");
		 DEBUG("Closing files...");
		 iterate_opened(__close);
		 DEBUG("Done closing");
		 DEBUG("");
		 */

	char path1[] = "/null";
	buf = malloc(50);
	fn = open(path1, O_RDWR);
	/*DEBUG("Read %d", read(fn, buf, 15));
		DEBUG("Text %s", buf);
		DEBUG("Seeking %d", lseek(fn, 0, 0));
		DEBUG("Write %d", write(fn, "jjjije\nifjeiffdfdfdfdfj", 15));
		DEBUG("Seeking %d", lseek(fn, 0, 0));
		DEBUG("Read %d", read(fn, buf, 15));
		DEBUG("Text %s", buf);*/

	DEBUG("********************************");
	char* path2 = "/readme";
	char* rd_buf = malloc(50);
	char* wr_buf = malloc(50);
	fn = open(path2, O_RDWR);
	int result = read(fn, rd_buf, 50);
	DEBUG("Read %d %s", result, rd_buf);
	DEBUG("********************************");
	path2 = "/a";
	fn = open(path2, O_RDWR|O_CREAT);
	wr_buf = "testing";
	result = write(fn, wr_buf,7);
	DEBUG("Write %d", result);
	lseek(fn,0,0);
	result = read(fn, rd_buf, 7);
	DEBUG("Read %d %s", result, rd_buf);
	DEBUG("********************************");
	ext2_remove_file(&RAMFS_START,path2);
	fn = open(path2, O_RDWR|O_CREAT);
	//lseek(fn, 10,0);
	wr_buf = "testing";
	//result = write(fn, wr_buf,1);
	//DEBUG("Write %d", result);
	//lseek(fn,0,0);
	rd_buf = malloc(50);
	result = read(fn, rd_buf, 7);
	DEBUG("Read %d %s", result, rd_buf);
	DEBUG("********************************");
	free(buf);
	/*
		 char path2[] = "/a";
		 DEBUG("FILE CREATE TEST");
		 DEBUG("Creating file %d", create_file(path2));
		 fn = open(path2, O_RDWR);
		 DEBUG("Wrote %d", write(fn, "Testing file /a", 15));
		 DEBUG("Seeking %d", lseek(fn, 0, 0));
		 DEBUG("Read %d", read(fn, buf, 15));
		 DEBUG("Read %s", buf);
	//int inum = get_inode_by_path(&RAMFS_START, path);
	//DEBUG("Inode %d", inum); 
	//DEBUG("Size %d", ext2_get_size(&RAMFS_START, inum)); 
	//DEBUG("Done creating");
	//DEBUG("");
	free(buf);
	*/

	/*
		 DEBUG("FILE REMOVE TEST");
		 DEBUG("%d", get_block_size(&RAMFS_START));
		 uint32_t rootnum = get_inode_by_path(&RAMFS_START, "/");
		 DEBUG("Root inode %d", rootnum);
		 fn = open("/", 1);
		 size_t rootsize = 1024;
		 buf = malloc(1024);
		 DEBUG("Read %d", read(fn, buf, rootsize));
		 path = "/readme";
		 DEBUG("Removing file %d", ext2_remove_file(&RAMFS_START, path));
		 DEBUG("Removing file %d", ext2_remove_file(&RAMFS_START, path));
		 */


	/*
		 printk("-----------------------------------\n");
		 fn = file_open("/large_test_file3", O_RDWR);
		 fn = file_open("/large_test_file4", O_RDWR);
		 fn = file_open("/large_test_file", O_RDWR);
		 file_seek(fn,get_block_size(deviceRAMFS_START)*12,0);
		 char* read_buf = malloc(get_block_size(deviceRAMFS_START)*12);
		 char* write_buf = "****Testing****";
		 int result = file_write(fn,write_buf, strlen(write_buf));
		 printk("Write1: \n%d\n", result);
		 file_seek(fn,get_block_size(deviceRAMFS_START)*11,0);
		 result = file_read(fn,read_buf,get_block_size(deviceRAMFS_START)*2);
		 printk("Read1: \n%s \n%d\n",read_buf, result);
		 printk("\nGet: %d\n", get_inode_by_path(&RAMFS_START, "/test1"));
		 */

	run_all();
	DEBUG("Done");
}

void init_fs(void) {
	INFO("Initing...");
	INIT_LIST_HEAD(&open_files.head);
	spinlock_init(&open_files.lock);
	INFO("Done initing.");
}

void deinit_fs(void) {
	INFO("Deiniting...");
	iterate_opened(__close);
	spinlock_deinit(&open_files.lock);
	INFO("Done deiniting.");
}

static int open(char *path, int access) {
	static uint32_t n = 3; // 0 1 2 reserved in Linux
	DEBUG("HERE");
	spin_lock(&open_files.lock);
	struct file *fd = malloc(sizeof(struct file));

	// if filesystem of path is ext2 then...
	set_file_interface(&fd->interface, ext2);
	fd->access = access;
	DEBUG("HERE2 %d", exists(path));
	if (exists(path)) {
		DEBUG("HEREA %s", path);
		fd->fileid = file_open(fd, path, access);
		//fd->fileid = fd->interface.open(&RAMFS_START, path, access);
	}
	else if (file_has_access(fd, O_WRONLY) && file_has_access(fd, O_CREAT)) {
		DEBUG("HEREB");
		int id = create_file(path);
		if (!id) {
			spin_unlock(&open_files.lock);
			return -1;
		}
		DEBUG("Created %s %d", path, id);
		fd->fileid = id;
	}
	else {
		DEBUG("HEREC");
		spin_unlock(&open_files.lock);
		return -1;
	}

	DEBUG("HERE3");
	fd->filenum = n++; 
	fd->position = 0;

	// check already opened
	//if (!get_open_file(fd->filenum)) { 
	list_add(&fd->file_node, &open_files.head);
	spinlock_init(&fd->lock);
	DEBUG("Opened %s %d %d", path, fd->filenum, fd->fileid);
	//}

	spin_unlock(&open_files.lock);
	return fd->filenum;
}

static void __close(struct file *fd) {
	spin_lock(&open_files.lock);
	spinlock_deinit(&fd->lock);
	list_del((struct list_head*)fd);
	free(fd);
	spin_unlock(&open_files.lock);
}

static int close(uint32_t filenum) {
	struct file *fd = get_open_file(filenum);
	if (!fd) {
		return 0;
	}
	__close(fd);
	return 1;
}

static ssize_t read(int filenum, char *buf, size_t num_bytes) {
	struct file *fd = get_open_file(filenum);

	//  RDONLY or RDWR will return the same
	if (fd == NULL || !file_has_access(fd, O_RDONLY)) {
		return -1;
	}

	size_t n = file_read(fd, buf, num_bytes);
	//size_t n = fd->interface.read(&RAMFS_START, fd->fileid, buf, num_bytes, fd->position);
	fd->position += n;
	//printk("n: %d pos: %d\n", n, fd->position);
	return n;
}

static ssize_t write(int filenum, char *buf, size_t num_bytes) {
	struct file *fd = get_open_file(filenum);

	//  WRONLY or RDWR will return the same
	if (fd == NULL || !file_has_access(fd, O_WRONLY)) {
		return -1;
	}
	if (file_has_access(fd, O_APPEND)) {
		__lseek(fd, 0, 2);
	}

	size_t n = file_write(fd, buf, num_bytes);
	fd->position += n;
	return n;
}

static ssize_t __lseek(struct file *fd, size_t offset, int whence) {
	if(whence == 0) {
		fd->position = offset;
	}
	else if(whence == 1) {
		fd->position += offset;
	}
	else if(whence == 2) {
		size_t size = file_get_size(fd);
		//size_t size = fd->interface.get_size(&RAMFS_START, fd->fileid);
		//uint64_t size = ext2_get_size((uint32_t)fd->filenum);
		//printk("file size = %d\n",size);
		fd->position = size + offset;
	}
	else {
		return -1;
	}
	return fd->position;
}

//pos = 0 -> beginning of file, 1 -> current position, 2 -> end of file
static ssize_t lseek(int filenum, size_t offset, int whence) {
	struct file *fd = get_open_file(filenum);
	if (fd == NULL) {
		return -1;
	}
	return __lseek(fd, offset, whence);
}

static ssize_t tell(int filenum) {
	struct file *fd = get_open_file(filenum);
	if (fd == NULL) {
		return -1;
	}
	return fd->position; 
}

// returns 1 if file exists, 0 other
static int exists(char *path) {
	return ext2_exists(&RAMFS_START,path);
}

static int remove(char* path) {
	return ext2_remove_file(&RAMFS_START, path);
}

// TODO: move to ext2 level
static void directory_list(char* path) {
	char* tempname;
	struct ext2_dir_entry_2* directory_entry;
	struct ext2_inode * dir = get_inode(&RAMFS_START, get_inode_by_path(&RAMFS_START,path));
	//get pointer to block where directory entries reside using inode of directory
	directory_entry = (struct ext2_dir_entry_2*)get_block(&RAMFS_START,dir->i_block[0]);
	//scan only valid files in directory
	while(directory_entry->inode != 0){
		//create temporary name
		tempname = (char*)malloc(directory_entry->name_len*sizeof(char));
		int i;
		//copy file name to temp name, then null terminate it
		for(i = 0; i < directory_entry->name_len; i++){
			tempname[i] = directory_entry->name[i];
		}
		tempname[i] = '\0';
		// TODO: change to simply ignore any file starting with a . (hidden files)
		if(strcmp(tempname,".") && strcmp(tempname,"..")) { //strcmp -> 0 = equal, !0 = not equal
			printk("%s\n", tempname);
		}
		directory_entry = (struct ext2_dir_entry_2*)((void*)directory_entry+directory_entry->rec_len);
		free(tempname);
	}
}

static int file_has_access(struct file *fd, int access) {
	return ((fd->access & access) == access);
}

static void iterate_opened(void (*callback)(struct file*)) {
	struct list_head *cur;
	struct list_head *temp;
	struct file *fd;

	list_for_each_safe(cur, temp, &open_files.head) {
		fd = (struct file*)cur;
		callback(fd);
	}
}

static void file_print(struct file* fd) {
	printk("%d %d\n", fd->filenum, fd->fileid);
}

// creates file with the given path
static uint32_t create_file(char* path) {
	return ext2_create_file(&RAMFS_START, path);
}

static struct file* get_open_file(uint32_t filenum) {
	struct list_head *cur;
	struct file *fd;

	list_for_each(cur, &open_files.head) {
		fd = (struct file*)cur;
		if (fd->filenum == filenum) {
			return fd;
		}
	}
	return NULL;
}

static void set_file_interface(struct file_int *fi, enum Filesystem fs) {
	if (fs == ext2) {
		fi->open = ext2_open;
		fi->read = ext2_read;
		fi->write = ext2_write;
		fi->get_size = ext2_get_size;
	}
}



#include "testfs.c"
