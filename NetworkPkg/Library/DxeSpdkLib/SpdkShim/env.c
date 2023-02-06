/*-
 *   env.c - Implements SPDK env for EDK2 enviornment

 *   Copyright (c) 2020, Dell EMC All rights reserved
 *   SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "spdk/stdinc.h"
#include "spdk/util.h"
#include "spdk/env.h"

#include "nvme_internal.h"

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/TimerLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#define SPDK_ALLOC_SIGNATURE SIGNATURE_32('S','P','D','K')
typedef struct {
  UINT32    Signature;
  UINTN     Size;
} SPDK_ALLOC_HEAD;

#define SPDK_ALLOC_OVERHEAD sizeof(SPDK_ALLOC_HEAD)

void *
spdk_malloc(
  size_t   size,
  size_t   align,
  uint64_t *phys_addr,
  int      socket_id,
  uint32_t flags
)
{
  SPDK_ALLOC_HEAD  *PoolHdr;
  UINTN            NewSize = 0;
  VOID             *Data;

  NewSize = (UINTN)(size)+SPDK_ALLOC_OVERHEAD;
  Data = AllocatePool(NewSize);
  if (Data != NULL) {
    PoolHdr = (SPDK_ALLOC_HEAD *)Data;
    PoolHdr->Signature = SPDK_ALLOC_SIGNATURE;
    PoolHdr->Size = size;
    return (VOID *)(PoolHdr + 1);
  }
  return NULL;
}

void *
spdk_zmalloc (
  size_t   size, 
  size_t   align, 
  uint64_t *phys_addr, 
  int      socket_id,
  uint32_t flags
  )
{
  void *buf = spdk_malloc (size, align, phys_addr, socket_id, flags);
  if (buf) {
    memset (buf, 0, size);
  }
  return buf;
}

void *
spdk_realloc (
  void   *buf,
  size_t Size,
  size_t align
)
{
  SPDK_ALLOC_HEAD  *OldPoolHdr;
  SPDK_ALLOC_HEAD  *NewPoolHdr;
  VOID             *Data;
  UINTN            OldSize;

  Size = (UINTN)Size + SPDK_ALLOC_OVERHEAD;
  Data = AllocatePool (Size);

if (Data != NULL) {
  NewPoolHdr = (SPDK_ALLOC_HEAD *)Data;
  NewPoolHdr->Signature = SPDK_ALLOC_SIGNATURE;
  NewPoolHdr->Size = Size;
  if (buf != NULL) {
    OldPoolHdr = (SPDK_ALLOC_HEAD *)buf - 1;
    ASSERT (OldPoolHdr->Signature == SPDK_ALLOC_SIGNATURE);
    OldSize = OldPoolHdr->Size;
    CopyMem ((VOID *)(NewPoolHdr + 1), buf, MIN (OldSize, Size));
    FreePool ((VOID *)OldPoolHdr);
  }
  return (VOID *)(NewPoolHdr + 1);
}
return NULL;
}

void
spdk_free (
  void *buf
  )
{
  if (buf != NULL) {
    buf = (char*)buf - SPDK_ALLOC_OVERHEAD;
    FreePool (buf);
  }
}

uint64_t
spdk_vtophys (
  const void     *buf, 
  uint64_t *size
  )
{
  return (uintptr_t)buf;
}

void *
spdk_dma_malloc_socket (
  size_t   size, 
  size_t   align, 
  uint64_t *phys_addr,
  int      socket_id
  )
{
  return spdk_malloc (size, align, phys_addr, socket_id, (SPDK_MALLOC_DMA | SPDK_MALLOC_SHARE));
}

void *
spdk_dma_zmalloc_socket (
  size_t   size, 
  size_t   align, 
  uint64_t *phys_addr, 
  int      socket_id)
{
  return spdk_zmalloc (size, align, phys_addr, socket_id, (SPDK_MALLOC_DMA | SPDK_MALLOC_SHARE));
}

void *
spdk_dma_malloc (
  size_t   size, 
  size_t   align, 
  uint64_t *phys_addr
  )
{
    return spdk_dma_malloc_socket (size, align, phys_addr, SPDK_ENV_SOCKET_ID_ANY);
}

void *
spdk_dma_zmalloc (
  size_t   size, 
  size_t   align, 
  uint64_t *phys_addr
  )
{
    return spdk_dma_zmalloc_socket (size, align, phys_addr, SPDK_ENV_SOCKET_ID_ANY);
}

void
spdk_dma_free (
  void *buf
  )
{
    FreePool (buf);
}

void *
spdk_memzone_reserve_aligned (
  const char *name, 
  size_t len, 
  int socket_id,
  unsigned flags, 
  unsigned align
  )
{
  return AllocatePool (len);
}

void *
spdk_memzone_reserve (
  const char *name,
  size_t     len, 
  int        socket_id, 
  unsigned   flags
  )
{
  return spdk_memzone_reserve_aligned (name, len, socket_id, flags, 64);
}

void *
spdk_memzone_lookup (
  const char *name
  )
{
  return NULL;
}

int
spdk_memzone_free (
  const char *name
  )
{
  return -1;
}

void
spdk_memzone_dump (
  FILE *f
  )
{
  return;
}

bool
spdk_process_is_primary (
  void
  )
{
  return 1;
}

uint64_t spdk_get_ticks (
  void
  )
{
  return 1;
}

uint64_t 
spdk_get_ticks_hz (
  void
  )
{
  //similar to emulator
  return 1000000000ULL;
}

void
spdk_delay_us (
  unsigned int us
  )
{
  gBS->Stall (us);
}

void 
spdk_pause (
  void
  )
{
  return;
}

void
spdk_unaffinitize_thread (
  void
  )
{
  return;
}

void *
spdk_call_unaffinitized (
  void *cb (void *arg), 
  void *arg
  )
{
  return NULL;
}

struct spdk_ring_ele {
  void *ele;
  TAILQ_ENTRY (spdk_ring_ele) link;
};

struct spdk_ring {
  TAILQ_HEAD (, spdk_ring_ele) elements;
  pthread_mutex_t lock;
  size_t          count;
};

struct spdk_ring *
spdk_ring_create (
  enum spdk_ring_type type, 
  size_t              count, 
  int                 socket_id
  )
{
  struct spdk_ring *ring = NULL;

  ring = AllocatePool (sizeof(*ring));
  if (ring == NULL) {
      return NULL;
  }

  if (pthread_mutex_init (&ring->lock, NULL)) {
      free(ring);
      return NULL;
  }

  TAILQ_INIT (&ring->elements);
  return ring;
}

void
spdk_ring_free (
  struct spdk_ring *ring
  )
{
  struct spdk_ring_ele *ele = NULL, *tmp = NULL;

  if (ring == NULL) {
      return;
  }

  TAILQ_FOREACH_SAFE (ele, &ring->elements, link, tmp) {
      free (ele);
  }

  pthread_mutex_destroy (&ring->lock);
  free (ring);
}

size_t
spdk_ring_enqueue (
  struct spdk_ring *ring, 
  void             **objs, 
  size_t           count,
  size_t           *free_space
  )
{
  struct spdk_ring_ele *ele = NULL;
  size_t i;
 
  pthread_mutex_lock (&ring->lock);
 
  for (i = 0; i < count; i++) {
      ele = AllocatePool (sizeof (*ele));
      if (!ele) {
          break;
      }
 
      ele->ele = objs[i];
      TAILQ_INSERT_TAIL (&ring->elements, ele, link);
      ring->count++;
  }
 
  pthread_mutex_unlock (&ring->lock);
  return i;
}

size_t
spdk_ring_dequeue (
  struct spdk_ring *ring, 
  void **objs, 
  size_t count
  )
{
  struct spdk_ring_ele *ele = NULL, *tmp = NULL;
  size_t i = 0;

  if (count == 0) {
      return 0;
  }

  pthread_mutex_lock (&ring->lock);

  TAILQ_FOREACH_SAFE (ele, &ring->elements, link, tmp) {
      TAILQ_REMOVE (&ring->elements, ele, link);
      ring->count--;
      objs[i] = ele->ele;
      free (ele);
      i++;
      if (i >= count) {
          break;
      }
  }

  pthread_mutex_unlock (&ring->lock);
  return i;
}

size_t
spdk_ring_count (
  struct spdk_ring *ring
  )
{
  return ring->count;
}

void
spdk_env_dpdk_dump_mem_stats ( 
  FILE *file
  )
{
  return;
}

int
spdk_pci_addr_compare (
  const struct spdk_pci_addr *a1,
  const struct spdk_pci_addr *a2
  )
{
  return 0;
}

int
spdk_pci_addr_parse (
  struct spdk_pci_addr *addr, 
  const char *bdf
  )
{
  return 0;
}

void spdk_pci_device_detach (
  struct spdk_pci_device *device
  )
{
  return;
}
