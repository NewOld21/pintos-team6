#include "list.h"
#include "../debug.h"

/* Our doubly linked lists have two header elements: the "head"
   just before the first element and the "tail" just after the
   last element.  The `prev' link of the front header is null, as
   is the `next' link of the back header.  Their other two links
   point toward each other via the interior elements of the list.

   An empty list looks like this:

   +------+     +------+
   <---| head |<--->| tail |--->
   +------+     +------+

   A list with two elements in it looks like this:

   +------+     +-------+     +-------+     +------+
   <---| head |<--->|   1   |<--->|   2   |<--->| tail |<--->
   +------+     +-------+     +-------+     +------+

   The symmetry of this arrangement eliminates lots of special
   cases in list processing.  For example, take a look at
   list_remove(): it takes only two pointer assignments and no
   conditionals.  That's a lot simpler than the code would be
   without header elements.

   (Because only one of the pointers in each header element is used,
   we could in fact combine them into a single header element
   without sacrificing this simplicity.  But using two separate
   elements allows us to do a little bit of checking on some
   operations, which can be valuable.) */

static bool is_sorted (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) UNUSED;

/* Returns true if ELEM is a head, false otherwise. */
/*
 * 함수명    : is_head
 * 설명      : 주어진 리스트 요소(elem)가 리스트의 head인지 확인한다.
 *             head 노드는 prev가 NULL이고 next는 NULL이 아닌 특성을 가진다.
 *             (주의: PintOS에서는 실제 head와 tail은 dummy 요소임)
 * 인자      : struct list_elem *elem - 확인할 리스트 노드
 * 반환값    : head이면 true, 그렇지 않으면 false
 * 관련 함수 : 없음
 * 주의 사항 : list 구조체 안의 head는 dummy 노드이며, 실제 데이터는 head->next부터 시작됨
 */
static inline bool
is_head (struct list_elem *elem) {
	return elem != NULL && elem->prev == NULL && elem->next != NULL;
}

/* Returns true if ELEM is an interior element,
   false otherwise. */
/*
 * 함수명    : is_interior
 * 설명      : 주어진 리스트 요소(elem)가 리스트 내부의 실제 노드인지 확인한다.
 *             내부 노드는 prev와 next가 모두 NULL이 아닌 요소이며,
 *             리스트의 head 및 tail(dummy 노드)이 아닌 일반 노드를 의미한다.
 * 인자      : struct list_elem *elem - 확인할 리스트 노드
 * 반환값    : 내부 노드이면 true, 그렇지 않으면 false
 * 관련 함수 : list_insert(), list_remove()
 * 주의 사항 : head와 tail은 dummy 노드이며, 그 사이에 있는 노드만 실제 데이터 요소이다.
 */

static inline bool
is_interior (struct list_elem *elem) {
	return elem != NULL && elem->prev != NULL && elem->next != NULL;
}

/* Returns true if ELEM is a tail, false otherwise. */

/*
 * 함수명    : is_tail
 * 설명      : 주어진 리스트 요소(elem)가 리스트의 tail(dummy)인지 확인한다.
 *             tail 노드는 리스트의 끝을 나타내며, next가 NULL인 것이 특징이다.
 * 인자      : struct list_elem *elem - 확인할 리스트 노드
 * 반환값    : tail이면 true, 그렇지 않으면 false
 * 관련 함수 : 없음
 * 주의 사항 : PintOS의 리스트 구조에서 tail은 dummy 노드이며,
 *             실제 데이터는 tail 바로 앞의 노드들이다.
 */
static inline bool
is_tail (struct list_elem *elem) {
	return elem != NULL && elem->prev != NULL && elem->next == NULL;
}

/* Initializes LIST as an empty list. */

/*
 * 함수명    : list_init
 * 설명      : 주어진 리스트를 초기화한다.
 * 인자      : struct list *list - 초기화할 리스트 주소
 * 반환값    : 없음
 * 관련 함수 : 없음
 * 주의 사항 : 초기상태는 head와 tail노드를 서로 연결해줌
 */
void
list_init (struct list *list) {
	ASSERT (list != NULL);
	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;
}

