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
int getNumFreeBlocks();
int display(void);

void mkssfs(int fresh){

    if (fresh){//fresh is 1, a new file system must be created
        printf("**MAKING NEW FILE SYSTEM!**\n");
        init_fresh_disk("ssfs_disk",BlockSize,BlockNumber);

        //clear all batablocks to null before starting
        for(int i = 0; i < BlockNumber; i++){
            char blockTemp[1024] = {0};
            write_blocks(i, 1, blockTemp);
        }
    
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
            inode_t inode = {.size = -1, .direct = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, .indirect = -1};
          inodeList[i] = inode;
        }
        write_blocks(2, 13, &inodeList); //inodelist

        inode_t jnode = {.size = 12800, .direct = {2,3,4,5,6,7,8,9,10,11,12,13,14}, .indirect = -1};//point to blocks 2-14
        superblock_t superblock = {"ssfs", BlockSize, jnode};
        write_blocks(0, 1, &superblock);//superblock
        
//        printf("superblock magic num: %s\n",superblock.magic);
//        printf("superblock jnode pointers: %d, %d, %d, %d\n", superblock.jroot.direct[0],superblock.jroot.direct[1],superblock.jroot.direct[2],superblock.jroot.direct[3]);

        
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
        //inodeList[inodeNum].direct[]
        for(int i = 0; i < nblocks; i++){
            int freeB = getFreeBlock(); //get free block
            inodeList[inodeNum].direct[i] = freeB+2;
            writeFreeBlock(freeB);
            printf("^^^NODE POINTS TO BLOCK: %d\n",freeB+2);
//            char blockTemp[1024]= {'\0'};
//            write_blocks(freeB+2,1,blockTemp);
            
            
        }
        write_blocks(2, 13, &inodeList);
        return 1;
    
}


