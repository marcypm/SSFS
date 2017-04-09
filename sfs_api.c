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
#include <math.h>

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
inode_t inodeList[208];

openFile_t openFiles[201];


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
    
        inodeList[inodeNum].size = inodeSize; //set inodes size
        
        for(int i = 0; i < nblocks; i++){
            int freeB = getFreeBlock();
            inodeList[inodeNum].direct[i] = freeB;
            writeFreeBlock(freeB);
        }
        write_blocks(2, 13, &inodeList);
        return 1;
    
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
    while (strcmp(dirName,name) != 0){
        j++;
        if(j == 200)
            break;
        dirName =rootDir.filenameList[j];
    }
    
    //TODO: if rootDir.inodeList[j] is already in an openFiles[X].inodeNum exit
    
    int k =0;
    while(openFiles[k].inodeNum != -1){//look for empty slot in openFileDescriptor
        k++;
        if(k == 200)
            break;
    }
    if(strcmp(dirName,name) == 0){ //found file with corresponding name, at entry of rootDir j
        printf("FILE FOUND!\n");
        openFiles[k].inodeNum = rootDir.inodeList[j];
        openFiles[k].read = 0;
        int inodeSize = inodeList[rootDir.inodeList[j]].size;
        //int inodeMod = inodeSize%BlockSize;
        
        openFiles[k].write = (inodeSize-1);//is this correct?
        
        printf("inodeNum is: %d\n", openFiles[k].inodeNum);
        printf("read is: %d\n", openFiles[k].read);
        return k;
    }else{
        //if file not found create a new entry (get inode and point it to free block) set size to 0 and add to openFile/return index its in
        printf("FILE NOT FOUND\n");
        //update FBM, rootDir, inodeList
        int freeNode = getFreeInode();
        linkInodePointers(1,freeNode,0);//updated inodeList & FBM
        
        int d = 0;
        while(rootDir.inodeList[d] != -1){
            if(d == 200)
                return -1;
            d++;
        }
        
        rootDir.inodeList[d] = freeNode;
        strcpy(rootDir.filenameList[d],name);
        
        write_blocks(15,3,&rootDir);
        
        openFiles[k].inodeNum = freeNode;
        openFiles[k].read = 0;
        openFiles[k].write = 0;
        return k;
        
    }
    
}


int ssfs_fclose(int fileID){
    if(fileID >= 200 || fileID<0)//prevent out of bound & not negative
        return -1;
    openFiles[fileID].inodeNum = -1;
    openFiles[fileID].read = -1;
    openFiles[fileID].write = -1;
    return 0;
}
int ssfs_frseek(int fileID, int loc){
    if(fileID >= 200 || fileID<0)//prevent out of bound & not negative
        return -1;
    //is loc out of bound?
    openFiles[fileID].read = loc;
    return 0;
}
int ssfs_fwseek(int fileID, int loc){
    if(fileID >= 200 || fileID<0)//prevent out of bound & not negative
        return -1;
    //is loc out of bound?
    openFiles[fileID].write = loc;
    return 0;
}
int ssfs_fwrite(int fileID, char *buf, int length){
    if (openFiles[fileID].inodeNum == -1)//file not open
        return -1;

    int inodeNum = openFiles[fileID].inodeNum;
    int fileSize = inodeList[inodeNum].size;
    //need to check that write pointer is within size?
    
    if((openFiles[fileID].write != (fileSize-1)) && ((fileSize)>1) ){//write_p is not at EOF
        //printf("Here\n");
        int filePointers = fileSize/1024;
        if(fileSize%1024 != 0)
            filePointers++;//# of inodes for file
        int bufAble;
        int writeable = fileSize - openFiles[fileID].write -1;//amount of writeable bytes

        if(length > writeable){//tempBuf size needs to be writeabe
            bufAble = writeable;
        }else{
            bufAble = length;
        }
        char bufCopy[bufAble];
        strncpy(bufCopy, buf, bufAble);
        //make a list of length (inodesNeeded) and store the index of all inode pointers
        //then find where the write pointer is at use that as a starting write location
        
        int inodePointers[filePointers];
        
            int inodeLink = inodeNum;
            int nextNode =1;
        
        for(int i = 1; i <= filePointers; i++){//need to fill up inodepointer Array
            int nextInode = ceil(i/14); //indicates what inode of the file we are on
            int pointer = i%14;//indicated what pointer of the inode we are on 1-14
            if(i%14 == 0)
                pointer = 14;
            
            if(nextNode < nextInode){//neeed to change inodes
                nextNode = nextInode;
                inodeLink = inodeList[inodeLink].indirect;
            }
            
            inodePointers[i-1] = inodeList[inodeLink].direct[pointer];
        }
        
        //what index is the write pointer at?
        int writeblock = (openFiles[fileID].write)/1024;//block the writer is at
        int writeLoc = (openFiles[fileID].write+1)%1024;//where you will start writing
        int start = 0;
        int stop = 1023 - writeLoc; //could be 0 if only one byte is being written in
        if(bufAble < stop+1)
            stop = bufAble-1;
        
        if(writeLoc != 0){//there is space to fill at the end of block
            int byteNum = stop +1;
            char blockTemp[1024];
            read_blocks(inodePointers[writeblock], 1, blockTemp);
            for(int i = 0; i < byteNum; i++){
                blockTemp[writeLoc+i]=bufCopy[start+i]; //will this work?
            }
            write_blocks(inodePointers[writeblock], 1, blockTemp);
            if (stop == bufAble - 1)
                return bufAble;
            
            start = stop + 1;
            
            if(bufAble-1-stop < 1024){//whats left wont fill whole block, ie in last block
                stop = bufAble-1;
            }else {
               stop = stop + 1024;
            }
            writeblock++;//next block to be read and modified
            
        }
        
        while(start != (bufAble)){//end condition is when start is at end of bufCopy
            char blockTemp[1024];
            int byteNum = stop - start +1;
            read_blocks(inodePointers[writeblock], 1, blockTemp);
            for(int i = 0; i < byteNum; i++){
                blockTemp[writeLoc+i]=bufCopy[start+i]; //will this work?
            }
            write_blocks(inodePointers[writeblock], 1, blockTemp);
            
            start = stop + 1;
        
            if(bufAble-1-stop < 1024){//whats left wont fill whole block, ie in last block
                stop = bufAble-1;
            }else {
                stop = stop + 1024;
            }
            writeblock++;//next block to be read and modified
            
        }
        
        
        return bufAble;
        
    }else {//write_p is at EOF. file can get bigger
        //how many blocks is the buffer gonna take?
        //do we have enough space? (if not return -1)
        //will this require multiple inodes?
        //do we have enough inodes? <-- probably
        
    }
    

    
    return 0;
}
int ssfs_fread(int fileID, char *buf, int length){
    if (openFiles[fileID].inodeNum == -1)//file not open
        return -1;
    
    
    return 0;
}
int ssfs_remove(char *file){//remove inode, rootDir reference, and openFile if open
    return 0;
}


int main(void){
    mkssfs(1);
    ssfs_fopen("textfile");
    ssfs_fopen("textme");
    ssfs_fopen("texte");
    ssfs_fwrite(1, "xxaxabcedfghi", 13);

}