/* Returns the beginning of LIST.  */

/*
 * 함수명    : list_begin
 * 설명      : 주어진 리스트의 첫 번째 실제 노드(즉, head 다음 노드)를 반환한다.
 *             리스트가 비어 있다면 list_end()와 같은 값을 반환한다.
 * 인자      : struct list *list - 대상 리스트
 * 반환값    : 리스트의 첫 번째 요소 (head 다음 노드)
 * 관련 함수 : 없음
 * 주의 사항 : 리스트가 비어 있는 경우, 반환값은 list_end()와 같아진다.
 */
struct list_elem *
list_begin (struct list *list) {
	ASSERT (list != NULL);
	return list->head.next;
}

/* Returns the element after ELEM in its list.  If ELEM is the
   last element in its list, returns the list tail.  Results are
   undefined if ELEM is itself a list tail. */

/*
 * 함수명    : list_next
 * 설명      : 주어진 리스트 요소(elem)의 다음 요소를 반환한다.
 *             만약 elem이 리스트의 마지막 요소라면 tail(dummy)을 반환한다.
 * 인자      : struct list_elem *elem - 다음 요소를 찾을 대상 노드
 * 반환값    : elem의 다음 요소
 * 관련 함수 : 없음
 * 주의 사항 : elem이 tail인 경우 동작이 정의되지 않음 (사용 금지)
 */ 
struct list_elem *
list_next (struct list_elem *elem) {
	ASSERT (is_head (elem) || is_interior (elem));
	return elem->next;
}


/*
 * 함수명    : list_end
 * 설명      : 주어진 리스트의 끝을 나타내는 tail(dummy) 노드의 주소를 반환한다.
 *             이 값은 리스트 순회 시 종료 조건으로 사용된다.
 * 인자      : struct list *list - 대상 리스트
 * 반환값    : 리스트의 tail 노드 주소
 * 관련 함수 : 없음
 * 주의 사항 : 리스트의 실제 마지막 요소는 list_end()의 직전 노드다.
 */
struct list_elem *
list_end (struct list *list) {
	ASSERT (list != NULL);
	return &list->tail;
}

/* Returns the LIST's reverse beginning, for iterating through
   LIST in reverse order, from back to front. */

/*
 * 함수명    : list_rbegin
 * 설명      : 리스트의 마지막 실제 요소를 반환한다.
 *             이는 tail(dummy)의 바로 앞 노드이다.
 *             리스트가 비어 있으면 head(dummy)를 반환한다.
 * 인자      : struct list *list - 대상 리스트
 * 반환값    : 리스트의 마지막 요소 (tail 이전 노드)
 * 관련 함수 : 없음
 * 주의 사항 : 리스트가 비어 있으면 head == tail.prev 이다.
 */ 
struct list_elem *
list_rbegin (struct list *list) {
	ASSERT (list != NULL);
	return list->tail.prev;
}

/* Returns the element before ELEM in its list.  If ELEM is the
   first element in its list, returns the list head.  Results are
   undefined if ELEM is itself a list head. */


/*
 * 함수명    : list_prev
 * 설명      : 주어진 리스트 요소(elem)의 이전 요소를 반환한다.
 * 인자      : struct list_elem *elem - 대상 리스트 노드
 * 반환값    : elem 바로 앞에 있는 리스트 노드
 * 관련 함수 : 없음
 * 주의 사항 : elem이 head인 경우 동작이 정의되지 않음 (사용 금지)
 */   
struct list_elem *
list_prev (struct list_elem *elem) {
	ASSERT (is_interior (elem) || is_tail (elem));
	return elem->prev;
}

/* Returns LIST's head.

   list_rend() is often used in iterating through a list in
   reverse order, from back to front.  Here's typical usage,
   following the example from the top of list.h:

   for (e = list_rbegin (&foo_list); e != list_rend (&foo_list);
   e = list_prev (e))
   {
   struct foo *f = list_entry (e, struct foo, elem);
   ...do something with f...
   }
   */

