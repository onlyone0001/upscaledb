/**
 * Copyright (C) 2005, 2006 Christoph Rupp (chris@crupp.de)
 * see file LICENSE for license and copyright information
 *
 * an object which handles a database page 
 *
 */

#ifndef HAM_PAGE_H__
#define HAM_PAGE_H__

#include <ham/hamsterdb.h>
#include "endian.h"
#include "cursor.h"

/*
 * indices for page lists
 *
 * each page is a node in several linked lists - via _npers._prev and 
 * _npers._next. both members are arrays of pointers and can be used
 * with _npers._prev[PAGE_LIST_BUCKET] etc. (or with the macros 
 * defined below).
 */

/* a bucket in the hash table of the cache manager */
#define PAGE_LIST_BUCKET           0
/* a node in the linked list of a transaction */
#define PAGE_LIST_TXN              1
/* garbage collected pages */
#define PAGE_LIST_GARBAGE          2
/* list of all cached pages */
#define PAGE_LIST_CACHED           3
/* array limit */
#define MAX_PAGE_LISTS             4

struct ham_page_t;
typedef struct ham_page_t ham_page_t;

/**
 * the page structure
 */
struct ham_page_t {
    /**
     * the header is non-persistent and NOT written to disk. 
     * it's caching some run-time values which
     * we don't want to recalculate whenever we need them.
     */
    struct {
        /** address of this page */
        ham_offset_t _self;

        /** reference to the database object */
        ham_db_t *_owner;

        /** non-persistent flags */
        ham_u32_t _flags;

        /** cache counter - used by the cache module */
        ham_u32_t _cache_cntr;

        /** inuse-counter counts the number of transactions, which access
         * this page*/
        ham_u32_t _inuse;

        /** linked lists of pages - see comments above */
        ham_page_t *_prev[MAX_PAGE_LISTS], *_next[MAX_PAGE_LISTS];

        /** linked list of all cursors which point to that page */
        ham_cursor_t *_cursors;

    } _npers; 

    /**
     * from here on everything will be written to disk 
     */
    union page_union_t {

        /*
         * this header is only available if the (non-persistent) flag
         * NPERS_NO_HEADER is not set! 
         *
         * all blob-areas in the file do not have such a header, if they
         * span page-boundaries
         */
        struct page_union_header_t {
            /**
             * flags of this page
             */
            ham_u32_t _flags;
    
            /**
             * some reserved bytes
             */
            ham_u32_t _reserved1;
            ham_u32_t _reserved2;

            /** 
             * this is just a blob - the backend (hashdb, btree etc) 
             * will use it appropriately
             */
            ham_u8_t _payload[1];
        } _s;

        /*
         * a char pointer
         */
        ham_u8_t _p[1];

    } *_pers;

};

/**
 * the size of struct page_union_t, without the payload byte
 *
 * !!
 * this is not equal to sizeof(struct page_union_t)-1, because of
 * padding (i.e. on gcc 4.1, 64bit the size would be 15 bytes)
 */
#define SIZEOF_PAGE_UNION_HEADER        12

/**
 * get the address of this page
 */
#define page_get_self(page)          (ham_db2h_offset((page)->_npers._self))

/**
 * set the address of this page
 */
#define page_set_self(page, a)       (page)->_npers._self=ham_h2db_offset(a)

/** 
 * get the database object which 0wnz this page 
 */
#define page_get_owner(page)         ((page)->_npers._owner)

/** 
 * set the database object which 0wnz this page 
 */
#define page_set_owner(page, db)     (page)->_npers._owner=db

/** 
 * get the previous page of a linked list
 */
#ifdef HAM_DEBUG
extern ham_page_t *
page_get_previous(ham_page_t *page, int which);
#else
#   define page_get_previous(page, which)    ((page)->_npers._prev[(which)])
#endif /* HAM_DEBUG */

/** 
 * set the previous page of a linked list
 */
#ifdef HAM_DEBUG
extern void
page_set_previous(ham_page_t *page, int which, ham_page_t *other);
#else
#   define page_set_previous(page, which, p) (page)->_npers._prev[(which)]=(p)
#endif /* HAM_DEBUG */

/** 
 * get the next page of a linked list
 */
#ifdef HAM_DEBUG
extern ham_page_t *
page_get_next(ham_page_t *page, int which);
#else
#   define page_get_next(page, which)        ((page)->_npers._next[(which)])
#endif /* HAM_DEBUG */

/** 
 * set the next page of a linked list
 */
#ifdef HAM_DEBUG
extern void
page_set_next(ham_page_t *page, int which, ham_page_t *other);
#else
#   define page_set_next(page, which, p)     (page)->_npers._next[(which)]=(p)
#endif /* HAM_DEBUG */

/**
 * get linked list of cursors
 */
