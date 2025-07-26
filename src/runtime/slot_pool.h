/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Pergyra Language Project nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PERGYRA_SLOT_POOL_H
#define PERGYRA_SLOT_POOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Pool index type for efficient indexing
 */
typedef uint32_t PoolIndex;

#define NULL_INDEX ((PoolIndex)-1)
#define INVALID_INDEX ((PoolIndex)-2)

/*
 * Generic slot pool for homogeneous data structures
 * Level 1: High-performance pool-based allocation
 */
typedef struct
{
    void       *data;           /* Raw data array */
    size_t      elementSize;    /* Size of each element */
    size_t      capacity;       /* Maximum number of elements */
    size_t      count;          /* Current number of allocated elements */
    bool       *occupied;       /* Occupancy bitmap */
    PoolIndex  *freeList;       /* Free index stack */
    size_t      freeListTop;    /* Top of free list stack */
    
    /* Performance optimization */
    bool        cacheOptimized; /* Memory layout optimized for cache */
    size_t      cacheLineSize;  /* Cache line size (typically 64 bytes) */
    
    /* Statistics */
    uint64_t    totalAllocations;
    uint64_t    totalDeallocations;
    uint64_t    peakUsage;
} SlotPool;

/*
 * Smart slot types for complex ownership relationships
 * Level 2: Reference counting and ownership management
 */
typedef enum
{
    SLOT_OWNED,    /* Unique ownership (like unique_ptr) */
    SLOT_SHARED,   /* Shared ownership (like shared_ptr) */
    SLOT_WEAK      /* Weak reference (like weak_ptr) */
} SlotType;

/*
 * Smart slot handle with reference counting
 */
typedef struct
{
    uint32_t    slotId;
    SlotType    slotType;
    uint32_t    refCount;      /* For shared slots */
    uint32_t    weakCount;     /* For weak reference tracking */
    uint32_t    generation;    /* ABA problem prevention */
    void       *data;          /* Pointer to actual data */
} SmartSlot;

/*
 * Pool-based linked list node
 * Optimized for cache-friendly traversal
 */
typedef struct
{
    int32_t     value;         /* Node data */
    PoolIndex   next;          /* Index to next node */
    PoolIndex   prev;          /* Index to previous node */
    uint32_t    generation;    /* For safety */
} LinkedListNode;

/*
 * Pool-based tree node
 * Using weak references to prevent cycles
 */
typedef struct
{
    int32_t     value;         /* Node data */
    int32_t     height;        /* For AVL balancing */
    PoolIndex   left;          /* Left child index */
    PoolIndex   right;         /* Right child index */
    PoolIndex   parent;        /* Parent index (weak reference) */
    uint32_t    generation;
} TreeNode;

/*
 * Graph node for complex relationships
 * Level 3: Hybrid approach
 */
typedef struct
{
    uint32_t    nodeId;        /* Unique node identifier */
    void       *data;          /* Node data */
    PoolIndex  *edges;         /* Array of edge indices */
    size_t      edgeCount;     /* Number of edges */
    size_t      edgeCapacity;  /* Edge array capacity */
    uint32_t    generation;
} GraphNode;

/*
 * Graph edge structure
 */
typedef struct
{
    PoolIndex   fromNode;      /* Source node index */
    PoolIndex   toNode;        /* Target node index */
    float       weight;        /* Edge weight */
    uint32_t    generation;
} GraphEdge;

/*
 * Complete graph structure using hybrid pools
 */
typedef struct
{
    SlotPool   *nodePool;      /* Pool for graph nodes */
    SlotPool   *edgePool;      /* Pool for graph edges */
    uint32_t   *nodeMap;       /* Hash map: nodeId -> poolIndex */
    size_t      nodeMapSize;   /* Size of hash map */
    
    /* Statistics */
    uint64_t    totalNodes;
    uint64_t    totalEdges;
} Graph;

/*
 * SlotPool operations
 */
SlotPool   *SlotPoolCreate(size_t elementSize, size_t capacity, bool cacheOptimized);
void        SlotPoolDestroy(SlotPool *pool);
PoolIndex   SlotPoolAlloc(SlotPool *pool);
bool        SlotPoolFree(SlotPool *pool, PoolIndex index);
void       *SlotPoolGet(SlotPool *pool, PoolIndex index);
bool        SlotPoolIsValid(SlotPool *pool, PoolIndex index);
void        SlotPoolPrintStats(const SlotPool *pool);