/*
 * 함수명    : list_rend
 * 설명      : 역방향 순회 시 종료 조건으로 사용되는 head(dummy)의 주소를 반환한다.
 * 인자      : struct list *list - 대상 리스트
 * 반환값    : 리스트의 head 노드 주소
 * 관련 함수 : 없음
 * 주의 사항 : 리스트의 시작(dummy head)은 실제 데이터가 아님
 */
struct list_elem *
list_rend (struct list *list) {
	ASSERT (list != NULL);
	return &list->head;
}

/* Return's LIST's head.

   list_head() can be used for an alternate style of iterating
   through a list, e.g.:

   e = list_head (&list);
   while ((e = list_next (e)) != list_end (&list))
   {
   ...
   }
   */

/*
 * 함수명    : list_head
 * 설명      : 리스트의 시작점인 head(dummy)의 주소를 반환한다.
 * 인자      : struct list *list - 대상 리스트
 * 반환값    : 리스트의 head 노드 주소
 * 관련 함수 : 없음
 * 주의 사항 : head는 실제 데이터가 아닌 dummy 노드다.
 */
struct list_elem *
list_head (struct list *list) {
	ASSERT (list != NULL);
	return &list->head;
}

/* Return's LIST's tail. */

/*
 * 함수명    : list_tail
 * 설명      : 리스트의 끝을 나타내는 tail(dummy) 노드의 주소를 반환한다.
 * 인자      : struct list *list - 대상 리스트
 * 반환값    : 리스트의 tail 노드 주소
 * 관련 함수 : 없음
 * 주의 사항 : tail은 실제 데이터가 아닌 dummy 노드이며,
 *             마지막 실제 요소는 tail의 직전 노드이다.
 */
struct list_elem *
list_tail (struct list *list) {
	ASSERT (list != NULL);
	return &list->tail;
}

/* Inserts ELEM just before BEFORE, which may be either an
   interior element or a tail.  The latter case is equivalent to
   list_push_back(). */

/*
 * 함수명    : list_insert
 * 설명      : 지정한 리스트에서 before 요소 앞에 새로운 요소 elem을 삽입한다.
 *             즉, elem은 before의 이전 노드와 before 사이에 위치하게 된다.
 * 인자      : struct list_elem *before - 삽입할 위치 (기준 노드)
 *             struct list_elem *elem - 삽입할 새로운 노드
 * 반환값    : 없음
 * 관련 함수 : 없음
 * 주의 사항 : before와 elem 모두 유효한 포인터여야 하며,
 *             before는 반드시 리스트 내부의 노드여야 한다.
 */
void
list_insert (struct list_elem *before, struct list_elem *elem) {
	ASSERT (is_interior (before) || is_tail (before));
	ASSERT (elem != NULL);

	elem->prev = before->prev;
	elem->next = before;
	before->prev->next = elem;
	before->prev = elem;
}

/* Removes elements FIRST though LAST (exclusive) from their
   current list, then inserts them just before BEFORE, which may
   be either an interior element or a tail. */

/*
 * 함수명    : list_splice
 * 설명      : first부터 last 직전까지의 노드들을 현재 위치(before) 앞에 모두 삽입한다.
 *             first ~ last 구간은 원래 리스트에서 깨끗하게 잘려 나와 새로운 위치로 이동된다.
 * 인자      : struct list_elem *before - 삽입 기준 위치 (그 앞에 구간이 붙는다)
 *             struct list_elem *first - 이동할 구간의 시작 노드
 *             struct list_elem *last  - 이동할 구간의 끝 직후 노드 (last 자체는 포함되지 않음)
 * 반환값    : 없음
 * 관련 함수 : list_prev()
 * 주의 사항 : first == last인 경우 아무 작업도 수행하지 않음
 */

void
list_splice (struct list_elem *before,
		struct list_elem *first, struct list_elem *last) {
	ASSERT (is_interior (before) || is_tail (before));
	if (first == last)
		return;
	last = list_prev (last);

	ASSERT (is_interior (first));
	ASSERT (is_interior (last));

	/* Cleanly remove FIRST...LAST from its current list. */
	first->prev->next = last->next;
	last->next->prev = first->prev;

	/* Splice FIRST...LAST into new list. */
	first->prev = before->prev;
	last->next = before;
	before->prev->next = first;
	before->prev = last;
}

