/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __SHARED_HASH_H__
#define __SHARED_HASH_H__

/*
 * hash.h: General-Purpose Hash Tables.
 *
 * These two hash table implementations store void pointers as keys, using
 * user-provided functions for hashing and equality testing. The void pointers
 * are not interpreted by the hash table implementation, except:
 *
 * - The void pointers are passed to the provided hash() and equals() functions.
 *
 * - The sentinel values NULL and (void*)~0UL have special meanings and cannot
 *   be used as keys.
 *
 * The equality function should return non-zero for equal hash keys. At a
 * minimum, the provided functions must satisfy:
 *
 *    equals(a, b)  ==>  hash(a) = hash(b)
 *
 * The hash() function should avoid clustering in the low bits of the hash
 * value.
 *
 * A NULL equals() function is equivalent to a function returning a == b, but
 * faster.
 */

typedef unsigned hash_value_t;
typedef const void *hash_key_t;

typedef hash_value_t (*hash_function_t)(hash_key_t);
typedef int (*hash_equality_t)(hash_key_t, hash_key_t);

typedef struct hash_functions_ {
  hash_function_t hash;
  hash_equality_t equals;
} hash_functions_t;

/*
 * Predefined hash functions for strings.
 *
 * The hash keys are interpreted as pointers to NUL-terminated strings.
 */
extern const hash_functions_t hash_functions_strings;

/*
 * Predefined hash functions for directly hashed keys.
 *
 * The pointers are compared by value with no indirection. These hash functions
 * can also be used for integer keys. Just cast the integers to hash_key_t.
 */
extern const hash_functions_t hash_functions_direct;

#define INT2HKEY(i) (hash_key_t)(long)(i)
#define HKEY2INT(k) (int)(long)(k)

/*
 * Hash Set.
 *
 * A hashset_t is a hash table that stores a set of keys with no associated
 * information.
 */
typedef struct hashset_ *hashset_t;

/*
 * Allocate a hashset which uses the provided functions to interpret keys.
 *
 * The returned hashset_t handle should be passed to hashset_free() to
 * deallocate
 * memory.
 */
hashset_t hashset_alloc(hash_functions_t);

/*
 * Free all memory used by a hashset.
 *
 * Note that any memory referenced by keys in the set is not freed.
 */
void hashset_free(hashset_t);

/*
 * Erase all keys in the set.
 */
void hashset_clear(hashset_t);

/*
 * Get the number of keys in the set.
 */
unsigned hashset_size(hashset_t);

/*
 * Look up a key and return the equivalent stored key, or NULL.
 */
hash_key_t hashset_lookup(hashset_t, hash_key_t);

/*
 * Insert a new key.
 *
 * Note that this function assumes that no equivalent key is present in the
 * set, i.e. hashset_lookup() would return false.
 *
 * Use hashset_replace() if an equivalent key may be in the set already.
 *
 * The key cannot be NULL or (hash_key_t)~0UL.
 */
void hashset_insert(hashset_t, hash_key_t);

/*
 * Insert a new key or replace an existing key.
 *
 * If an equivalent key already exists, replace it with the new key and return
 * the old one. If no equivalent key is in the set, insert the new key and
 * return NULL.
 */
hash_key_t hashset_replace(hashset_t, hash_key_t);

/*
 * Erase a key from the set and return it.
 *
 * Return NULL if no equivalent key was found.
 */
hash_key_t hashset_erase(hashset_t, hash_key_t);

/*
 * Call f with every key in the hash set.
 *
 * Note that the iteration order is a function of both the hash function and
 * the sequence of hashset_* calls leading up to this one. If pointers are
 * directly hashed, address space layout randomization can cause different
 * iteration orders in otherwise identical executions.
 *
 * The function f must not modify the hash table.
 */
void hashset_iterate(hashset_t, void (*f)(hash_key_t, void *context),
                     void *context);

