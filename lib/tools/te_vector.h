/* SPDX-License-Identifier: Apache-2.0 */
/** @file
 * @brief Dynamic array
 *
 * @defgroup te_tools_te_vec Dymanic array
 * @ingroup te_tools
 * @{
 *
 * Implementation of dymanic array
 *
 * Example of using:
 * @code
 * // Initialize the dynamic vector to store a value of type int
 * te_vec vec = TE_VEC_INIT(int)
 * int number = 42;
 * ...
 * // Put number into dynamic array
 * CHECK_RC(TE_VEC_APPEND(&vec, number));
 * ...
 * // Copy from c array
 * int numbers[] = {4, 2};
 * CHECK_RC(te_vec_append_array(&vec, numbers, TE_ARRAY_LEN(numbers)));
 * ...
 * // Change the first element
 * TE_VEC_GET(int, &vec, 0) = 100;
 * ...
 * // Finish work with vector, free the memory.
 * te_vec_free(&vec);
 * @endcode
 *
 * Copyright (C) 2019-2022 OKTET Labs Ltd. All rights reserved.
 */

#ifndef __TE_VEC_H__
#define __TE_VEC_H__

#include "te_defs.h"
#include "te_dbuf.h"

#if HAVE_ASSERT_H
#include <assert.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Function type for vector element destructors.
 *
 * The destructor accepts a pointer to a vector element
 * which it is not supposed to modify, *not* a pointer
 * to a deallocatable memory, so e.g. a destructor
 * that frees a `char *` typed element would look like:
 * @code
 * static void item_destroy_str(const void *item)
 * {
 *     char *buf = *(void * const *)item;
 *     free(buf);
 * }
 * @endcode
 *
 * The destructor function shall be prepared to deal
 * correctly with all-zero initialized elements.
 *
 * @param item   A pointer to a vector element data.
 */
typedef void te_vec_item_destroy_fn(const void *item);


/**
 * A destructor to release vector elements that contain
 * a single heap memoy pointer.
 */
extern te_vec_item_destroy_fn te_vec_item_free_ptr;

/** Dymanic array */
typedef struct te_vec {
    te_dbuf data;           /**< Data of array */
    size_t element_size;    /**< Size of one element in bytes */
    te_vec_item_destroy_fn *destroy; /**< Element destructor. */
} te_vec;

/**
 * Complete initializer for a te_vec.
 *
 * @param type_         Element type.
 * @param grow_factor_  Grow factor (see TE_DBUF_DEFAULT_GROW_FACTOR).
 * @param destroy_      Element destructor (or @c NULL).
 */
#define TE_VEC_INIT_COMPLETE(type_, grow_factor_, destroy_) \
    (te_vec){                                               \
        .data = TE_DBUF_INIT(grow_factor_),                 \
        .element_size = sizeof(type_),                      \
        .destroy = (destroy_),                              \
    }

/** Initialization from type and custom grow factor and type */
#define TE_VEC_INIT_GROW_FACTOR(type_, grow_factor_) \
    TE_VEC_INIT_COMPLETE(type_, grow_factor_, NULL)

/** Initialization from type only */
#define TE_VEC_INIT(_type) \
    TE_VEC_INIT_GROW_FACTOR(_type, TE_DBUF_DEFAULT_GROW_FACTOR)

/**
 * Vector initializer with a possibly non-null destructor.
 *
 * @param type_     Element type.
 * @param destroy_  Element destructor or @c NULL.
 */
#define TE_VEC_INIT_DESTROY(type_, destroy_) \
    TE_VEC_INIT_COMPLETE(type_, TE_DBUF_DEFAULT_GROW_FACTOR, destroy_)

/*
 * Set a vector element destructor with sanity checks.
 *
 * The following rules are used to minimize the risk of
 * calling a destructor on improper data:
 * - it is always possible to set a destructor to @c NULL;
 * - if a vector already has a non-null destructor,
 *   the new destructor must be the same;
 * - if a vector has a null destructor and has some elements,
 *   the destructor remains null to support wider range of
 *   legacy cases;
 * - otherwise the null destructor is replaced with the new one.
 *
 * @param vec       Vector.
 * @param destroy   New destructor function.
 *
 * @exception TE_FATAL_ERROR if the new destructor and the old
 *            one are not the same.
 */
extern void te_vec_set_destroy_fn_safe(te_vec *vec,
                                       te_vec_item_destroy_fn *destroy);

/**
 * Access for element in array
 *
 * @param _type     Type of element
 * @param _te_vec   Dynamic vector
 * @param _index    Index of element
 *
 * @return Element of array
 *
 * @note For vectors with a non-null destructor
 *       te_vec_replace() should be used instead of
 *       mutable TE_VEC_GET(), as it guarantees proper
 *       disposal of old element contents.
 */
