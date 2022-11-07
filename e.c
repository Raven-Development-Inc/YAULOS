#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

uint16_t current_place = 0;

typedef struct {
    uint16_t header[3]; // FS1
    uint16_t startingSector; // aka root
} superblock_t;
typedef struct {
    uint16_t type; // dir/file
    uint16_t next_block; // next block of data / EOF (0xfff8 - 0xffff)
    uint16_t inline_data[0x1ff-2];
} block_t; // essentially some sort of dir/file/other stuff

uint16_t get_page(uint16_t sector_addr) {
    return sector_addr >> 8;
}

uint16_t get_raw_addr(uint16_t sector_addr) {
    return (sector_addr & 0xff) * 512;
}

block_t * traversefs(uint16_t ** memory, int amount) { // move thru FS, mostly unusable
    superblock_t * superblock = (superblock_t*)memory[0];
    assert(superblock->header[0] == 'F');
    assert(superblock->header[1] == 'S');
    assert(superblock->header[2] == '1');
    block_t * currblock = NULL;
    for(int i = 0; i < amount; i++) {
        if(currblock == NULL) {
            currblock = (block_t*)(&memory[get_page(superblock->startingSector)][get_raw_addr(superblock->startingSector)]);
        } else {
            currblock = (block_t*)(&memory[get_page(currblock->next_block)][get_raw_addr(currblock->next_block)]);
        }
    }
    return currblock;
}

block_t * getBlock(uint16_t ** memory, block_t * block, uint16_t index) { // read block from the directory/block by index
    return (block_t*)(&memory[get_page(block->inline_data[index])][get_raw_addr(block->inline_data[index])]);
}

void print16(uint16_t * str) { // print dynamic amount of chars represented by uint16_t
    int i = 0;
    while(str[i] != 0) {
        printf("%c", str[i]);
        i++;
    }
}

void sprint16(uint16_t * str, uint16_t size) { // print fixed amount of uint16_t values as hex and their char representative
    for(uint16_t i = 0; i < size; i++) {
        printf("%4x (%c) ", str[i], str[i]);
    }
}

void sprint8(uint8_t * str, uint16_t size) { // print fixed amount uint8_t values as hex and their char representative
    for(uint16_t i = 0; i < size; i++) {
        printf("%2x (%c) ", str[i], str[i]);
    }
}

superblock_t * generateSuperblock(uint16_t starting_sector) { // generate the root block of the file system
    superblock_t * superblock = malloc(sizeof(superblock_t));
    superblock->startingSector = starting_sector;
    superblock->header[0] = 'F';
    superblock->header[1] = 'S';
    superblock->header[2] = '1';
    return superblock;
}

block_t * generateFile(uint16_t * data, uint16_t size) { // generate the block which acts as a file
    // WIP to be over 1 block size
    block_t * block = malloc(sizeof(block_t));
    block->next_block = 0xffff;
    block->type = 1; //file
    memmove(block->inline_data, data, size*sizeof(uint16_t));
    return block;
}

block_t * generateDirectory() { // generate the block which acts as a directory
    block_t * directory = malloc(sizeof(block_t));
    directory->next_block = 0xffff;
    directory->type = 0; // directory
    return directory;
}

uint16_t append_file(uint16_t** memory, uint16_t diraddr, uint16_t fileaddr) { // append file to the directory
    block_t * directory = (block_t*)&memory[get_page(diraddr)][get_raw_addr(diraddr)];
    block_t * file = (block_t*)&memory[get_page(fileaddr)][get_raw_addr(fileaddr)];
    int i = 0;
    for(i = 0; directory->inline_data[i] != 0; i++) {};
    directory->inline_data[i] = fileaddr;
    return i;
}

uint16_t placeBlock(uint16_t ** memory, block_t * block) { // place block on memory and return its index
    memmove(&memory[get_page(current_place)][get_raw_addr(current_place)], block, sizeof(block_t));
    return current_place++;
}

uint16_t placeSuperblock(uint16_t ** memory, superblock_t * superblock) { // place superblock on the first place on memory, and assert no fs is present at this point
    assert(current_place == 0);
    memmove(&memory[get_page(current_place)][get_raw_addr(current_place)], superblock, sizeof(superblock_t));
    return current_place++;
}

block_t * readBlock(uint16_t ** memory, uint16_t addr) { // read block from memory by its index and return pointer to it
    return (block_t*)&memory[get_page(addr)][get_raw_addr(addr)];
}

uint16_t placeFile(uint16_t ** memory, block_t * file, uint16_t diraddr) { // place block on memory and link it to the directory
    uint16_t fd = placeBlock(memory, file);
    append_file(memory, diraddr, fd);
    return fd;
}

uint16_t * toUint16String(char * str) { // make a uint16_t string from char string
    uint16_t * buf = (uint16_t*)malloc(strlen(str)*sizeof(str)+1);
    for(int i = 0; i < strlen(str); i++) {
        buf[i] = str[i];
    }
    return buf;
}

uint16_t * readFile(uint16_t ** memory, uint16_t addr) { // read contents of a file and return the uint16_t string
    block_t * file = readBlock(memory, addr);
    uint16_t block_amount = 1;
    block_t * currblock = file;
    for(; currblock->next_block != 0xffff; block_amount++) currblock = readBlock(memory, currblock->next_block);
    //printf("[Debug] Block amount: %d\n", block_amount);
    uint16_t * outputData = (uint16_t*)malloc(block_amount*sizeof(uint16_t)*(0x1ff-2));
    currblock = file;
    uint16_t i = 0;
    for(; currblock->next_block != 0xffff; i++) {
        memmove(outputData + i*0x1fd, currblock->inline_data, 0x1fd*sizeof(uint16_t));
        currblock = readBlock(memory, currblock->next_block);
    }
    memmove(outputData + i*0x1fd, currblock->inline_data, 0x1fd*sizeof(uint16_t));
    return outputData;
}


int main() {
    uint16_t **memory = malloc(5*sizeof(uint16_t*));
    for(int i = 0; i < 5; i++) {
        memory[i] = malloc(0xffff*sizeof(uint16_t));
    }
    
    superblock_t * superblock = generateSuperblock(1);
    placeSuperblock(memory, superblock); // initialization of superblock

    uint16_t * data1 = toUint16String("hello\n"); // translating the string to uint16string
    block_t * file1 = generateFile(data1, 7); // generating file
    uint16_t file_placed = placeBlock(memory, file1); // placing file on memory

    block_t * dir1 = generateDirectory(); // generating directory (root)
    uint16_t dir_placed = placeBlock(memory, dir1); // placing directory

    uint16_t index = append_file(memory, dir_placed, file_placed); // appending file to the directory
    
    block_t * dirOut = readBlock(memory, dir_placed); // reading the directory
    
    block_t * fileAppended = readBlock(memory, dirOut->inline_data[index]); // finding the file

    uint16_t * memdump = malloc(current_place*sizeof(block_t)); // declare a dump
    for(int i = 0; i < current_place; i++) {
        memmove(memdump + i * (sizeof(block_t)/2+1), &memory[get_page(i)][get_raw_addr(i)], sizeof(block_t)); // dump all the used data by the fs
    }
    

    uint16_t ** memd = malloc(5*sizeof(uint16_t*)); // initialize reusable memory acting as uint16_t ** memory declared before
    
    
    for(int i = 0; i < 5; i++) {
        memd[i] = NULL; // initialization
    }
    memd[0] = memdump; // initialization

    block_t * dirOut2 = readBlock(memd, dir_placed); // read directory from the dump memory
    print16(readFile(memd, dirOut2->inline_data[index])); // read file placed at `index` in directory
