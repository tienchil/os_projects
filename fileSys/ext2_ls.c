#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"


/* Declare functions from ext2_util.c */
extern int is_exist(unsigned char *disk_img, char *path);
extern void print_files(unsigned char *disk_img, int inode_num, 
                                                int flag, char *path);
extern unsigned char *init_disk(char *path);


int main(int argc, char **argv) {

    unsigned char *disk;
    char *s;
    int exist;
    int flag = 0;

    // Check number of arguments
    if ((argc < 3) || (argc > 4)) {

        fprintf(stderr, 
                 "Usage: ext2_ls <image file name> (-a) <absolute path>\n");
        exit(1);
        
    }

    // Check if the flag is correct
    if ((argc == 4) && (strcmp(argv[2], "-a") != 0)) {

        fprintf(stderr, 
                 "flag Usage: ext2_ls <image file name> -a <absolute path>\n");
        exit(1);

    }

    disk = init_disk(argv[1]);

    // Check if the file or directory exists.
    if (argc == 4) {

        s = argv[3];
        exist = is_exist(disk, s);
        flag = 1;

    } else {

        s = argv[2];
        exist = is_exist(disk, s);

    }

    if (exist == -ENOENT) {

        fprintf(stderr, "No such file or directory.\n");

        exit(ENOENT);

    }

    print_files(disk, exist, flag, s);

    return 0;

}
