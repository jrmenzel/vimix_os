/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

// typedef size_t Align;

union header
{
    struct
    {
        union header *ptr;
        size_t size;
    } s;
    // Align x;
};

typedef union header Header;

const Header *MAGIC_VALUE = (const Header *)0x42F00;

static Header g_base_pointer;
static Header *g_free_pointer = NULL;

void t(Header *p)
{
    if (p->s.size > 4096)
    {
        p->s.size++;
        p->s.size--;
    }
}

void free(void *ap)
{
    if (ap == NULL) return;

    Header *bp = (Header *)ap - 1;
    if (bp->s.ptr != MAGIC_VALUE)
    {
        // not a pointer allocated by malloc()
        return;
    }

    Header *p;
    for (p = g_free_pointer; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    {
        if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
        {
            break;
        }
    }

    if (bp + bp->s.size == p->s.ptr)
    {
        bp->s.size += p->s.ptr->s.size;
        t(bp);
        bp->s.ptr = p->s.ptr->s.ptr;
    }
    else
    {
        bp->s.ptr = p->s.ptr;
    }

    if (p + p->s.size == bp)
    {
        p->s.size += bp->s.size;
        t(p);
        p->s.ptr = bp->s.ptr;
    }
    else
    {
        p->s.ptr = bp;
    }

    g_free_pointer = p;
}

static Header *morecore(size_t nu)
{
    if (nu < 4096)
    {
        nu = 4096;
    }

    char *p = sbrk(nu * sizeof(Header));
    if (p == (char *)-1)
    {
        return NULL;
    }

    Header *hp = (Header *)p;
    hp->s.size = nu;
    hp->s.ptr = (Header *)MAGIC_VALUE;
    t(hp);
    free((void *)(hp + 1));
    return g_free_pointer;
}

void *malloc(size_t size_in_bytes)
{
    if (size_in_bytes == 0) return NULL;
    Header *p = NULL;
    Header *prevp = NULL;

    size_t nunits = (size_in_bytes + sizeof(Header) - 1) / sizeof(Header) + 1;

    prevp = g_free_pointer;
    if (prevp == NULL)
    {
        g_base_pointer.s.ptr = g_free_pointer = prevp = &g_base_pointer;
        g_base_pointer.s.size = 0;
    }

    for (p = prevp->s.ptr;; prevp = p, p = p->s.ptr)
    {
        t(p);
        if (p->s.size >= nunits)
        {
            if (p->s.size == nunits)
            {
                prevp->s.ptr = p->s.ptr;
            }
            else
            {
                p->s.size -= nunits;
                t(p);
                p += p->s.size;
                p->s.size = nunits;
                t(p);
            }
            g_free_pointer = prevp;
            p->s.ptr = (Header *)MAGIC_VALUE;
            return (void *)(p + 1);
        }
        if (p == g_free_pointer)
        {
            p = morecore(nunits);
            if (p == NULL)
            {
                errno = ENOMEM;
                return NULL;
            }
        }
    }
}

void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
    {
        return malloc(size);
    }

    Header *bp = (Header *)ptr - 1;
    size_t old_size = (bp->s.size - 1) * sizeof(Header);

    if (size == old_size) return ptr;

    void *new_ptr = malloc(size);
    if (new_ptr == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    size_t min = (size > old_size) ? old_size : size;
    memmove(new_ptr, ptr, min);
    free(ptr);

    return new_ptr;
}
