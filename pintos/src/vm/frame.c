#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"

struct lock lru_lock;
struct list lru_list;
struct list_elem *clock;
struct hash lru_hash;

/*struct frame_entry{
  struct thread *t;
  void *upage;
  void *kpage;
  struct list_elem elemlist;
  struct hash_elem hash;
  bool ref_bit;
  struct sppt_entry *entry;
};*/
  
unsigned frame_hash_func(const struct hash_elem *e, void *aux){
  struct frame_entry *e_entry = hash_entry(e,struct frame_entry,hash);
  return hash_int((int)e_entry->kpage);
}

bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b,void *aux){
  struct frame_entry *a_entry = hash_entry(a,struct frame_entry,hash);
  struct frame_entry *b_entry = hash_entry(b,struct frame_entry,hash);
  return a_entry->kpage < b_entry->kpage;
}

void lru_init(void){
  lock_init(&lru_lock);
  list_init(&lru_list);
  //printf("list address is %x\n",&lru_lock);
  hash_init(&lru_hash,frame_hash_func,frame_less_func,NULL);
  clock=NULL;
}


struct frame_entry *alloc_frame_entry(enum palloc_flags flags){
  lock_acquire(&lru_lock);
  void *frame=palloc_get_page(flags);
  //printf("frame address is %x\n",frame);
  if(frame==NULL){
     //PANIC("palloc failed\n");
     //printf("alloc\n");
     try_to_free_pages();
     frame= palloc_get_page(PAL_USER);
     //printf("done first\n");
     if(frame==NULL)
        printf("fail!!!!!!\n");
  }
  struct frame_entry *entry= (struct frame_entry *)malloc(sizeof(struct frame_entry));
  if(entry==NULL){
    printf("frame entry malloc failed\n");
  }
  //struct page *entry =(struct page*)malloc(sizeof(struct page));
  //struct frame_entry *entry;
  //printf("frame entry address is %x\n",entry);
  memset(entry,0,sizeof(struct frame_entry));
  //entry->thread=thread_current();
  //entry->kaddr=frame;
  entry->t = thread_current();
  //entry->kpage = frame;
  entry->kpage=frame;
  //entry->ref_bit = true;
  list_push_back(&lru_list,&entry->elemlist);
  hash_insert(&lru_hash,&entry->hash);
  lock_release(&lru_lock);
  //printf("alloc finished\n");
  return entry;
}    



void free_frame(void *kpage){
  lock_acquire(&lru_lock);
  //printf("lock acquire!\n"); 
  /*struct frame_entry *temp;
  temp->kpage=kpage;
  printf("before find\n");
  struct hash_elem *elem = hash_find(&lru_hash,&temp->hash);
  printf("before hash\n");
  struct frame_entry *found = hash_entry(elem,struct frame_entry,hash);
  printf("base installed\n");
  hash_delete(&lru_hash,&found->hash);
  list_remove(&found->elem);
  palloc_free_page(kpage);
  //free?
  free(&found);*/
  free_after_lock(kpage);
  lock_release(&lru_lock);
}

void free_after_lock(void *kpage){
  //printf("pass\n");
  ASSERT (lock_held_by_current_thread (&lru_lock));
  struct frame_entry temp;
  temp.kpage=kpage;
  struct hash_elem *elem = hash_find(&lru_hash,&temp.hash);
  if(elem!=NULL){
  struct frame_entry *found = hash_entry(elem,struct frame_entry,hash);
  hash_delete(&lru_hash,&found->hash);
  /*if(&found->elemlist==list_begin(&lru_list)){
     printf("connection re\n");
     list_end(&lru_list)->next=list_begin(&lru_list);
  }*/
  pagedir_clear_page(found->t->pagedir,found->entry->upage);
  list_remove(&found->elemlist);
  //printf("before\n");
  palloc_free_page(kpage);
  //printf("after\n");
  //free?
  free(found);
  }
  /*if(elem==NULL){
    printf("just exit\n");
  }*/
  //printf("free finshed\n");
}

