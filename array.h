/*
 * BSD-3-Clause
 *
 * Copyright 2021 Ozan Tezcan
 *           2023 Way Yan Win
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ARRAY_H
#define ARRAY_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_VERSION "2.0.0"

#define array_def(T, name)                                                     \
  typedef struct {                                                             \
    bool oom;                                                                  \
    size_t cap;                                                                \
    size_t size;                                                               \
    /* NOLINTNEXTLINE */                                                       \
    T *elems;                                                                  \
  } array_##name;

/**
 * Init array
 * @param a array
 */
#define array_init(a)                                                          \
  do {                                                                         \
    memset((a), 0, sizeof(*(a)));                                              \
  } while (0)

/**
 * Destroy array
 * @param a array
 */
#define array_term(a)                                                          \
  do {                                                                         \
    free((a)->elems);                                                          \
    array_init(a);                                                             \
  } while (0)

/**
 * Add elem to array, call array_oom(v) to see if 'add' failed because of out
 * of memory.
 *
 * @param a array
 * @param k elem
 */
#define array_add(a, k)                                                        \
  do {                                                                         \
    const size_t _max = SIZE_MAX / sizeof(*(a)->elems);                        \
    size_t _cap;                                                               \
    void *_p;                                                                  \
                                                                               \
    if ((a)->cap == (a)->size) {                                               \
      if ((a)->cap > _max / 2) {                                               \
        (a)->oom = true;                                                       \
        break;                                                                 \
      }                                                                        \
      _cap = (a)->cap == 0 ? 8 : (a)->cap * 2;                                 \
      _p = realloc((a)->elems, _cap * sizeof(*((a)->elems)));                  \
      if (_p == NULL) {                                                        \
        (a)->oom = true;                                                       \
        break;                                                                 \
      }                                                                        \
      (a)->cap = _cap;                                                         \
      (a)->elems = _p;                                                         \
    }                                                                          \
    (a)->oom = false;                                                          \
    (a)->elems[(a)->size++] = k;                                               \
  } while (0)

/**
 * Deletes items from the array without deallocating underlying memory
 * @param a array
 */
#define array_clear(a)                                                         \
  do {                                                                         \
    (a)->size = 0;                                                             \
    (a)->oom = false;                                                          \
  } while (0)

/**
 * @param a array
 * @return true if last add operation failed, false otherwise.
 */
#define array_oom(a) ((a)->oom)

/**
 * Get element at index i, if 'i' is out of range, result is undefined.
 *
 * @param a array
 * @param i index
 * @return element at index 'i'
 */
#define array_at(a, i) ((a)->elems[i])

/**
 * @param a array
 * @return element count
 */
#define array_size(a) ((a)->size)

/**
 *   @param a array
 *   @param i element index, If 'i' is out of the range, result is undefined.
 */
#define array_del(a, i)                                                        \
  do {                                                                         \
    size_t idx = (i);                                                          \
    assert(idx < (a)->size);                                                   \
                                                                               \
    const size_t _cnt = (a)->size - (idx)-1;                                   \
    if (_cnt > 0) {                                                            \
      memmove(&((a)->elems[idx]), &((a)->elems[idx + 1]),                      \
              _cnt * sizeof(*((a)->elems)));                                   \
    }                                                                          \
    (a)->size--;                                                               \
  } while (0)

/**
 * Deletes the element at index i, replaces last element with the deleted
 * element unless deleted element is the last element. This is faster than
 * moving elements but elements will no longer be in the 'add order'
 *
 * arr[a,b,c,d,e,f] -> array_del_unordered(arr, 2) - > arr[a,b,f,d,e]
 *
 * @param a array
 * @param i index. If 'i' is out of the range, result is undefined.
 */
#define array_del_unordered(a, i)                                              \
  do {                                                                         \
    size_t idx = (i);                                                          \
    assert(idx < (a)->size);                                                   \
    (a)->elems[idx] = (a)->elems[(--(a)->size)];                               \
  } while (0)

/**
 * Deletes the last element. If current size is zero, result is undefined.
 * @param a array
 */
#define array_del_last(a)                                                      \
  do {                                                                         \
    assert((a)->size != 0);                                                    \
    (a)->size--;                                                               \
  } while (0)

/**
 * Sorts the array using qsort()
 * @param a   array
 * @param cmp comparator, check qsort() documentation for details
 */
#define array_sort(a, cmp)                                                     \
  (qsort((a)->elems, (a)->size, sizeof(*(a)->elems), cmp))

/**
 * Returns last element. If array is empty, result is undefined.
 * @param a array
 */
#define array_last(a) (a)->elems[(a)->size - 1]

/**
 * @param a    array
 * @param elem elem
 */
#define array_foreach(a, elem)                                                 \
  for (size_t _k = 1, _i = 0; _k && _i != (a)->size; _k = !_k, _i++)           \
    for (elem = (a)->elems[_i]; _k; _k = !_k)

/**
 * @param a    array
 * @param elem elem
 */
#define array_enumerate(a, _i, elem)                                           \
  for (size_t _k = 1, _i = 0; _k && _i != (a)->size; _k = !_k, _i++)           \
    for (elem = (a)->elems[_i]; _k; _k = !_k)

#define array_eq(a1, a2, cmp)                                                  \
  ({                                                                           \
    int n;                                                                     \
    bool result = true;                                                        \
    if ((n = array_size((a1))) != array_size((a2)))                            \
      result = false;                                                          \
    else {                                                                     \
      for (int i = 0; i < n; i++)                                              \
        if (!cmp(array_at((a1), i), array_at((a2), i)))                        \
          result = false;                                                      \
    }                                                                          \
    result;                                                                    \
  })

#define array_contains(a, i, T)                                                \
  ({                                                                           \
    bool result = false;                                                       \
    array_foreach((a), T _l) {                                                 \
      if (T##_eq(_l, i))                                                       \
        result = true;                                                         \
    }                                                                          \
    result;                                                                    \
  })

//       (type, name)
array_def(int, int);
array_def(unsigned int, uint);
array_def(long, long);
array_def(long long, ll);
array_def(unsigned long, ulong);
array_def(unsigned long long, ull);
array_def(uint32_t, 32);
array_def(uint64_t, 64);
array_def(double, double);
array_def(const char *, str);
array_def(void *, ptr);

#endif
