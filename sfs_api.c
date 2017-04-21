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

openFile_t openFiles[222];


int getFreeBlock();
int getFreeInode();
int writeFreeBlock();
int linkInodePointers(int nblocks, int inodeNum, int inodeSize);
int getNumFreeBlocks();
int display(int display);
int disp = 1; //Change to bring up file system display everytime a file is writen to or added to system

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



    }

}

int linkInodePointers(int nblocks, int inodeNum, int inodeSize){//Link free inode's pointers to free FBM blocks and set them to 0 & set size

        inodeList[inodeNum].size = inodeSize; //set inodes size
        for(int i = 0; i < nblocks; i++){
            int freeB = getFreeBlock(); //get free block
            inodeList[inodeNum].direct[i] = freeB+2;
            writeFreeBlock(freeB);



        }
        write_blocks(2, 13, &inodeList);
        return 1;

}


int getFreeInode(){// returns the index of free inode in inodeList

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
        if(j == 200){
            j= 199;
            break;
        }
        dirName =rootDir.filenameList[j];
    }


    int k =0;
    while(openFiles[k].inodeNum != -1){//look for empty slot in openFileDescriptor
        k++;
        if(k == 200){
            k =199;
            break;
        }
    }

    if(strcmp(dirName,name) == 0){ //found file with corresponding name, at entry of rootDir j
        openFiles[k].inodeNum = rootDir.inodeList[j];

        int inodeSize = inodeList[rootDir.inodeList[j]].size;
        openFiles[k].read = (inodeSize-1);
        openFiles[k].write = (inodeSize-1);//is this correct? YES
        if(inodeSize == 0){
            openFiles[k].read = 0;
            openFiles[k].write = 0;
        }

        display(disp);
        return k;
    }else{
        //if file not found create a new entry (get inode and point it to free block) set size to 0 and add to openFile/return index its in
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
        memcpy(rootDir.filenameList[d],name, strlen(name));

        write_blocks(15,3,&rootDir);

        openFiles[k].inodeNum = freeNode;
        openFiles[k].read = 0;
        openFiles[k].write = 0;

        display(disp);
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
    display(disp);
    return 0;
}
int ssfs_frseek(int fileID, int loc){
    if(fileID >= 200 || fileID<0)//prevent out of bound & not negative
        return -1;
    if (openFiles[fileID].inodeNum == -1)//file not open
        return -1;
    if(loc < 0 || loc > inodeList[openFiles[fileID].inodeNum].size)
        return -1; //is loc out of bound
    openFiles[fileID].read = loc-1;
    display(disp);
    return 0;
}
int ssfs_fwseek(int fileID, int loc){
    if(fileID >= 200 || fileID<0)//prevent out of bound & not negative
        return -1;
    if (openFiles[fileID].inodeNum == -1)//file not open
        return -1;
    if(loc < 0 || loc > inodeList[openFiles[fileID].inodeNum].size)
        return -1; //is loc out of bound
    openFiles[fileID].write = loc-1;
    display(disp);
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
    int newFileSize = openFiles[fileID].write + 1 + length;

    int newSize = openFiles[fileID].write + 1 + length;




        int freeBlocks = getNumFreeBlocks();
        int spaceAtLastBlock = 1024-((openFiles[fileID].write+1) % 1024);

        int BlocksNeeded = ceil((length-spaceAtLastBlock)/1024); //blocks needed after space taken up in last block
        int writePointerBlock = openFiles[fileID].write/1024;
        int newBlocksNeeded = writePointerBlock + BlocksNeeded;
        int currentBlocksTaken = ((int)ceil((double)fileSize/1024)); //current blocks taken up by system

        if (newBlocksNeeded <= (currentBlocksTaken-writePointerBlock))
            newBlocksNeeded = 0;
        else
            newBlocksNeeded = newBlocksNeeded- (currentBlocksTaken-writePointerBlock);

        if (freeBlocks < newBlocksNeeded)
            return -1; //too big, doesnt have enough space to write

        int fileBlocks = ceil((double)fileSize/1024); //blocks occupied by file as is
        if(fileSize == 0)
            fileBlocks = 1;
        int inodePointers[fileBlocks + newBlocksNeeded];//will contain pointers to newblocks and write block


        int inodeLink = inodeNum;
        int nextNode =1;

        for(int i = 1; i <= (fileBlocks + newBlocksNeeded); i++){//need to fill up inodepointer Array
            int pointer = i%14;//indicated what pointer of the inode we are on 1-14
            if(i%14 == 0)
                pointer = 14;

            if(inodeList[inodeLink].direct[pointer-1] == -1){

                inodeList[inodeLink].direct[pointer-1] = getFreeBlock()+2;

                writeFreeBlock(getFreeBlock());

            }

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
        int writeblock = (openFiles[fileID].write)/1024;//block write is in
        int writeLoc = (openFiles[fileID].write+1)%1024;//location to start writing
        if(fileSize == 0)
            writeLoc =0;

        int start = 0;
        int stop = 1023 - writeLoc; //could be 0 if only one byte is being written

        char bufCopy[length];
        memcpy(bufCopy, buf, length);

        if(length < stop+1){ //if buffer doesnt need all that space to write
            stop = length-1;
        }
        if(writeLoc != 0){//there is space to fill at the end of block
            int byteNum = stop +1;
            char blockTemp[1024] = {0};
            read_blocks(inodePointers[writeblock], 1, blockTemp);
            for(int i = 0; i < byteNum; i++){
                blockTemp[writeLoc+i]=bufCopy[start+i]; //will this work?
            }
            write_blocks(inodePointers[writeblock], 1, blockTemp);

            if (stop == length - 1){
                openFiles[fileID].write = newFileSize-1;//updates write pointer to point at the end
                openFiles[fileID].read = newFileSize-1;//updates where the read pointer should be
                inodeList[inodeNum].size = newFileSize;
                display(disp);
                return length;
            }
            start = stop + 1;

            if(length-1-stop < 1024){//whats left wont fill whole block, ie in last block
                stop = length-1;
            }else {
                stop = stop + 1024;
            }
            writeblock++;//next block to be read and modified

        }

        while(start != length){
            char blockTemp[1024];
            int byteNum = stop - start +1;

            read_blocks(inodePointers[writeblock], 1, blockTemp);

            for(int i = 0; i < byteNum; i++){
                blockTemp[writeLoc+i]=bufCopy[start+i];
            }

            write_blocks(inodePointers[writeblock], 1, blockTemp);


            char testblock[1024] = {0};
            read_blocks(inodePointers[writeblock], 1,testblock);

            start = stop + 1;
            openFiles[fileID].write = newFileSize-1;//updates write pointer to point at the end
            openFiles[fileID].read = newFileSize-1;//updates where the read pointer should be

            if(length-1-stop < 1024){//whats left wont fill whole block, ie in last block
                stop = length-1;
            }else {
                stop = stop + 1024;
            }
            writeblock++;//next block to be read and modified
        }


        inodeList[inodeNum].size = newFileSize;

        display(disp);
        return length;





}
int ssfs_fread(int fileID, char *buf, int length){
    if(fileID >= 200 || fileID<0)//prevent out of bound & not negative
        return -1;
    if (openFiles[fileID].inodeNum == -1)//file not open
        return -1;
    int readLength = openFiles[fileID].read + 1;
    int blocksNeeded = (int)ceil((double)length/1024);//# of blocks needed from 0 to hopefully read pointer
    if(length == 0)
        blocksNeeded = 1;
    int inodePointers[blocksNeeded];

    int inodeNum = openFiles[fileID].inodeNum;
    int inodeLink = inodeNum;
    int nextNode =1;
    for(int i = 1; i <= blocksNeeded; i++){//need to fill up inodepointer Array
        int nextInode = ceil(i/14); //indicates what inode of the file we are on
        int pointer = i%14;//indicated what pointer of the inode we are on 1-14
        if(i%14 == 0)
            pointer = 14;

        inodePointers[i-1] = inodeList[inodeLink].direct[pointer-1];


        if((nextNode < ceil((i+1)/14)) && i != blocksNeeded){//neeed to change inodes
            nextNode = ceil((i+1)/14);
            inodeLink = inodeList[inodeLink].indirect;
        }


    }

    char blockTemp[1024];
    char readBuf[length+1];
//    for(int i = 0; i <= length; i++){
//        //readBuf[i] = '\0';
//    }

    int startingBlock =0;

    int i = 0;
    int readIndex = 0;
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

int display(int display){
    if(display == 1){
    printf("---------------------------------------------------\n\n");
    printf("\trootDir: \n");
    for(int i =0; i < 200; i++){
        if(rootDir.inodeList[i] != -1)
            printf("\t\t%s\n",rootDir.filenameList[i]);
    }
    printf("\n\tOpened Files: \n");
    for(int i =0; i < 200; i++){

        if(openFiles[i].inodeNum != -1){
            char *fileText[1000];
            ssfs_fread(i, fileText, 1000);

            for(int k =0; k < 200; k++){
                if(rootDir.inodeList[k] == openFiles[i].inodeNum)
                    printf("\t\t[%d] %s\tr:%d  w:%d   ",i, rootDir.filenameList[i] ,openFiles[i].read, openFiles[i].write);
            }
            printf("\t: %s\n",fileText);
        }
    }
    }
    return 1;
}

/*
int main(void){
    mkssfs(1);
    ssfs_fopen("textfile");
    //display(1);
    ssfs_fopen("textme");
   // display(1);
    ssfs_fopen("texte");
    //display(1);
    char *sentence = "Graphic Card is life. I need a 1080 for Christmas. As a side note, it's quite obvious when people copy from github. Usually there's more than one guy copying for the same repository.";
    char *sentence2 = "1234566789999900000000000000000000000000xxx";
    int len = ssfs_fwrite(1,sentence,strlen(sentence));
    ssfs_fwseek(1, strlen(sentence));

    int len2 = ssfs_fwrite(1,sentence,strlen(sentence));


//    printf("length: %d\n",len);
//    char *buffer[100] = {0};
//    printf("BUFFER IS NOW: %s\n",buffer);
//    ssfs_fread(1,buffer,100);
//    printf("BUFFER IS NOW: %s\n",buffer);



}

*/
