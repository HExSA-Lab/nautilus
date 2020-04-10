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
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2016, Brady Lee and David Williams
 * Copyright (c) 2016, Peter Dinda
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Brady Lee <BradyLee2016@u.northwestern.edu>
 *           David Williams <davidwilliams2016@u.northwestern.edu>
 *           Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/fs.h>
#include <nautilus/testfs.h>
#include <nautilus/shell.h>
#include <nautilus/blkdev.h>

#define INFO(fmt, args...)  INFO_PRINT("fs: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("fs: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("fs: " fmt, ##args)

#ifndef NAUT_CONFIG_DEBUG_FILESYSTEM
#undef DEBUG
#define DEBUG(fmt, args...)
#endif

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);

#define FILE_LOCK_CONF uint8_t _file_lock_flags
#define FILE_LOCK(fd) _file_lock_flags = spin_lock_irq_save(&fd->lock)
#define FILE_UNLOCK(fd) spin_unlock_irq_restore(&fd->lock, _file_lock_flags);



//typedef enum {EXT2}      nk_fs_type_t;
//typedef enum {BLOCK,NET} nk_fs_media_t;

struct nk_fs_open_file_state {
    spinlock_t lock;

    struct list_head file_node;
    
    struct nk_fs  *fs;
    void          *file;
    
    size_t   position;
    int      flags;
};


static spinlock_t state_lock;
static struct list_head fs_list;
static struct list_head open_files;

static void map_over_open_files(void (*callback)(nk_fs_fd_t)) 
{
    struct list_head *cur;
    struct list_head *temp;
    nk_fs_fd_t fd;
    
    list_for_each_safe(cur, temp, &open_files) {
	fd = list_entry(cur,struct nk_fs_open_file_state, file_node);
	callback(fd);
    }
}


//TODO: deal with hard links

static ssize_t __seek(nk_fs_fd_t fd, size_t offset, int whence);

//static void directory_list(char *path);

static int path_stat(struct nk_fs *fs, char *path, struct nk_fs_stat *st) 
{
    if (fs && fs->interface && fs->interface->stat_path) {
	return fs->interface->stat_path(fs->state, path, st);
    } else {
	return -1;
    }
}


static int file_stat(struct nk_fs *fs, void *file, struct nk_fs_stat *st) 
{
    if (fs && fs->interface && fs->interface->stat) {
	return fs->interface->stat(fs->state, file, st);
    } else {
	return -1;
    }
}


static void * file_open(struct nk_fs *fs, char *path, int access) 
{
    if (fs && fs->interface && fs->interface->open_file) {
	return fs->interface->open_file(fs->state, path);
    } else {
	return 0;
    }
}


static void *file_create(struct nk_fs *fs, char* path) 
{
    if (fs && fs->interface && fs->interface->create_file) {
	return fs->interface->create_file(fs->state, path);
    } else {
	return 0;
    }
}

static int file_trunc(nk_fs_fd_t fd, off_t len)
{
    if (fd && fd->fs && fd->fs->interface && fd->fs->interface->trunc_file) {
	return fd->fs->interface->trunc_file(fd->fs->state,fd->file,len);
    } else {
	return -1;
    }
}

static inline ssize_t file_read(nk_fs_fd_t fd, char *buf, size_t num_bytes) 
{
    if (!FS_FD_ERR(fd) && fd->fs && fd->fs->interface 
	&& fd->fs->interface->read_file) {
	return fd->fs->interface->read_file(fd->fs->state, 
					    fd->file, 
					    buf, 
					    fd->position,
					    num_bytes);
    } else {
	return -1;
    }
}

static inline ssize_t file_write(nk_fs_fd_t fd, char *buf, size_t num_bytes) 
{
    if (!FS_FD_ERR(fd) && fd->fs && fd->fs->interface 
	&& fd->fs->interface->write_file) {
	return fd->fs->interface->write_file(fd->fs->state, 
					     fd->file, 
					     buf, 
					     fd->position,
					     num_bytes);
    } else {
	return -1;
    }
}


static int exists(struct nk_fs *fs, char *path) 
{
    //    DEBUG("Exists (%s, %s)\n",fs->name,path);
    if (fs && fs->interface && fs->interface->exists) { 
	return fs->interface->exists(fs->state, path);
    } else {
	return 0;
    }
}

static int remove(struct nk_fs *fs, char* path) 
{
    if (fs && fs->interface && fs->interface->remove) { 
	return fs->interface->remove(fs->state, path);
    } else {
	return -1;
    }
}