#define TE_VEC_GET(_type, _te_vec, _index) \
    (*(_type *)te_vec_get_safe(_te_vec, _index, sizeof(_type)))

/**
 * For each element in vector
 *
 * Example:
 * @code
 *
 * struct netinterface {
 *     int index;
 *     const char *name;
 * };
 *
 * te_vec vec = TE_VEC_INIT(struct netinterface);
 * ... // Fill vector with values
 *
 * struct netinterface *netif;
 * TE_VEC_FOREACH(&vec, netif)
 * {
 *      printf("interface '%s' have index %d", netif->name, netif->index);
 * }
 * @endcode
 *
 * @param _te_vec   Dynamic vector
 * @param _elem     Pointer of type contain in vector
 */
#define TE_VEC_FOREACH(_te_vec, _elem)                                        \
    for ((_elem) = te_vec_size(_te_vec) != 0 ?                                \
            te_vec_get_safe(_te_vec, 0, sizeof(*(_elem))) : NULL;             \
         te_vec_size(_te_vec) != 0 &&                                         \
         ((void *)(_elem)) <= te_vec_get(_te_vec, te_vec_size(_te_vec) - 1);  \
         (_elem)++)

/**
 * Add element to the vector's tail
 *
 * @param _te_vec   Dynamic vector
 * @param _val      New element
 *
 * @return Status code (always 0).
 */
#define TE_VEC_APPEND(_te_vec, _val) \
    (te_vec_append_array_safe(_te_vec, &(_val), 1, sizeof(_val)))

/**
 * Add element to the vector's tail
 * @param _te_vec   Dynamic vector
 * @param _type     Element type
 * @param _val      New element
 *
 * @return Status code (always 0).
 */
#define TE_VEC_APPEND_RVALUE(_te_vec, _type, _val) \
    (te_vec_append_array_safe(_te_vec, (_type[]){_val}, 1, sizeof(_type)))

/**
 * Append elements from C-like array to the dynamic array (safe version)
 *
 * @param _te_vec        Dymanic vector
 * @param _elements      Elements of array
 * @param _count         Count of @p elements
 *
 * @return Status code (always 0).
 */
#define TE_VEC_APPEND_ARRAY(_te_vec, _elements, _count) \
    (te_vec_append_array_safe(_te_vec, _elements, _size, sizeof(*(_elements))))

/**
 * Count of elements contains in dynamic array
 *
 * @param vec       Dynamic vector
 *
 * @return Count of elements
 */
static inline size_t
te_vec_size(const te_vec *vec)
{
    assert(vec != NULL);
    assert(vec->element_size != 0);
    return vec->data.len / vec->element_size;
}

/**
 * Access to a pointer of element in array
 *
 * @param _vec       Dynamic vector
 * @param _index     Index of element
 *
 * @return Pointer to element
 */
#if __STDC_VERSION__ >= 201112L
#define te_vec_get(_vec, _index)                                \
    (_Generic((_vec),                                            \
             te_vec*: te_vec_get_mutable,                        \
             const te_vec*: te_vec_get_immutable)(_vec, _index))
#else
#define te_vec_get(_vec, _index)                                        \
    (__builtin_choose_expr(                                             \
        __builtin_types_compatible_p(__typeof__(_vec), const te_vec *), \
                                     te_vec_get_immutable,              \
                                     te_vec_get_mutable)(_vec, _index))
#endif

static inline const void *
te_vec_get_immutable(const te_vec *vec, size_t index)
{
    assert(vec != NULL);
    assert(index < te_vec_size(vec));
    return vec->data.ptr + index * vec->element_size;
}

static inline void *
te_vec_get_mutable(te_vec *vec, size_t index)
{
    assert(vec != NULL);
    assert(index < te_vec_size(vec));
    return vec->data.ptr + index * vec->element_size;
}

/**
 * Safe version of @ref te_vec_get
 *
 * @param _vec              Dynamic vector
 * @param _index            Index of element
 * @param _element_size     Expected size of type in array
 *
 * @return Pointer to element
 */
#if __STDC_VERSION__ >= 201112L
#define te_vec_get_safe(_vec, _index, _element_size) \
    (_Generic((_vec),                                  \
             te_vec*: te_vec_get_safe_mutable,         \
             const te_vec*: te_vec_get_safe_immutable) \
             (_vec, _index, _element_size))
#else
#define te_vec_get_safe(_vec, _index, _element_size) \
    (__builtin_choose_expr(                    \
        __builtin_types_compatible_p(          \
            __typeof__(_vec), const te_vec *), \
            te_vec_get_safe_immutable,         \
            te_vec_get_safe_mutable)(_vec, _index, _element_size))
#endif

