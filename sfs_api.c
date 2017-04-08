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
#define BlockNumber 1024

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

typedef struct rootDir_t{ //root directory containing inodes & matching Names
    //needs to be able to hold 200 inodes
    //matches inode number?? with filename
    
    int inodeList[200];//index of inode in inodeList
    unsigned char filenameList[200][MaxChar];
    
} rootDir_t;

int getFreeBlock();
int getFreeInode();
int writeFreeBlock();
int linkInodePointers(int nblocks, int inodeNum, int inodeSize);


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
        
        //NOTE: FBM 0 ==> disk_block 2
        //      disk_block = FBM + 2
        
        write_blocks(1, 1, &Fbm);//FBM

        //**creating superBlock**
        inode_t inodeList[200]; //maintain a list of inodes in mem
        for(int i = 0; i < 200; i++){ //initialize with inodes all of size -1 (empty)
          inode_t inode = {.size = -1};
          inodeList[i] = inode;
        }
        write_blocks(2, 13, &inodeList); //inodelist

        inode_t jnode = {.size = 12800, .direct = {2,3,4,5,6,7,8,9,10,11,12,13,14}};//point to blocks 2-14
        superblock_t superblock = {"ssfs", BlockSize, jnode};
        write_blocks(0, 1, &superblock);//superblock
        
        printf("superblock magic num: %s\n",superblock.magic);
        printf("superblock jnode pointers: %d, %d, %d, %d\n", superblock.root.direct[0],superblock.root.direct[1],superblock.root.direct[2],superblock.root.direct[3]);

        
        
        linkInodePointers(3,getFreeInode(), 2800);//should link free inode's pointers to free FBM blocks and set them to 0 & set inode size
        //FBM should allocate 13, 14, 15 to rootDir
        //0-12 is already allocated to inodeList
        //superB & FBM arent shown in FBM
        
        rootDir_t rootDir = {.inodeList[100] = {-1}, .filenameList = {{"Hey"}} };//TODO: need to initialize properly....
        
        write_blocks(15,3,&rootDir);//rootDir
        
        printf("size of rootDir: %lu\n", sizeof(rootDir));//400
        printf("size of inodeList: %lu\n", sizeof(inodeList));//12800
        printf("rootDir int: %d\n",rootDir.inodeList[101]);
        printf("rootDir name: %s\n",rootDir.filenameList[100]);
        



        /* QUESTIONS:
                      What is size of inode?
                      Magic number of superblock?


        */

    }else{//retrieve old file system
        init_disk("ssfs_disk", BlockSize, BlockNumber);
        
        //testing...
        block_t fbm;
        printf("fbm %d is: %u\n",1, fbm.bytes[1]);
        read_blocks(1,1,&fbm);
        printf("fbm %d is: %u\n",1, fbm.bytes[1]);
    }

}

int linkInodePointers(int nblocks, int inodeNum, int inodeSize){//Link free inode's pointers to free FBM blocks and set them to 0 & set size
    //TODO: need to take care of the case where multiple inodes may be needed to point to a file...
    
    inode_t inodeList[208];
    read_blocks(2,13,&inodeList); //needed to get inode number
    
    return -1;
}


int getFreeInode(){// returns the index of free inode in inodeList
    inode_t inodeList[208];
    read_blocks(2,13,&inodeList);
    for(int i =0; i < 200; i++){
        if(inodeList[i].size == -1){
            return i;
        }
    }
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

int writeFreeBlock(int free){ //FBM changes availability to 0
    block_t fbm;
    //printf("fbm %d is: %u\n",1, read.bytes[1]);
    read_blocks(1,1,&fbm);
    
    if (fbm.bytes[free] == 0){//aready occupied
        printf("TAKEN\n");
        return -1;
    }
    fbm.bytes[free] = 0;
    write_blocks(1,1,&fbm);
    
    return 1;//successful
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
  mkssfs(1);


}