/*
 * Hash Map.
 *
 * A hashmap_t is a hash table that maps a set of keys to data pointers.
 *
 * The keys are treated exactly the same as for a hashset_t, and have the same
 * restrictions (i.e., no NULL and ~0UL keys allowed).  The data pointers can
 * have any value.
 */
typedef struct hashmap_ *hashmap_t;
typedef const void *hash_data_t;

/*
 * Allocate a hashmap.
 *
 * The returned handle must be freed with hashmap_free().
 */
hashmap_t hashmap_alloc(hash_functions_t);

/*
 * Free all memory used by a hashmap.
 *
 * Note that any memory referenced by keys and data pointers in the map is not
 * freed.
 */
void hashmap_free(hashmap_t);

/*
 * Erase all (key, data) entries in the map.
 */
void hashmap_clear(hashmap_t);

/*
 * Return the number of (key, data) pairs in the map.
 */
unsigned hashmap_size(hashmap_t);

/*
 * Look up a key and return the equivalent stored key, or NULL.
 *
 * If if a key was found and data is not NULL, *data will be set to the
 * corresponding data value, otherwise it is not changed.
 */
hash_key_t hashmap_lookup(hashmap_t, hash_key_t, hash_data_t *data);

/*
 * Insert a new (key, data) pair.
 *
 * This function assumes that no equivalent key is present in the map, i.e.
 * hashmap_lookup() would return NULL.
 *
 * Use hashmap_replace() if an equivalent key may exist in the map already.
 */
void hashmap_insert(hashmap_t, hash_key_t, hash_data_t);

/*
 * Insert or replace a (key, data) pair.
 *
 * If an equivalent key already exists, replace it with the new key and *data,
 * and return the old key. On return, *data is set to the old data.
 *
 * If no equivalent key is present, insert the new key and *data and return
 * NULL. The *data value is not updated.
 */
hash_key_t hashmap_replace(hashmap_t, hash_key_t, hash_data_t *data);

/*
 * Erase a key from the map and return it.
 *
 * If a (key, data) pair was erased and data is not NULL, update *data with the
 * erased data value.
 *
 * If no key was erased, return NULL and leave *data unchanged.
 */
hash_key_t hashmap_erase(hashmap_t, hash_key_t, hash_data_t *data);

/*
 * Call f with every (key, data) pair in the hash map.
 *
 * The function f must not modify the hash table.
 */
void hashmap_iterate(hashmap_t,
                     void (*f)(hash_key_t, hash_data_t, void *context),
                     void *context);

/*
 * Helpers for computing hash values for composite data structures, using a
 * hash accumulator. These macros implement parts of the Jenkins hash function.
 *
 * See also the functions string_hash() and direct_hash() in hash.c.
 *
 * hash_value_T hash_function(const struct foo *data)
 * {
 *     hash_accu_t hacc = HASH_ACCU_INIT;
 *
 *     HASH_ACCU_ADD(hacc, data->int_member);
 *     HASH_ACCU_ADD(hacc, data->pointer_member);
 *     HASH_ACCU_FINISH(accu);
 *     return HASH_ACCU_VALUE(accu);
 * }
 *
 */

typedef struct hash_accu_ {
  hash_value_t a;
} hash_accu_t;

#define HASH_ACCU_INIT                                                         \
  {                                                                            \
    0                                                                          \
  }

#define HASH_ACCU_ADD(accu, data)                                              \
  do {                                                                         \
    (accu).a += (hash_value_t)(data);                                          \
    (accu).a += (accu).a << 10;                                                \
    (accu).a ^= (accu).a >> 6;                                                 \
  } while (0)

#define HASH_ACCU_FINISH(accu)                                                 \
  do {                                                                         \
    (accu).a += (accu).a << 3;                                                 \
    (accu).a ^= (accu).a >> 11;                                                \
    (accu).a += (accu).a << 15;                                                \
  } while (0)

#define HASH_ACCU_VALUE(accu) ((accu).a + 0)

#endif /* __SHARED_HASH_H__ */
