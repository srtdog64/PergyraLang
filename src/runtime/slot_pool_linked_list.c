#include "slot_pool.h"

#include <stdlib.h>

/*
 * Create pool-based linked list
 */
LinkedList *
LinkedListCreate(size_t capacity)
{
    LinkedList *list;
    
    list = malloc(sizeof(LinkedList));
    if (list == NULL)
        return NULL;
    
    list->nodePool = SlotPoolCreate(sizeof(LinkedListNode), capacity, true);
    if (list->nodePool == NULL) {
        free(list);
        return NULL;
    }
    
    list->head = NULL_INDEX;
    list->tail = NULL_INDEX;
    list->count = 0;
    
    return list;
}

/*
 * Destroy linked list
 */
void
LinkedListDestroy(LinkedList *list)
{
    if (list == NULL)
        return;
    
    if (list->nodePool != NULL)
        SlotPoolDestroy(list->nodePool);
    
    free(list);
}

/*
 * Add node to back of list
 */
PoolIndex
LinkedListPushBack(LinkedList *list, int32_t value)
{
    PoolIndex       newIndex;
    LinkedListNode *newNode;
    LinkedListNode *tailNode;
    
    if (list == NULL)
        return NULL_INDEX;
    
    newIndex = SlotPoolAlloc(list->nodePool);
    if (newIndex == NULL_INDEX)
        return NULL_INDEX;
    
    newNode = (LinkedListNode *)SlotPoolGet(list->nodePool, newIndex);
    newNode->value = value;
    newNode->next = NULL_INDEX;
    newNode->prev = list->tail;
    newNode->generation = 1;
    
    if (list->tail != NULL_INDEX) {
        tailNode = (LinkedListNode *)SlotPoolGet(list->nodePool, list->tail);
        tailNode->next = newIndex;
    } else {
        list->head = newIndex;
    }
    
    list->tail = newIndex;
    list->count++;
    
    return newIndex;
}

/*
 * Add node to front of list
 */
PoolIndex
LinkedListPushFront(LinkedList *list, int32_t value)
{
    PoolIndex       newIndex;
    LinkedListNode *newNode;
    LinkedListNode *headNode;
    
    if (list == NULL)
        return NULL_INDEX;
    
    newIndex = SlotPoolAlloc(list->nodePool);
    if (newIndex == NULL_INDEX)
        return NULL_INDEX;
    
    newNode = (LinkedListNode *)SlotPoolGet(list->nodePool, newIndex);
    newNode->value = value;
    newNode->next = list->head;
    newNode->prev = NULL_INDEX;
    newNode->generation = 1;
    
    if (list->head != NULL_INDEX) {
        headNode = (LinkedListNode *)SlotPoolGet(list->nodePool, list->head);
        headNode->prev = newIndex;
    } else {
        list->tail = newIndex;
    }
    
    list->head = newIndex;
    list->count++;
    
    return newIndex;
}

/*
 * Remove node from list
 */
bool
LinkedListRemove(LinkedList *list, PoolIndex nodeIndex)
{
    LinkedListNode *node;
    LinkedListNode *prevNode;
    LinkedListNode *nextNode;
    
    if (list == NULL || !SlotPoolIsValid(list->nodePool, nodeIndex))
        return false;
    
    node = (LinkedListNode *)SlotPoolGet(list->nodePool, nodeIndex);
    
    /* Update previous node */
    if (node->prev != NULL_INDEX) {
        prevNode = (LinkedListNode *)SlotPoolGet(list->nodePool, node->prev);
        prevNode->next = node->next;
    } else {
        list->head = node->next;
    }
    
    /* Update next node */
    if (node->next != NULL_INDEX) {
        nextNode = (LinkedListNode *)SlotPoolGet(list->nodePool, node->next);
        nextNode->prev = node->prev;
    } else {
        list->tail = node->prev;
    }
    
    /* Free the node */
    SlotPoolFree(list->nodePool, nodeIndex);
    list->count--;
    
    return true;
}

/*
 * Traverse list and call visitor function
 */
void
LinkedListTraverse(LinkedList *list, void (*visitor)(int32_t value))
{
    PoolIndex       current;
    LinkedListNode *node;
    
    if (list == NULL || visitor == NULL)
        return;
    
    current = list->head;
    while (current != NULL_INDEX) {
        node = (LinkedListNode *)SlotPoolGet(list->nodePool, current);
        visitor(node->value);
        current = node->next;
    }
}

/*
 * Get node by index
 */
LinkedListNode *
LinkedListGetNode(LinkedList *list, PoolIndex index)
{
    if (list == NULL || !SlotPoolIsValid(list->nodePool, index))
        return NULL;
    
    return (LinkedListNode *)SlotPoolGet(list->nodePool, index);
}