/*
 * Smart slot operations
 */
SmartSlot  *SmartSlotCreateOwned(void *data, size_t dataSize);
SmartSlot  *SmartSlotCreateShared(void *data, size_t dataSize);
SmartSlot  *SmartSlotCreateWeak(SmartSlot *shared);
SmartSlot  *SmartSlotClone(SmartSlot *slot);
bool        SmartSlotUpgrade(SmartSlot *weakSlot);
void        SmartSlotDestroy(SmartSlot *slot);
bool        SmartSlotIsValid(const SmartSlot *slot);

/*
 * LinkedList operations using SlotPool
 */
typedef struct
{
    SlotPool   *nodePool;
    PoolIndex   head;
    PoolIndex   tail;
    size_t      count;
} LinkedList;

LinkedList     *LinkedListCreate(size_t capacity);
void            LinkedListDestroy(LinkedList *list);
PoolIndex       LinkedListPushBack(LinkedList *list, int32_t value);
PoolIndex       LinkedListPushFront(LinkedList *list, int32_t value);
bool            LinkedListRemove(LinkedList *list, PoolIndex nodeIndex);
void            LinkedListTraverse(LinkedList *list, void (*visitor)(int32_t value));
LinkedListNode *LinkedListGetNode(LinkedList *list, PoolIndex index);

/*
 * AVL Tree operations using SlotPool
 */
typedef struct
{
    SlotPool   *nodePool;
    PoolIndex   root;
    size_t      count;
} AVLTree;

AVLTree    *AVLTreeCreate(size_t capacity);
void        AVLTreeDestroy(AVLTree *tree);
PoolIndex   AVLTreeInsert(AVLTree *tree, int32_t value);
bool        AVLTreeRemove(AVLTree *tree, int32_t value);
PoolIndex   AVLTreeFind(AVLTree *tree, int32_t value);
void        AVLTreeTraverseInOrder(AVLTree *tree, void (*visitor)(int32_t value));
TreeNode   *AVLTreeGetNode(AVLTree *tree, PoolIndex index);

/*
 * Graph operations using hybrid pools
 */
Graph      *GraphCreate(size_t maxNodes, size_t maxEdges);
void        GraphDestroy(Graph *graph);
PoolIndex   GraphAddNode(Graph *graph, uint32_t nodeId, void *data);
PoolIndex   GraphAddEdge(Graph *graph, uint32_t fromId, uint32_t toId, float weight);
bool        GraphRemoveNode(Graph *graph, uint32_t nodeId);
bool        GraphRemoveEdge(Graph *graph, PoolIndex edgeIndex);
PoolIndex   GraphFindNode(Graph *graph, uint32_t nodeId);
void        GraphTraverseBFS(Graph *graph, uint32_t startNodeId, 
                            void (*visitor)(uint32_t nodeId, void *data));
void        GraphTraverseDFS(Graph *graph, uint32_t startNodeId,
                            void (*visitor)(uint32_t nodeId, void *data));

/*
 * Performance testing and benchmarking
 */
typedef struct
{
    double      allocationTime;    /* Average allocation time (ns) */
    double      accessTime;        /* Average access time (ns) */
    double      traversalTime;     /* Average traversal time (ns) */
    size_t      cacheHits;         /* Cache hits during operations */
    size_t      cacheMisses;       /* Cache misses during operations */
    double      memoryUtilization; /* Memory utilization percentage */
} PerformanceMetrics;

PerformanceMetrics BenchmarkLinkedList(size_t nodeCount, size_t iterations);
PerformanceMetrics BenchmarkAVLTree(size_t nodeCount, size_t iterations);
PerformanceMetrics BenchmarkGraph(size_t nodeCount, size_t edgeCount, size_t iterations);

/*
 * Utility functions
 */
uint64_t GetTimestampNs(void);
void     PrefetchMemory(const void *ptr, size_t size);
bool     IsAlignedToCache(const void *ptr);

#endif /* PERGYRA_SLOT_POOL_H */