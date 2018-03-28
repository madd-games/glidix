/**
 * VFS Unit Test.
 * Wow, it's hard to get glidix kernel code to run under linux, who would've thought.
 */
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#undef _SYS_TYPES_H
#define	_SYS_TYPES_H
#undef WNOHANG
#undef WUNTRACE
#undef WCONTINUED
#define	time _fake_time
#include <glidix/vfs.h>
#include <glidix/sched.h>
#undef _SYS_TYPES_H

#undef time
time_t _fake_time()
{
	return time(NULL);
};

time_t time(time_t *ptr)
{
	return 0;
};

Thread _test_thread;
Thread* getCurrentThread()
{
	return &_test_thread;
};

void* _kmalloc(size_t size, const char *aid, int lineno)
{
	return malloc(size);
};

void _kfree(void *block, const char *who, int line)
{
	free(block);
};

void ftUp(FileTree *ft)
{
};

void ftDown(FileTree *ft)
{
};

void ftUncache(FileTree *ft)
{
};

int ftTruncate(FileTree *ft, size_t sz)
{
};

void semInit(Semaphore *sem) {};
void semInit2(Semaphore *sem, int count) {};
void semWait(Semaphore *sem) {};
void semSignal(Semaphore *sem) {};

int havePerm(uint64_t perm)
{
	return 1;
};

void _panic(const char *filename, int lineno, const char *funcname, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	printf("Kernel panic: ");
	vprintf(fmt, ap);
	va_end(ap);
	exit(1);
};

int main()
{
	printf("Initializing the VFS...\n");
	vfsInit();
	
	printf("Test 1: Failing to resolve non-existent paths\n");
	int error;
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, "/test", 0, &error);
	assert(dref.dent == NULL);
	assert(error == ENOENT);

	printf("Test 2: Successfully resolving '.'\n");
	dref = vfsGetDentry(VFS_NULL_IREF, "/.", 0, &error);
	assert(dref.dent != NULL);
	assert(strcmp(dref.dent->name, "/") == 0);
	vfsUnrefDentry(dref);

	printf("Test 3: Successfully resolving '..'\n");
	dref = vfsGetDentry(VFS_NULL_IREF, "/..", 0, &error);
	assert(dref.dent != NULL);
	assert(strcmp(dref.dent->name, "/") == 0);
	vfsUnrefDentry(dref);

	printf("Test 4: Successfully resolving '/'\n");
	dref = vfsGetDentry(VFS_NULL_IREF, "/", 0, &error);
	assert(dref.dent != NULL);
	assert(strcmp(dref.dent->name, "/") == 0);
	vfsUnrefDentry(dref);

	printf("Test 5: Creating a new dentry, '/test'\n");
	dref = vfsGetDentry(VFS_NULL_IREF, "/test", 1, &error);
	assert(dref.dent != NULL);
	assert(dref.dent->ino == 0);
	assert(strcmp(dref.dent->name, "test") == 0);
	vfsUnrefDentry(dref);
	
	printf("Test 6: Get root directory and check refcount\n");
	InodeRef rootnode = vfsGetRoot();
	assert(rootnode.inode->refcount == 5);
	
	printf("Test 7: Create a regular file inode\n");
	Inode *testfile = vfsCreateInode(rootnode.inode->fs, 0644);
	assert(testfile != NULL);

	printf("Test 8: Link the inode to /test\n");
	vfsLinkInode(dref, testfile);
	assert(testfile->refcount == 2);
	vfsDownrefInode(testfile);

	printf("Test 9: Resolve /test and check that the inode is linked correctly\n");
	dref = vfsGetDentry(VFS_NULL_IREF, "/test", 0, &error);
	assert(dref.dent != NULL);
	assert(dref.dent->target == testfile);
	assert(strcmp(dref.dent->name, "test") == 0);
	assert(dref.dent->ino == testfile->ino);
	assert(testfile->links == 1);
	vfsUnrefDentry(dref);

	printf("Test 10: Create directory /var\n");
	assert(vfsMakeDir(VFS_NULL_IREF, "/var", 0755) == 0);

	printf("Test 11: Check the reference count of said directory\n");
	dref = vfsGetDentry(VFS_NULL_IREF, "/var", 0, &error);
	assert(dref.dent != NULL);
	assert(dref.dent->target->refcount == 1);
	vfsUnrefDentry(dref);

	printf("Test 12: Failure to create directories without a parent\n");
	assert(vfsMakeDir(VFS_NULL_IREF, "/var/log/test", 0755) == ENOENT);

	printf("Test 13: Failure to delete directories when normal file declared\n");
	dref = vfsGetDentry(VFS_NULL_IREF, "/var", 0, &error);
	assert(dref.dent != NULL);
	assert(vfsUnlinkInode(dref, 0) == EISDIR);

	printf("Test 14: Successfully delete directories when they are empty\n");
	dref = vfsGetDentry(VFS_NULL_IREF, "/var", 0, &error);
	assert(dref.dent != NULL);
	assert(vfsUnlinkInode(dref, VFS_AT_REMOVEDIR) == 0);

	printf("Test 15: Ensure the directory was really deleted\n");
	dref = vfsGetDentry(VFS_NULL_IREF, "/var", 0, &error);
	assert(dref.dent == NULL);

	printf("Test 16: Failure to delete normal files as directories\n");
	dref = vfsGetDentry(VFS_NULL_IREF, "/test", 0, &error);
	assert(dref.dent != NULL);
	assert(vfsUnlinkInode(dref, VFS_AT_REMOVEDIR) == ENOTDIR);

	printf("Test 17: Deleting normal files correctly\n");
	dref = vfsGetDentry(VFS_NULL_IREF, "/test", 0, &error);
	assert(dref.dent != NULL);
	assert(vfsUnlinkInode(dref, 0) == 0);

	printf("Test 18: Make sure it was really deleted\n");
	dref = vfsGetDentry(VFS_NULL_IREF, "/test", 0, &error);
	assert(dref.dent == NULL);

	printf("All tests completed.\n");
	return 0;
};
