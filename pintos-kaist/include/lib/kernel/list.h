#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

/* Doubly linked list.
 *
 * This implementation of a doubly linked list does not require
 * use of dynamically allocated memory.  Instead, each structure
 * that is a potential list element must embed a struct list_elem
 * member.  All of the list functions operate on these `struct
 * list_elem's.  The list_entry macro allows conversion from a
 * struct list_elem back to a structure object that contains it.

 * For example, suppose there is a needed for a list of `struct
 * foo'.  `struct foo' should contain a `struct list_elem'
 * member, like so:

 * struct foo {
 *   struct list_elem elem;
 *   int bar;
 *   ...other members...
 * };

 * Then a list of `struct foo' can be be declared and initialized
 * like so:

 * struct list foo_list;

 * list_init (&foo_list);

 * Iteration is a typical situation where it is necessary to
 * convert from a struct list_elem back to its enclosing
 * structure.  Here's an example using foo_list:

 * struct list_elem *e;

 * for (e = list_begin (&foo_list); e != list_end (&foo_list);
 * e = list_next (e)) {
 *   struct foo *f = list_entry (e, struct foo, elem);
 *   ...do something with f...
 * }

 * You can find real examples of list usage throughout the
 * source; for example, malloc.c, palloc.c, and thread.c in the
 * threads directory all use lists.

 * The interface for this list is inspired by the list<> template
 * in the C++ STL.  If you're familiar with list<>, you should
 * find this easy to use.  However, it should be emphasized that
 * these lists do *no* type checking and can't do much other
 * correctness checking.  If you screw up, it will bite you.

 * Glossary of list terms:

 * - "front": The first element in a list.  Undefined in an
 * empty list.  Returned by list_front().

 * - "back": The last element in a list.  Undefined in an empty
 * list.  Returned by list_back().

 * - "tail": The element figuratively just after the last
 * element of a list.  Well defined even in an empty list.
 * Returned by list_end().  Used as the end sentinel for an
 * iteration from front to back.

 * - "beginning": In a non-empty list, the front.  In an empty
 * list, the tail.  Returned by list_begin().  Used as the
 * starting point for an iteration from front to back.

 * - "head": The element figuratively just before the first
 * element of a list.  Well defined even in an empty list.
 * Returned by list_rend().  Used as the end sentinel for an
 * iteration from back to front.

 * - "reverse beginning": In a non-empty list, the back.  In an
 * empty list, the head.  Returned by list_rbegin().  Used as
 * the starting point for an iteration from back to front.
 *
 * - "interior element": An element that is not the head or
 * tail, that is, a real list element.  An empty list does
 * not have any interior elements.*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 리스트에 들어갈 요소(노드)
struct list_elem {
	struct list_elem *prev;     /* Previous list element. */
	struct list_elem *next;     /* Next list element. */
};

// 리스트 
struct list {
	struct list_elem head;      /* List head. */
	struct list_elem tail;      /* List tail. */
};

/* Converts pointer to list element LIST_ELEM into a pointer to
   the structure that LIST_ELEM is embedded inside.  Supply the
   name of the outer structure STRUCT and the member name MEMBER
   of the list element.  See the big comment at the top of the
   file for an example. */

/*
 * 함수명    : list_entry
 * 설명      : 리스트 요소(list_elem *)가 속한 원래 구조체의 포인터를 반환한다.
 *             구조체 내부에 포함된 list_elem의 위치를 기준으로,
 *             전체 구조체의 시작 주소를 역산하여 해당 구조체의 포인터를 얻는다.
 *             예를 들어 list_elem이 struct thread의 elem 필드라면,
 *             list_entry를 통해 해당 thread 구조체 전체의 주소를 복원할 수 있다.
 * 인자      : struct list_elem *elem - 구조체 안에 포함된 리스트 노드 포인터
 *             STRUCT_TYPE - 포함하고 있는 전체 구조체의 타입
 *             MEMBER_NAME - 구조체 안에서 list_elem이 선언된 멤버 이름
 * 반환값    : STRUCT_TYPE* - 원래 구조체의 시작 주소를 가리키는 포인터
 * 주의 사항 : 내부적으로 offsetof 매크로를 사용하므로,
 *             list_elem은 구조체 내부의 정확한 필드명을 지정해야 함.
 * 
 * 매크로명   : offsetof = 구조체 내부에서 특정 멤버가 구조체 시작 주소로부터
 *                         몇 바이트 떨어져 있는지를 계산한다.
 */

#define list_entry(LIST_ELEM, STRUCT, MEMBER)           \
	((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
		- offsetof (STRUCT, MEMBER.next)))

void list_init (struct list *);

/* List traversal. */
struct list_elem *list_begin (struct list *);
struct list_elem *list_next (struct list_elem *);
struct list_elem *list_end (struct list *);

struct list_elem *list_rbegin (struct list *);
struct list_elem *list_prev (struct list_elem *);
struct list_elem *list_rend (struct list *);

struct list_elem *list_head (struct list *);
struct list_elem *list_tail (struct list *);

/* List insertion. */
void list_insert (struct list_elem *, struct list_elem *);
void list_splice (struct list_elem *before,
		struct list_elem *first, struct list_elem *last);
void list_push_front (struct list *, struct list_elem *);
void list_push_back (struct list *, struct list_elem *);

/* List removal. */
struct list_elem *list_remove (struct list_elem *);
struct list_elem *list_pop_front (struct list *);
struct list_elem *list_pop_back (struct list *);

/* List elements. */
struct list_elem *list_front (struct list *);
struct list_elem *list_back (struct list *);

/* List properties. */
size_t list_size (struct list *);
bool list_empty (struct list *);

/* Miscellaneous. */
void list_reverse (struct list *);

/* Compares the value of two list elements A and B, given
   auxiliary data AUX.  Returns true if A is less than B, or
   false if A is greater than or equal to B. */
/*
 * 타입명    : list_less_func
 * 설명      : 리스트 정렬 또는 삽입 시 사용할 비교 함수의 타입(형식)을 정의한 typedef이다.
 *             이 타입은 특정 정렬 기준을 정의한 함수를 함수 포인터로 전달할 수 있도록 한다.
 *             실제 비교 로직은 이 타입을 따르는 사용자가 정의한 함수에서 수행된다.
 * 인자      : const struct list_elem *a - 비교할 첫 번째 리스트 요소
 *             const struct list_elem *b - 비교할 두 번째 리스트 요소
 *             void *aux - 비교에 필요한 부가 데이터 (예: 우선순위 기준 등)
 * 반환값    : bool - a가 b보다 앞에 와야 하면 true, 그렇지 않으면 false
 * 사용 함수 : list_insert_ordered(), list_sort(), list_entry()
 * 주의 사항 : 이 타입은 비교 함수의 ‘형태’를 정의한 것이며,
 *             실제 비교 로직은 이 타입을 기반으로 작성된 함수에서 구현되어야 한다.
 */
typedef bool list_less_func (const struct list_elem *a,
                             const struct list_elem *b,
                             void *aux);

/* Operations on lists with ordered elements. */
void list_sort (struct list *,
                list_less_func *, void *aux);
void list_insert_ordered (struct list *, struct list_elem *,
                          list_less_func *, void *aux);
void list_unique (struct list *, struct list *duplicates,
                  list_less_func *, void *aux);

/* Max and min. */
struct list_elem *list_max (struct list *, list_less_func *, void *aux);
struct list_elem *list_min (struct list *, list_less_func *, void *aux);

#endif /* lib/kernel/list.h */
