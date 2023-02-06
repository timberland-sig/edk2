/*-
 *   pthread_shim.h - Implements header file for pthread shim

 *   Copyright (c) 2020, Dell EMC All rights reserved
 *   SPDX-License-Identifier: BSD-2-Clause-Patent
 */


#ifndef SPDK_PTHREAD_SHIM_H
#define SPDK_PTHREAD_SHIM_H

typedef UINTN pthread_mutex_t;
typedef UINTN pthread_mutexattr_t;

#define __thread        // thread local storage  

#define PTHREAD_MUTEX_INITIALIZER 0

#define PTHREAD_PROCESS_PRIVATE 0
#define PTHREAD_PROCESS_SHARED  1
#define PTHREAD_MUTEX_STALLED   0
#define PTHREAD_MUTEX_ROBUST    1

typedef UINT64 pid_t;

/* Mutex types.  */
enum
{
  PTHREAD_MUTEX_TIMED_NP,
  PTHREAD_MUTEX_RECURSIVE_NP,
  PTHREAD_MUTEX_ERRORCHECK_NP,
  PTHREAD_MUTEX_ADAPTIVE_NP,
  PTHREAD_MUTEX_NORMAL = PTHREAD_MUTEX_TIMED_NP,
  PTHREAD_MUTEX_RECURSIVE = PTHREAD_MUTEX_RECURSIVE_NP,
  PTHREAD_MUTEX_ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK_NP,
  PTHREAD_MUTEX_DEFAULT = PTHREAD_MUTEX_NORMAL
#ifdef __USE_GNU
  /* For compatibility.  */
  , PTHREAD_MUTEX_FAST_NP = PTHREAD_MUTEX_TIMED_NP
#endif
};

int 
pthread_mutex_init (
  pthread_mutex_t           *m,
  const pthread_mutexattr_t *a
  );

int 
pthread_mutex_destroy (
  pthread_mutex_t *m
  );

int pthread_mutex_lock (
  pthread_mutex_t *x
  );

int pthread_mutex_trylock (
  pthread_mutex_t *x
  );

int pthread_mutex_unlock (
  pthread_mutex_t *x
  );

int pthread_mutex_consistent (
  pthread_mutex_t *x
  );

int pthread_mutexattr_init (
  pthread_mutexattr_t *attr
  );

int pthread_mutexattr_destroy (
  pthread_mutexattr_t *attr
  );

int pthread_mutexattr_setpshared (
  pthread_mutexattr_t *attr, 
  int                 pshared
  );

int pthread_mutexattr_setrobust (
  pthread_mutexattr_t *attr, 
  int                 robustness
  );

int pthread_mutexattr_settype (
  pthread_mutexattr_t *attr, 
  int                 type
  );

#endif //SPDK_PTHREAD_SHIM_H