int nk_fs_init(void) 
{
    INIT_LIST_HEAD(&fs_list);
    INIT_LIST_HEAD(&open_files);
    spinlock_init(&state_lock);
    INFO("inited\n");
    return 0;
}

int nk_deinit_fs(void) 
{
    if (!list_empty(&open_files)) {
	ERROR("Open files remain.. closing them\n");
	map_over_open_files((void (*)(nk_fs_fd_t))nk_fs_close);
    }
    if (!list_empty(&fs_list)) {
	ERROR("registered filesystems remain\n");
    }
    spinlock_deinit(&state_lock);
    INFO("deinited\n");
    return 0;
}

struct nk_fs *nk_fs_register(char *name, uint64_t flags, struct nk_fs_int *inter, void *state)
{
    STATE_LOCK_CONF;
    struct nk_fs *f = malloc(sizeof(*f));

    DEBUG("register fs with name %s, flags 0x%lx, interface %p, and state %p\n", name, flags, inter, state);

    if (!f) {
	ERROR("Failed to allocate filesystem\n");
	return 0;
    }
    
    memset(f,0,sizeof(*f));

    strncpy(f->name,name,FS_NAME_LEN); f->name[FS_NAME_LEN-1]=0;
    strncpy(f->mount_path,"",MOUNT_PATH_LEN); f->mount_path[MOUNT_PATH_LEN-1]=0;
    f->flags = flags;
    f->interface = inter;
    f->state = state;

    STATE_LOCK();
    list_add(&f->fs_list_node,&fs_list);
    STATE_UNLOCK();
    
    INFO("Added filesystem with name %s and flags 0x%lx\n", f->name,f->flags);
    
    return f;
}

int            nk_fs_unregister(struct nk_fs *f)
{
    STATE_LOCK_CONF;
    STATE_LOCK();
    list_del(&f->fs_list_node);
    STATE_UNLOCK();
    INFO("Unregistered filesystem %s\n",f->name);
    free(f);
    return 0;
}

static struct nk_fs *__fs_find(char *name)
{
    struct list_head *cur;
    struct nk_fs *target=0;
    list_for_each(cur,&fs_list) {
	if (!strncasecmp(list_entry(cur,struct nk_fs,fs_list_node)->name,name,FS_NAME_LEN)) { 
	    target = list_entry(cur,struct nk_fs, fs_list_node);
	    break;
	}
    }
    return target;
}

struct nk_fs *nk_fs_find(char *name)
{
    STATE_LOCK_CONF;
    struct nk_fs *fs=0;
    STATE_LOCK();
    fs = __fs_find(name);
    STATE_UNLOCK();
    return fs;
}

static char *decode_path(char *path, char *fs_name)
{
    uint64_t n = strlen(path);
    uint64_t i;

    DEBUG("decode path %s\n",path);

    for (i=0;(i<n) && (path[i]!=':');i++) {}

    if (i>=n) {
	// no devicename found, assume rootfs is meant
	strcpy(fs_name,"rootfs");
    } else {
	// i=index of first ":", split name
	strncpy(fs_name,path,i);
	fs_name[i]=0;
	path=path+i+1;
    }
    DEBUG("decoded as fs %s and path %s\n",fs_name, path);
    return path;
}

int nk_fs_stat(char *path, struct nk_fs_stat *st)
{
    STATE_LOCK_CONF;
    struct nk_fs *fs;
    char fs_name[strlen(path)+1];

    path = decode_path(path,fs_name);

    DEBUG("decode has fs_name %s path %s\n", fs_name,path);

    STATE_LOCK();
    fs = __fs_find(fs_name);
    STATE_UNLOCK();

    if (!fs) { 
	ERROR("Cannot find filesystem named %s\n",fs_name);
	return -1;
    }

    return path_stat(fs, path, st);
}

int nk_fs_truncate(char *path, off_t len)
{
    nk_fs_fd_t fd = nk_fs_open(path,O_RDWR,0);
    
    if (FS_FD_ERR(fd)) { 
	return -1;
    } else{
	if (nk_fs_ftruncate(fd,len)) { 
	    return -1;
	} else {
	    return nk_fs_close(fd);
	}
    }
}	

nk_fs_fd_t nk_fs_creat(char *path, int mode) 
{
    return nk_fs_open(path,O_WRONLY|O_TRUNC|O_CREAT,0);
}

