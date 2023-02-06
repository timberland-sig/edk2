/*-
 *    pthread_shim.c - Shim module for pthread functions for single-threaded systems, such as UEFI

 *   Copyright (c) 2020, Dell EMC All rights reserved
 *   SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "pthread_shim.h"

int
pthread_mutex_init (
  pthread_mutex_t           *__mutex,
  const pthread_mutexattr_t *__mutexattr
  )
{
  return 0;
}

int
pthread_mutex_consistent (
  pthread_mutex_t *__mutex
  )
{
  return 0;
}

int
pthread_mutex_destroy (
  pthread_mutex_t *__mutex
)
{
  return 0;
}

int
pthread_mutex_lock (
  pthread_mutex_t *__mutex
  ) 
{
  return 0;
}

int
pthread_mutex_unlock (
  pthread_mutex_t *__mutex
  ) 
{
  return 0;
}

int
pthread_mutexattr_init (
  pthread_mutexattr_t *__attr
  ) 
{
  return 0;
}

int
pthread_mutexattr_destroy (
  pthread_mutexattr_t *__attr
  ) 
{
  return 0;
}

int
pthread_mutexattr_setpshared (
  pthread_mutexattr_t *__attr,
  int                 __pshared
  )
{
  return 0;
}

int
pthread_mutexattr_settype (
  pthread_mutexattr_t *__attr, 
  int                 __kind
  ) 
{
  return 0;
}

int
pthread_mutexattr_setrobust (
  pthread_mutexattr_t *__attr,
  int                 __robustness
  )
{
  return 0;
}
