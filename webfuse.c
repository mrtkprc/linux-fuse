#define FUSE_USE_VERSION 26

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <unistd.h>
#include <fuse.h>
#include <tidy.h>
#include <buffio.h>
#include <stdio.h>
#include <errno.h>
size_t size_of_tidy = 4096;

char *unTidyRWPath = NULL;
int isHtml = 0;

// Mount edilen path'i unmount path karsiligina donusturur.
// Ornegin; cat second/foo.html -> first/foo.html
static char* Mount2UnMountPath(const char* path)
{
    char *rPath= malloc(sizeof(char)*(strlen(path)+strlen(unTidyRWPath)+1));
    strcpy(rPath,unTidyRWPath);
    if (rPath[strlen(rPath)-1]=='/') {
        rPath[strlen(rPath)-1]='\0';
    }
    strcat(rPath,path);
    return rPath;
}

static int fuse_tidy_getattr(const char *path, struct stat *st_data)
{
    int res = 0;
    
    char *untidyPath=Mount2UnMountPath(path);
    memset(st_data, 0, sizeof(struct stat));
    
    if (strstr(untidyPath, ".html") == NULL) { //HTML degilse
        lstat(untidyPath, st_data);//st_data-->untidypath
        isHtml = 0;
    }
    else // HTML ise
    {
        isHtml = 1;
        st_data->st_mode = S_IFREG | 0455;
        st_data->st_nlink = 1;
        st_data->st_size = size_of_tidy;
    }
    return 0; 
}

static int fuse_tidy_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi)
{
    DIR *dp;//directory
    struct dirent *de;//directory entry
    int res;
    char *untidyPath=Mount2UnMountPath(path);

    dp = opendir(untidyPath);
    free(untidyPath);
    if(dp == NULL) {
        res = -errno;
        return res;
    }

    while((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0))
            break;
    }

    closedir(dp);
    return 0;
}

static int fuse_tidy_open(const char *path, struct fuse_file_info *finfo)
{
    int res;
    int flags = finfo->flags;

    if ((flags & O_WRONLY) || (flags & O_RDWR) || (flags & O_CREAT) || (flags & O_EXCL) || (flags & O_TRUNC) || (flags & O_APPEND)) {
        return -EROFS;
    }

    char *untidyPath=Mount2UnMountPath(path);

    res = open(untidyPath, flags);

    free(untidyPath);
    if(res == -1) {
        return -errno;
    }
    close(res);
    return 0;
}

void Tidy(char* input )
{
    TidyBuffer output = {0};
    TidyDoc tdoc = tidyCreate();              
    tidyOptSetBool( tdoc, TidyXhtmlOut, yes ); // force xhtml
    tidyOptSetInt(tdoc , TidyIndentContent,1);
    tidyOptSetInt(tdoc , TidyIndentSpaces,4); //indent with 4 spaces
    tidyParseString( tdoc, input );           
    tidyCleanAndRepair( tdoc );               
    tidyRunDiagnostics( tdoc );               
    tidyOptSetBool(tdoc, TidyForceOutput, yes);
    tidySaveBuffer( tdoc, &output );          
    strcpy(input, output.bp);
    tidyBufFree( &output );
    tidyRelease( tdoc ); 
}

static int fuse_tidy_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
    memset(buf , 0 , strlen(buf));
    int fd;
    int res;

    char *untidyPath=Mount2UnMountPath(path);
    fd = open(untidyPath, O_RDONLY);
    free(untidyPath);
    if(fd == -1) {
        res = -errno;
        return res;
    }
    //verilen offsetten oku
    res = pread(fd, buf, size, offset);
    if(res == -1) {
        res = -errno;
    }
    close(fd);
    if(isHtml)
    {
        Tidy(buf);
    }
    size_of_tidy = strlen(buf);
    res = size_of_tidy;
    return res;
}

static int fuse_tidy_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *finfo)
{
    return -EROFS;
}


static int fuse_tidy_access(const char *path, int mode)
{
    int res;
    char *untidyPath=Mount2UnMountPath(path);
    //yazma izni kontrol edilir.
    if (mode & W_OK)
        return -EROFS;
    //access - kullanıcının dosyaya erisimini kontrol eder.
    res = access(untidyPath, mode);
    free(untidyPath);
    if (res == -1) {
        return -errno;
    }
    return res;
}


struct fuse_operations fuse_tidy_operation_list = 
{
    .getattr     = fuse_tidy_getattr,
    .readdir     = fuse_tidy_readdir,
    .open        = fuse_tidy_open,
    .read        = fuse_tidy_read,
    .write       = fuse_tidy_write,
    .access      = fuse_tidy_access,
};
//Main programdan gelen argumanlari fuse un argumanlarina gecen fonksiyon pointer i
static int fuse_tidy_cmd_opt_transfer(void *data, const char *arg, int key,struct fuse_args *outargs)
{
    if(key == FUSE_OPT_KEY_NONOPT && unTidyRWPath == NULL)
    {
        unTidyRWPath = strdup(arg);
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    int result;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    //Main programdan gelen komut satırı argümanlarını fuse'ın komut satırı argüman fonksiyon pointerına geç
    result = fuse_opt_parse(&args, NULL, NULL, fuse_tidy_cmd_opt_transfer); 
    if (result != 0)
    {
        printf("Please, write valid arguments\n");
        exit(EXIT_FAILURE);
    }
    fuse_main(args.argc, args.argv, &fuse_tidy_operation_list, NULL);
    return 0;
}
