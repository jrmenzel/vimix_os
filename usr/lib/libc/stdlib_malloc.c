/* SPDX-License-Identifier: MIT */

#include <stdlib.h>
#include <unistd.h>

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header
{
    struct
    {
        union header *ptr;
        uint32_t size;
    } s;
    Align x;
};

typedef union header Header;

static Header g_base_pointer;
static Header *g_free_pointer;

void free(void *ap)
{
    Header *p;

    Header *bp = (Header *)ap - 1;
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
        bp->s.ptr = p->s.ptr->s.ptr;
    }
    else
    {
        bp->s.ptr = p->s.ptr;
    }

    if (p + p->s.size == bp)
    {
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    }
    else
    {
        p->s.ptr = bp;
    }

    g_free_pointer = p;
}

static Header *morecore(uint32_t nu)
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
    free((void *)(hp + 1));
    return g_free_pointer;
}

void *malloc(size_t size_in_bytes)
{
    Header *p = NULL;
    Header *prevp = NULL;

    uint32_t nunits = (size_in_bytes + sizeof(Header) - 1) / sizeof(Header) + 1;

    prevp = g_free_pointer;
    if (prevp == NULL)
    {
        g_base_pointer.s.ptr = g_free_pointer = prevp = &g_base_pointer;
        g_base_pointer.s.size = 0;
    }

    for (p = prevp->s.ptr;; prevp = p, p = p->s.ptr)
    {
        if (p->s.size >= nunits)
        {
            if (p->s.size == nunits)
            {
                prevp->s.ptr = p->s.ptr;
            }
            else
            {
                p->s.size -= nunits;
                p += p->s.size;
                p->s.size = nunits;
            }
            g_free_pointer = prevp;
            return (void *)(p + 1);
        }
        if (p == g_free_pointer)
        {
            p = morecore(nunits);
            if (p == NULL)
            {
                return NULL;
            }
        }
    }
}