#define page_get_cursors(page)           (page)->_npers._cursors

/**
 * set linked list of cursors
 */
#define page_set_cursors(page, c)        (page)->_npers._cursors=c

/**
 * get persistent page flags
 */
#define page_get_pers_flags(page)        ham_db2h32((page)->_pers->_s._flags)

/**
 * set persistent page flags
 */
#define page_set_pers_flags(page, f)     (page)->_pers->_s._flags=ham_h2db32(f)

/**
 * get non-persistent page flags
 */
#define page_get_npers_flags(page)       (page)->_npers._flags

/**
 * set non-persistent page flags
 */
#define page_set_npers_flags(page, f)    (page)->_npers._flags=f

/**
 * get the cache counter
 */
#define page_get_cache_cntr(page)        (page)->_npers._cache_cntr

/**
 * set the cache counter
 */
#define page_set_cache_cntr(page, c)     (page)->_npers._cache_cntr=c

/** page->_pers was allocated with malloc, not mmap */
#define PAGE_NPERS_MALLOC            1
/**  page is dirty */
#define PAGE_NPERS_DIRTY             2
/** page is in use */
#define PAGE_NPERS_INUSE             4
/** page will be deleted when committed */
#define PAGE_NPERS_DELETE_PENDING   16
/** page has no header */
#define PAGE_NPERS_NO_HEADER        32

/** 
 * get the dirty-flag
 */
#define page_is_dirty(page)      (page_get_npers_flags(page)&PAGE_NPERS_DIRTY)

/** 
 * set the dirty-flag
 */
#define page_set_dirty(page, d)  page_set_npers_flags(page, \
            d ? page_get_npers_flags(page)|PAGE_NPERS_DIRTY : \
            page_get_npers_flags(page)&(~PAGE_NPERS_DIRTY))

/** 
 * get the "in use"-flag
 */
#define page_get_inuse(page)    (page)->_npers._inuse

/** 
 * increment the "in use"-flag
 */
#define page_inc_inuse(page)    ++(page)->_npers._inuse

/** 
 * decrement the "in use"-flag
 */
#define page_dec_inuse(page)    do { ham_assert(page_get_inuse(page)!=0, \
                                     ("decrementing empty inuse-flag")); \
                                     --(page)->_npers._inuse; } while (0)

/**
 * set the page-type
 */
#define page_set_type(page, t)   do { \
            page_set_pers_flags(page, page_get_pers_flags(page)&0x0fffffff);\
            page_set_pers_flags(page, page_get_pers_flags(page)|t);         \
        } while (0)

/**
 * get the page-type
 */
#define page_get_type(page)      (page_get_pers_flags(page)&0xf0000000)

/**
 * valid page types
 *
 * page types always have the highest nybble of the persistent flags
 */
#define PAGE_TYPE_UNKNOWN       0x00000000
#define PAGE_TYPE_HEADER        0x10000000
#define PAGE_TYPE_B_ROOT        0x20000000
#define PAGE_TYPE_B_INDEX       0x30000000
#define PAGE_TYPE_FREELIST      0x40000000

/**
 * get pointer to persistent payload (after the header!)
 */
#define page_get_payload(page)           (page)->_pers->_s._payload

/**
 * get pointer to persistent payload (including the header!)
 */
#define page_get_raw_payload(page)       (page)->_pers->_p

/**
 * set pointer to persistent data
 */
#define page_set_pers(page, p)           (page)->_pers=p

/**
 * get pointer to persistent data
 */
#define page_get_pers(page)              (page)->_pers

/**
 * check if a page is in a linked list
 */
extern ham_bool_t 
page_is_in_list(ham_page_t *head, ham_page_t *page, int which);

/**
 * linked list functions: insert the page at the beginning of a list
 *
 * @remark returns the new head of the list
 * TODO release build: replace this function with a macro 
 */
extern ham_page_t *
page_list_insert(ham_page_t *head, int which, ham_page_t *page);

/**
 * linked list functions: insert the page at the beginning of a ring list
 *
 * @remark returns the new head of the list
 * TODO release build: replace this function with a macro 
 */
extern ham_page_t *
page_list_insert_ring(ham_page_t *head, int which, ham_page_t *page);

/**
 * linked list functions: remove the page from a list
 *
 * @remark returns the new head of the list
 * TODO release build: replace this function with a macro 
 */
extern ham_page_t *
page_list_remove(ham_page_t *head, int which, ham_page_t *page);

/**
 * add a cursor to this page
 */
extern void
page_add_cursor(ham_page_t *page, ham_cursor_t *cursor);

/**
 * remove a cursor from this page
 */
extern void
page_remove_cursor(ham_page_t *page, ham_cursor_t *cursor);


#endif /* HAM_PAGE_H__ */
