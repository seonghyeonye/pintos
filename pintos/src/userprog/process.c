#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "threads/synch.h"
#include "threads/init.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
//#include "threads/malloc.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
void extract_filename(char *file_name, char *filename);
//void* alloc_frame(void* umap);
//struct lock file_locktwo;
/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;
  struct list_elem *e;
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
  char filename[256];
  extract_filename(file_name,filename);
  
  struct file *open_file = filesys_open(filename); 
  if(open_file==NULL)
     return -1;
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (filename, PRI_DEFAULT, start_process, fn_copy);
  sema_down(&thread_current()->load_sema);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy);
  for(e=list_begin(&thread_current()->child_list);e!=list_end(&thread_current()->child_list);e=list_next(e)){
     struct thread* t = list_entry(e,struct thread, child);
     if(t->flag==1){
       return process_wait(tid);
     }
  
  }
  return tid;
}


/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  struct list_elem *e;
  
 // vm_init(&thread_current()->vm);
  /* Initialize interrupt frame and load executable. */
  //lru_init();
  //swap_init();
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);
  
  /* If load failed, quit. */
  palloc_free_page (file_name);
  sema_up(&thread_current()->parent->load_sema);
  if (!success) 
    {
    thread_current()->flag=1; 
    thread_exit ();
  }
  //if(success){
  //printf("succeed!!!!!!!!!!!!!!!\n");
  //}
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  //printf("after");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{ 
  struct list_elem *e;
  int exit_num;
  if(!list_empty(&thread_current()->child_list)){
  for (e = list_begin (&thread_current()->child_list); e != list_end (&thread_current()->child_list);
       e = list_next (e)){
     struct thread *child_thread = list_entry(e,struct thread, child);
     if(child_tid == child_thread -> tid){
        if(thread_current()->childone==-1)
           thread_current()->childone= child_tid;
        if(child_thread->flag==1){
             //printf("right\n");
             return -1;}
        struct list_elem child_remove = child_thread->child;
        list_remove(&child_remove);
        sema_down(&child_thread->child_sema);
        exit_num = child_thread->exit_num;
        sema_up(&child_thread->exit_sema);
        return exit_num;
     }
  }
  }
  return -1;
}


/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  struct list_elem *e;

  //file_close(cur->file);
  struct file *deletefile = thread_current()->fd[2];
  if(deletefile!=NULL){
     file_close(deletefile);
  }
  //vm_destroy(&cur->vm);
  sema_up(&cur->child_sema);
  //file_close(cur->file);
  //vm_destroy(&cur->vm);
  /*for (e = list_begin (&cur->child_list); e != list_end (&cur->child_list);
       e = list_next (e))
    {
      struct thread *child = list_entry (e, struct thread, child);
      sema_down (&child->child_sema);
      //printf("child left\n");
      sema_up (&child->exit_sema);
    }*/
  //printf("destroy\n");
  struct list_elem *mmap_e = list_begin(&cur->mmap_list);
  /*for(int i=0;i<thread_current()->mapid;i++){
     
     do_munmap(i);
  }*/
  //printf("mmap list size is %d\n",list_size(&cur->mmap_list));
  //for(mmap_e= list_begin(&cur->mmap_list); mmap_e!=list_end(&cur->mmap_list);mmap_e=list_next(mmap_e)){
  while(mmap_e!=list_end(&cur->mmap_list)){
     struct mmap_file *remove = list_entry(mmap_e,struct mmap_file,elem);
     mmap_e=list_next(mmap_e);
     do_munmap(remove);
  }
  //}
  //printf("munmap finished\n");
  vm_destroy(&cur->vm);
  //printf("file closed");
  file_close(cur->file);
  for (e = list_begin (&cur->child_list); e != list_end (&cur->child_list);
       e = list_next (e))
    {
      struct thread *child = list_entry (e, struct thread, child);
      sema_down (&child->child_sema);
      //printf("child left\n");
      sema_up (&child->exit_sema);
    }

  //printf("vm destroyed");
  sema_down(&cur->exit_sema);
  //file_close(cur->file);
  //vm_destroy(&cur->vm);
  //file_close(cur->file);
  //&cur->vm =NULL;