/* Inserts ELEM at the beginning of LIST, so that it becomes the
   front in LIST. */

/*
 * 함수명    : list_push_front
 * 설명      : 리스트의 가장 앞에 새로운 요소를 삽입한다.
 *             결과적으로 삽입된 요소는 첫 번째 실제 노드가 된다.
 * 인자      : struct list *list - 삽입 대상 리스트
 *             struct list_elem *elem - 삽입할 요소
 * 반환값    : 없음
 * 관련 함수 : list_insert()
 * 주의 사항 : 없음
 */

void
list_push_front (struct list *list, struct list_elem *elem) {
	list_insert (list_begin (list), elem);
}

/* Inserts ELEM at the end of LIST, so that it becomes the
   back in LIST. */

/*
 * 함수명    : list_push_back
 * 설명      : 리스트의 가장 끝에 새로운 요소를 삽입한다.
 *             결과적으로 삽입된 요소는 마지막 실제 노드가 된다.
 * 인자      : struct list *list - 삽입 대상 리스트
 *             struct list_elem *elem - 삽입할 요소
 * 반환값    : 없음
 * 관련 함수 : list_insert()
 * 주의 사항 : 없음
 */
void
list_push_back (struct list *list, struct list_elem *elem) {
	list_insert (list_end (list), elem);
}

/* Removes ELEM from its list and returns the element that
   followed it.  Undefined behavior if ELEM is not in a list.

   It's not safe to treat ELEM as an element in a list after
   removing it.  In particular, using list_next() or list_prev()
   on ELEM after removal yields undefined behavior.  This means
   that a naive loop to remove the elements in a list will fail:

 ** DON'T DO THIS **
 for (e = list_begin (&list); e != list_end (&list); e = list_next (e))
 {
 ...do something with e...
 list_remove (e);
 }
 ** DON'T DO THIS **

 Here is one correct way to iterate and remove elements from a
list:

for (e = list_begin (&list); e != list_end (&list); e = list_remove (e))
{
...do something with e...
}

If you need to free() elements of the list then you need to be
more conservative.  Here's an alternate strategy that works
even in that case:

while (!list_empty (&list))
{
struct list_elem *e = list_pop_front (&list);
...do something with e...
}
*/

/*
 * 함수명    : list_remove
 * 설명      : 주어진 리스트 요소(elem)를 리스트에서 제거하고,
 *             그 다음 요소를 반환한다.
 * 인자      : struct list_elem *elem - 제거할 요소
 * 반환값    : elem 바로 다음 요소
 * 관련 함수 : 없음
 * 주의 사항 : 제거 후 해당 elem 포인터를 사용하면 동작이 정의되지 않음.
 */
struct list_elem *
list_remove (struct list_elem *elem) {
	ASSERT (is_interior (elem));
	elem->prev->next = elem->next;
	elem->next->prev = elem->prev;
	return elem->next;
}

/* Removes the front element from LIST and returns it.
   Undefined behavior if LIST is empty before removal. */

/*
 * 함수명    : list_pop_front
 * 설명      : 리스트의 첫 번째 실제 요소를 제거하고 반환한다.
 * 인자      : struct list *list - 대상 리스트
 * 반환값    : 제거된 요소의 포인터
 * 관련 함수 : list_front(), list_remove()
 * 주의 사항 : 리스트가 비어 있으면 동작이 정의되지 않음
 */
struct list_elem *
list_pop_front (struct list *list) {
	struct list_elem *front = list_front (list);
	list_remove (front);
	return front;
}

/* Removes the back element from LIST and returns it.
   Undefined behavior if LIST is empty before removal. */
/*
 * 함수명    : list_pop_back
 * 설명      : 리스트의 마지막 실제 요소를 제거하고 반환한다.
 * 인자      : struct list *list - 대상 리스트
 * 반환값    : 제거된 요소의 포인터
 * 관련 함수 : list_back(), list_remove()
 * 주의 사항 : 리스트가 비어 있으면 동작이 정의되지 않음
 */

struct list_elem *
list_pop_back (struct list *list) {
	struct list_elem *back = list_back (list);
	list_remove (back);
	return back;
}

