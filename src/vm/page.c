#include "page.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "round.h"
static unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED);
static bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
static void page_destructor(struct hash_elem *e, void *aux UNUSED);

static unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED) 
{
    const struct page *p = hash_entry (p_, struct page, hash_elem);
    return hash_bytes (&p->vaddr, sizeof(p->vaddr));
}

static bool page_less (const struct hash_elem *a_, const struct hash_elem *b_,
                void *aux UNUSED)
{
    const struct page *a = hash_entry (a_, struct page, hash_elem);
    const struct page *b = hash_entry (b_, struct page, hash_elem);
    return (a->vaddr > b->vaddr);
}

static void page_destructor(struct hash_elem *e, void *aux UNUSED)
{
    struct page *p = hash_entry(e, struct page, hash_elem);
    free(p);
}

void page_table_init()
{
    DBG_MSG_VM("[VM: %s] Init spt\n", thread_name());
    thread_current()->supp_table = malloc(sizeof(struct hash));
    hash_init(thread_current()->supp_table, page_hash, page_less, NULL);
}

void page_table_insert(const uint32_t *address, uint32_t * aux)
{
    struct page *p = malloc(sizeof(struct page));
    p->vaddr = address;
    p->aux = aux;
    // DBG_MSG_VM("[VM: %s] Insert 0x%x and 0x%x to spt\n", thread_name(), p->vaddr, p->aux);
    struct hash_elem *e = hash_insert(thread_current()->supp_table, &p->hash_elem);
}

void page_table_remove(struct page *p)
{
    hash_delete(thread_current()->supp_table, &p->hash_elem);
    free(p);
}

struct page *page_table_lookup(const uint32_t *address)
{
    struct page p;
    struct hash_elem *e = NULL;
    p.vaddr = address;
    // DBG_MSG_VM("[VM: %s] hash table: 0x%x\n", thread_name(), thread_current()->supp_table);
    e = hash_find(thread_current()->supp_table, &p.hash_elem);
    // DBG_MSG_VM("[VM: %s] finding addr 0x%x on return 0x%x\n", thread_name(), p.vaddr, e);
    return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

void page_table_destroy()
{
    DBG_MSG_VM("[VM: %s] destroy spt\n", thread_name());
    hash_destroy(thread_current()->supp_table, page_destructor);
    free(thread_current()->supp_table);
}
