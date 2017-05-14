/*
 *  (C) 2010 by Computer System Laboratory, IIS, Academia Sinica, Taiwan.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef __OPTIMIZATION_H
#define __OPTIMIZATION_H

/* Comment the next line to disable optimizations. */
#define ENABLE_OPTIMIZATION
//#define ENABLE_OPTIMIZATION_DEBUG

#ifdef ENABLE_OPTIMIZATION_DEBUG
/*
    fprintf(stderr, "[SHADOW STACK] \n");
*/
#endif
#define ENABLE_OPTIMIZATION_SHACK
#define ENABLE_OPTIMIZATION_IBTC

/*
 * Link list facilities
 */
struct list_head {
    struct list_head *next, *prev;
};
typedef struct list_head list_t;
/*
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
*/

void list_init(list_t *l);
int list_empty(list_t *l);
void list_add(list_t *new_list, list_t *head);
void list_del(list_t *entry);

/*
 * Shadow Stack
 */

#if TCG_TARGET_REG_BITS == 32
#define tcg_gen_mov_ptr         tcg_gen_mov_i32
#define tcg_gen_st_ptr          tcg_gen_st_i32
#define tcg_gen_brcond_ptr      tcg_gen_brcond_i32
#define tcg_temp_free_ptr       tcg_temp_free_i32
#define tcg_temp_local_new_ptr  tcg_temp_local_new_i32
#else
#define tcg_gen_mov_ptr         tcg_gen_mov_i64
#define tcg_gen_st_ptr          tcg_gen_st_i64
#define tcg_gen_brcond_ptr      tcg_gen_brcond_i64
#define tcg_temp_free_ptr       tcg_temp_free_i64
#define tcg_temp_local_new_ptr  tcg_temp_local_new_i64
#endif

#if TARGET_LONG_BITS == 32
#define TCGv TCGv_i32
#else
#define TCGv TCGv_i64
#endif

#define MAX_CALL_SLOT   (16 * 1024)
#define SHACK_SIZE      (16 * 1024)

typedef struct shadow_pair
{
    struct list_head l;
    target_ulong guest_eip;
    unsigned long *host_eip;
} shadow_pair;

void shack_set_shadow(CPUState *env, target_ulong guest_eip, unsigned long *host_eip);
inline void insert_unresolved_eip(CPUState *env, target_ulong next_eip, unsigned long *slot);
unsigned long lookup_shadow_ret_addr(CPUState *env, target_ulong pc);
void push_shack(CPUState *env, TCGv_ptr cpu_env, target_ulong next_eip);
void pop_shack(TCGv_ptr cpu_env, TCGv next_eip);

void dump_shack_structure(CPUState *env);
void dump_shack(CPUState *env);

void SHACK_HASHTBL_DUMP(CPUState *env);
struct shadow_pair* SHACK_HASHTBL_LOOKUP(CPUState *env, target_ulong guest_eip);
struct shadow_pair* SHACK_HASHTBL_INSERT(CPUState *env, target_ulong guest_eip, unsigned long *host_eip);
void SHACK_HASHTBL_REMOVE(struct shadow_pair *sp);

/*
 * Indirect Branch Target Cache
 */
#define IBTC_CACHE_BITS     (16)
#define IBTC_CACHE_SIZE     (1U << IBTC_CACHE_BITS)
#define IBTC_CACHE_MASK     (IBTC_CACHE_SIZE - 1)

struct jmp_pair
{
    target_ulong guest_eip;
    TranslationBlock *tb;
};

struct ibtc_table
{
    struct jmp_pair htable[IBTC_CACHE_SIZE];
};

int init_optimizations(CPUState *env);
void update_ibtc_entry(TranslationBlock *tb);

#endif

/*
 * vim: ts=8 sts=4 sw=4 expandtab
 */
