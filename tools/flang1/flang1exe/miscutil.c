/*
 * Copyright (c) 2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/** \file
    \brief Compiler miscellaneous utility programs.
 */

#include "gbldefs.h"
#include "global.h"
#include "error.h"

#include <stdbool.h>
#include "flang/ArgParser/xflag.h"

/**
   \brief Allocate space for and make new filename using mkperm.
 */
char *
mkfname(char *oldname, char *oldsuf, char *newsuf)
{
  char *p;

  /*  get enough space for oldname, newsuf, '.', '\0', and 1 extra: */
  p = getitem(8, strlen(oldname) + strlen(newsuf) + 3);
  strcpy(p, oldname);
  return (mkperm(p, oldsuf, newsuf));
}

LOGICAL
is_xflag_bit(int indx)
{
  return is_xflag_bitvector(indx);
}

/** \brief Called only from main() */
void
set_xflag(int indx, INT val)
{
  set_xflag_value(flg.x, indx, val);
  /* XXX Unexpected side effect: "set x flag" should not be upping opt level */
  if (indx == 9 && flg.opt < 2) /* max cnt for unroller */
    flg.opt = 2;
}

/** \brief Called only from main() */
void
set_yflag(int indx, INT val)
{
  unset_xflag_value(flg.x, indx, val);
}

void
fprintf_str_esc_backslash(FILE *f, char *str)
{
  int ch;
  fputc('"', f);
  while ((ch = *str++)) {
    fputc(ch, f);
    if (ch == '\\')
      fputc('\\', f);
  }
  fputc('"', f);
}

/*
 * error message
 */
static void
invalid_size(char* funcname, int dtsize, int size, char* stgname)
{
  static char *message = "%s: STG %s has invalid datatype size (%d) or structure size(%d)";
  char *msg = (char *)malloc(strlen(message) + strlen(funcname) + strlen(stgname) + 20);
  sprintf(msg, message, funcname, stgname, dtsize, size);
  interr(msg, 0, 4);
} /* invalid_size */

/*
 * memory management
 *  allocate STG data structure, set the appropriate fields
 *  element zero is reserved, so stg_avail is initialized to 1
 */
static void
stg_alloc_base(STG *stg, int dtsize, int size, char *name)
{
  if (dtsize > 0 && size > 0) {
    memset(stg, 0, sizeof(STG));
    stg->stg_size = size;
    stg->stg_dtsize = dtsize;
    stg->stg_avail = 1;
    stg->stg_cleared = 0;
    stg->stg_name = name;
    stg->stg_base = (void *)sccalloc(stg->stg_dtsize * stg->stg_size);
  } else {
    invalid_size("stg_alloc", dtsize, size, name);
  }
} /* stg_alloc_base */

/*
 * clear 'n' elements of the data structure starting at 'r'
 * reset stg_cleared if we're initializing or extending the cleared region
 */
void
stg_clear(STG *stg, int r, int n)
{
  if (r >= 0 && n > 0) {
    memset((char *)(stg->stg_base) + (r * stg->stg_dtsize), 0,
         n * stg->stg_dtsize);
    if (r == stg->stg_cleared) {
      stg->stg_cleared += n;
    } else if (r == 0 && n > stg->stg_cleared) {
      stg->stg_cleared = n;
    }
  }
} /* stg_clear */

/*
 * clear the data structure up to stg_avail
 */
void
stg_clear_all(STG *stg)
{
  stg_clear(stg, 0, stg->stg_avail);
} /* stg_clear_all */

/*
 * allocate STG data structure, clear element zero
 */
void
stg_alloc(STG *stg, int dtsize, int size, char *name)
{
  stg_alloc_base(stg, dtsize, size, name);
  stg_clear(stg, 0, 1);
} /* stg_alloc */

/*
 * deallocate STG data structure
 */
void
stg_delete(STG *stg)
{
  if (stg->stg_base)
    sccfree((char *)stg->stg_base);
  memset(stg, 0, sizeof(STG));
} /* stg_delete */

/*
 * reallocate STG structure if we need the extra size (if stg_avail > stg_size)
 *  reallocate any sidecars as well
 *  the new size will be 2*(stg_avail-1), which must be >= 2*stg_size
 */
void
stg_need(STG *stg)
{
  STG *thisstg;
  /* if the compiler has recycled some previously allocated space,
   * we need to reset the stg_cleared region */
  if (stg->stg_cleared > stg->stg_avail)
    stg->stg_cleared = stg->stg_avail;
  if (stg->stg_avail > stg->stg_size) {
    int oldsize, newsize;
    oldsize = stg->stg_size;
    newsize = (stg->stg_avail - 1) * 2;
    /* reallocate stg and all its sidecars */
    for (thisstg = stg; thisstg; thisstg = (STG *)thisstg->stg_sidecar) {
      thisstg->stg_size = newsize;
      thisstg->stg_base = (void *)sccrelal(
          (char *)thisstg->stg_base, thisstg->stg_size * thisstg->stg_dtsize);
    }
  }
  if (stg->stg_avail > stg->stg_cleared) {
    /* clear any new elements */
    for (thisstg = stg; thisstg; thisstg = (STG *)thisstg->stg_sidecar) {
      stg_clear(thisstg, thisstg->stg_cleared, thisstg->stg_avail - thisstg->stg_cleared);
    }
  }
} /* stg_need */

