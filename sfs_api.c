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

#define MaxChar 11
#define BlockSize 1024
#define BlockNumber 1024

//Structures

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

    inode_t jroot; //points to file containing all inodes (jnode)

    inode_t shadow[4];//???
    int lastShadow;

} superblock_t;

typedef struct rootDir_t{ //root directory containing inodes & matching Names
    //needs to be able to hold 200 inodes
    //matches inode number?? with filename
    
    int inodeList[200];//index of inode in inodeList
    unsigned char filenameList[200][MaxChar];
    
} rootDir_t;

typedef struct openFile_t{
    int inodeNum;
    int read;
    int write;
}openFile_t;

//Global Vars

rootDir_t rootDir;
block_t FBM;
openFile_t openFiles[201];
inode_t inodeList[208];



int getFreeBlock();
int getFreeInode();
int writeFreeBlock();
int linkInodePointers(int nblocks, int inodeNum, int inodeSize);


void mkssfs(int fresh){

    if (fresh){//fresh is 1, a new file system must be created
        printf("**MAKING NEW FILE SYSTEM!**\n");
        init_fresh_disk("ssfs_disk",BlockSize,BlockNumber);

    
        for(int i =0; i < BlockNumber; i++){//initialize empty block (all 1)
            FBM.bytes[i] = 1;
            //printf("block %d is: %u\n",i, FBM.bytes[i]);
        }

        for(int i = 0; i < 13; i++){ //takes care inodeList (superblock and FBM are't track by FBM)
          FBM.bytes[i] = 0;
        }
        
        //NOTE: FBM 0 ==> disk_block 2
        //      disk_block = FBM + 2
        
        write_blocks(1, 1, &FBM);//FBM

        //**creating superBlock**
        
        for(int i = 0; i < 200; i++){ //initialize with inodes all of size -1 (empty)
          inode_t inode = {.size = -1};
          inodeList[i] = inode;
        }
        write_blocks(2, 13, &inodeList); //inodelist

        inode_t jnode = {.size = 12800, .direct = {2,3,4,5,6,7,8,9,10,11,12,13,14}};//point to blocks 2-14
        superblock_t superblock = {"ssfs", BlockSize, jnode};
        write_blocks(0, 1, &superblock);//superblock
        
        printf("superblock magic num: %s\n",superblock.magic);
        printf("superblock jnode pointers: %d, %d, %d, %d\n", superblock.jroot.direct[0],superblock.jroot.direct[1],superblock.jroot.direct[2],superblock.jroot.direct[3]);

        
        linkInodePointers(3,getFreeInode(), 3000);//should link free inode's pointers to free FBM blocks and set them to 0 & set inode size

        
        
        for(int i = 0; i < 200; i++){
            rootDir.inodeList[i] = -1;
            strcpy(rootDir.filenameList[i],"-1");
        }
        
        write_blocks(15,3,&rootDir);//rootDir
        
        for(int i = 0; i<201; i++){//initialized fileDescriptors to -1
            openFiles[i].inodeNum = -1;
            openFiles[i].read = -1;
            openFiles[i].write = -1;
            
        }
        
        //printf("size of rootDir: %lu\n", sizeof(rootDir));//400
        //printf("size of inodeList: %lu\n", sizeof(inodeList));//12800

        
        /* QUESTIONS:
         
                      Magic number of superblock?


        */

    }else{//retrieve old file system
        init_disk("ssfs_disk", BlockSize, BlockNumber);
        
        for(int i = 0; i<201; i++){//initialized fileDescriptors to -1
            openFiles[i].inodeNum = -1;
            openFiles[i].read = -1;
            openFiles[i].write = -1;
            
        }
        
        read_blocks(1,1,&FBM);
        read_blocks(15,3,&rootDir);
        read_blocks(2,13,&inodeList);
        
        //testing....
        
        
    }

}

int linkInodePointers(int nblocks, int inodeNum, int inodeSize){//Link free inode's pointers to free FBM blocks and set them to 0 & set size
    //TODO: need to take care of the case where multiple inodes may be needed to point to a file...
    
    inode_t inodeList[208];
    read_blocks(2,13,&inodeList); //needed to get inode number
    
    if(nblocks > 14){//super long entry :/
        return -1;
    }else{
        
        inodeList[inodeNum].size = inodeSize; //set inodes size
        
        for(int i = 0; i < nblocks; i++){
            int freeB = getFreeBlock();
            inodeList[inodeNum].direct[i] = freeB;
            writeFreeBlock(freeB);
        }
        
        return 1;
    }
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


  for(int i =0; i < BlockNumber; i++){
    if (FBM.bytes[i] == 1)
      return i;
  }
return -1;
}

int writeFreeBlock(int free){ //FBM changes availability to 0

    
    if (FBM.bytes[free] == 0){//aready occupied
        printf("TAKEN\n");
        return -1;
    }
    FBM.bytes[free] = 0;
    write_blocks(1,1,&FBM);
    return 1;//successful
}


int ssfs_fopen(char *name){ //index of newsly created/opened entry (disk block NUMBER: can be from 16-1023)
    
    //check if file exists by searching rootDir, if file if found add it to openFile list and return the index its in
    //open file in append mode
    int j =0;
    char *dirName =rootDir.filenameList[j];
    while (strcpm(dirName,name) != 0){
        j++; //TODO: can go out of bound!
        if(j == 201)
            return -1;
        dirName =rootDir.filenameList[j];
    }
    int k =0;
    while(openFiles[k].inodeNum != -1){//look for empty slot in openFileDescriptor
        k++;
        if(k == 201)
            return -1;
    }
    
    if(strcpm(dirName,name) == 0){ //found file with corresponding name, at entry of rootDir j
        openFiles[k].inodeNum = rootDir.inodeList[j];
        openFiles[k].read = 0;
        int inodeSize = inodeList[rootDir.inodeList[j]].size;
        int inodeMod = inodeSize%BlockSize;
        
        openFiles[k].write = inodeMod;
        
        return k;
    }else{
        //if file not found create a new entry (get inode and point it to free block) set size to 0 and add to openFile/return index its in

    }
    
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