/* Returns the front element in LIST.
   Undefined behavior if LIST is empty. */
/*
 * 함수명    : list_front
 * 설명      : 리스트의 첫 번째 실제 요소를 반환한다.
 * 인자      : struct list *list - 대상 리스트
 * 반환값    : 리스트의 첫 노드 (head 다음 요소)
 * 관련 함수 : 없음
 * 주의 사항 : 리스트가 비어 있으면 동작이 정의되지 않음
 */
struct list_elem *
list_front (struct list *list) {
	ASSERT (!list_empty (list));
	return list->head.next;
}

/* Returns the back element in LIST.
   Undefined behavior if LIST is empty. */
/*
 * 함수명    : list_back
 * 설명      : 리스트의 마지막 실제 요소를 반환한다.
 * 인자      : struct list *list - 대상 리스트
 * 반환값    : 리스트의 마지막 노드 (tail 이전 요소)
 * 관련 함수 : 없음
 * 주의 사항 : 리스트가 비어 있으면 동작이 정의되지 않음
 */
struct list_elem *
list_back (struct list *list) {
	ASSERT (!list_empty (list));
	return list->tail.prev;
}

/* Returns the number of elements in LIST.
   Runs in O(n) in the number of elements. */

/*
 * 함수명    : list_size
 * 설명      : 리스트 내 요소의 개수를 반환한다.
 *             내부적으로 전체를 순회하며 카운트하므로 O(n) 시간 복잡도를 가진다.
 * 인자      : struct list *list - 대상 리스트
 * 반환값    : 리스트의 요소 개수 (size_t)
 * 관련 함수 : list_begin(), list_end(), list_next()
 * 주의 사항 : 없음
 */
size_t
list_size (struct list *list) {
	struct list_elem *e;
	size_t cnt = 0;

	for (e = list_begin (list); e != list_end (list); e = list_next (e))
		cnt++;
	return cnt;
}

/* Returns true if LIST is empty, false otherwise. */
/*
 * 함수명    : list_empty
 * 설명      : 리스트가 비어 있는지 여부를 반환한다.
 * 인자      : struct list *list - 검사 대상 리스트
 * 반환값    : 비어 있으면 true, 아니면 false
 * 관련 함수 : list_begin(), list_end()
 * 주의 사항 : 없음
 */
bool
list_empty (struct list *list) {
	return list_begin (list) == list_end (list);
}

/* Swaps the `struct list_elem *'s that A and B point to. */
/*
 * 함수명    : swap
 * 설명      : 두 포인터가 가리키는 list_elem 포인터의 값을 서로 교환한다.
 *             주로 링크 구조(예: prev, next)를 바꾸는 데 사용된다.
 * 인자      : struct list_elem **a - 바꿀 첫 번째 요소의 주소
 *             struct list_elem **b - 바꿀 두 번째 요소의 주소
 * 반환값    : 없음
 * 관련 함수 : 없음
 * 주의 사항 : 두 포인터는 유효한 list_elem을 가리켜야 한다.
 */
static void
swap (struct list_elem **a, struct list_elem **b) {
	struct list_elem *t = *a;
	*a = *b;
	*b = t;
}

/* Reverses the order of LIST. */
/*
 * 함수명    : list_reverse
 * 설명      : 리스트의 모든 요소의 순서를 반대로 뒤집는다.
 *             각 노드의 prev와 next 포인터를 모두 바꾸고,
 *             리스트의 head와 tail 방향도 반대로 전환한다.
 * 인자      : struct list *list - 뒤집을 리스트
 * 반환값    : 없음
 * 관련 함수 : swap()
 * 주의 사항 : 리스트가 비어 있으면 아무 작업도 수행하지 않음
 */
void
list_reverse (struct list *list) {
	if (!list_empty (list)) {
		struct list_elem *e;

		for (e = list_begin (list); e != list_end (list); e = e->prev)
			swap (&e->prev, &e->next);
		swap (&list->head.next, &list->tail.prev);
		swap (&list->head.next->prev, &list->tail.prev->next);
	}
}

