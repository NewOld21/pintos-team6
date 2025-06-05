/* uninit.c: 초기화되지 않은 페이지의 구현
 *
 * 모든 페이지는 처음에 초기화되지 않은 페이지로 생성됩니다. 첫 번째 페이지 폴트가 발생하면,
 * 핸들러 체인이 uninit_initialize (page->operations.swap_in)를 호출합니다.
 * uninit_initialize 함수는 페이지를 특정 페이지 객체(anon, file, page_cache)로 변환합니다. 
 * 이때 페이지 객체를 초기화하고, vm_alloc_page_with_initializer 함수에서 전달된 
 * 초기화 콜백을 호출합니다.
 * */

#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* 이 구조체는 수정하지 마세요 */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,  // 페이지가 메모리에 로드될 때 초기화 함수 호출
	.swap_out = NULL,  // 페이지가 메모리에서 교체될 때 함수 없음
	.destroy = uninit_destroy,  // 페이지가 파괴될 때 호출할 함수
	.type = VM_UNINIT,  // 페이지 타입: 초기화되지 않은 페이지
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}
/* 첫 번째 페이지 폴트 시 페이지를 초기화 */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;

	/* 먼저 가져온 후, page_initialize가 값을 덮어쓸 수 있습니다 */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: 이 함수는 수정이 필요할 수 있습니다. */
	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
}

/* uninit_page가 보유한 자원을 해제합니다. 대부분의 페이지는 다른 페이지 객체로 변환되지만, 
 * 프로세스가 종료될 때 uninit 페이지가 남을 수 있으며, 이 페이지는 실행 중에 참조되지 않습니다.
 * 페이지는 호출자에 의해 해제됩니다. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit UNUSED = &page->uninit;
	/* TODO: 이 함수는 구현해야 합니다.
	 * TODO: 만약 해제할 것이 없다면 그냥 리턴합니다. */
}
