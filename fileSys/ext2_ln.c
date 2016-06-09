#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libgen.h>
#include "ext2.h"


/* Declare functions from ext2_util.c */
extern int is_exist(unsigned char *disk_img, char *path);
extern int check_path(unsigned char *disk_img, char *path, int inode_num);
extern unsigned char *init_disk(char *path);
extern unsigned int available_inode(unsigned char *disk_img);
extern char *get_filename(char *path);
extern struct ext2_inode *get_inode(unsigned char *disk_img, int inode_num);
extern unsigned int available_block(unsigned char *disk_img);
extern void insert_entry(unsigned char *disk_img, struct ext2_inode *inode, 
                                  unsigned int inode_num, char *name, char t);
extern int get_file_inode(unsigned char *disk_img, 
                                 int inode_num, char *filename);


int main(int argc, char **argv) {

    unsigned char *disk;
    char buf[EXT2_NAME_LEN+1];
    char *s;
    char *dir;
    char *source_name;
    char *link_name;
    int exist;
    int flag = 0;

    // Check number of arguments
    if ((argc < 4) || (argc > 5)) {

        fprintf(stderr, 
                 "Usage: ext2_ln <image file name> (-s) <absolute path> <absolute path>\n");
        exit(1);
        
    }

    // Check if the flag is correct
    if ((argc == 5) && (strcmp(argv[2], "-s") != 0)) {

        fprintf(stderr, 
                 "flag Usage: ext2_ln <image file name> -s <absolute path> <absolute path>\n");
        exit(1);

    }

    disk = init_disk(argv[1]);

    // Check if the source exists.
    if (argc == 4) {

        s = argv[2];
        exist = is_exist(disk, s);

    } else {

        s = argv[3];
        exist = is_exist(disk, s);
        flag = 1;

    }

    if (exist == -ENOENT) {

        fprintf(stderr, "No such file or directory.\n");

        exit(ENOENT);

    }

    // Check if the link exists.
    if (argc == 4) {

        s = argv[3];
        exist = is_exist(disk, s);

    } else {

        s = argv[4];
        exist = is_exist(disk, s);
        flag = 1;

    }

    if (exist > 0) {

        if (check_path(disk, s, exist) != -EEXIST) {

            fprintf(stderr, "Is a directory.\n");
            exit(EISDIR);

        }

        fprintf(stderr, "The link exists.\n");
        exit(EEXIST);

    }
    
    int s_inode_num;

    // Getting the inode of source
    if (argc == 4) {

        s = argv[2];
        exist = is_exist(disk, s);
        source_name = basename(argv[2]);

    } else {

        s = argv[3];
        exist = is_exist(disk, s);
        source_name = basename(argv[3]);

    }

    s_inode_num = get_file_inode(disk, exist, source_name);

    // Update the inode of source
    struct ext2_inode *source_inode = get_inode(disk, s_inode_num);

    if (flag != 1) {

        source_inode->i_links_count++;

    }
    

    /* Create a new entry in the directory of link */

    //Getting the inode of link
    if (argc == 4) {

        strncpy(buf, argv[3], EXT2_NAME_LEN+1);
        s = argv[3];

        dir = dirname(buf);

        exist = is_exist(disk, dir);
        link_name = basename(s);

    } else {

        strncpy(buf, argv[4], EXT2_NAME_LEN+1);
        s = argv[4];

        dir = dirname(buf);

        exist = is_exist(disk, dir);
        link_name = basename(s);

    }

    // Insert the directory entry
    struct ext2_inode *dir_inode = get_inode(disk, exist);

    if (flag == 1) {

        insert_entry(disk, dir_inode, s_inode_num, link_name, EXT2_FT_SYMLINK);

    } else {

        insert_entry(disk, dir_inode, s_inode_num, link_name, EXT2_FT_REG_FILE);

    }
    
    return 0;

}