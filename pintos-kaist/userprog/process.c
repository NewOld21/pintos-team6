#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);

/* initd 및 기타 프로세스를 위한 일반적인 프로세스 초기화 함수 */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* FILE_NAME에서 로드된 "initd"라는 첫 번째 유저랜드 프로그램을 시작한다.
 * 새 스레드는 스케줄될 수 있으며(실행 상태가 될 수 있고), 심지어 종료될 수도 있다.
 * process_create_initd()가 반환되기 전에 새 스레드가 스케줄되거나 종료될 수 있다.
 * 스레드 ID를 반환하며, 스레드를 생성할 수 없을 경우 TID_ERROR를 반환한다.
 * 이 함수는 한 번만 호출되어야 한다는 점에 유의하라.*/
//Create process and thread 
tid_t
process_create_initd (const char *file_name) { //run_task에서 파라미터에 
	char *fn_copy;	//파싱하지 않은 인자가 들어온다.
	tid_t tid;

	/* FILE_NAME의 복사본을 생성하라
	 * 그렇지 않으면 호출자와 load() 함수 사이에 경쟁 상태(race condition)가 발생할 수 있다. */
	fn_copy = palloc_get_page (0); //하나의 사용 가능한 페이지를 확보하고 해당 페이지의 커널 가상 주소를 반환한다.
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);	//fn_copy(버퍼), file_name(복사할 str), PGSIZE(버퍼의 사이즈) 

	// - Passing - //
	char *token;
	char *strl;
	token = strtok_r (file_name, " ", &strl);
	
	/* FILE_NAME을 실행할 새로운 스레드를 생성하라 */
	//initd 를 실행하는 스레드를 생성
	tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);			//page 할당을 해제한다.
	return tid;								//스레드 id를 반환한다.
}

/* 첫 번째 사용자 프로세스를 실행하는 스레드 함수. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* 현재 프로세스를 `name`이라는 이름으로 복제합니다.
 * 새로 생성된 프로세스의 스레드 ID를 반환하며,
 * 실패한 경우 TID_ERROR를 반환합니다. */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
    struct f_thread *ft = palloc_get_page(PAL_ZERO);
    if (ft == NULL)
        return TID_ERROR;

    ft->pif = if_;
    ft->pt = thread_current();

    tid_t tid = thread_create(name, PRI_DEFAULT, __do_fork, ft);
    if (tid == TID_ERROR){
		palloc_free_page(ft);
		return TID_ERROR; 
	}

    sema_down(&thread_current()->fork_sema); // 자식이 fork 완료 후 signal할 때까지 대기
	if(!thread_current()->fork_succ){	
		palloc_free_page(ft);
		return TID_ERROR;
	}
    return tid;
}


#ifndef VM
/* Project 2에서만 사용되는 함수로,
 * pml4_for_each 함수에 전달되어 부모의 주소 공간을 복제합니다. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. 커널 주소는 복제하지 않음 */
	if (is_kernel_vaddr(va))
		return true;

	/* 2. 부모 페이지 얻기 */
	parent_page = pml4_get_page(parent->pml4, va);
	if (parent_page == NULL)
		return false;

	/* 3. 자식용 새 페이지 할당 */
	newpage = palloc_get_page(PAL_USER);
	if (newpage == NULL)
		return false;

	/* 4. 내용 복사 및 writable 여부 판단 */
	memcpy(newpage, parent_page, PGSIZE);
	writable = is_writable(pte);

	/* 5. 자식의 페이지 테이블에 등록 */
	if (!pml4_set_page(current->pml4, va, newpage, writable)) {
		palloc_free_page(newpage);
		return false;
	}
	return true;
}

#endif

