#include "phys_alloc.h"

/* Very small free-list of 4 KiB pages. This allocator expects that the caller passed a contiguous
   region of memory to manage (base,length). We implement a trivial singly-linked free-list. */

typedef struct free_node { struct free_node *next; } free_node_t;

static uintptr_t phys_base = 0;
static size_t phys_len = 0;
static free_node_t *free_list = NULL;

void phys_init(uintptr_t base, size_t length) {
    phys_base = base;
    phys_len = length & ~0xFFF;
    free_list = NULL;

    /* populate free list with 4KiB entries */
    uintptr_t p = phys_base;
    while (p + 4096 <= phys_base + phys_len) {
        free_node_t *n = (free_node_t*) (uintptr_t) p;
        n->next = free_list;
        free_list = n;
        p += 4096;
    }
}

void *phys_alloc_page(void) {
    if (!free_list) return NULL;
    free_node_t *n = free_list;
    free_list = n->next;
    return (void*) (uintptr_t) n;
}
void phys_free_page(void *page) {
    free_node_t *n = (free_node_t*) page;
    n->next = free_list;
    free_list = n;
}

/* kmalloc: very tiny page-granular allocator */
void *kmalloc(size_t size) {
    size_t pages = (size + 4095) / 4096;
    if (pages == 0) return NULL;
    if (pages == 1) return phys_alloc_page();
    /* allocate 'pages' and coalesce contiguous blocks (not implemented). For simplicity, return NULL for >1 page. */
    return NULL;
}
void kfree(void *ptr) {
    if (!ptr) return;
    phys_free_page(ptr);
}