int getFreeInode(){// returns the index of free inode in inodeList
    
    for(int i =0; i < 200; i++){
//        printf("inode[%d] size: %d\n",i, inodeList[i].size);
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

int getNumFreeBlocks(){
    int freeBlocks =0;
    for(int i =0; i < BlockNumber; i++){
        if (FBM.bytes[i] == 1)
            freeBlocks++;
    }
    return freeBlocks;
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
        
        int inodeSize = inodeList[rootDir.inodeList[j]].size;
        //int inodeMod = inodeSize%BlockSize;
        openFiles[k].read = (inodeSize-1);
        openFiles[k].write = (inodeSize-1);//is this correct? YES
        if(inodeSize == 0){
            openFiles[k].read = 0;
            openFiles[k].write = 0;
        }
//        printf("inodeNum is: %d\n", openFiles[k].inodeNum);
        return k;
    }else{
        //if file not found create a new entry (get inode and point it to free block) set size to 0 and add to openFile/return index its in
        printf("FILE NOT FOUND\n");
        //update FBM, rootDir, inodeList
        
        int freeNode = getFreeInode();
//        printf("FREENODE: %d\n", freeNode);
        linkInodePointers(1,freeNode,0);//updated inodeList & FBM
        
        int d = 0;
        while(rootDir.inodeList[d] != -1){
            if(d == 200)
                return -1;
            d++;
        }
        
        rootDir.inodeList[d] = freeNode;
        memcpy(rootDir.filenameList[d],name, strlen(name));
        
        write_blocks(15,3,&rootDir);
//        printf("copied Filename: %s\n",rootDir.filenameList[d] );
//      printf("openFiles[%d]: \n", k);
        openFiles[k].inodeNum = freeNode;
        openFiles[k].read = 0;
        openFiles[k].write = 0;
        
        
        return k;
        
    }
    
}


int ssfs_fclose(int fileID){
    if(fileID >= 200 || fileID<0)//prevent out of bound & not negative
        return -1;
    if (openFiles[fileID].inodeNum == -1)//file not open
        return -1;
    openFiles[fileID].inodeNum = -1;
    openFiles[fileID].read = -1;
    openFiles[fileID].write = -1;
    return 0;
}
int ssfs_frseek(int fileID, int loc){
    if(fileID >= 200 || fileID<0)//prevent out of bound & not negative
        return -1;
    if (openFiles[fileID].inodeNum == -1)//file not open
        return -1;
    if(loc < 0 || loc > inodeList[openFiles[fileID].inodeNum].size-1)
        return -1; //is loc out of bound
    openFiles[fileID].read = loc;
    return 0;
}
int ssfs_fwseek(int fileID, int loc){
    if(fileID >= 200 || fileID<0)//prevent out of bound & not negative
        return -1;
    if (openFiles[fileID].inodeNum == -1)//file not open
        return -1;
    if(loc < 0 || loc > inodeList[openFiles[fileID].inodeNum].size-1)
        return -1; //is loc out of bound
    openFiles[fileID].write = loc;
    return 0;
}

int ssfs_fwrite(int fileID, char *buf, int length){
    if (openFiles[fileID].inodeNum == -1)//file not open
        return -1;

    for(int i = 0; i < 200; i++){
        if(openFiles[fileID].inodeNum == rootDir.inodeList[i])
            printf("File writen to: %s\n",rootDir.filenameList[i]);
    }
    
    int inodeNum = openFiles[fileID].inodeNum;
    int fileSize = inodeList[inodeNum].size;
    //need to check that write pointer is within size?
    printf("IN WRITE BLOCK\n");
    printf("read is at: %d\n", openFiles[fileID].read);
    printf("write is at: %d\n", openFiles[fileID].write);
    printf("size is: %d\n", fileSize);
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
        memcpy(bufCopy, buf, bufAble);
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
            
            inodePointers[i-1] = inodeList[inodeLink].direct[pointer-1];
            
            
            if((nextNode < ceil((i+1)/14)) && i != filePointers){//neeed to change inodes
                nextNode = ceil((i+1)/14);
                inodeLink = inodeList[inodeLink].indirect;
            }
            
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
            char blockTemp[1024] ={0};
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
        
        printf("in EOF BLOCK\n");
        int freeBlocks = getNumFreeBlocks();
        int spaceAtLastBlock = fileSize % 1024;
        int newBlocksNeeded = ceil((length-spaceAtLastBlock)/1024);
//        printf("new blocks needed: %d\n", newBlocksNeeded);
//        printf("free blocks: %d\n", freeBlocks);
        if (freeBlocks < newBlocksNeeded)
            return -1; //too big, doesnt have enough space to write
        
        int fileBlocks = ceil(fileSize/1024); //blocks occupied by file as is
        if(fileSize == 0)
            fileBlocks = 1;
//        printf("blocks occupied by file: %d\n", fileBlocks);
        int inodePointers[fileBlocks + newBlocksNeeded];//will contain pointers to newblocks and write block

        
        int inodeLink = inodeNum;
        int nextNode =1;
        
        for(int i = 1; i <= (fileBlocks + newBlocksNeeded); i++){//need to fill up inodepointer Array
            //int nextInode = ceil(i/14); //indicates what inode of the file we are on
            int pointer = i%14;//indicated what pointer of the inode we are on 1-14
            if(i%14 == 0)
                pointer = 14;
            printf("*****inodePointers Loop!!!!!!!\n");
            
            if(inodeList[inodeLink].direct[pointer-1] == -1){
                printf("need to get another Inode!!!\n");
                inodeList[inodeLink].direct[pointer-1] = getFreeBlock()+2;
                printf("block pointed to: %d\n", getFreeBlock()+2);
                writeFreeBlock(getFreeBlock());
                
            }
//            printf("block in inodePointer: %d\n",inodeList[inodeLink].direct[pointer-1]);
            //printf("~=====Inode pointer points to: %d\n", inodeList[inodeLink].direct[pointer-1]);
            printf("\nPOINTER IS: %d\n", pointer);
            inodePointers[i-1] = inodeList[inodeLink].direct[pointer-1];
            if((nextNode < ceil((i+1)/14)) && i != (fileBlocks + newBlocksNeeded)){//neeed to change inodes
                nextNode = ceil((i+1)/14);
                if(inodeList[inodeLink].indirect != -1){
                    inodeLink = inodeList[inodeLink].indirect; //already a working node linked to it :)
                }else{
                    int freeIN = getFreeInode();
                    if (freeIN == -1)
                        return -1;
                    inodeList[inodeLink].indirect =freeIN;
                    
                    
                    linkInodePointers(1, freeIN, 0);//dont care about linked inode size
                }
                    
            }
            
            
        }
        printf("blockPOINTER ARRAY: [%d]=%d , [%d]=%d\n",0,inodeList[inodeLink].direct[0], 1, inodeList[inodeLink].direct[1]  );
        int writeblock = (openFiles[fileID].write)/1024;//block write is in
        int writeLoc = (openFiles[fileID].write+1)%1024;//location to start writing
        if(fileSize == 0)
            writeLoc =0;

        int start = 0;
        int stop = 1023 - writeLoc; //could be 0 if only one byte is being written
//        printf("stop here is: %d\n", stop);
        
        char bufCopy[length];
        memcpy(bufCopy, buf, length);
        
        if(length < stop+1){ //if buffer doesnt need all that space to write
            //printf("........THIS IS EXECUTING.......");
            stop = length-1;
        }
//        printf("stop here is: %d\n", stop);
        if(writeLoc != 0){//there is space to fill at the end of block
//            printf("iin write LOOP\n");
            int byteNum = stop +1;
            char blockTemp[1024] = {0};
            read_blocks(inodePointers[writeblock], 1, blockTemp);
//            printf("bufCopy is: %s\n", bufCopy);
            for(int i = 0; i < byteNum; i++){
//                printf("[%d] ",writeLoc+i);
//                printf("bufCopy: %c\n",bufCopy[start+i]);
                blockTemp[writeLoc+i]=bufCopy[start+i]; //will this work?
//                printf("bufCopy: %c\n",blockTemp[writeLoc+i]);
            }
            printf(">***writing block back into mem\n");
            write_blocks(inodePointers[writeblock], 1, blockTemp);
            printf("entry is: %s\n", blockTemp);
            if (stop == length - 1)
                return length;
            
            start = stop + 1;
            
            if(length-1-stop < 1024){//whats left wont fill whole block, ie in last block
                stop = length-1;
            }else {
                stop = stop + 1024;
            }
            writeblock++;//next block to be read and modified
            
        }
        
        while(start != length){
            char blockTemp[1024] = {'\0'};
            int byteNum = stop - start +1;
            printf("\nWRITEBLOCK IS: %d\n",writeblock);
            printf("\<<<>>>>inodePointer points to: %d\n",inodePointers[writeblock]);
            read_blocks(inodePointers[writeblock], 1, blockTemp);
            printf("\n.....Fetched block is: %s\n", blockTemp);
//            printf("entry is: %s\n", blockTemp);
            //blockTemp[6] = '\0';
//            printf("entry isC: %c\n", blockTemp[6]);
            for(int i = 0; i < byteNum; i++){
                blockTemp[writeLoc+i]=bufCopy[start+i]; //will this work?
//                printf("blockTemp[%d] <== %c\n",writeLoc+i,bufCopy[start+i]);
            }
            printf("while loop byteNUM:> %d\n", byteNum);
            printf("****writing block back into mem\n");
            write_blocks(inodePointers[writeblock], 1, blockTemp);
            
            printf("OVERWRITTEN BLOCK IS: %s\n", blockTemp);
//            printf("entry is: %s\n", blockTemp);
//            printf("entry is: %c\n", blockTemp[5]);
//            printf("entry is: %c\n", blockTemp[6]);
            
            //testing.... trying to fetch back block
            char testblock[1024] = {0};
//            printf("InodePOINTER IS: %d\n",inodePointers[writeblock]);
            read_blocks(inodePointers[writeblock], 1,testblock);
//            printf("entry is: %s\n", testblock);
//            printf("entry is: %c\n", testblock[5]);
//            printf("entry is: %c\n", testblock[6]);
            start = stop + 1;
            openFiles[fileID].write = stop;//updates write pointer to point at the end
            openFiles[fileID].read = stop;//updates where the read pointer should be
            
            if(length-1-stop < 1024){//whats left wont fill whole block, ie in last block
                stop = length-1;
            }else {
                stop = stop + 1024;
            }
            writeblock++;//next block to be read and modified
        }
        
        
        inodeList[inodeNum].size = inodeList[inodeNum].size + length;
        
        //write back into mem
        //write_blocks();
        return length;
    }
    

    
    return -1;
}
int ssfs_fread(int fileID, char *buf, int length){
    
    if(fileID >= 200 || fileID<0)//prevent out of bound & not negative
        return -1;
    if (openFiles[fileID].inodeNum == -1)//file not open
        return -1;
    int readLength = openFiles[fileID].read + 1;
    //printf("size: %d\n", inodeList[openFiles[fileID].inodeNum].size);
    //printf("read: %d\n", openFiles[fileID].read);
    //printf("readLength: %d\n", readLength);
    if(readLength > length)
        return -1; //length too long
    //need to get inodePointers
    //printf("READING...\n");
    int blocksNeeded = (int)ceil((double)length/1024);//# of blocks needed from 0 to hopefully read pointer
    if(length == 0)
        blocksNeeded = 1;
    int inodePointers[blocksNeeded];
    
    int inodeNum = openFiles[fileID].inodeNum;
    int inodeLink = inodeNum;
    int nextNode =1;
    //printf("Length: %d\n", (int)ceil((double)length/1024));
    for(int i = 1; i <= blocksNeeded; i++){//need to fill up inodepointer Array
        int nextInode = ceil(i/14); //indicates what inode of the file we are on
        int pointer = i%14;//indicated what pointer of the inode we are on 1-14
        if(i%14 == 0)
            pointer = 14;
      //  printf("MAKING INODE HERE!!!!!\n");
      //  printf("inodePointers[%d] <= %d\n",i-1 , inodeList[inodeLink].direct[pointer-1]);
      //  printf("^INODE is: %d\n", inodeLink);
      //  printf("^pointer NUM is: %d\n", pointer);
        inodePointers[i-1] = inodeList[inodeLink].direct[pointer-1];
        
        
        if((nextNode < ceil((i+1)/14)) && i != blocksNeeded){//neeed to change inodes
            nextNode = ceil((i+1)/14);
            inodeLink = inodeList[inodeLink].indirect;
        }
        
        
    }
    
    char blockTemp[1024] = {0};
    char readBuf[length+1];
    for(int i = 0; i <= length; i++){
        readBuf[i] = '\0';
    }
    
    //int readblock = (openFiles[fileID].read)/1024; //block read is in
    //int readLoc = (openFiles[fileID].read)%1024;//location to end reading
    //int endRead = readLoc;//# of bytes to read in last tempblock
    int startingBlock =0;
    //if(length < endRead+1)
        //endRead = length - 1;
    
    //int start = 0;

    //printf("readblock: %d\n", readblock);

    int i = 0;
    //int readIndex = (readLoc+i)%1024;
    int readIndex = 0;
    //char blockTemp[1024];
    read_blocks(inodePointers[startingBlock], 1, blockTemp);
    while( (i) != length){//copying byte by byte

        
        
     
        readBuf[i] = blockTemp[readIndex];
   
        i++;
        
        readIndex = (readIndex+1)%1024;
        if(readIndex == 0){
            startingBlock++;
            read_blocks(startingBlock, 1, blockTemp);
        }
        
    }
    strcpy(buf, readBuf);
    openFiles[fileID].read= inodeList[inodeNum].size -1;
    if (inodeList[inodeNum].size == 0)
        openFiles[fileID].read= 0 ;
    return length;
}
int ssfs_remove(char *file){//remove inode, rootDir reference, and openFile if open
    int j =0;
    char *dirName = rootDir.filenameList[j];
    while (strcmp(dirName,file) != 0){
        j++;
        if(j == 201)
            return -1;//file name not found
        dirName =rootDir.filenameList[j];
    }
    
    int inodeNum = rootDir.inodeList[j];
    for(int i = 0; i < 200; i++){ //close file if one was opened
        if(openFiles[i].inodeNum == inodeNum){
            openFiles[i].inodeNum = -1;
            openFiles[i].read = -1;
            openFiles[i].write = -1;
        }
    }
    
    rootDir.inodeList[j] = -1;
    strcpy(rootDir.filenameList[j],"-1");
    write_blocks(15,3,&rootDir);//write back to mem rootDir
    
    inode_t inode = {.size = -1, .direct = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, .indirect = -1};
    inodeList[inodeNum] = inode;
    write_blocks(2, 13, &inodeList);
    
    return 0;
}