struct list_elem *get_next_lru_clock(){
  /*if(clock==list_end(&lru_list))
      return NULL;
  else if(clock==NULL)
     return list_begin(&lru_list);
  else
     return list_next(clock);*/
 // if(clock==list_end(&lru_list))
   //  list_end(&lru_list)->next=list_begin(&lru_list);
  ASSERT (lock_held_by_current_thread (&lru_lock));
  if(clock==NULL||clock==list_end(&lru_list)){
     if(list_empty(&lru_list))
        return NULL;
     //printf("end enter:list begin is %x\n",list_begin(&lru_list));
     return clock =list_begin(&lru_list);
     //return clock;
     }
  clock=list_next(clock);
  if(clock==list_end(&lru_list))
       return get_next_lru_clock();
  return clock;
  /*else{
     //printf("enter\n");
     clock= list_next(clock);
     //return clock;
  }
  //struct frame_entry *test_entry = list_entry(clock,struct frame_entry,elemlist);
  //return test_entry;
 printf("clock is after %x\n",clock);
  if(clock==NULL||clock||list_end(&lru_list)){
     if(list_empty(&lru_list))
    */
 
}

void try_to_free_pages(){
  //printf("list begin addr %x\n", list_begin(&lru_list));
  //printf("clock is %x\n",clock);
  //printf("list size is %d\n",list_size(&lru_list));
  //printf("list end is %x\n",list_end(&lru_list)); 
  //printf("hash size is %d\n",hash_size(&lru_hash));
  //struct list_elem *point = get_next_lru_clock;
  size_t length=list_size(&lru_list);
  //struct frame_entry *test_entry = hash_entry(point,struct frame_entry,hash);
  for(int i=0;i<2*length;i++){
    //printf("clock is next %x\n",list_next(clock));
    struct list_elem *e = get_next_lru_clock();
    //struct frame_entry *test_entry =get_next_lru_clock();
    //printf("clock is after %x,%d\n",clock,i);
    //printf("list next of list begin %x\n",list_next(list_begin(&lru_list)));
    struct frame_entry *test_entry = list_entry(e,struct frame_entry,elemlist);
    //printf("upage is %x for %d\n",test_entry->upage,i); 
    if(test_entry->entry->ref_bit==false){
     //freei
    //  printf("type is %d\n",test_entry->entry->type);
      ASSERT(test_entry->entry!=NULL);
      ASSERT(test_entry->t!=NULL);
  //    printf("page dir %x\n",test_entry);
      //pagedir_clear_page(test_entry->t->pagedir,test_entry->entry->upage);
      //printf("test_entry kpage is %x\n",test_entry->kpage);
      switch(test_entry->entry->type){
         case VM_BIN:
           if(pagedir_is_dirty(test_entry->t->pagedir,test_entry->entry->upage)){
              test_entry->entry->swap_slot=swap_out(test_entry->kpage);
              test_entry->entry->type = VM_ANON;
           }
           break;
         case VM_ANON:
           test_entry->entry->swap_slot=swap_out(test_entry->kpage);
      /*lock_release(&lru_lock);
      free_frame(test_entry->kpage);
      lock_acquire(&lru_lock);*/
           //printf("vm anon\n");
           //free_after_lock(test_entry->kpage);
      //swap_in(index,upage);
           break;
         case VM_FILE:
           if(pagedir_is_dirty(test_entry->t->pagedir,test_entry->entry->upage)){
              if(file_write_at(test_entry->entry->file,test_entry->entry->upage,test_entry->entry->read_bytes,test_entry->entry->offset)!=(int)test_entry->entry->read_bytes){
                 printf("writing to file in try to free failed\n");
              }
           }
           break;
         default:
           printf("failed!\n"); 
       }
       free_after_lock(test_entry->kpage);
       break;
    }
    else{
     
    //printf("ref bit is %d for %d\n",test_entry->ref_bit,i);
    test_entry->entry->ref_bit=false; 
    pagedir_set_accessed(test_entry->t->pagedir,test_entry->entry->upage,false);
    }
  }
}
