#ifndef _PAGE_H_
#define _PAGE_H_
#include "hash.h"
#include "threads/thread.h"


struct page
{
    struct hash_elem hash_elem;
    uint8_t *vaddr; /* Virtual address */
    uint8_t *aux; /* Aux data, depend on segment of vaddr */
};

void page_table_init(struct thread *t);
struct page *page_table_insert(struct thread *t, const uint8_t *address, uint8_t *aux);
void page_table_remove(struct thread *t, struct page *p);
struct page *page_table_lookup(struct thread *t, const uint8_t *address);
void page_table_destroy(struct thread *t);




#endif // !_PAGE_H_