/* 부모 프로세스의 실행 컨텍스트를 복제하는 자식 스레드 함수입니다.
 * 힌트: parent->tf에는 사용자 영역의 레지스터 정보가 없기 때문에,
 *       process_fork()의 두 번째 인자인 if_를 이 함수로 전달해야 합니다. */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct f_thread *f_parent = (struct f_thread *) aux;
	struct thread *current = thread_current ();
	/* TODO: process_fork()에서 전달한 parent_if를 이곳으로 넘겨야 합니다. */
	struct intr_frame *parent_if = f_parent->pif;
	struct thread *parent = f_parent->pt;
	bool succ = true;
	/* 1. 부모의 레지스터 상태를 지역 스택으로 복사합니다. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));

	/* 2. 부모의 페이지 테이블(주소 공간)을 복제합니다. */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: 여기에 여러분의 코드를 작성하세요.
	 * TODO: 파일 객체를 복제할 때는 include/filesys/file.h의 file_duplicate를 사용하세요.
	 * TODO: 부모는 자식 리소스 복제가 전부 성공한 후에만 fork()에서 반환되어야 합니다. */
	for(int i=2; i<64; i++){
		if(parent->fdt[i] != NULL){
			current->fdt[i] = file_duplicate(parent->fdt[i]);
			if(current->fdt[i]==NULL){
				goto error;
			}
		}
	}
	if_.R.rax = 0;  // 자식 스레드 return값은 0
	process_init ();
	
	palloc_free_page(f_parent);
	parent->fork_succ = true;
	sema_up(&parent->fork_sema);
	
	/* 복제가 성공적으로 끝났다면, 자식 프로세스를 실행합니다. */
	if (succ){
		do_iret (&if_);
	}
		
error:
	parent->fork_succ = false;
	sema_up(&parent->fork_sema);
	sys_exit(-1);
	//thread_exit ();
}


/* 현재 실행 컨텍스트를 f_name으로 전환한다.
 * Returns -1 on fail. */
int
process_exec (void *f_name) { //실행하려는 바이너리 파일의 이름?
	char *file_name = f_name;
	bool success;
	
//////////// -- Passing -- ////////////
	char *token;
	char *strl;
	token = strtok_r (file_name, " ", &strl);
	char *argv[99];
	int argc = 0;
	while (token != NULL) {
		argv[argc] = token;
		argc++;
		token = strtok_r(NULL, " ", &strl);  // 다음 호출
	}
	file_name = argv[0];


	/* 현재 실행 컨텍스트를 f_name으로 전환한다.
	 * 이는 현재 스레드가 다시 스케줄될 때 발생하기 때문이다,
	 * 실행 정보를 해당 멤버 변수에 저장하기 때문이다.. */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;	//유저 데이터 세그먼트?
	_if.cs = SEL_UCSEG;						//유저 코드 세그먼트
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* 먼저 현재 컨텍스트를 종료(kill)한다. */
	process_cleanup ();


///////////////////////////////////////

	success = load (file_name, &_if);

//////////// -- Argument Passing -- ////////////
	
	argv[argc] = NULL;
	// argv개수(인자 개수) rdi 저장 //
	_if.R.rdi = argc;   // USER_STACK
	_if.R.rsi = (uint64_t) argv;
    

    char *str[argc];
	uint64_t new_rsp = (uint64_t)_if.rsp; //값을 뺄때 *8한 값이 빼진다.

	//  argv인자값 스택 삽입  //
    for(int i = 0; i < argc; i++){
		uintptr_t b = strlen(argv[i])+1;
		_if.rsp = _if.rsp - b;
       	str[i] = _if.rsp;
		memcpy(str[i], argv[i], b);
	}


    //  패딩  //
    uintptr_t num = _if.rsp;
    _if.rsp = _if.rsp & ~0x7;
   	uint8_t check_num = num - (_if.rsp);
	
	// 패딩 값은 0 으로 //
   	memset(_if.rsp, 0 , check_num);
    
	//  주소값 넣기 전 NULL값 넣기  //
    _if.rsp = _if.rsp - 0x8;
	memset((char *)_if.rsp, 0 , 8);

	//  argv인자 주소값 스택 삽입  //
	char **str2;
    for(int i =argc-1; i >= 0; i--){
		_if.rsp = _if.rsp - 0x8;
        str2 = _if.rsp;
        memcpy(str2, &str[i], 8);
	}
	// argv[0] 주소값을 저장한 스택 주소값 rsi 저장 //
  	_if.R.rsi = (uint64_t)_if.rsp;

	//  가짜 주소값 스택 삽입  //
    _if.rsp = _if.rsp - 0x8;
    memset((char *)_if.rsp, 0 , 8);


	 /*/* BUF의 SIZE 바이트를 16바이트씩 한 줄로 묶어 16진수 형식으로 콘솔에 출력합니다.
   각 줄에는 숫자 오프셋이 포함되며, BUF의 첫 바이트를 기준으로 OFS에서 시작합니다.
   ASCII가 true일 경우, 해당하는 ASCII 문자도 함께 출력됩니다. */
  

    //hex_dump(_if.rsp, _if.rsp , USER_STACK - _if.rsp , true);
   
	/* 불로오기를 실패하면 프로그램을 종료한다. */
	palloc_free_page (f_name);
	if (!success)
		return -1;

	/* 전환된 프로세스를 시작한다.. */
	//printf("Jumping to user process...\n");
	do_iret (&_if); //실행할 레지스터를 잘 지정해야 한다?
	NOT_REACHED ();
}


