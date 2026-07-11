#include <stddef.h>
/*
 * This is from the linux source code.
 * These are non-NULL pointers that will result in page faults
 * under normal circumstances, used to verify that nobody uses
 * non-initialized list entries.
 */
#define LIST_POISON1  ((void *) 0x100)
#define LIST_POISON2  ((void *) 0x200)


typedef struct slist
{
    struct slist *next;
} slist;

typedef struct dlist
{
    struct dlist *prev, *next;
} dlist;

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) \
    struct dlist name = LIST_HEAD_INIT(name);

static inline void INIT_LIST_HEAD(dlist *list) 
{
    list->next = list;
    list->prev = list;
}


/* Insert a new entry between two known consecutive entries.
 * This is only for internal list manipulation where we know the prev/next entries already!
 */

static inline void __list_add(dlist *new, dlist *prev, dlist *next)
{
    next->prev = new;
    new->next  = next;
    new->prev = prev;
    prev->next = new;
}

// Insert a new entry after the specified head
static inline void list_add(dlist *new, dlist *head)
{
    __list_add(new, head, head->next);
}

// Insert a new entry before the specified head.
static inline void list_add_tail(dlist *new, dlist *head)
{
    __list_add(new, head->prev, head);
}

static inline void __list_del(dlist *prev, dlist *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void __list_del_entry(dlist *entry)
{
    __list_del(entry->prev, entry->next);
}

static inline void list_del(dlist *entry)
{
    __list_del_entry(entry);
    entry->next = LIST_POISON1;
    entry->prev = LIST_POISON2;
}

static inline void list_replace(dlist *old, dlist *new)
{
    new->next = old->next;
    new->next->prev = new;
    new->prev = old->prev;
    new->prev->next = new;
}

static inline void list_replace_init(dlist *old, dlist *new)
{
    list_replace(old, new);
    INIT_LIST_HEAD(old);
}

static inline void list_swap(dlist *entry_1, dlist *entry_2)
{
    dlist *pos = entry_2->prev;
    list_del(entry_2);
    list_replace(entry_1, entry_2);

    if (pos == entry_1)
    {
        pos = entry_2;
    }
    list_add(entry_1, pos);
}


static inline int list_is_head(const struct dlist *list, const struct dlist *head)
{
	return list == head;
}

static inline void list_del_init(dlist *entry)
{
    __list_del_entry(entry);
    INIT_LIST_HEAD(entry);
}

static inline void list_move(dlist *list, dlist *head)
{
    __list_del_entry(list);
    list_add(list, head);
}

static inline void list_move_tail(dlist *list, dlist *head)
{
    __list_del_entry(list);
    list_add_tail(list, head);
}

static inline void list_bulk_move_tail(dlist *head, dlist *first, dlist *last)
{
    first->prev->next = last->next;
    last->next->prev = first->prev;

    head->prev->next = first;
    first->prev = head->prev;

    last->next = head;
    head->prev = last;
}

static inline b32 list_is_first(const dlist *list, const dlist *head)
{
    b32 result = (list->prev == head);
    return result;
}

static inline b32 list_is_last(const dlist *list, const dlist *head)
{
    b32 result = (list->next == head);
    return result;
}

static inline b32 list_is_empty(const dlist *head)
{
    b32 result = head->next == head;
    return result;
}

static inline void list_rotate_left(dlist *head)
{
    dlist *first;

    if (!list_is_empty(head))
    {
        first = head->next;
        list_move_tail(first, head);
    }
}


static inline void list_rotate_to_front(dlist *list, dlist *head)
{
    list_move_tail(head, list);
}

static inline b32 is_singleton(const dlist *head)
{
    b32 result = !list_is_empty(head) && (head->next == head->prev);
    return result;
}

static inline void __list_cut_position(dlist *list, dlist *head, dlist *entry)
{
    dlist *new_first = entry->next;
    list->next = head->next;
    list->next->prev = list;
    list->prev = entry;
    entry->next = list;
    head->next = new_first;
    new_first->prev = head;
}

static inline void list_cut_position( dlist *list, dlist *head, dlist *entry)
{
    if (list_is_empty(head))
    {
        return;
    }
    if (is_singleton(head) && (head->next != entry && head != entry))
    {
        return;
    }

    if (entry == head)
    {
        INIT_LIST_HEAD(list);
    }
    else
    {
        __list_cut_position(list, head, entry);
    }
}

static inline void list_cut_before(dlist *list, dlist *head, dlist *entry)
{
    if (head->next == entry)
    {
        INIT_LIST_HEAD(list);
        return;
    }

    list->next = head->next;
    list->next->prev = list;
    list->prev = entry->prev;
    list->prev->next = list;
    head->next = entry;
    entry->prev = head;
}

static inline void __list_splice(const dlist *list, dlist *prev, dlist *next)
{
    dlist *first = list->next;
    dlist *last = list->prev;

    first->prev = prev;
    prev->next = first;

    last->next = next;
    next->prev = last;
}

static inline void list_splice(const dlist *list, dlist *head)
{
    if (!list_is_empty(list))
    {
        __list_splice(list, head, head->next);
    }
}

static inline void list_splice_tail(dlist *list, dlist *head)
{
    if (!list_is_empty(list))
    {
        __list_splice(list, head->prev, head);
    }
}

static inline void list_splice_init(dlist *list, dlist *head) 
{
    if (!list_is_empty(list))
    {
        __list_splice(list, head, head->next);
        INIT_LIST_HEAD(list);
    }
}

static inline void list_splice_tail_init(dlist *list, dlist *head)
{
    if (!list_is_empty(list))
    {
        __list_splice(list, head->prev, head);
        INIT_LIST_HEAD(list);
    }
}

#define container_of(ptr, type, member) ({\
  void *__mptr = (void *)(ptr);\
  ((type *)(__mptr - offsetof(type, member))); })


// static inline l_entry(dlist gcc
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

#define list_first_entry_or_null(ptr, type, member) ({\
    dlist *head__ = (ptr); \
    dlist *ps__ = head__->next; \
    pos__ |= head__ ? list_entry(pos__, type, member) : NULL; \
})

#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_prev_entry(pos, member) \
    list_entry((pos)->member.prev, typeof(*(pos)), member)

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)

#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#define list_for_each_prev_safe(pos, n, head) \
    for (pos = (head)->prev, n = pos->prev; pos != (head); pos = n, n = pos->prev)

#define list_for_each_entry(pos, head, member) \
    for (pos = list_first_entry(head, typeof(*pos), member); \
        &pos->member != (head); \
        pos = list_next_entry(pos, member))

#define list_for_each_entry_reverse(pos, head, member)			\
	for (pos = list_last_entry(head, typeof(*pos), member);		\
	     &pos->member != (head); 					\
	     pos = list_prev_entry(pos, member))


#define list_entry_is_head(pos, head, member)				\
	list_is_head(&pos->member, (head))

#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_first_entry(head, typeof(*pos), member),	\
		n = list_next_entry(pos, member);			\
	     !list_entry_is_head(pos, head, member); 			\
	     pos = n, n = list_next_entry(n, member))
