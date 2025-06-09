/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page *file_page = &page->file;
	file_page->va = page->va;
	struct lazy_info *info =  page->uninit.aux;
	file_page->file = info->file;
	file_page->len = info->read_bytes;
	file_page->ofs = info->ofs;
	
	
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva) {
	struct file_page *file_page = &page->file;
	struct lazy_info *aux = (struct lazy_info *)page->uninit.aux;

	off_t offset = aux->ofs;
	size_t read_bytes = aux->read_bytes;
	size_t zero_bytes = aux->zero_bytes;

	if (file_read_at(aux->file, kva, read_bytes, offset) != (int)read_bytes)
		return false;

	memset(kva + read_bytes, 0, zero_bytes);
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	uint64_t *pml4 = thread_current()->pml4;

	if(pml4_is_dirty(pml4, file_page->va)){
		if(file_write_at(file_page->file, page->frame->kva, file_page->len, file_page->ofs) == 0){
			return;
		}
		pml4_set_dirty(pml4,file_page->va);
	}
	pml4_clear_page(pml4, page->va);
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t ofs) {
	struct file *f = file_reopen(file);
	size_t read_bytes = length + ofs < file_length(f) ? length : (file_length(f) - ofs);
	size_t zero_bytes = pg_round_up(length) - read_bytes;
		
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (addr) == 0);
	ASSERT (ofs % PGSIZE == 0);
	void *origin_addr = addr;
	while (read_bytes > 0 || zero_bytes > 0) {
		/* 이 페이지를 어떻게 채울지 계산합니다.
		 * FILE에서 PAGE_READ_BYTES 바이트를 읽고,
		 * 마지막 PAGE_ZERO_BYTES 바이트는 제로화합니다. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: lazy_load_segment에 정보를 전달할 aux 설정 */
		struct lazy_info *aux = (struct lazy_info *)calloc(1, sizeof(struct lazy_info));
		aux->file = f;
		aux->ofs = ofs;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;
		aux->len = length;

		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, aux))
			return NULL;

		// 진행
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;
		ofs += page_read_bytes;
	}
	return origin_addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table spt = thread_current()->spt;
	struct page *page = spt_find_page(&spt, addr);
	if(page != NULL){
		if(page_get_type(page) == VM_FILE){
			destroy(page);
			spt_remove_page(&spt, page);
		}
	}
	return;
}