int display(void){
    printf("read pointer of inode 0: %d\n",openFiles[0].read);
    printf("\trootDir: \n");
    for(int i =0; i < 200; i++){
        if(rootDir.inodeList[i] != -1)
            printf("\t\t%s\n",rootDir.filenameList[i]);
    }
    printf("\n\tOpened Files: \n");
    for(int i =0; i < 200; i++){
        
        if(openFiles[i].inodeNum != -1){
            char *fileText[20] = {0};
            ssfs_fread(i, fileText, 20);
            
            for(int k =0; k < 200; k++){
                if(rootDir.inodeList[k] == openFiles[i].inodeNum)
                    printf("\t\t[%d] %s\tr:%d  w:%d   ",i, rootDir.filenameList[i] ,openFiles[i].read, openFiles[i].write);
            }
            printf("\t: %s\n",fileText);
        }
    }
    return 1;
}


int main(void){
    mkssfs(1);
    ssfs_fopen("textfile");
    display();
    ssfs_fopen("textme");
    display();
    ssfs_fopen("texte");
    display();
    char *sentence = "...>>>>The ssfs_fopen() opens a file and returns an integer that corresponds to the index of the entry for the newly opened file in the file descriptor table. If the file does not exist, it creates a new file and sets its size to 0. If the file exists, the file is opened in append mode (i.e., set the write file pointer to the end of the file and read at the beginning of the file). The ssfs_fclose() closes a file, i.e., removes the entry from the open file descriptor table. On success, ssfs_fclose() should return 0 and a negative value otherwise. The ssfs_fwrite() writes the given number of bytes of buffered data in buf into the open file, starting from the current write file pointer. This in effect could increase the size of the file by the given number of bytes (it may not increase the file size by the number of bytes written if the write pointer is located at a location other than the end of the file). The ssfs_fwrite() should return the number of bytes written. The ssfs_fread() follows a similar behavior. @@@ ssfs_rfseek() moves the read pointer and ssfs_wfseek() moves the write pointer to the given location. It returns 0 on success and a &*&*&*&*&*&*&";
    int len = ssfs_fwrite(1,sentence,strlen(sentence));
    printf("length: %d\n",len);
    char *buffer[20] = {0};
    printf("BUFFER IS NOW: %s\n",buffer);
    ssfs_fread(1,buffer,20);
    printf("BUFFER IS NOW: %s\n",buffer);
    display();

}


