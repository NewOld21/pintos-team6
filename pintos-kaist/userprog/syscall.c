#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/palloc.h"
#include "userprog/process.h"


void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

///////////// - System Call - /////////////////
void sys_halt (void);
void sys_exit (int status);
tid_t sys_fork (const char *thread_name, struct intr_frame *f);
int sys_exec (const char *cmd_line);
int sys_wait (tid_t pid);
bool sys_create (const char *file, unsigned initial_size);
bool sys_remove (const char *file);
int sys_open (const char *file);
int sys_filesize (int fd);
int sys_read (int fd, void *buffer, unsigned size);
int sys_write (int fd, const void *buffer, unsigned size);
void sys_seek (int fd, unsigned position);
unsigned sys_tell (int fd);
void sys_close (int fd);
///////////// - System Call - /////////////////
void *sys_mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void sys_munmap (void *addr);

struct lock file_lock;



void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);
	
	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
    lock_init(&file_lock);
}

/* The main system call interface */
// 인수는 %rdi, %rsi, %rdx, %r10, %r8, %r9 순서로 전달 //
void
syscall_handler (struct intr_frame *f UNUSED) {
    // TODO: Your implementation goes here.
#ifdef VM
    thread_current()->stack_point = f->rsp;
#endif
    switch (f->R.rax) {
        case 0:  // SYS_HALT
            sys_halt();
            break;
        case 1:  // SYS_EXIT
            sys_exit(f->R.rdi);
            break;
        case 2:  // SYS_FORK
            f->R.rax = sys_fork(f->R.rdi, f);
            break;
        case 3:  // SYS_EXEC
            f->R.rax = sys_exec(f->R.rdi);
            break;
        case 4:  // SYS_WAIT
            f->R.rax = sys_wait(f->R.rdi);
            break;
        case 5:  // SYS_CREATE
            f->R.rax = sys_create(f->R.rdi, f->R.rsi);
            break;
        case 6:  // SYS_REMOVE
            f->R.rax = sys_remove(f->R.rdi);
            break;
        case 7:  // SYS_OPEN
            f->R.rax = sys_open(f->R.rdi);
            break;
        case 8:  // SYS_FILESIZE
            f->R.rax = sys_filesize(f->R.rdi);
            break;
        case 9:  // SYS_READ
            f->R.rax = sys_read(f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        case 10: // SYS_WRITE
            f->R.rax = sys_write(f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        case 11: // SYS_SEEK
            sys_seek(f->R.rdi, f->R.rsi);
            break;
        case 12: // SYS_TELL
            f->R.rax = sys_tell(f->R.rdi);
            break;
        case 13: // SYS_CLOSE
            sys_close(f->R.rdi);
            break;
        case 14: // SYS_MMAP
            f->R.rax = sys_mmap((void *)f->R.rdi, (size_t)f->R.rsi, (int)f->R.rdx, (struct file*) f->R.r10, (off_t)f->R.r8);
            break;
        case 15: // SYS_MUNMAP
            sys_munmap((void *)f->R.rdi);
            break;
        case 16: // SYS_CHDIR
            break;
        case 17: // SYS_MKDIR
            break;
        case 18: // SYS_READDIR
            break;
        case 19: // SYS_ISDIR
            break;
        case 20: // SYS_INUMBER
            break;
        case 21: // SYS_SYMLINK
            break;
        case 22: // SYS_DUP2
            break;
        case 23: // SYS_MOUNT
            break;
        case 24: // SYS_UMOUNT
            break;
        default:
            // Unknown system call
            break;
    }
}

bool check_pml4_addr(char *file){
    struct thread *cur  = thread_current();
    if(file==NULL ||!is_user_vaddr(file)){
        return false;
    }
    // VM X
    // if( pml4_get_page(cur->pml4,file)==NULL){
    //     return false;
    // }
    return true;
}



// SYS_HALT
void sys_halt (void){
	power_off();
}

// SYS_EXIT
void sys_exit (int status){

    struct list_elem *e;
	struct thread *cur = thread_current ();
    for (e = list_begin(&cur->parent->child); e != list_end(&cur->parent->child); e = list_next(e)) {
        struct child_status *cs = list_entry(e, struct child_status, elem);
		if (cur->tid == cs->tid){
            cs->exit_status = status;
            cs->has_exited = true;
            sema_up(&cs->wait_sema);
            break;
		}
	}

	printf("%s: exit(%d)\n",cur->name ,status);
	thread_exit ();
}
// SYS_FORK
tid_t sys_fork (const char *thread_name, struct intr_frame *f){
	return process_fork(thread_name, f);
}			
// SYS_EXEC
int sys_exec (const char *cmd_line)	{ 
    if(!check_pml4_addr(cmd_line)){
        sys_exit(-1);
    }
    char *copy_cmd_line = palloc_get_page(PAL_ZERO);
    if(copy_cmd_line == NULL){
        sys_exit(-1);
    }
    strlcpy(copy_cmd_line, cmd_line, PGSIZE);

    if(process_exec(copy_cmd_line) == -1){ // 실패
        //palloc_free_page(copy_cmd_line);
        sys_exit(-1); 
    } 
    NOT_REACHED();
    return -1;
    
}
// SYS_WAIT
int sys_wait (tid_t pid){
	return process_wait(pid);
}	
// SYS_CREATE
bool sys_create (const char *file, unsigned initial_size){
    if(!check_pml4_addr(file)){
        sys_exit(-1);
    }

    lock_acquire(&file_lock);
    bool f = filesys_create(file, initial_size);
    lock_release(&file_lock);
	return f;
}			
// SYS_REMOVE
bool sys_remove (const char *file){
	if(!check_pml4_addr(file)){
        sys_exit(-1);
    }
    lock_acquire(&file_lock);
	bool res = filesys_remove(file); 
    lock_release(&file_lock);
	return res;
}

// SYS_OPEN
int sys_open(const char *file) {

	if (file == NULL || !is_user_vaddr(file)) sys_exit(-1);

    int fd;
    bool check = true;
	struct thread *cur = thread_current();
	for (fd = 2; fd < 64; fd++) {
		if (cur->fdt[fd] == NULL) {
            check = false;
			break;
		}
	}
    if(check){
        return -1;
    }
    lock_acquire(&file_lock);
	struct file *f = filesys_open(file);  // 커널 버퍼 → OK
    if (!strcmp(thread_name(), file))
      file_deny_write(f);
    lock_release(&file_lock);
	if (f == NULL) return -1;
	cur->fdt[fd] = f;
	return fd;
}


// SYS_FILESIZE
int sys_filesize (int fd){
    struct thread *cur = thread_current();
    
    if(fd<2 || fd>64){
        sys_exit(-1);
    }
    if (cur->fdt[fd] == NULL){
        return 0;
    }
    lock_acquire(&file_lock);
    int res = file_length(cur->fdt[fd]);
    lock_release(&file_lock);
    return res;
	
}		
// SYS_READ
int sys_read (int fd, void *buffer, unsigned size){
    struct thread *cur = thread_current();
    // 표준 입력(Standard Input) //
    if(fd == 0){
        for(int i=0; i<size; i++){
            ((char*)buffer)[i] = input_getc();
        }
        return size;
    }
    if(fd<2 || fd>64){
        sys_exit(-1);
    }
#ifdef VM
    struct page *page = spt_find_page(&cur->spt, buffer);
    if (page != NULL && !page->writable)
        sys_exit(-1);
#endif
    struct  file *f = cur->fdt[fd];
    if(!check_pml4_addr(buffer)){
        sys_exit(-1);
    }
    if(f==NULL){
        return -1;
    }
    lock_acquire(&file_lock);
    int read_size = file_read(f, buffer, size);
    lock_release(&file_lock);
    return read_size;
}	
// SYS_WRITE
int sys_write (int fd, const void *buffer, unsigned size){
	if (buffer == NULL || !is_user_vaddr(buffer))
		return -1;
    if(!check_pml4_addr(buffer)){
        sys_exit(-1);
    }
    if(fd==0){
        return -1;
    }
    else if(fd==1){
		putbuf(buffer, size);
		return size;
	}else if(fd<2 || fd>64){
        sys_exit(-1);
    }
   
    lock_acquire(&file_lock);
    int res = file_write(thread_current()->fdt[fd],buffer,size);
    lock_release(&file_lock);
    if(res<0){
        return -1;
    }
	return res;
}
// SYS_SEEK
void sys_seek (int fd, unsigned position){
	if(fd<2 || fd>64){
        sys_exit(-1);
    }
    struct thread *cur = thread_current();
    struct file *f = cur->fdt[fd];
    
    if (f == NULL){
        return 0;
    }
    lock_acquire(&file_lock);
    file_seek(f, position);
    lock_release(&file_lock);
}
// SYS_TELL
unsigned sys_tell (int fd){
    if(fd<2 || fd>64){
        sys_exit(-1);
    }
    struct thread *cur = thread_current();
    struct file *f = cur->fdt[fd];

    if (f == NULL){
        return 0;
    }
    lock_acquire(&file_lock);
    off_t res = file_tell(f);
    lock_release(&file_lock);
    return res;
}
// SYS_CLOSE
void sys_close(int fd) {
    if (fd < 2 || fd >= 64) {
        sys_exit(-1);  
    }
    struct thread *cur = thread_current();

    if(cur->fdt[fd] == NULL){
        sys_exit(-1);
    }
    struct file *f = cur->fdt[fd];
    cur->fdt[fd] = NULL;

    file_allow_write(f);
    lock_acquire(&file_lock);
    file_close(f);
    lock_release(&file_lock);
}


void *sys_mmap (void *addr, size_t length, int writable, int fd, off_t offset){

	if (!addr || addr != pg_round_down(addr))
        return NULL;

    if (offset != pg_round_down(offset))
        return NULL;

    if (!is_user_vaddr(addr) || !is_user_vaddr(addr + length))
        return NULL;

    if (spt_find_page(&thread_current()->spt, addr))
        return NULL;

    struct file *f = thread_current()->fdt[fd];
    if (f == NULL)
        return NULL;

    if (file_length(f) == 0 || (int)length <= 0)
        return NULL;

    return do_mmap(addr, length, writable, f, offset); // 파일이 매핑된 가상 주소 반환
}

void sys_munmap (void *addr){
	if(addr == NULL || pg_round_down(addr)!=addr){
		sys_exit(-1);
	}
	return do_munmap(addr);
}