//  thread_current()->vm = NULL;
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
 // palloc_free_page);
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      //printf("up to here\n");
      pagedir_destroy (pd);
    }
  //printf("exiting now\n");
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);


static void
push_to_stack (char *file_name, void** esp){
 int argc=0;
 int i=0;
 char *token, *save_ptr, *result;
 char temp[256];
 strlcpy(temp,file_name,strlen(file_name)+1);
 for(token = strtok_r (temp," ",&save_ptr); token != NULL;
token = strtok_r (NULL, " ", &save_ptr)){
    argc++;
 }
 char *argv[argc+1];
 strlcpy(temp,file_name,strlen(file_name)+1);
 
 for(token = strtok_r (file_name," ",&save_ptr); token != NULL;
token = strtok_r (NULL, " ", &save_ptr)){
  argv[i] = token;
  i++;
  }
  result=argv[0];

  int total_size=0;
  char *tempv[argc+1];
  for(i=argc-1;i>=0;i--){
     int size = strlen(argv[i])+1;
     total_size+=size;
     *esp -= size;
     memcpy(*esp,argv[i],size);
     tempv[i] = *esp;
  }
  //align
  int remainder = total_size%4;
  if(remainder!=0){*esp -= 4-remainder;}
  
  int zero = 0;
  //NULL
  *esp -= 4;
  memcpy(*esp,&zero,4);
  for(i=argc-1;i>=0;i--){
    *esp -= 4;
    memcpy(*esp,&tempv[i],4); 
  }
  //argv
  void* tempesp = *esp;
  *esp -= 4;
  memcpy(*esp,&tempesp,4);
  //argc
  *esp -= 4;
  memcpy(*esp,&argc,4);
  *esp -= 4;
  memcpy(*esp,&zero,4);
  //temp=NULL;
//  palloc_free_page(token);
  //palloc_free_page(save_ptr);
  //palloc_free_page(result);
  //printf("push stack finish\n");
}

void extract_filename(char *file_name, char *filename)
{
  int i;
  strlcpy(filename, file_name, strlen(file_name)+1);
  for (i=0; filename[i]!='\0' && filename[i] != ' '; i++);
  filename[i] = '\0';
}

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */

bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();

  //vm_init(&t->vm);
  //printf("vm address in load function %x\n",&t->vm);

  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  char filename[256];
  extract_filename((char*)file_name,filename);
  //printf("filename is %s\n",filename);
  /* Open executable file. */
  //lock_acquire(&file_lock);
  file = filesys_open (filename);
  //lock_release(&file_lock);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
  t->file=file;
  //file_deny_write(file);
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;
      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);
      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
		  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
		 goto done;
 	      }
          else
            goto done;
          break;
        }
    }
  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;
  
  push_to_stack((char*)file_name,esp);
  /* Start address. */
  //printf("file length in load is %d\n",file_length(file));
  *eip = (void (*) (void)) ehdr.e_entry;
  thread_current()->esp =*esp;   
  file_deny_write(file);
  success = true;
  //sema_up(&t->child_sema);
 done:
  /* We arrive here whether the load is successful or not. */
