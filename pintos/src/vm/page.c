#include "lib/kernel/hash.h"
//#include <hash.h>
#include "threads/thread.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

//unsigned vm_hash_func(const struct hash_elem *e, void *aux);
//bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b,void *aux);


/*void vm_init(struct hash *vm){
  hash_init(&thread_current()->vm,vm_hash_func,vm_less_func,NULL);
}*/


unsigned vm_hash_func(const struct hash_elem *e, void *aux){
  struct sppt_entry *e_entry = hash_entry(e,struct sppt_entry,elem);
  return hash_int((int)e_entry->upage);
}

bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b,void *aux){
  struct sppt_entry *a_entry = hash_entry(a,struct sppt_entry,elem);
  struct sppt_entry *b_entry = hash_entry(b,struct sppt_entry,elem);
  return a_entry->upage < b_entry->upage;
}

void vm_init(struct hash *vm){  
  hash_init(vm,vm_hash_func,vm_less_func,NULL);
//  printf("vm address is %x\n", vm->buckets);
}

/*bool insert_vme_file(struct hash *vm,void *kpage, void *upage,struct file* file,size_t page_read_bytes,size_t page_zero_bytes,size_t offset){
  struct sppt_entry *entry= malloc(sizeof(struct sppt_entry));
  entry->upage=upage;
  entry->read_bytes=page_read_bytes;
  entry->zero_bytes=page_zero_bytes;
  entry->offset=offset;
  entry->file=file;
  //entry->kpage=kpage;
  struct hash_elem *insert_elem = hash_insert(vm,&entry->elem);
  if(insert_elem==NULL)
     return true;
  return false;
} 

bool insert_vme2(struct hash *vm,void *upage, void* kpage){
  struct sppt_entry *entry=malloc(sizeof(struct sppt_entry));
  entry->upage=upage;
  //entry->kpage=kpage;
  struct hash_elem *insert_elem = hash_insert(vm,&entry->elem);
  if(insert_elem==NULL)
      return true;
  return false;
}*/

bool insert_vme(struct hash *vm,struct sppt_entry *entry){
  //struct sppt_entry *entry=malloc(sizeof(struct sppt_entry));
  //entry->upage=upage;
  //entry->kpage=kpage;
  struct hash_elem *insert_elem = hash_insert(vm,&entry->elem);
  if(insert_elem==NULL)
      return true;
  return false;
}


bool delete_vme(struct hash *vm, struct sppt_entry *vme){
  struct hash_elem *delete_elem = hash_delete(vm,&vme->elem);
  if(delete_elem==NULL)
     return false;
  lock_acquire(&lru_lock);
  free_after_lock(vme->upage);
  lock_release(&lru_lock);
  free(vme);
  return true;
}

struct sppt_entry *find_vme(void *upage){
  //void *page_start=pg_round_down(upage);
  //printf("thread tid is %d\n",thread_current()->tid);
  struct hash *vm = &thread_current()->vm;
  
  struct sppt_entry temp;
  temp.upage = pg_round_down(upage);
  struct hash_elem *elem = hash_find(vm,&temp.elem);
  //printf("hash finding\n");
  if(elem==NULL) {
    //printf("null!");
    return NULL;
  }
  struct sppt_entry *elem_entry = hash_entry(elem,struct sppt_entry,elem);
  //printf("file length in find vme is %d\n",file_length(elem_entry->file));
  return elem_entry;
}

void destroy_func(struct hash_elem *elems, void *aux){
  struct sppt_entry *temp = hash_entry(elems,struct sppt_entry,elem);
  //printf("destroying\n"); 
  if(temp->upage!=NULL){
     lock_acquire(&lru_lock);
     //printf("enter after lock part\n");
     free_after_lock(pagedir_get_page(thread_current()->pagedir,temp->upage));
     lock_release(&lru_lock);
  }
  //printf("freen\n");

  //free temp needed?
  free(temp);
}

void vm_destroy(struct hash *vm){
  hash_destroy(vm,destroy_func);
  //printf("exiting\n");
  //free(vm);
}

bool load_file(void* kaddr, struct sppt_entry *vme){
 //lock_acquire(&lru_lock);
 //if(vme->read_bytes==0){
   // memset(kaddr,0,PGSIZE);
   // return true;
 //}
 //else {
  
   // file_seek(vme->file,vme->offset);
 //printf("vme -> offset is %d\n",vme->offset);
 //printf("kaddr in load file is %x\n",kaddr);
 //printf("vme-> read_bytes is %d\n",(int)vme->read_bytes);
 //printf("file in load file is %x\n",vme->file);
 //printf("file length is %d\n",(int)file_length(vme->file));
    //int read_bytes = (int)file_read(vme->file, kaddr,PGSIZE);
    
    //ASSERT (kaddr != NULL);
 //ASSERT (vme != NULL);
 //ASSERT (vme->type == VM_BIN || vme->type == VM_FILE);
 // printf("filename is %s\n",vme->file->pos);
 //int read_bytes = file_read_at(vme->file,kaddr,vme->read_bytes,vme->offset);
 //printf("read bytes is %d\n",read_bytes);
 //printf("vme-> read_bytes is %d\n",(int)vme->read_bytes);
 //printf("vme-> zero bytes is %d\n",vme->zero_bytes);
 //if(read_bytes!=(int)vme->read_bytes){
 //printf("file read %d\n",read_bytes);
 if(file_read_at(vme->file,kaddr,vme->read_bytes,vme->offset)!=(int)vme->read_bytes){
   printf("load failed!\n");
   //lock_release(&lru_lock);
   return false;
 }
 //return true;
 
 memset (kaddr + vme->read_bytes, 0, vme->zero_bytes);
 //memset(kaddr+,0,PGSIZE);
 //lock_release(&lru_lock);
 return true;
 }

/*bool add_mmap(struct file *file, int32_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes){
  if(find_vme(upage)!=NULL)
     return false;
  struct sppt_entry *spte = malloc(sizeof(struct sppt_entry));
  if(spte==NULL){
     printf("malloc failed\n");
     return false;
  }
  //if(find_vme(upage)!=NULL)
    // return false;
  spte->type=VM_FILE;
  spte->file=file;
  spte->offset=ofs;
  spte->upage=upage;
  spte->read_bytes=read_bytes;
  spte->zero_bytes=zero_bytes;
  spte->writable=true;
  spte->ref_bit=true;
  hash_insert(&thread_current()->vm,&spte->elem);
  return true;
}*/

