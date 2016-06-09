#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include "ext2.h"


#define BLOCK_NUM 128

/* A function for reading the disk */
unsigned char *init_disk(char *path) {

	unsigned char *disk;

	int fd = open(path, O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (disk == MAP_FAILED) {

        perror("mmap");
        exit(1);

    }

    return disk;

}

/* Return the pointer to the superblock */
struct ext2_super_block *get_sb(unsigned char *disk_img) {

	struct ext2_super_block *sb;

	sb = (struct ext2_super_block *) (disk_img + EXT2_BLOCK_SIZE);

	return sb;

}

/* Return the pointer to the group description block */ 
struct ext2_group_desc *get_gd(unsigned char *disk_img) {

	struct ext2_group_desc *gd;

	gd = (struct ext2_group_desc *) (disk_img + EXT2_BLOCK_SIZE*2);

	return gd;

}


/* A function for checking file type */
char inode_file_type (unsigned short mode) {

    if (mode & EXT2_S_IFLNK) {

        return 'f';

    } else if (mode & EXT2_S_IFREG) {

        return 'f';

    } else if (mode & EXT2_S_IFDIR) {

        return 'd';

    }

    // None type
    return 'n';

}


/* A function for checking directory type */
char dir_type (unsigned char file_type) {

    if (file_type & EXT2_FT_UNKNOWN) {

        return 'u';

    } else if (file_type & EXT2_FT_REG_FILE) {

        return 'f';

    } else if (file_type & EXT2_FT_DIR) {

        return 'd';

    }

    // None type
    return 'n';

}

/* Return the next available inode number */
unsigned int available_inode(unsigned char *disk_img) {

	int i;

	// inode bitmap
	unsigned char *inode_map = 
	               (unsigned char *) (disk_img + EXT2_BLOCK_SIZE*4);

	// Super block
	struct ext2_super_block *sb = get_sb(disk_img);

	// Group desc
	struct ext2_group_desc *gd = get_gd(disk_img);


	// i starts at 11
	for (i = 11; i < 32; i++) {

		// index of bitmap
	    int j = i / 8;

	    // index of inode in bitmap
	    int k = i % 8;

	    // Check if it's free
	    if (!((inode_map[j] >> k) & 1)) {

	    	inode_map[j] = inode_map[j] | (1 << k);

	    	// Update super block and group desc
	    	sb->s_free_inodes_count--;
	    	gd->bg_free_inodes_count--;

	    	// Inode number is index + 1
	        return (unsigned int) i+1;

	    }

	}

	// No inode is available
	return -ENOSPC;

}

/* Get the next available block */
unsigned int available_block(unsigned char *disk_img) {

	int i;

	// block bitmap
	unsigned char *block_map = 
	               (unsigned char *) (disk_img + EXT2_BLOCK_SIZE*3);

	// Super block
	struct ext2_super_block *sb = get_sb(disk_img);

	// Group desc
	struct ext2_group_desc *gd = get_gd(disk_img);


	for (i = 0; i < BLOCK_NUM; i++) {

		// index of bitmap
		int j = i / 8;

	    // index of block in bitmap
	    int k = i % 8;

	    // Check if it's free
	    if (!((block_map[j] >> k) & 1)) {

	    	block_map[j] = block_map[j] | (1 << k);

	    	// Update super block and group desc
	    	sb->s_free_blocks_count--;
	    	gd->bg_free_blocks_count--;

	    	// Inode number is index + 1
	        return (unsigned int) i+1;

	    }

	}

	// No block is available
	return -ENOSPC;

}

/* unset an inode or a block in the bitmap. */
void unset_bitmap(unsigned char *bitmap, int num) {

	// index of bitmap
	int j = num / 8;

    // index of block in bitmap
    int k = num % 8;

    // Mask
    unsigned int mask = ~(1 << k);

    bitmap[j] = bitmap[j] & mask;

}



/* This function returns the pointer to a paricular inode */
struct ext2_inode *get_inode(unsigned char *disk_img, int inode_num) {

	// The pointer to the inode table
	unsigned char *inode_table = disk_img + EXT2_BLOCK_SIZE*5;

	// The index of the inode
	int idx = inode_num - 1;

	struct ext2_inode *inode =
	    (struct ext2_inode *) (inode_table + idx*sizeof(struct ext2_inode));

	return inode;

}


/* Return the inode number upon successfully 
          finding the file or diresctory. */
int find_inode(unsigned char *disk_img, char *path, int inode_num) {

	char buf[EXT2_NAME_LEN+1];
	char *s = path;

	// Get pointer to the inode
	struct ext2_inode *inode = get_inode(disk_img, inode_num);
	char t = inode_file_type(inode->i_mode);

	// Get the pointer to data blocks
	unsigned int *block = inode->i_block;

	strncpy(buf, s, EXT2_NAME_LEN+1);

	// End of the path.
	if (buf[1] == '\0') {

		return inode_num;

	}

	// Check every block
	while(*block) {

		// The inode is a directory, traverse further is possible
		if (t == 'd') {

			unsigned char *length = (disk_img + (*block)*EXT2_BLOCK_SIZE);

			struct ext2_dir_entry_2 *dir = 
				    (struct ext2_dir_entry_2 *) (length);

			int curr_len = 0;

			// Check data in this block
			while (curr_len < EXT2_BLOCK_SIZE) {

				// Clear the buffer

				memset(buf, '\0', EXT2_NAME_LEN+1);

				dir = (struct ext2_dir_entry_2 *) (length + curr_len);

				strncpy(buf, dir->name, dir->name_len);

				if (strncmp(s+1, buf, dir->name_len) == 0) {
					// The directory exists, go to the next inode

					char dir_t = dir_type(dir->file_type);

					s = s + dir->name_len + 1;

					// the path specifies a file
					if (dir_t == 'f') {

						dir = (struct ext2_dir_entry_2 *) (length);

					}

					return find_inode(disk_img, s, dir->inode);

				}

                curr_len = curr_len + dir->rec_len;

			}

		}

		block = block + sizeof(unsigned int);

	}

	// The file is not in the block
	return -ENOENT;

}


/* Checks if a file or a directory exists */
int is_exist(unsigned char *disk_img, char *path) {

	char buf[EXT2_NAME_LEN+1];
	char *s = path;

	strncpy(buf, s, EXT2_NAME_LEN+1);

	// Base case
	if (buf[0] != '/') {

		return -ENOENT;

	}

	// The current directory exists, check the inode
	return find_inode(disk_img, s, EXT2_ROOT_INO);

}

/* Prints all files in an inode
   If the flag is 1, prints . and .. as well */
void print_files(unsigned char *disk_img, 
	             int inode_num, int flag, char *path) {

	char buf[EXT2_NAME_LEN+1];

	// Variables for checking if the path specifies a file
	const char delim[2] = "/";
	char *token;
	int N = strlen(path);
	char s[N+1];
	int is_file = 0; // An indicator for printing a filename

	// Get the pointer to the inode
	struct ext2_inode *inode = get_inode(disk_img, inode_num);

	// Get the pointer to data blocks
	unsigned int *block = inode->i_block;

	unsigned char *length = (disk_img + (*block)*EXT2_BLOCK_SIZE);

	struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (length);

	// Check every block
	while(*block) {

		int curr_len = 0;

		// Counter for the loop
		int i = 0;

		while (curr_len < EXT2_BLOCK_SIZE) {

			// Clear the buffer
			strncpy(buf, "\0", EXT2_NAME_LEN+1);

			dir = (struct ext2_dir_entry_2 *) (length + curr_len);



			// Get the filename
			strncpy(buf, dir->name, dir->name_len);

			// Get each file/directory mentioned in the path
			strncpy(s, path, N);
			token = strtok(s, delim);

			// Check if the path specifies a file or .. or .
			while (token != NULL) {

				if (strncmp(token, buf, dir->name_len) == 0) {

					// Case 1: .
					if (strncmp(buf, ".", dir->name_len) == 0) {

						break;

					} else if (strncmp(buf, "..", dir->name_len) == 0){ 
						// Case 2: ..

						print_files(disk_img, dir->inode, flag, "\0");

						is_file = 1;

					} else {

						// Case 3: a file
						printf("%s\n", buf);

						is_file = 1;


					}
					
				} 

				token = strtok(NULL, delim);

			}

			curr_len = curr_len + dir->rec_len;

		}

		dir = (struct ext2_dir_entry_2 *) (length);
		curr_len = 0;

		// Check data in this block
		while (curr_len < EXT2_BLOCK_SIZE) {

			// Clear the buffer
			strncpy(buf, "\0", EXT2_NAME_LEN+1);

			dir = (struct ext2_dir_entry_2 *) (length + curr_len);

			strncpy(buf, dir->name, dir->name_len);


			// The path specifies a directory
			if ((!is_file) && (dir->name_len != 0)) {

				if (i < 2) {

					// Flag is set, print . and ..
					if (flag == 1) {

						printf("%s\n", buf);

					}

				} else {

					printf("%s\n", buf);

				}

			}

			curr_len = curr_len + dir->rec_len;
			i++;

		}

		block = block + sizeof(unsigned int);

	}

}

/* Check what type of file the path specifies.
   If the path is a file and it exists, returns -EEXIST.
   Otherwise, return the inode number  */
int check_path(unsigned char *disk_img, char *path, int inode_num) {

	char buf[EXT2_NAME_LEN+1];
	const char delim[2] = "/";
	char *token;
	int N = strlen(path);
	char s[N+1];

	if (inode_num <= 0) {

		return inode_num;

	}

	// Get the pointer to the inode
	struct ext2_inode *inode = get_inode(disk_img, inode_num);

	// Get the pointer to data blocks
	unsigned int *block = inode->i_block;

	unsigned char *length = (disk_img + (*block)*EXT2_BLOCK_SIZE);

	struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (length);

	// Check every block
	while(*block) {

		int curr_len = 0;

		while (curr_len < EXT2_BLOCK_SIZE) {

			// Clear the buffer
			strncpy(buf, "\0", EXT2_NAME_LEN+1);

			dir = (struct ext2_dir_entry_2 *) (length + curr_len);

			// Get the filename
			strncpy(buf, dir->name, dir->name_len);

			// Get each file/directory mentioned in the path
			strncpy(s, path, N);
			token = strtok(s, delim);

			// Check if the path specifies a file or .. or .
			while (token != NULL) {

				if (strncmp(token, buf, dir->name_len) == 0) {

					// Case 1: .
					if (strncmp(buf, ".", dir->name_len) == 0) {

						return dir->inode;

					} else if (strncmp(buf, "..", dir->name_len) == 0){ 
						// Case 2: ..

						return dir->inode;

					} else {

						// Case 3: a file
						return -EEXIST;

					}
					
				} 

				token = strtok(NULL, delim);

			}

			curr_len = curr_len + dir->rec_len;

		}

		block = block + sizeof(unsigned int);

	}

	// No special case: file does not exists, or the ending is not . or ..
	return inode_num;

}


/* Insert a directory entry into a block from an inode. 
   inode: the inode of directory containing file with inode number inode_num
   name: the filename
   t: the type of entry */
void insert_entry(unsigned char *disk_img, struct ext2_inode *inode, 
	                  unsigned int inode_num, char *name, unsigned char t) {

	int factor;
	int new_entry_size;
	int entry_size;

    // Get the pointer to data blocks
	unsigned int *block = inode->i_block;

	unsigned char *length = (disk_img + (*block)*EXT2_BLOCK_SIZE);

	struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (length);

	// Get the size of the entry
	factor = (sizeof(struct ext2_dir_entry_2) + strlen(name));

	if ((factor % 4) == 0) {

		new_entry_size = factor;

	} else {

		new_entry_size = 4*((factor / 4) + 1);

	}


	while (*block) {

		int curr_len = 0;

		while (curr_len < EXT2_BLOCK_SIZE) {

			dir = (struct ext2_dir_entry_2 *) (length + curr_len);

			// The last entry, insert here
			if ((curr_len + dir->rec_len) == EXT2_BLOCK_SIZE) {

				// size needs to be multiple of 4

				factor = (sizeof(struct ext2_dir_entry_2) + strlen(dir->name));

				if ((factor % 4) == 0) {

					entry_size = factor;

				} else {

					entry_size = 4*((factor / 4) + 1);

				}

				// Check the size
				if ((dir->rec_len - entry_size) > 0) {

					if ((dir->rec_len - new_entry_size - entry_size) >= 0) {

						// Find the new rec_len
						unsigned short new_rec_len = dir->rec_len - entry_size;

						dir->rec_len = entry_size;
						curr_len = curr_len + entry_size;

						// Set the entry
						dir = (struct ext2_dir_entry_2 *) (length + curr_len);

						dir->inode = inode_num;
						dir->name_len = strlen(name);
						dir->rec_len = new_rec_len;
						dir->file_type = t;

						strncpy(dir->name, name, strlen(name));

						break;

					}


				}

			}

			curr_len = curr_len + dir->rec_len;

		}

		block = block + sizeof(unsigned int);

	}

}

/* Return the inode number of a file with filename 
   inode_num: the inode number of the directory containing the file */
int get_file_inode(unsigned char *disk_img, int inode_num, char *filename) {

	char buf[EXT2_NAME_LEN+1];

	struct ext2_inode *inode = get_inode(disk_img, inode_num);

    // Get the pointer to data blocks
    unsigned int *block = inode->i_block;

    unsigned char *length = (disk_img + (*block)*EXT2_BLOCK_SIZE);

    struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) (length);

    // Check every block
    while(*block) {

        int curr_len = 0;

        while (curr_len < EXT2_BLOCK_SIZE) {

            // Clear the buffer
            strncpy(buf, "\0", EXT2_NAME_LEN+1);

            dir = (struct ext2_dir_entry_2 *) (length + curr_len);

            strncpy(buf, dir->name, dir->name_len);

            // Compare the name
            if (strncmp(filename, buf, dir->name_len) == 0) {

                return dir->inode;

            }

            curr_len = curr_len + dir->rec_len;

        }

        block = block + sizeof(unsigned int);

    }

    // Cannot find the file
    return -1;

}