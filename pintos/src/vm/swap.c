#include <bitmap.h>
#include <debug.h>
#include "vm/swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static struct block *block;
static struct bitmap *swap_bit;
//size_t sector_num
//static struct lock swap_lock;
//extern struct lock file_lock;

void swap_init(){
  //lock_init(&swap_lock);
  block = block_get_role(BLOCK_SWAP);
  //printf("block address %x\n",block);
  //printf("block size is %d\n",block_size(block));
  //sector_num = PGSIZE/BLOCK_SECTOR_SIZE;
  //size_t swap_size = block_size(block)/sector_num;
  //size_t swap_size = block_size(block)/BLOCK_SECTOR_SIZE;
  //size_t swap_size = block_size(block)/8;
  
  swap_bit = bitmap_create(1024);//2^22/2^12
  //printf("swap bit address malloc %x\n",swap_bit);
}

void swap_in(size_t used_index, void* kaddr){
  //lock_acquire(&swap_lock);
  //lock_acquire(&file_lock);
  if(bitmap_test(swap_bit,used_index)==false)
    PANIC("swap in failed.");

  //printf("into for loop\n");
  for(int i=0;i<8;i++){
    block_read(block,used_index*8+i,kaddr+512*i);
  }  
  //printf("after for loop\n");
  bitmap_set(swap_bit,used_index,false);
  //printf("end\n");
  //lock_release(&file_lock);
}

size_t swap_out(void* kaddr){
  //lock_acquire(&file_lock);
  size_t out_index = bitmap_scan(swap_bit,0,1,false);
 // for(int i=0;i<512;i++){
   //  block_write(block,out_index*512+i,kaddr+(i*512));
  for(int i=0;i<8;i++){
     //block_write(block,out_index*+i,kaddr+(BLOCK_SECTOR_SIZE*i));
     //printf("block is %x\n",block);
//     printf("kaddr is %x\n",kaddr);
     //printf("block size %d\n",block_size(block));
     //block_write(block,0,kaddr);
     block_write(block,out_index*8+i,kaddr+(512*i));
     //printf("write %d at %x\n",i,kaddr+i*512);
  }
  bitmap_set(swap_bit,out_index,true);
  //lock_release(&file_lock);
  //printf("swap out complete %d\n",i);
  return out_index;
}
