/*
 Marcel Morin - 260605670
 ECSE 427 Operating Systems
 Assignment 3 - Simple Shadow File System
 April 9th, 2017
 */

#include "sfs_api.h"
#include "disk_emu.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>

#define MaxChar 10
#define BlockSize 1024
#define BlockNumber 200

//Global Vars & Structures

typedef struct inode_t{
    int size;//if size = -1, then inode is empty, otherwise
    int direct[14];//points to the blocks where data is stored
    int indirect;//points to another inode if more blocks are needed

} inode_t;

typedef struct block_t{
    unsigned char bytes[BlockSize];
} block_t;

typedef struct superblock_t{
    unsigned char magic[4];
    int bSize;

    inode_t root; //points to file containing all inodes (jnode)

    inode_t shadow[4];//???
    int lastShadow;

} superblock_t;

int getFreeBlock();
int getFreeInode();


void mkssfs(int fresh){

    if (fresh){//fresh is 1, a new file system must be created
        printf("**MAKING NEW FILE SYSTEM!**\n");
        init_fresh_disk("ssfs_disk",BlockSize,BlockNumber);

        block_t Fbm;
        for(int i =0; i < BlockNumber; i++){//initialize empty block (all 1)
            Fbm.bytes[i] = 1;
            //printf("block %d is: %u\n",i, Fbm.bytes[i]);
        }


        for(int i = 0; i < 13; i++){ //takes care inodeList (superblock and FBM are't track by FBM)
          Fbm.bytes[i] = 0;
        }

        write_blocks(1, 1, &Fbm);

        //**creating superBlock**
        // need to
        inode_t inodeList[200]; //maintain a list of inodes in mem
        for(int i = 0; i < BlockNumber; i++){ //initialize with inodes all of size -1 (empty)
          inode_t inode = {.size = -1};
          inodeList[i] = inode;
        }
        inode_t inode = {.size = 98};
        inodeList[0] = inode;
        //write this list into mem
        write_blocks(2, 13, &inodeList);

        inode_t jnode = {.size = 12800, .direct = {2,3,4,5,6,7,8,9,10,11,12,13,14}};//point to blocks 2-14
        printf("1st inode size: %d\n", inodeList[0].size);
        superblock_t superblock = {"ssfs", BlockSize, jnode};
        printf("superblock magic num: %s\n",superblock.magic);
        printf("superblock jnode pointers: %d, %d, %d, %d\n", superblock.root.direct[0],superblock.root.direct[1],superblock.root.direct[2],superblock.root.direct[3]);

        write_blocks(0, 1, &superblock);

        int freeblock = getFreeBlock();
        printf("Free block at: %d\n", freeblock);

        printf("retrieveing superblock...\n");
        superblock_t tempsuper;
        read_blocks(0,1,&tempsuper);
        printf("tempsuper magic num: %s\n",tempsuper.magic);

        inode_t newJnode = tempsuper.root;
        inode_t newInodeList[200];
        read_blocks(2, 13, &newInodeList);
        printf("new 1st inode size: %d\n", newInodeList[0].size);
        //print
        //once superblock, FBM, inodeList are in mem i need to create a rootDir
        //rootDir will match inodes and names together
        //rootDir is pointed by inode in inodeList
        //rootDir will take up about 4blocks
        //get free inode from list (using function) and change its size to ?____?


        //Need to make freeblock function
        //free inode function



        /* QUESTIONS:
                      What is size of inode?
                      Magic number of superblock?
                      Do i need to worry about shadow nodes?
                      How to make the rootDir?


        */

    }else{//retrieve old file system
        init_disk("ssfs_disk", BlockSize, BlockNumber);
        block_t fbm;
        printf("fbm %d is: %u\n",1, fbm.bytes[1]);
        read_blocks(1,1,&fbm);
        printf("fbm %d is: %u\n",1, fbm.bytes[1]);
    }

}

int getFreeInode(){

  return -1;
}

int getFreeBlock(){ //returns the index of a free block
  block_t fbm;
  //printf("fbm %d is: %u\n",1, read.bytes[1]);
  read_blocks(1,1,&fbm);

  for(int i =0; i < BlockNumber; i++){
    if (fbm.bytes[i] == 1)
      return i;
  }
return -1;
}

int ssfs_fopen(char *name){
    return 0;
}
int ssfs_fclose(int fileID){
    return 0;
}
int ssfs_frseek(int fileID, int loc){
    return 0;
}
int ssfs_fwseek(int fileID, int loc){
    return 0;
}
int ssfs_fwrite(int fileID, char *buf, int length){
    return 0;
}
int ssfs_fread(int fileID, char *buf, int length){
    return 0;
}
int ssfs_remove(char *file){
    return 0;
}


int main(void){
  printf("MAKING SSFS\n");
  mkssfs(1);


}
