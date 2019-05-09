#include "page.h"
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

void page_table_init(struct thread *t)
{
    DBG_MSG_VM("[VM: %s] Init spt of %s\n", thread_name(),t->name);
    t->supp_table = malloc(sizeof(struct hash));
    hash_init(t->supp_table, page_hash, page_less, NULL);
}

void page_table_insert(struct thread *t, const uint8_t *address, uint8_t * aux)
{
    struct page *p = malloc(sizeof(struct page));
    p->vaddr = address;
    p->aux = aux;
    // DBG_MSG_VM("[VM: %s] Insert 0x%x and 0x%x to spt\n", thread_name(), p->vaddr, p->aux);
    struct hash_elem *e = hash_insert(t->supp_table, &p->hash_elem);
}

void page_table_remove(struct thread *t, struct page *p)
{
    ASSERT(hash_delete(t->supp_table, &p->hash_elem) == p);
    free(p);
}

struct page *page_table_lookup(struct thread *t, const uint8_t *address)
{
    struct page p;
    struct hash_elem *e = NULL;
    p.vaddr = address;
    e = hash_find(t->supp_table, &p.hash_elem);
    return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

void page_table_destroy(struct thread *t)
{
    DBG_MSG_VM("[VM: %s] destroy spt\n", thread_name());
    hash_destroy(t->supp_table, page_destructor);
    free(t->supp_table);
}
