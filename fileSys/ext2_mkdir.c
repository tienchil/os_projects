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
extern struct ext2_super_block *get_sb(unsigned char *disk_img);
extern struct ext2_group_desc *get_gd(unsigned char *disk_img);


int main(int argc, char **argv) {

    unsigned char *disk;
    char buf[EXT2_NAME_LEN+1];
    char *s;
    char *dir;
    char *filename;

    if(argc != 3) {

        fprintf(stderr, "Usage: ext2_cp <disk image> <absolute path>\n");
        exit(1);
        
    }

    disk = init_disk(argv[1]);

    // Get super block and the gourp desc
    struct ext2_super_block *sb = get_sb(disk);
    struct ext2_group_desc *gd = get_gd(disk);

    strncpy(buf, argv[2], EXT2_NAME_LEN+1);
    dir = dirname(buf); // getting the path

    // Check if the directory already exists
    s = argv[2];

    int exist = is_exist(disk, s);

    // The directory already exist
    if (exist != -ENOENT) {

        fprintf(stderr, "The directory exists.\n");
        exit(EEXIST);

    }

    // Get the directory name
    filename = basename(argv[2]);

    int dir_exist = is_exist(disk, dir); // Getting the inode of path

    if (dir_exist == -ENOENT) {
        // The path does not exist
        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);

    }

    // Get the new inode
    unsigned int new_inode_num = available_inode(disk);

    if (new_inode_num == -ENOSPC) {

        exit(ENOSPC);

    }

    // Set the new inode
    struct ext2_inode *new_inode = get_inode(disk, new_inode_num);

    new_inode->i_mode = EXT2_S_IFDIR;
    new_inode->i_dtime = 0;
    new_inode->i_size = EXT2_BLOCK_SIZE;
    new_inode->i_links_count = 1;
    new_inode->i_blocks = 2;

    // Update the group desc
    gd->bg_used_dirs_count++;
    gd->bg_free_inodes_count--;
    sb->s_free_inodes_count--;


    // Insert the directory entry
    struct ext2_inode *dir_inode = get_inode(disk, dir_exist);
    insert_entry(disk, dir_inode, new_inode_num, filename, EXT2_FT_DIR);


    // Allocate a block
    unsigned int new_block = available_block(disk);

    // Update the super block and group desc
    gd->bg_free_blocks_count--;
    sb->s_free_blocks_count--;

    new_inode->i_block[0] = new_block;

    unsigned char *block = (disk + (int) new_block*EXT2_BLOCK_SIZE);

    // Set entries
    int curr_len = 0;
    unsigned short default_size = 12;

    struct ext2_dir_entry_2 *dir_entry = (struct ext2_dir_entry_2 *) (block);

    // Setting "."
    strncpy(dir_entry->name, ".", 2);
    dir_entry->rec_len = default_size;
    dir_entry->name_len = strlen(dir_entry->name);
    dir_entry->file_type = EXT2_FT_DIR;
    dir_entry->inode = new_inode_num;

    curr_len = curr_len + dir_entry->rec_len;

    dir_entry = (struct ext2_dir_entry_2 *) (block + curr_len);

    // Setting ".."
    strncpy(dir_entry->name, "..", 3);
    dir_entry->rec_len = (unsigned short) (EXT2_BLOCK_SIZE - curr_len);
    dir_entry->name_len = strlen(dir_entry->name);
    dir_entry->file_type = EXT2_FT_DIR;
    dir_entry->inode = dir_exist;


    return 0;

}