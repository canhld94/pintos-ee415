#ifndef _PAGE_H_
#define _PAGE_H_
#include "hash.h"

struct page
{
    struct hash_elem hash_elem;
    uint32_t *vaddr; /* Virtual address */
    uint32_t *aux; /* Aux data, depend on segment of vaddr */
};

void page_table_init();
void page_table_insert(const uint32_t *address, uint32_t *aux);
void page_table_remove(struct page *p);
struct page *page_table_lookup(const uint32_t *address);
void page_table_destroy();




#endif // !_PAGE_H_