/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int
process_wait (tid_t child_tid) {
	if(child_tid == 0){
		return -1;
	}
	
	struct thread *cur = thread_current();
	struct list_elem *e;
	
	for (e = list_begin(&cur->child); e != list_end(&cur->child); e = list_next(e)) {
		struct child_status *cs = list_entry(e, struct child_status, elem);
		if (cs->tid == child_tid) {
			if (cs->has_been_waited)
				return -1;

			cs->has_been_waited = true;

			// 자식이 아직 종료되지 않았으면 대기
			if (!cs->has_exited)
				sema_down(&cs->wait_sema);

			// 자식의 종료 상태 수거 및 정리
			int status = cs->exit_status;
			list_remove(&cs->elem);
			palloc_free_page(cs);
			return status;
		}
	}

	// child_tid에 해당하는 자식을 찾지 못한 경우
	return -1;
}


/* Exit the process. This function is called by thread_exit (). */
void
process_exit (void) {
	struct thread *cur = thread_current ();
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */
	for(int i=2; i<64; i++){
		if(cur->fdt[i]!=NULL){
			file_close(cur->fdt[i]);
			cur->fdt[i] = NULL;
		}
	}
	process_cleanup ();
}

/* 현재 프로세스의 자원을 해제(free) 한다. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();
	if (curr->running_file != NULL) {
		file_close(curr->running_file);
		curr->running_file = NULL;
	}

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* 현재 프로세스의 페이지 디렉터리를 파괴(destroy)하고, 이전 페이지 디렉터리로 전환한다.
	 * 커널 전용 페이지 디렉터리(kernel-only page directory)로 전환한다. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* 여기서 올바른 순서가 매우 중요하다. 반드시 다음을 설정해야 한다.
		 * 페이지 디렉터리를 전환하기 전에 cur->pagedir을 반드시 NULL로 설정해야 한다,
		 * 그렇지 않으면 타이머 인터럽트가 다시 이전 페이지 디렉터리로 전환하는 거를 방지하기 위해서
		 * 기본 페이지 디렉터리(base page directory)를 활성화해야 한다.
		 * 프로세스의 페이지 디렉터리를 파괴하기 전에 기본 
		 * 그렇지 않으면 현재 활성화된 페이지 디렉터리가 이미 파괴된 것이 될 수 있다.
		 * 그렇게 하지 않으면 이미 해제되고(clear) 페이지 디렉터리가 활성화된 상태가 되어버린다. */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* 실행 파일 헤터.  See [ELF1] 1-4 to 1-8.
 * 이것은 ELF 바인너리의 가장 앞부분에 나타난다. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* FILE_NAME에서 ELF 실행 파일을 현재 스레드로 로드한다.
 * 실행 파일의 진입점을 *RIP에 저장한다.
 * 그리고 초기 스택 포인터를 *RSP에 저장한다.
 * 성공하면 true를 반환하고, 그렇지 않으면 false를 반환한다. */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();	//thread *t는 현재 running thread를 가리키는 포인터
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* 페이지 디렉터리를 할당하고 활성화한다. */
	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());


	/* 실행 파일을 연다. */
	file = filesys_open (file_name);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}

	/* 실행 파일의 헤더를 읽고 검증한다. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* 프로그램 헤더들을 읽는다. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* 이 세그먼트는 무시한다. 왜? */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
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

	/* 스택을 초기화한다. */
	if (!setup_stack (if_))
		goto done;

	/* 시작 주소. */
	if_->rip = ehdr.e_entry;
	
	success = true;
	t->running_file = file;
	return success;