/* Returns true only if the list elements A through B (exclusive)
   are in order according to LESS given auxiliary data AUX. */
/*
 * 함수명    : is_sorted
 * 설명      : 주어진 리스트 구간 [a, b) 가 less 비교 기준에 따라 정렬되어 있는지 확인한다.
 *             순회 중에 정렬 기준이 깨지는 요소가 발견되면 false를 반환한다.
 * 인자      : struct list_elem *a - 시작 노드 (포함)
 *             struct list_elem *b - 종료 노드 (포함하지 않음)
 *             list_less_func *less - 비교 함수 포인터
 *             void *aux - 비교 함수에 전달할 부가 정보
 * 반환값    : 정렬 상태면 true, 아니면 false
 * 관련 함수 : 없음
 * 주의 사항 : a == b인 경우, 즉 비교할 노드가 없으면 true 반환
 */
static bool
is_sorted (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) {
	if (a != b)
		while ((a = list_next (a)) != b)
			if (less (a, list_prev (a), aux))
				return false;
	return true;
}

/* Finds a run, starting at A and ending not after B, of list
   elements that are in nondecreasing order according to LESS
   given auxiliary data AUX.  Returns the (exclusive) end of the
   run.
   A through B (exclusive) must form a non-empty range. */

/*
 * 함수명    : find_end_of_run
 * 설명      : 리스트 구간 [a, b)에서 less 기준에 따라 정렬된 구간을 찾는다.
 *             첫 번째 정렬되지 않은 노드에서 중단하며 해당 노드를 반환한다.
 * 인자      : struct list_elem *a - 시작 노드 (포함)
 *             struct list_elem *b - 종료 노드 (포함하지 않음)
 *             list_less_func *less - 비교 함수 포인터
 *             void *aux - 비교 함수에 전달할 부가 정보
 * 반환값    : 정렬이 유지되는 마지막 노드의 다음 노드 (b 이전까지 순회)
 * 관련 함수 : 없음
 * 주의 사항 : 입력 구간 [a, b)는 비어 있어선 안 되며, less 함수는 NULL이 아니어야 한다.
 */
static struct list_elem *
find_end_of_run (struct list_elem *a, struct list_elem *b,
		list_less_func *less, void *aux) {
	ASSERT (a != NULL);
	ASSERT (b != NULL);
	ASSERT (less != NULL);
	ASSERT (a != b);

	do {
		a = list_next (a);
	} while (a != b && !less (a, list_prev (a), aux));
	return a;
}

/* Merges A0 through A1B0 (exclusive) with A1B0 through B1
   (exclusive) to form a combined range also ending at B1
   (exclusive).  Both input ranges must be nonempty and sorted in
   nondecreasing order according to LESS given auxiliary data
   AUX.  The output range will be sorted the same way. */

/*
 * 함수명    : inplace_merge
 * 설명      : 정렬된 두 개의 구간 [a0, a1b0)과 [a1b0, b1)를 비교 함수 less 기준에 따라 병합한다.
 *             병합은 리스트 요소들을 직접 재배치하는 방식으로 in-place로 수행된다.
 * 인자      : struct list_elem *a0 - 첫 번째 정렬 구간의 시작 노드 (포함)
 *             struct list_elem *a1b0 - 두 번째 정렬 구간의 시작 노드 (a0 구간의 끝, b0 구간의 시작 / 포함)
 *             struct list_elem *b1 - 전체 병합 종료 지점 (포함하지 않음)
 *             list_less_func *less - 비교 기준 함수 포인터
 *             void *aux - 비교 함수에 전달할 부가 정보
 * 반환값    : 없음
 * 관련 함수 : is_sorted(), list_next(), list_splice(), list_prev()
 * 주의 사항 : 두 정렬 구간은 모두 정렬 상태여야 하며, [a0, b1) 구간은 비어 있어선 안 됨.
 */
