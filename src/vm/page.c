#include "page.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "round.h"
#include "threads/pte.h"
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
    t->page_mgm = malloc(sizeof(struct page_mgm));
    t->page_mgm->page_table = malloc(sizeof(struct hash));
    hash_init(t->page_mgm->page_table, page_hash, page_less, NULL);
    lock_init(&t->page_mgm->lock);
}

struct page* page_table_insert(struct thread *t, const uint8_t *address, uint8_t * aux)
{
    struct page *p = malloc(sizeof(struct page));
    p->vaddr = address;
    p->aux = aux;
    // DBG_MSG_VM("[VM: %s] Insert 0x%x and 0x%x to spt\n", thread_name(), p->vaddr, p->aux);
    lock_acquire(&t->page_mgm->lock);
    struct hash_elem *e = hash_insert(t->page_mgm->page_table, &p->hash_elem);
    lock_release(&t->page_mgm->lock);
    if(e != NULL) free(p);
    return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

void page_table_remove(struct thread *t, struct page *p)
{
    ASSERT(hash_delete(t->page_mgm->page_table, &p->hash_elem) == p);
    free(p);
}

struct page *page_table_lookup(struct thread *t, const uint8_t *address)
{
    struct page p;
    struct hash_elem *e = NULL;
    p.vaddr = (uint32_t) address;
    lock_acquire(&t->page_mgm->lock);
    e = hash_find(t->page_mgm->page_table, &p.hash_elem);
    if(e == NULL)
    {
        p.vaddr = (uint32_t) address | PTE_AVL;
        e = hash_find(t->page_mgm->page_table, &p.hash_elem);
    }
    lock_release(&t->page_mgm->lock);
    return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

void page_table_destroy(struct thread *t)
{
    DBG_MSG_VM("[VM: %s] destroy spt\n", thread_name());
    lock_acquire(&t->page_mgm->lock);
    hash_destroy(t->page_mgm->page_table, page_destructor);
    free(t->page_mgm->page_table);
    lock_release(&t->page_mgm->lock);
    free(t->page_mgm);
}
