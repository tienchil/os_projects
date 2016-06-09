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
extern struct ext2_super_block *get_sb(unsigned char *disk_img);
extern struct ext2_group_desc *get_gd(unsigned char *disk_img);
extern void unset_bitmap(unsigned char *bitmap, int num);


int main(int argc, char **argv) {

    unsigned char *disk;
    char buf[EXT2_NAME_LEN+1];
    char *s;
    char *dir;
    char *filename;
    int i;

    if(argc != 3) {

        fprintf(stderr, 
                 "Usage: ext2_cp <disk image> <disk: absolute path>\n");
        exit(1);
        
    }

    disk = init_disk(argv[1]);

    // Get bitmaps, super block and the gourp desc
    struct ext2_super_block *sb = get_sb(disk);
    struct ext2_group_desc *gd = get_gd(disk);

    unsigned char *block_map = (unsigned char *) (disk + EXT2_BLOCK_SIZE*3);
    unsigned char *inode_map = (unsigned char *) (disk + EXT2_BLOCK_SIZE*4);


    // Check if the file on the disk exists
    strncpy(buf, argv[2], EXT2_NAME_LEN+1);
    s = argv[2];

    dir = dirname(buf);

    int exist = is_exist(disk, s);

    if (exist == -ENOENT) {
        // The directory does not exist

        fprintf(stderr, "No such file or directory.\n");
        exit(ENOENT);
        
    }

    // The directory exists, check if it's existing file or a directory
    if (check_path(disk, s, exist) != -EEXIST) {
        // the file does exist

        fprintf(stderr, "Is a directory.\n");
        exit(EISDIR);

    }

    
    filename = basename(s);
    exist = is_exist(disk, dir);

    int inode_num = get_file_inode(disk, exist, filename);

    struct ext2_inode *file_inode = get_inode(disk, inode_num);
 

    // Check the type of the file
    if (file_inode->i_mode & EXT2_S_IFREG) {
        // A file

        // Update the inode info
        file_inode->i_dtime++;
        file_inode->i_links_count--;


        // The last link
        if (file_inode->i_links_count == 0) {

            // Update the inode info
            file_inode->i_blocks = 0;
            file_inode->i_size = 0;
            file_inode->i_mode = 0;

            unset_bitmap(inode_map, inode_num);

            // Update the super block and group desc
            gd->bg_free_inodes_count++;
            sb->s_free_inodes_count++;

            // Clear the block
            for (i = 0; i < 12; i++) {

                if (file_inode->i_block[i]) {

                    unset_bitmap(block_map, file_inode->i_block[i]);

                    // Update the super block and group desc
                    gd->bg_free_blocks_count++;
                    sb->s_free_blocks_count++;

                    file_inode->i_block[i] = 0;

                }  

            }

            // Clear indirect block
            if (file_inode->i_block[12]) {

                unset_bitmap(block_map, file_inode->i_block[12]);

                // Update the super block and group desc
                gd->bg_free_blocks_count++;
                sb->s_free_blocks_count++;

                int indirect = file_inode->i_block[12];

                unsigned int *in_block = (unsigned int *) 
                                             (disk + indirect*EXT2_BLOCK_SIZE);

                // Update info
                while (in_block) {

                    unset_bitmap(block_map, *in_block);

                    // Update the super block and group desc
                    gd->bg_free_blocks_count++;
                    sb->s_free_blocks_count++;

                    in_block = in_block + sizeof(unsigned int *);

                }

            }

        }

        // Delete the directory entry
        struct ext2_inode *dir_inode = get_inode(disk, exist);

        unsigned int *block = dir_inode->i_block;

        unsigned char *length = (disk + (*block)*EXT2_BLOCK_SIZE);

        struct ext2_dir_entry_2 *dir_entry = 
                                  (struct ext2_dir_entry_2 *) (length);

        while (*block) {

            int curr_len = 0;

            while (curr_len < EXT2_BLOCK_SIZE) {

                dir_entry = (struct ext2_dir_entry_2 *) (length + curr_len);

                // Clear the buffer
                strncpy(buf, "\0", EXT2_NAME_LEN+1);

                strncpy(buf, dir_entry->name, dir_entry->name_len);

                // Compare the name
                if (strncmp(filename, buf, dir_entry->name_len) == 0) {

                    // Simply mark it as empty and unknown
                    memset(dir_entry->name, 0, EXT2_NAME_LEN);
                    dir_entry->inode = 0;
                    dir_entry->name_len = 0;
                    dir_entry->file_type = EXT2_FT_UNKNOWN;

                    break;

                }

                curr_len = curr_len + dir_entry->rec_len;

            }

            block = block + sizeof(unsigned int);

        }

    } 

    else if (file_inode->i_mode & EXT2_S_IFLNK) {
        // A symlink

        struct ext2_inode *dir_inode = get_inode(disk, exist);

        // sumlink, simply delete the directory entry
        unsigned int *block = dir_inode->i_block;

        unsigned char *length = (disk + (*block)*EXT2_BLOCK_SIZE);

        struct ext2_dir_entry_2 *dir_entry = 
                                  (struct ext2_dir_entry_2 *) (length);

        while (*block) {

            int curr_len = 0;

            while (curr_len < EXT2_BLOCK_SIZE) {

                dir_entry = (struct ext2_dir_entry_2 *) (length + curr_len);

                // Clear the buffer
                strncpy(buf, "\0", EXT2_NAME_LEN+1);

                strncpy(buf, dir_entry->name, dir_entry->name_len);

                // Compare the name
                if (strncmp(filename, buf, dir_entry->name_len) == 0) {

                    // Simply mark it as empty and unknown
                    memset(dir_entry->name, 0, EXT2_NAME_LEN);
                    dir_entry->inode = 0;
                    dir_entry->name_len = 0;
                    dir_entry->file_type = EXT2_FT_UNKNOWN;

                    break;

                }

                curr_len = curr_len + dir_entry->rec_len;

            }

            block = block + sizeof(unsigned int);

        }

    }


    return 0;

}