static void
inplace_merge (struct list_elem *a0, struct list_elem *a1b0,
		struct list_elem *b1,
		list_less_func *less, void *aux) {
	ASSERT (a0 != NULL);
	ASSERT (a1b0 != NULL);
	ASSERT (b1 != NULL);
	ASSERT (less != NULL);
	ASSERT (is_sorted (a0, a1b0, less, aux));
	ASSERT (is_sorted (a1b0, b1, less, aux));

	while (a0 != a1b0 && a1b0 != b1)
		if (!less (a1b0, a0, aux))
			a0 = list_next (a0);
		else {
			a1b0 = list_next (a1b0);
			list_splice (a0, list_prev (a1b0), a1b0);
		}
}

/* Sorts LIST according to LESS given auxiliary data AUX, using a
   natural iterative merge sort that runs in O(n lg n) time and
   O(1) space in the number of elements in LIST. */

/*
 * 함수명    : list_sort
 * 설명      : 리스트를 비교 함수 less 기준에 따라 정렬한다.
 *             내부적으로 자연 병합 정렬(natural merge sort)을 사용하며, 시간 복잡도는 O(n log n),
 *             추가 공간은 O(1)로 작동한다.
 * 인자      : struct list *list - 정렬 대상 리스트
 *             list_less_func *less - 비교 기준 함수 포인터
 *             void *aux - 비교 함수에 전달할 부가 정보
 * 반환값    : 없음
 * 관련 함수 : list_begin(), list_end(), find_end_of_run(), inplace_merge()
 * 주의 사항 : 리스트는 NULL이 아니어야 하며, less 함수도 반드시 유효해야 한다.
 */
void
list_sort (struct list *list, list_less_func *less, void *aux) {
	size_t output_run_cnt;        /* Number of runs output in current pass. */

	ASSERT (list != NULL);
	ASSERT (less != NULL);

	/* Pass over the list repeatedly, merging adjacent runs of
	   nondecreasing elements, until only one run is left. */
	do {
		struct list_elem *a0;     /* Start of first run. */
		struct list_elem *a1b0;   /* End of first run, start of second. */
		struct list_elem *b1;     /* End of second run. */

		output_run_cnt = 0;
		for (a0 = list_begin (list); a0 != list_end (list); a0 = b1) {
			/* Each iteration produces one output run. */
			output_run_cnt++;

			/* Locate two adjacent runs of nondecreasing elements
			   A0...A1B0 and A1B0...B1. */
			a1b0 = find_end_of_run (a0, list_end (list), less, aux);
			if (a1b0 == list_end (list))
				break;
			b1 = find_end_of_run (a1b0, list_end (list), less, aux);

			/* Merge the runs. */
			inplace_merge (a0, a1b0, b1, less, aux);
		}
	}
	while (output_run_cnt > 1);

	ASSERT (is_sorted (list_begin (list), list_end (list), less, aux));
}

/* Inserts ELEM in the proper position in LIST, which must be
   sorted according to LESS given auxiliary data AUX.
   Runs in O(n) average case in the number of elements in LIST. */

/*
 * 함수명    : list_insert_ordered
 * 설명      : 리스트 내에서 비교 함수 less 기준에 따라 요소를 정렬된 위치에 삽입한다.
 * 인자      : struct list *list - 삽입 대상 리스트
 *             struct list_elem *elem - 삽입할 요소
 *             list_less_func *less - 비교 함수 포인터
 *             void *aux - 비교 함수에 전달할 부가 정보
 * 반환값    : 없음
 * 관련 함수 : list_begin(), list_end(), list_next(), list_insert()
 * 주의 사항 : 리스트가 정렬 상태여야 하며, less는 NULL이 아니어야 한다.
 */   
void // ### 이걸로 list 정렬하자 ### //
list_insert_ordered (struct list *list, struct list_elem *elem,
		list_less_func *less, void *aux) {
	struct list_elem *e;

	ASSERT (list != NULL);
	ASSERT (elem != NULL);
	ASSERT (less != NULL);

	for (e = list_begin (list); e != list_end (list); e = list_next (e))
		if (less (elem, e, aux))
			break;
	return list_insert (e, elem);
}

/* Iterates through LIST and removes all but the first in each
   set of adjacent elements that are equal according to LESS
   given auxiliary data AUX.  If DUPLICATES is non-null, then the
   elements from LIST are appended to DUPLICATES. */

