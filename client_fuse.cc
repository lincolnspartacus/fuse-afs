#define FUSE_USE_VERSION 29
#define _FILE_OFFSET_BITS  64

#include <fuse.h>
#include <stdio.h>
//#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <cassert>
#include <string>
#include <vector>
#include <experimental/filesystem>
#include "RPC.h"
#include "Cache.h"
#include "timing.h"
#include <iostream>

#define DIRTY_MODE  (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH)

#define debug 0

static int client_getattr( const char *path, struct stat *st )
{
	// GNU's definitions of the attributes (http://www.gnu.org/software/libc/manual/html_node/Attribute-Meanings.html):
	// 		st_uid: 	The user ID of the file’s owner.
	//		st_gid: 	The group ID of the file.
	//		st_atime: 	This is the last access time for the file.
	//		st_mtime: 	This is the time of the last modification to the contents of the file.
	//		st_mode: 	Specifies the mode of the file. This includes file type information (see Testing File Type) and the file permission bits (see Permission Bits).
	//		st_nlink: 	The number of hard links to the file. This count keeps track of how many directories have entries for this file. If the count is ever decremented to zero, then the file itself is discarded as soon 
	//						as no process still holds it open. Symbolic links are not counted in the total.
	//		st_size:	This specifies the size of a regular file in bytes. For files that are really devices this field isn’t usually meaningful. For symbolic links this specifies the length of the file name the link refers to.
	
    // Always fetch the attributes from server??
    // TODO: NO, Keep some data structure for the file's metadata
    // OR use normal file i/o and modify the attributes

    if (debug)
    printf("[%s] Requested attr : %s\n", __func__, path);

    st->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
    st->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem

    if(strcmp( path, "/" ) == 0)
    {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2; // Two links for "." and ".."
    }
    else
    {
        int rc=RPC::client_getattr(path, st);
        return rc;
    }

	return 0;
}

static int client_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi )
{
    if (debug)
	printf( "[%s] Listing contents of %s\n", __func__, path);

	filler( buffer, ".", NULL, 0 ); // Current Directory
	filler( buffer, "..", NULL, 0 ); // Parent Directory

    std::vector<std::string> dir_entries;

    int rc = RPC::client_readdir(path, dir_entries);
    if(rc != 0)
        return -EIO;

    for(int i = 0; i < dir_entries.size(); ++i)
        filler(buffer, dir_entries[i].c_str(), NULL, 0);

	return 0;
}

static int client_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi )
{
    // Always read data from the cached copy
    if (debug)
	printf( "[%s] read : %s, %lu, %lu\n", __func__, path, offset, size );
	
    int fd;
	int res;

	fd = fi->fh;
	
	if (fd == -1)
		return -errno;

	res = pread(fd, buffer, size, offset);
	if (res == -1)
		res = -errno;

	return res;
}

static int client_open(const char *path, struct fuse_file_info *fi)
{
    // Check if copy is present in cache, if present then do RECHECK.
    // Else fetch from server
    if (debug)
    printf("[%s] open : %s\n", __func__, path);
    int rc = RPC::client_open(path,fi->flags);
    if(rc < 0)
        return rc;

    fi->fh = rc;

    return 0;
}

static int client_write(const char *path, const char *buf, size_t size,
             off_t offset, struct fuse_file_info *fi)
{
    long double start = time_monotonic();
    // Always write only to local cache copy
    // TODO: Tolerate crashes with a local update protocol
    if (debug)
    printf("[%s] write : %s\n", __func__, path);
    int fd;
	int res;

	(void) fi;

	fd = fi->fh;
	
	if (fd == -1)
		return -errno;

    //fchmod(fd, DIRTY_MODE);
	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;
    if (res != -1)
        fsync(fd);

    long double end = time_monotonic();
    std::cout << std::fixed << "Write time: " << (end - start) << "\n";

	return res;
}

static int client_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    // Contact server and ask to create the file. Create it in the cache
    // as well.
    if (debug)
    printf("[%s] create : %s\n", __func__, path);
	int res = RPC::client_create(path,fi->flags,mode);
    if(res<0)
        return res;
   
	fi->fh = res;
	return 0;
}

static int client_mkdir(const char *path, mode_t mode)
{
    if (debug)
    printf("[%s] mkdir : %s\n", __func__, path);
    int res=RPC::client_mkdir(path,mode);

    return res;
}

static int client_rmdir(const char *path)
{
    if (debug)
    printf("[%s] rmdir : %s\n", __func__, path);
    int res=RPC::client_rmdir(path);

    return res;
}

static int client_unlink(const char *path)
{
    if (debug)
    printf("[%s] unlink : %s\n", __func__, path);
    int rc=RPC::client_unlink(path);
    //Also remove the local file from the Cache
    //What happens if the file does not exist in server but in cache?
    //Just remove the file?
	return rc;
}

static int client_release(const char *path, struct fuse_file_info *fi)
{
    long double start = time_monotonic();
    (void) path;
    if (debug)
	printf("[%s] release : %s\n", __func__, path);
    //close not supported by fuse
    //Has to use release or flush calls to send the file back to server
    //As per AFS semantics, the last file desc close must trigger flush to server
    //Reference: https://pages.cs.wisc.edu/~remzi/OSTEP/dist-afs.pdf Page-7
    struct stat s;
    fstat(fi->fh, &s);

    // tmp file is dirty, flush it back to server and update original file
    //if(s.st_mode & 0777 == DIRTY_MODE) {
    if(s.st_mtime != Cache::lookup(path)) {
        if (debug)
        printf("client_release : Dirty\n");
        int rc = RPC::client_close(path);
    }
    
	close(fi->fh);
    long double end = time_monotonic();
    std::cout << std::fixed << "Close time: " << (end - start) << "\n";

	return 0;
}

static struct fuse_operations operations;

static void fuse_init()
{
    operations.getattr	  = client_getattr;
    operations.readdir    = client_readdir;
    operations.read		  = client_read;
    operations.open       = client_open;
    operations.write      = client_write;
    operations.create     = client_create;
    operations.mkdir      = client_mkdir;
    operations.rmdir      = client_rmdir;
    operations.unlink     = client_unlink;
    operations.release    = client_release;
}

int main( int argc, char *argv[] )
{
    if(argc < 3) {
        printf("Usage : ./Client -f <mount-point> -ip <ip> -cache <cache-dir>\n");
        return 0;
    }

    std::string ip{"localhost:50051"}, cache_dir{"cache"};
    for(int i = 3; i < argc - 1; ++i) {
        if(!strcmp(argv[i], "-ip")) {
            ip = std::string{argv[i+1]};
        } else if(!strcmp(argv[i], "-cache")) {
            cache_dir = std::string{argv[i+1]};
        }
    }

    RPC::init(ip, cache_dir);
    fuse_init();
    Cache::init(cache_dir);

    int newArgc = 3; // ./Client -f <mount-point>
    return fuse_main( newArgc, argv, &operations, NULL );
}