nk_fs_fd_t nk_fs_open(char *path, int flags, int mode) 
{
    STATE_LOCK_CONF;
    struct nk_fs *fs;
    char fs_name[strlen(path)+1];

    DEBUG("open path %s, flags=%d, mode=%d\n",path,flags,mode);

    path=decode_path(path,fs_name);

    STATE_LOCK();
    fs = __fs_find(fs_name);
    STATE_UNLOCK();

    if (!fs) { 
	ERROR("Cannot find filesystem named %s\n",fs_name);
	return FS_BAD_FD;
    }

    nk_fs_fd_t fd = malloc(sizeof(*fd));
    if (!fd) { 
	ERROR("Can't allocate new open file entry\n");
	return FS_BAD_FD;
    }

    memset(fd,0,sizeof(*fd));
    spinlock_init(&fd->lock);
    fd->fs = fs;
    fd->flags = flags;

    if (exists(fs,path)) {
	DEBUG("path %s exists\n", path);
	fd->file = file_open(fs, path, flags);
    } else if (flags & O_CREAT) {
	DEBUG("path %s does not exist, but creating file\n",path);
	if ((fs->flags & NK_FS_READONLY)) { 
	    ERROR("Filesystem is not writeable so cannot create file\n");
	    return FS_BAD_FD;
	}
	fd->file = file_create(fs, path);
	if (!fd->file) {
	    ERROR("Cannot create file %s\n", path);
	    free(fd);
	    return FS_BAD_FD;
	} else {
	    DEBUG("Created file %s on fs %s file=%p ", path, fs_name, fd);
	} 
    } else {
	DEBUG("path %s does not exist, and no creation requested\n",path);
	free(fd);
	return FS_BAD_FD;
    }
    
    STATE_LOCK();
    list_add(&fd->file_node, &open_files);
    STATE_UNLOCK();

    if (flags & O_TRUNC) { 
	file_trunc(fd,0);
    }

    if (flags & O_APPEND) {
	__seek(fd, 0, 2);
    }

    DEBUG("Opened file %s on fs %s file=%p ", path, fs_name, fd);

    return fd;
}

int nk_fs_close(nk_fs_fd_t fd) 
{
    STATE_LOCK_CONF;

    STATE_LOCK();
    list_del(&fd->file_node);
    STATE_UNLOCK();

    free(fd);
    
    return 0;
}

ssize_t nk_fs_read(nk_fs_fd_t fd, void *buf, size_t num_bytes) 
{
    FILE_LOCK_CONF;

    DEBUG("attempt read of %ld bytes starting at position %lu\n", num_bytes, fd->position);

    if (FS_FD_ERR(fd) || !(fd->flags & O_RDONLY)) { // includes RDWR
	ERROR("Cannot read file not opened for reading\n");
	return -1;
    }

    FILE_LOCK(fd);
    ssize_t n = file_read(fd, buf, num_bytes);
    if (n>=0) {fd->position += n; }
    FILE_UNLOCK(fd);

    DEBUG("read %ld bytes ending at position %lu\n", n, fd->position);

    return n;
}

ssize_t nk_fs_write(nk_fs_fd_t fd, void *buf, size_t num_bytes) 
{
    FILE_LOCK_CONF;

    DEBUG("attempt write of %ld bytes starting at position %lu\n", num_bytes, fd->position);

    if (FS_FD_ERR(fd) || !(fd->flags & O_WRONLY)) { // includes RDWR
	ERROR("Cannot write file not opened for writing\n");
	return -1;
    }

    if (fd->fs->flags & NK_FS_READONLY) { 
	ERROR("Not a writeable filesystem\n");
	return -1;
    }

    FILE_LOCK(fd);
    ssize_t n = file_read(fd, buf, num_bytes);
    if (n>=0) {fd->position += n; }
    FILE_UNLOCK(fd);

    DEBUG("wrote %ld bytes ending at position %lu\n", n, fd->position);

    return n;
}

int nk_fs_ftruncate(nk_fs_fd_t fd, off_t len)
{
    FILE_LOCK_CONF;
    int rc;

    if (fd->fs->flags & NK_FS_READONLY) { 
	ERROR("Filesystem is not writeable so cannot truncate file\n");
	return -1;
    }

    FILE_LOCK(fd);
    rc = file_trunc(fd,len);
    FILE_UNLOCK(fd);
    return rc;
}

int nk_fs_fstat(nk_fs_fd_t fd, struct nk_fs_stat *st)
{
    return file_stat(fd->fs,fd->file,st);
}

