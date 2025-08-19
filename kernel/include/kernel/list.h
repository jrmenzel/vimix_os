/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

// circular double linked list, inspired by linux/list.h

struct list_head
{
    struct list_head *next, *prev;
};

/// @brief Inits the list as empty
/// @param head New list to init
static inline void list_init(struct list_head *head)
{
    head->next = head;
    head->prev = head;
}

/// @brief helper, insert new between prev and next
/// @param new New item
/// @param prev Will be prev of the new item
/// @param next Will be next of the new item
static inline void __list_add(struct list_head *new_item,
                              struct list_head *prev, struct list_head *next)
{
    next->prev = new_item;
    new_item->next = next;
    new_item->prev = prev;
    prev->next = new_item;
}

/// @brief Add a new item to the list
/// @param new New item
/// @param head List head, new item will be added after this
static inline void list_add(struct list_head *new_item, struct list_head *head)
{
    __list_add(new_item, head, head->next);
}

/// @brief Add a new item to the end of the list
/// @param new New item
/// @param head List head
static inline void list_add_tail(struct list_head *new_item,
                                 struct list_head *head)
{
    __list_add(new_item, head->prev, head);
}

/// @brief Deletes the entry pointed to from the list
/// @param entry
static inline void list_del(struct list_head *entry)
{
    struct list_head *prev = entry->prev;
    struct list_head *next = entry->next;

    prev->next = next;
    next->prev = prev;

    list_init(entry);  // cleanup for savety
}

/// @brief Returns true if the list is empty
/// @param head The list
/// @return True if empty
static inline int list_empty(const struct list_head *head)
{
    return (head->next == head);
}

// helper for loops, pos is an external temp var
#define list_for_each(pos, head) \
    for (pos = (head)->next; !(pos == (head)); pos = pos->next)