done:
	/* We arrive here whether the load is successful or not. */
	file_close (file);
	return success;
}


/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
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

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.e
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}


/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}


#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

bool lazy_load_segment(struct page *page, void*aux)
{
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */

	struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg*) aux;
	if (page->frame == NULL || page->frame->kva == NULL) {
		PANIC("frame not allocated");
	}

	// 1) 파일의 position을 ofs으로 지정한다.
	file_seek(lazy_load_arg->file, lazy_load_arg->ofs);
	// 2) 파일을 read_bytes만큼 물리 프레임에 읽어 들인다.
	if (file_read(lazy_load_arg->file, page->frame->kva, lazy_load_arg->read_bytes) != (int)(lazy_load_arg->read_bytes))
	{
		palloc_free_page(page->frame->kva);
		return false;
	}
	// 3) 다 읽은 지점부터 zero_bytes만큼 0으로 채운다.
	memset(page->frame->kva + lazy_load_arg->read_bytes, 0, lazy_load_arg->zero_bytes);

	return true;
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
						 uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0); // read_bytes + zero_bytes가 페이지 크기(PGSIZE)의 배수인지 확인
	ASSERT(pg_ofs(upage) == 0);											 // upage가 페이지 정렬되어 있는지 확인
	ASSERT(ofs % PGSIZE == 0)												 // ofs가 페이지 정렬되어 있는지 확인;

	while (read_bytes > 0 || zero_bytes > 0) // read_bytes와 zero_bytes가 0보다 큰 동안 루프를 실행
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE; // 최대로 읽을 수 있는 크기는 PGSIZE
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		// loading을 위해 필요한 정보를 포함하는 구조체를 만들어야 합니다.
		struct lazy_load_arg *lazy_load_arg = (struct lazy_load_arg *)malloc(sizeof(struct lazy_load_arg));
		lazy_load_arg->file = file;									 // 내용이 담긴 파일 객체
		lazy_load_arg->ofs = ofs;										 // 이 페이지에서 읽기 시작할 위치
		lazy_load_arg->read_bytes = page_read_bytes; // 이 페이지에서 읽어야 하는 바이트 수
		lazy_load_arg->zero_bytes = page_zero_bytes; // 이 페이지에서 read_bytes만큼 읽고 공간이 남아 0으로 채워야 하는 바이트 수
		// vm_alloc_page_with_initializer를 호출하여 대기 중인 객체를 생성합니다.
		if (!vm_alloc_page_with_initializer(VM_ANON, upage, writable, lazy_load_segment, lazy_load_arg))
			return false;

		/* Advance. */
		// 다음 반복을 위하여 읽어들인 만큼 값을 갱신합니다.
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		ofs += page_read_bytes;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack(struct intr_frame *if_)
{
	bool success = false;

	// 스택은 아래로 성장하므로, USER_STACK에서 PGSIZE만큼 아래로 내린 지점에서 페이지를 생성한다.
	void *stack_bottom = (void *)(((uint8_t *)USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	// 1) stack_bottom에 페이지를 하나 할당받는다.
	if (vm_alloc_page(VM_ANON | VM_MARKER_0, stack_bottom, 1))
	// VM_MARKER_0: 스택이 저장된 메모리 페이지임을 식별하기 위해 추가
	// writable: argument_stack()에서 값을 넣어야 하니 True
	{
		// 2) 할당 받은 페이지에 바로 물리 프레임을 매핑한다.
		success = vm_claim_page(stack_bottom);
		if (success)
			// 3) rsp를 변경한다. (argument_stack에서 이 위치부터 인자를 push한다.)
			if_->rsp = USER_STACK;
	}
	return success;
}
#endif /* VM */


