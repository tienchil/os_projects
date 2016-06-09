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

int main(int argc, char **argv) {

    unsigned char *disk;
    FILE *origin_file;
    struct stat file_stat;
    char buf[EXT2_NAME_LEN+1];
    char *s;
    char *dir;
    char *filename;
    int i;
    unsigned int new_block;

    if(argc != 4) {

        fprintf(stderr, 
                 "Usage: ext2_cp <disk image> <system: absolute path> <disk: absolute path>\n");
        exit(1);
        
    }

    disk = init_disk(argv[1]);

    // Open the file from the native system
    origin_file = fopen(argv[2], "r");

    if (!origin_file) {
        // The file does not exist
        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);

    }

    fstat(fileno(origin_file), &file_stat);
    int file_size = file_stat.st_size;

    // Check if the file on the disk exists
    strncpy(buf, argv[3], EXT2_NAME_LEN+1);
    s = argv[3];

    dir = dirname(buf);

    int dir_exist = is_exist(disk, dir);

    if (dir_exist == -ENOENT) {
        // The directory does not exist

        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);
        
    }

    int exist = is_exist(disk, s);

    // The directory exists, check if it's existing file or a directory
    if (check_path(disk, s, exist) == -EEXIST) {
        // the file exists
        fprintf(stderr, "File exists.\n");
        exit(EEXIST);

    }

    // It's a file that's not existing
    if (exist < 0) {

        filename = basename(argv[3]);

    } else {

        filename = basename(argv[2]);

    }

    

    // Find un-used inode
    int new_inode_num = available_inode(disk);

    if (new_inode_num == -ENOSPC) {

        exit(ENOSPC);

    }

    // Find the number of blocks to use
    int num_block = 1;
    int curr = file_size;

    while ((curr - EXT2_BLOCK_SIZE) > 0) {

        num_block++;

        curr = curr - EXT2_BLOCK_SIZE;

    }

    // Check if we need any indirect block
    if (num_block > 12) {

        num_block++;

    }

    struct ext2_inode *dir_inode;

    // Set the directory inode
    if (exist < 0) {

        dir_inode = get_inode(disk, dir_exist);
        insert_entry(disk, dir_inode, new_inode_num, 
                                    filename, EXT2_FT_REG_FILE);

    } else {

        dir_inode = get_inode(disk, exist);
        insert_entry(disk, dir_inode, new_inode_num, 
                                    filename, EXT2_FT_REG_FILE);

    }
    

    // Set the new inode
    struct ext2_inode *new_inode = get_inode(disk, new_inode_num);

    new_inode->i_mode = EXT2_S_IFREG;
    new_inode->i_dtime = 0;
    new_inode->i_size = file_size;
    new_inode->i_links_count = 1;
    new_inode->i_blocks = 2*num_block;

    // Get available blocks

    i = 0;

    while ((i < num_block) && (i < 12)) {

        new_block = available_block(disk);

        new_inode->i_block[i] = new_block;

        unsigned char *block = (disk + (int) new_block*EXT2_BLOCK_SIZE);

        // Copy the file data into the block
        int curr_len = 0;

        while ((curr_len + strlen(buf)) < EXT2_BLOCK_SIZE) {

            if (fgets(buf, EXT2_NAME_LEN+1, origin_file) != NULL) {

                memcpy(block, buf, EXT2_NAME_LEN+1);

                curr_len = curr_len + strlen(buf);
                block = block + strlen(buf);

            } else {

                break;

            }

        }

        
        i++;

    }


    // Large file needs indirect block pointer
    if (i > 11) {

        // Initialize the indirect block
        new_block = available_block(disk);

        new_inode->i_block[12] = new_block;

        // 
        unsigned int *indirect_block = 
                     (unsigned int *) (disk + (int) new_block*EXT2_BLOCK_SIZE);

        num_block = num_block - 13;


        for (i = 0; i < num_block; i++) {

            new_block = available_block(disk);


            *indirect_block = new_block;

            unsigned char *block = (disk + (int) new_block*EXT2_BLOCK_SIZE);

            // Copy the file data into the block
            int curr_len = 0;

            while ((curr_len + strlen(buf)) < EXT2_BLOCK_SIZE) {

                if (fgets(buf, EXT2_NAME_LEN+1, origin_file) != NULL) {

                    memcpy(block, buf, EXT2_NAME_LEN+1);

                    curr_len = curr_len + strlen(buf);
                    block = block + strlen(buf);

                } else {

                    break;

                }

            }

            indirect_block = indirect_block + sizeof(unsigned int);

        }

    }

    // Close the file
    fclose(origin_file);

    return 0;

}