//  lock_release(&file_lock);
  //file_close (file);
  //printf("file length in load is %d\n",file_length(file));
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
  //printf("file pos in load_segment is %x\n",file);
  file_seek (file, ofs);
 
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      //printf("read bytes in load_seg is %d\n",read_bytes);
      //printf("read bytes actual %d\n",page_read_bytes);
      //printf("file pos in load segment is %x\n",file);
      //if(page_zero_bytes==PGSIZE){
      	struct sppt_entry *entry= (struct sppt_entry*) malloc(sizeof(struct sppt_entry));
        if(entry==NULL)
          printf("malloc failed\n");
        memset(entry,0,sizeof(struct sppt_entry));
        entry->type=VM_BIN;
        entry->file=file;
        entry->upage=upage;
        entry->read_bytes=page_read_bytes;
        entry->zero_bytes=page_zero_bytes;
        entry->offset=ofs;
        entry->writable=writable;
        entry->ref_bit=1;
        insert_vme(&thread_current()->vm,entry);
        ofs+=page_read_bytes;
      //}
      //  printf("file pointer is %x\n",file);
        //printf("file length in load is %d\n",file_length(file));
      /*else{
  //    printf("vm addr in load segment func %x\n",&thread_current()->vm);
      //printf("upage addr in load segment %x\n",upage);
     // insert_vme_file(&thread_current()->vm,upage,file,page_read_bytes,page_zero_bytes,ofs); 
        //if(page_read_bytes!=0){
        struct frame_entry *frame=alloc_frame_entry(PAL_USER);
        void *kpage=frame->kpage;
        if(file_read(file,kpage,page_read_bytes)!=(int)page_read_bytes){
           printf("load failed\n");
           free_frame(kpage);
           return false;
        }
        memset (kpage + page_read_bytes, 0, page_zero_bytes);
        if (!install_page (upage, kpage, writable))
        {
          printf("install failed\n");
          free_frame(kpage);
          return false;
        }
      }*/
      //entry->
      /* Get a page of memory. */
      //printf("alloc first %x\n",upage);
      
//struct frame_entry *frame = alloc_frame_entry(PAL_USER);
//void *kpage=frame->kpage;
      //#endif*/
      /*if (kpage == NULL){
        printf("exception should be thrown\n"); 
        return false;
      }*/
      /* Load this page. */
      //printf("kpage in load segment is %x\n",kpage);
      //printf("file bytes in load segment is %d\n",file_read(file,kpage,page_read_bytes)); 
      /*if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
        //  palloc_free_page (kpage);
          //#ifndef VM
          free_frame(kpage);*/
          //#endif*/
         /* return false; 
        }
      memset (kpage + page_read_bytes, -1, page_zero_bytes);*/
      /* Add the page to the process's address space. */
      /*if (!install_page (upage, kpage, writable)) 
        {
          //palloc_free_page (kpage);
          //#ifndef VM
          free_frame(kpage);
         // #endif*/
          /*return false; 
        }*/
      //insert_vme_file(&thread_current()->vm,kpage,upage,file,page_read_bytes,page_zero_bytes,ofs);
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      
      //ofs+=page_read_bytes;
    }
  //printf("file length in load is %d\n",file_length(file));
  //printf("file pointer length in load is %x\n",sizeof(file));
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  //uint8_t *kpage;
  struct frame_entry *kpage;
  bool success = false;
  
  //printf("setup stack before %x\n",PHYS_BASE-PGSIZE);
  //kpage = alloc_frame (PAL_USER | PAL_ZERO);
  void *upage = ((uint8_t *) PHYS_BASE)-PGSIZE;
  struct sppt_entry *entry= (struct sppt_entry*) malloc(sizeof(struct sppt_entry));
  if(entry==NULL){
    printf("malloc failed\n");
  }
  kpage = alloc_frame_entry(PAL_USER|PAL_ZERO);
  //kpage->entry=entry;
  //printf("kpage in setup stack %x\n",kpage);  
  if (kpage != NULL) 
    {
      kpage->entry=entry;
      success = install_page (upage, kpage->kpage, true);
      if (success){
        *esp = PHYS_BASE;
      }
      else{
        free(entry);
        free_frame (kpage);
      }
    //insert_vme(&thread_current()->vm,kpage,upage);
    }
 // struct sppt_entry *entry= malloc(sizeof(struct sppt_entry));
  memset(kpage->entry,0,sizeof(struct sppt_entry));
  kpage->entry->type=VM_ANON;
  kpage->entry->upage=upage;
  //kpage->entry->kpage=kpage;
  kpage->entry->writable=true;
  insert_vme(&thread_current()->vm,kpage->entry);
  /*else{
    printf("exception should be thrown2\n");
  }*/
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();
  //struct sppt_entry *entry= malloc(sizeof(struct sppt_entry));
  /*struct sppt_entry *entry;
  entry->upage=upage;
  entry->kpage=kpage;*/
  //insert_vme(&t->vm,upage,kpage); 
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