static inline const void *
te_vec_get_safe_immutable(const te_vec *vec, size_t index, size_t element_size)
{
    assert(vec != NULL);
    assert(element_size == vec->element_size);
    return te_vec_get(vec, index);
}

static inline void *
te_vec_get_safe_mutable(te_vec *vec, size_t index, size_t element_size)
{
    assert(vec != NULL);
    assert(element_size == vec->element_size);
    return te_vec_get(vec, index);
}

/**
 * Append one element to the dynamic array
 *
 * If @p element is @c NULL, the new element will be zeroed.
 *
 * @param vec        Dymanic vector
 * @param element    Element for appending (may be @c NULL)
 *
 * @return Status code (always 0).
 */
extern te_errno te_vec_append(te_vec *vec, const void *element);

/**
 * Append elements from @p other dynamic array
 *
 * @param vec        Dymanic vector
 * @param other      Other dymanic vector
 *
 * @return Status code (always 0).
 *
 * @pre Both vectors must have a null element destructor.
 */
extern te_errno te_vec_append_vec(te_vec *vec, const te_vec *other);

/**
 * Append elements from C-like array to the dynamic array
 *
 * If @p elements is @c NULL, the news element will be zeroed.
 *
 * @param vec        Dymanic vector
 * @param elements   Elements of array (may be @c NULL)
 * @param count      Count of @p elements
 *
 * @return Status code (always 0).
 */
extern te_errno te_vec_append_array(te_vec *vec, const void *elements,
                                    size_t count);

/**
 * Append a formatted C-string to the dynamic array
 *
 * @param vec        Dynamic vector of C-strings
 * @param fmt        Format string
 * @param ...        Format string arguments
 *
 * @return Status code (always 0).
 */
extern te_errno te_vec_append_str_fmt(te_vec *vec, const char *fmt, ...)
                                      __attribute__((format(printf, 2, 3)));


/**
 * Replace the content of @p index'th element of @p vec with @p new_val.
 *
 * If @p new_val is @c NULL, the content of the element is zeroed.
 * A destructor function (if it is set) is called for the old content.
 *
 * If @p index is larger than the @p vec size, it is grown as needed.
 *
 * @param vec         Dynamic vector.
 * @param index       Index to replace.
 * @param new_val     New content (may be @c NULL).
 *
 * @return A pointer to the new contents of the element.
 */
extern void *te_vec_replace(te_vec *vec, size_t index, const void *new_val);

/**
 * Move the contents of a vector element to @p dest.
 *
 * This function is mostly useful for vectors with non-null destructors.
 * Basically, it implements move semantics for vector elements.
 *
 * If @p dest is not @c NULL, the contents of @p index'th element is copied
 * to @p dest; destructors are not called.
 *
 * If @p dest is @c NULL, the destructor is called on @p index'th element.
 *
 * In both cases the element is zeroed afterwards. This ensures that calling
 * the destructor for the second time would not cause bugs like double-free.
 *
 * @param vec        Dynamic vector.
 * @param index      Index to move.
 * @param dest       Destination (may be @c NULL).
 */
extern void te_vec_transfer(te_vec *vec, size_t index, void *dest);

/**
 * Move at most @p count elements from @p vec to @p dest_vec.
 *
 * If @p dest_vec is not @c NULL, elements starting from @p start_index
 * are appended to it and the elements are zeroed like with
 * te_vec_transfer().
 *
 * If @p dest_vec is @c NULL, the destructor is called on the elements.
 *
 * If @p start_index + @p count is greater than the vector size, @p count
 * is decreased as needed.
 *
 * @p dest_vec must either have a null element destructor or the same
 * destructor as @p vec.
 *
 * @param vec           Source vector.
 * @param start_index   Starting index.
 * @param count         Number of elements.
 * @param dest_vec      Destination vector (may be @c NULL).
 *
 * @return The number of actually transferred elements.
 */
extern size_t te_vec_transfer_append(te_vec *vec, size_t start_index,
                                     size_t count, te_vec *dest_vec);

/**
 * Remove elements from a vector
 *
 * If @p vec has a non-null element destructor,
 * it will be called for each element.
 *
 * @param vec           Dynamic vector
 * @param start_index   Starting index of elements to remove
 * @param count         Number of elements to remove
 */
extern void
te_vec_remove(te_vec *vec, size_t start_index, size_t count);

/**
 * Remove an element from a vector
 *
 * @param vec           Dynamic vector
 * @param index         Index of a element to remove
 */
static inline void
te_vec_remove_index(te_vec *vec, size_t index)
{
    return te_vec_remove(vec, index, 1);
}

/**
 * Safe version of @ref te_vec_append_array
 *
 * @param vec               Dymanic vector
 * @param elements          Elements of array
 * @param count             Count of @p elements
 * @param element_size      Size of one element in @p elements
 *
 * @return Status code (always 0)
 */
