#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

static int webfuse_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) 
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else if(strcmp(path,"/MERTGS")==0)
    {
        stbuf->st_mode = S_IFREG | 0755;
        stbuf->st_nlink = 2;
    }
    else
    {
        res = -ENOENT;
    }
    
    return res;

}

static int webfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, "MERTGS", NULL, 0);
    
    return 0;

}
static int webfuse_open(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

static struct fuse_operations webfuse_operations = {
    .open           = webfuse_open,
    .readdir        = webfuse_readdir,
    .getattr        = webfuse_getattr

};

int main(int argc, char *argv[])
{
    return fuse_main(argc, argv, &webfuse_operations , NULL);
}