bool handle_mm_fault(struct sppt_entry *entry){
   //printf("enter fault handler\n");
   //printf("file length in handle mm fault is %d\n",file_length(entry->file));
   struct frame_entry *frame = alloc_frame_entry(PAL_USER);
   ASSERT(frame!=NULL);
   frame->entry=entry;
   ASSERT(pg_ofs(frame->kpage)==0);
   ASSERT(entry!=NULL);
   //loading
   //printf("frame -> kpage is %x\n",frame->kpage);
   //printf("entry ->read_bytes is %d\n",entry->read_bytes);
   //printf("entry type is %d\n",entry->type);
   //printf("file pos is %x\n",entry->file);
   //printf("file length in handle mm fault is %d\n",file_length(entry->file)); 
   //if (file_read(entry->file,frame->kpage,entry->read_bytes) != (int)entry->read_bytes){
     // printf("load fail!\n");
   //}
   //load_file(frame->kpage,entry);
   //memset (frame->kpage + entry->read_bytes, 0, entry->zero_bytes);
  switch(entry->type){
   case VM_BIN:
   case VM_FILE:
     if (!load_file(frame->kpage,entry)||!install_page (entry->upage,frame->kpage,entry->writable)){
      //NOT_REACHED();
      //free_
        free_frame(frame->kpage);
        return false;
        //printf("install fail!\n");
     }
     return true;
   //case VM_FILE:
     //   return true;
     //printf("file exception\n");
   case VM_ANON:
      //printf("anon swap\n");
     swap_in(entry->swap_slot,frame->kpage);
     if(!install_page(entry->upage,frame->kpage,entry->writable)){
        free_frame(frame->kpage);
        return false;
     }
  // default:
   
    // printf("noooooooooooo\n");
   /*if(!load_file(entry->kpage,entry)||!install_page(entry->upage,entry->kpage,entry->writable)){
      printf("load or install fail!\n");
   }*/
   //#ifdef VM
   //install_page(vme->upage,kpage,0);
   //#endif
     return true;
   }
}

void do_munmap(struct mmap_file* mmap_file){
   //struct list vme_list = mmap_file->vme_list;
   struct list_elem *e=list_begin(&mmap_file->vme_list);
   //printf("list size is %d\n",list_size(&mmap_file->vme_list));
   while(e!=list_end(&mmap_file->vme_list)){
   //for(e=list_begin(&vme_list);e!=list_end(&vme_list);e=list_next(e)){
      struct sppt_entry *entry =list_entry(e,struct sppt_entry,mmap_elem);
      if(pagedir_get_page(thread_current()->pagedir, entry->upage)&&pagedir_is_dirty(thread_current()->pagedir,entry->upage)){
         if(file_write_at(entry->file,entry->upage,entry->read_bytes,entry->offset)!=(int)entry->read_bytes){
            printf("writing to file failed\n");
         }
      free_frame(pagedir_get_page(thread_current()->pagedir,entry->upage));
      }
      //printf("free frame enter\n");
     // free_frame(pagedir_get_page(thread_current()->pagedir,entry->upage));
      //printf("free frame out\n");
      e=list_remove(e);
      delete_vme(&thread_current()->vm,entry);
     // printf("delete vme\n"); 
      //e=list_remove(e);
     // printf("remove e\n");
   }
   //printf("before free");
   list_remove(&mmap_file->elem);
   free(mmap_file);
   //printf("munmap finish\n");
}

bool expand_stack(void* addr){
    void* upage = pg_round_down(addr);
    struct frame_entry *newframe = alloc_frame_entry(PAL_USER);
    //newframe->entry=entry;
    struct sppt_entry *entry = (struct sppt_entry *)malloc(sizeof(struct sppt_entry));    
    newframe->entry=entry;
    entry->upage =upage;
    entry->writable =1;
    entry->type = VM_ANON;
    insert_vme(&thread_current()->vm,entry);
    if(!install_page(upage,newframe->kpage,true)){
        printf("install in expand stack failed\n");
    }
    return true;  
}