static ssize_t __seek(nk_fs_fd_t fd, size_t offset, int whence) 
{
    if (whence == 0) {
	fd->position = offset;
    } else if (whence == 1) {
	fd->position += offset;
    } else if (whence == 2) {
	struct nk_fs_stat st;
	
	if (file_stat(fd->fs,fd->file,&st)) { 
	    ERROR("Cannot stat file\n");
	    return -1;
	}
	fd->position = st.st_size + offset;
    }	else {
	return -1;
    }
    
    return fd->position;
}
 
off_t nk_fs_seek(nk_fs_fd_t fd, off_t offset, int whence) 
{
    FILE_LOCK_CONF;
    ssize_t s;

    FILE_LOCK(fd);
    s = __seek(fd, offset, whence);
    FILE_UNLOCK(fd);
    return s;
}



ssize_t nk_fs_tell(nk_fs_fd_t fd) 
{
    return fd->position; 
}


void nk_fs_dump_filesystems()
{
    STATE_LOCK_CONF;
    struct list_head *cur;

    STATE_LOCK();

    list_for_each(cur,&fs_list) {
	struct nk_fs *fs = list_entry(cur,struct nk_fs,fs_list_node);
	nk_vc_printf("%s:\n", fs->name);
    }
    STATE_UNLOCK();
}


void nk_fs_dump_files()
{
    STATE_LOCK_CONF;
    struct list_head *cur;

    STATE_LOCK();

    list_for_each(cur,&open_files) {
	struct nk_fs_open_file_state *f = list_entry(cur,struct nk_fs_open_file_state,file_node);
	nk_vc_printf("%s:%p at %lu flags %x\n", f->fs->name,f->file,f->position,f->flags);
    }
    STATE_UNLOCK();
}


static int
handle_attach (char * buf, void * priv)
{
    char type[32], devname[32], fsname[32]; 
    uint64_t start, count;
    struct nk_block_dev *d;
    struct nk_block_dev_characteristics c;
    int rc;

    if (sscanf(buf,"attach %s %s %s",devname, type, fsname)!=3) {
        nk_vc_printf("Don't understand %s\n",buf);
        return -1;
    }

    if (!strcmp(type,"ext2")) { 
#ifndef NAUT_CONFIG_EXT2_FILESYSTEM_DRIVER 
        nk_vc_printf("Not compiled with EXT2 support, cannot attach\n");
        return -1;
#else
        if (nk_fs_ext2_attach(devname,fsname,0)) {
            nk_vc_printf("Failed to attach %s as ext2 volume with name %s\n", devname,fsname);
            return -1;
        } else {
            nk_vc_printf("Device %s attached as ext2 volume with name %s\n", devname,fsname);
            return 0;
        }
#endif
    } else {
        nk_vc_printf("FS type %s is not supported\n", type);
        return -1;
    }
}

static struct shell_cmd_impl attach_impl = {
    .cmd      = "attach",
    .help_str = "attach",
    .handler  = handle_attach,
};
nk_register_shell_cmd(attach_impl);



#if 0

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
#endif

static int
handle_fses (char * buf, void * priv)
{
    nk_fs_dump_filesystems();
    return 0;
}

static int
handle_ofs (char * buf, void * priv)
{
    nk_fs_dump_files();
    return 0;
}

static int
handle_cat (char * buf, void * priv)
{
    char data[SHELL_MAX_CMD];
    ssize_t ct, i;

    buf+=3;

    while (*buf && *buf==' ') { buf++;}

    if (!*buf) { 
        nk_vc_printf("No file requested\n");
        return 0;
    }

    nk_fs_fd_t fd = nk_fs_open(buf,O_RDONLY,0);

    if (FS_FD_ERR(fd)) { 
        nk_vc_printf("Cannot open \"%s\"\n",buf);
        return 0;
    }

    do {
        ct = nk_fs_read(fd, data, SHELL_MAX_CMD);
        if (ct<0) {
            nk_vc_printf("Error reading file\n");
            nk_fs_close(fd);
            return 0;
        }
        for (i=0;i<ct;i++) {
            nk_vc_printf("%c",data[i]);
        }
    } while (ct>0);

    return 0;
}

static struct shell_cmd_impl fses_impl = {
    .cmd      = "fses",
    .help_str = "fses",
    .handler  = handle_fses,
};
nk_register_shell_cmd(fses_impl);

static struct shell_cmd_impl ofs_impl = {
    .cmd      = "ofs",
    .help_str = "ofs",
    .handler  = handle_ofs,
};
nk_register_shell_cmd(ofs_impl);

static struct shell_cmd_impl cat_impl = {
    .cmd      = "cat",
    .help_str = "cat [path]",
    .handler  = handle_cat,
};
nk_register_shell_cmd(cat_impl);