/*
 * Allocate a sidecar, attach to list of sidecars
 */
void
stg_alloc_sidecar(STG *basestg, STG *stg, int dtsize, char *name)
{
  stg_alloc_base(stg, dtsize, basestg->stg_size, name);
  stg->stg_avail = basestg->stg_avail;
  /* clear sidecar for any already-allocated elements */
  stg_clear(stg, 0, stg->stg_avail);
  /* link this sidecar to the list of sidecars for the basestg */
  stg->stg_sidecar = basestg->stg_sidecar;
  basestg->stg_sidecar = (void *)stg;
} /* stg_alloc_sidecar */

/*
 * error message
 */
static void
sidecar_not_found(char *funcname, STG *basestg, STG *stg)
{
  /* sidecar not found, this is an error */
  static char *message = "%s: Sidecar %s to %s not found";
  /* +1 (below) for the null terminator */
  char *msg = (char *)malloc(strlen(message) + strlen(funcname) + strlen(basestg->stg_name) +
                             strlen(stg->stg_name) + 1);
  sprintf(msg, message, basestg->stg_name, stg->stg_name);
  interr(msg, 0, 4);
} /* sidecar_not_found */

/*
 * Deallocate a sidecar, detach from list of sidecars
 */
void
stg_delete_sidecar(STG *basestg, STG *stg)
{
  if (basestg->stg_sidecar == (void *)stg) {
    basestg->stg_sidecar = stg->stg_sidecar;
  } else {
    STG *sidecar;
    for (sidecar = (STG *)basestg->stg_sidecar; sidecar;
         sidecar = sidecar->stg_sidecar) {
      if (sidecar->stg_sidecar == (void *)stg) {
        sidecar->stg_sidecar = stg->stg_sidecar;
        break;
      }
    }
    if (!sidecar) {
      sidecar_not_found("stg_delete_sidecar", basestg, stg);
    }
  }
  stg_delete(stg);
} /* stg_delete_sidecar */

/*
 * reserve next n elements at stg_avail; increment stg_avail;
 * grow, if necessary;
 * clear newly allocated elements; return the first such element.
 */
int
stg_next(STG *stg, int n)
{
  int r = stg->stg_avail;
  /* if the compiler has recycled some previously allocated space,
   * we need to reset the stg_cleared region */
  if (stg->stg_cleared > r)
    stg->stg_cleared = r;
  stg->stg_avail += n;
  if (stg->stg_avail > stg->stg_size) {
    stg_need(stg);
  } else {
    stg_clear(stg, stg->stg_cleared, stg->stg_avail - stg->stg_cleared);
  }
  return r;
} /* stg_next */

/*
 * error message
 */
static void
too_small_for_freelist(char *funcname, STG *stg)
{
  static char *message =
      "%s: structure %s too small for a freelist link, size=%d";
  /* 12 (below) for the data structure size, which must be < sizeof(int) */
  char *msg = (char *)malloc(strlen(message) + strlen(funcname) +
                             strlen(stg->stg_name) + 12);
  sprintf(msg, message, funcname, stg->stg_name, stg->stg_dtsize);
  interr(msg, 0, 4);
} /* too_small_for_freelist */

/*
 * get next element from the free list, if it's not null.
 * reset the free list from the free list link.
 * otherwise, just get the next available element from stg_avail
 * the link to the next free element is stored at 'word 0' of the structure
 */
int
stg_next_freelist(STG *stg)
{
  int r = stg->stg_free;
  if (!r) {
    r = stg_next(stg, 1);
  } else {
    char *base;
    if (stg->stg_dtsize < sizeof(int))
      too_small_for_freelist("stg_next_freelist", stg);
    /* get stg_base */
    base = (char *)stg->stg_base;
    /* add the offset of the r'th element (r*dtsize) */
    base += r * stg->stg_dtsize;
    /* get the link to the next free element */
    stg->stg_free = *(int *)base;
  }
  /* clear the new element */
  stg_clear(stg, r, 1);
  return r;
} /* stg_next_freelist */

/*
 * add element to the free list
 * store the link to the next free element at 'word 0'
 */
void
stg_add_freelist(STG *stg, int r)
{
  char *base;
  if (stg->stg_dtsize < sizeof(int))
    too_small_for_freelist("stg_next_freelist", stg);
  /* clear the recycled element */
  stg_clear(stg, r, 1);
  /* get stg_base */
  base = (char *)stg->stg_base;
  /* add the offset of the r'th element (r*dtsize) */
  base += r * stg->stg_dtsize;
  /* link to the free list */
  *(int *)base = stg->stg_free;
  stg->stg_free = r;
} /* stg_add_freelist */