static inline te_errno
te_vec_append_array_safe(te_vec *vec, const void *elements,
                         size_t count, size_t element_size)
{
    assert(vec != NULL);
    assert(element_size == vec->element_size);
    return te_vec_append_array(vec, elements, count);
}

/**
 * Reset dynamic array (makes it empty), memory is not freed
 *
 * A destructor function is called for each element if it is defined.
 *
 * @param vec          Dynamic vector
 */
extern void te_vec_reset(te_vec *vec);

/**
 * Cleanup dynamic array and storage memory
 *
 * A destructor function is called for each element if it is defined.
 *
 * @param vec       Dynamic vector
 */
extern void te_vec_free(te_vec *vec);

/**
 * Free the dynamic array along with its elements.
 *
 * The function does exactly the same as te_vec_free() unless
 * @p vec has a null destructor, in which case it treats elements
 * as pointers to heap memory and free() them.
 *
 * @param vec        Dynamic vector of pointers
 *
 * @deprecated Use te_vec_free() with a proper destructor.
 */
extern void te_vec_deep_free(te_vec *vec);

/**
 * Append to a dynamic array of strings
 *
 * @param vec           Dynamic vector to append the array of strings to
 * @param elements      @c NULL terminated array of strings
 *
 * @return Status code (always 0).
 */
extern te_errno te_vec_append_strarray(te_vec *vec, const char **elements);


/**
 * Return an index of an element of @p vec pointed to by @p ptr.
 *
 * @return Zero-based index. The result is undefined if @p ptr is
 *         not pointing to the actual vector data
 */
static inline size_t
te_vec_get_index(const te_vec *vec, const void *ptr)
{
    size_t offset = (const uint8_t *)ptr - (const uint8_t *)vec->data.ptr;
    assert(offset < vec->data.len);

    return offset / vec->element_size;
}


/**
 * Split a string into chunks separated by @p sep.
 *
 * The copies of the chunks are pushed into the @p strvec.
 *
 * @note The element size of @p strvec must be `sizeof(char *)`.
 *
 * @note Adjacent separators are never skipped, so e.g.
 *       @c ':::' would be split into four chunks using colon as
 *       a separator. The only special case is an empty string
 *       which may be treated as no chunks depending on @p empty_is_none.
 *
 * @param[in]     str            String to split
 * @param[in,out] strvec         Target vector for string chunks.
 *                               The original contents is **not** destroyed,
 *                               new items are added to the end.
 * @param[in]     sep            Separator character
 * @param[in]     empty_is_none  If @c TRUE, empty string is treated
 *                               as having no chunks (so @p strvec is
 *                               not changed). Otherwise, an empty string
 *                               is treated as having a single empty chunk.
 *
 * @return Status code (always 0).
 */
extern te_errno te_vec_split_string(const char *str, te_vec *strvec, char sep,
                                    te_bool empty_is_none);

/**
 * Sort the elements of @p vec in place according to @p compar.
 *
 * @param vec      Vector to sort
 * @param compar   Comparison function (as for qsort())
 */
extern void te_vec_sort(te_vec *vec, int (*compar)(const void *elt1, const void *elt2));

/**
 * Search a sorted vector @p vec for an item equal to @p key
 * using @p compar as a comparison function.
 *
 * The function implements binary search, however unlike C standard
 * bsearch() it can be reliably used on non-unique matches, because
 * it returns the lowest and the highest indices of matching elements.
 *
 * @note Mind the order of arguments. @p compar expects the **first**
 * argument to be a key and the second argument to be an array element,
 * for a compatibility with bsearch(). However, the order of arguments of
 * the function itself is reverse wrt bsearch(): the vector goes first and
 * the key follows it for consistency with other vector functions.
 * For cases where the key has the same structure as the array element,
 * this should not matter.
 *
 * @note The vector must be sorted in a way compatible with @p compar, i.e.
 * by using te_vec_sort() with the same @p compar, however, the comparison
 * functions need not to be truly identical: the search comparison function
 * may treat more elements as equal than the sort comparison.
 *
 * @param[in]  vec     Vector to search in
 * @param[in]  key     Key to search
 * @param[in]  compar  Comparison function (as for bsearch())
 * @param[out] minpos  The lowest index of a matching element.
 * @param[out] maxpos  The highest index of a matchin element.
 *                     Any of @p minpos and @p maxpos may be @c NULL.
 *                     If they are both @c NULL, the function just checks
 *                     for an existence of a matching element.
 *
 * @return TRUE iff an element matching @p key is found.
 */
 extern te_bool te_vec_search(const te_vec *vec, const void *key,
                              int (*compar)(const void *key, const void *elt),
                              unsigned int *minpos, unsigned int *maxpos);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TE_VEC_H__ */
/**@} <!-- END te_tools --> */