/*
 * 함수명    : list_unique
 * 설명      : 정렬된 리스트에서 인접한 중복 요소를 제거한다.
 *             비교 기준은 less 함수로 수행되며, 중복으로 판단된 요소는 삭제된다.
 *             duplicates가 NULL이 아닌 경우, 삭제된 요소를 해당 리스트에 저장한다.
 * 인자      : struct list *list - 중복 제거 대상 리스트 (정렬 상태여야 함)
 *             struct list *duplicates - 제거된 요소를 저장할 리스트 (NULL 가능)
 *             list_less_func *less - 비교 함수 포인터
 *             void *aux - 비교 함수에 전달할 부가 정보
 * 반환값    : 없음
 * 관련 함수 : list_next(), list_remove(), list_push_back()
 * 주의 사항 : 리스트는 less 기준으로 정렬되어 있어야 하며,
 *             같은 요소인지 확인하기 위해 (a < b == false) && (b < a == false) 조건을 사용한다.
 */
void
list_unique (struct list *list, struct list *duplicates,
		list_less_func *less, void *aux) {
	struct list_elem *elem, *next;

	ASSERT (list != NULL);
	ASSERT (less != NULL);
	if (list_empty (list))
		return;

	elem = list_begin (list);
	while ((next = list_next (elem)) != list_end (list))
		if (!less (elem, next, aux) && !less (next, elem, aux)) {
			list_remove (next);
			if (duplicates != NULL)
				list_push_back (duplicates, next);
		} else
			elem = next;
}

/* Returns the element in LIST with the largest value according
   to LESS given auxiliary data AUX.  If there is more than one
   maximum, returns the one that appears earlier in the list.  If
   the list is empty, returns its tail. */
/*
 * 함수명    : list_max
 * 설명      : 주어진 리스트에서 less 비교 기준에 따라 가장 큰 값을 가진 요소를 반환한다.
 *             최대값 비교는 사용자가 넘긴 less 함수에 따라 수행되며,
 *             동일한 값이 여러 개일 경우 가장 나중에 등장하는 요소가 반환된다.
 * 인자      : struct list *list - 검색 대상 리스트
 *             list_less_func *less - 비교 기준 함수
 *             void *aux - 비교 함수에 전달할 부가 정보
 * 반환값    : 최대 요소의 포인터 (리스트가 비어 있으면 list_end() 반환)
 * 관련 함수 : list_begin(), list_end(), list_next()
 * 주의 사항 : 리스트가 비어 있는 경우 list_end()를 반환하므로 반드시 확인 필요
 */

struct list_elem *
list_max (struct list *list, list_less_func *less, void *aux) {
	struct list_elem *max = list_begin (list);
	if (max != list_end (list)) {
		struct list_elem *e;

		for (e = list_next (max); e != list_end (list); e = list_next (e))
			if (less (max, e, aux))
				max = e;
	}
	return max;
}

/* Returns the element in LIST with the smallest value according
   to LESS given auxiliary data AUX.  If there is more than one
   minimum, returns the one that appears earlier in the list.  If
   the list is empty, returns its tail. */

/*
 * 함수명    : list_min
 * 설명      : 주어진 리스트에서 less 비교 기준에 따라 가장 작은 값을 가진 요소를 반환한다.
 *             동일한 최소값이 여러 개 있을 경우, 가장 먼저 등장하는 요소가 반환된다.
 * 인자      : struct list *list - 검색 대상 리스트
 *             list_less_func *less - 비교 기준 함수
 *             void *aux - 비교 함수에 전달할 부가 정보
 * 반환값    : 최소 요소의 포인터 (리스트가 비어 있으면 list_end() 반환)
 * 관련 함수 : list_begin(), list_end(), list_next()
 * 주의 사항 : 리스트가 비어 있는 경우 list_end()를 반환하므로 반드시 확인 필요
 */
struct list_elem *
list_min (struct list *list, list_less_func *less, void *aux) {
	struct list_elem *min = list_begin (list);
	if (min != list_end (list)) {
		struct list_elem *e;

		for (e = list_next (min); e != list_end (list); e = list_next (e))
			if (less (e, min, aux))
				min = e;
	}
	return min;
}
