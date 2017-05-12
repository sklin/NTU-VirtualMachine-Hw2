/*
 *  (C) 2010 by Computer System Laboratory, IIS, Academia Sinica, Taiwan.
 *      See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include "exec-all.h"
#include "tcg-op.h"
#include "helper.h"
#define GEN_HELPER 1
#include "helper.h"
#include "optimization.h"

extern uint8_t *optimization_ret_addr;

/*
 * Link list facilities
 */

void list_init(list_t *l)
{
    l->next = l->prev = l;
}
int list_empty(list_t *l)
{
    return l->next == l;
}
void list_add(list_t *new_list, list_t *head)
{
    head->next->prev = new_list;
    new_list->next = head->next;
    new_list->prev = head;
    head->next = new_list;
}
void list_del(list_t *entry)
{
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
    // You should delete after calling list_del!
}

/*
 * Shadow Stack
 */

#define SHACK_HASHTBL_SIZE 1024

list_t *shadow_hash_list;

static inline void shack_init(CPUState *env)
{
    /* allocate shadow stack */
    // store guest return address
    env->shack = (uint64_t*) malloc(SHACK_SIZE * sizeof(uint64_t));
    env->shack_top = env->shack;
    env->shack_end = env->shack + SHACK_SIZE;

    // store a hash table
    env->shadow_hash_table = (list_t*) malloc(SHACK_HASHTBL_SIZE * sizeof(list_t));

    int i;
    for(i=0 ; i<SHACK_HASHTBL_SIZE ; ++i)
        list_init(&((list_t*)env->shadow_hash_table)[i]);
}

/*
 * shack_set_shadow()
 *  Insert a guest eip to host eip pair if it is not yet created.
 */
 void shack_set_shadow(CPUState *env, target_ulong guest_eip, unsigned long *host_eip)
{
    struct shadow_pair *sp = SHACK_HASHTBL_LOOKUP(env, guest_eip);
    if(sp) {
        sp->host_eip = host_eip; // TODO:
    }
}

/*
 * helper_shack_flush()
 *  Reset shadow stack.
 */
void helper_shack_flush(CPUState *env)
{
    env->shack_top = env->shack;
}

/*
 * push_shack()
 *  Push next guest eip into shadow stack.
 */
void push_shack(CPUState *env, TCGv_ptr cpu_env, target_ulong next_eip)
{
    /*
    if(shack_top == shack_end) { // stack not full
        helper_shack_flush(env);
    }

    struct shadow_pair *sp = SHACK_HASHTBL_LOOKUP(env, next_eip);
    if(!sp) {
        SHACK_HASHTBL_INSERT(env, next_eip, NULL);
    }
    shack_top = sp;
    shack_top += sizeof(void*);
    */
}

/*
 * pop_shack()
 *  Pop next host eip from shadow stack.
 */
void pop_shack(TCGv_ptr cpu_env, TCGv next_eip)
{
    /*
    if(shack_top != shack) {
        shack_top -= sizeof(void*);
        struct shadow_pair *sp = (struct shadow_pair*) shack_top;
        if(sp->guest_eip == next_eip && sp->host_eip != NUlL) {
            *gen_opc_ptr++ = INDEX_op_jmp;
            *gen_opparam_ptr++ = sp->host_eip;
        }
    }
    */
}

/*
 * SHACK_HASHTBL_LOOKUP()
 *  Lookup the hash table to find whether the guest_eip is in the hash table.
 */
struct shadow_pair* SHACK_HASHTBL_LOOKUP(CPUState *env, target_ulong guest_eip)
{
    unsigned int index = guest_eip % SHACK_HASHTBL_SIZE;
    list_t *head = &((list_t*)env->shadow_hash_table)[index];
    if(list_empty(head)) return NULL;

    list_t *l = head->next;
    while(l!=head) {
        l = l->next;
        struct shadow_pair *sp = container_of(l, struct shadow_pair, l);
        if(sp->guest_eip == guest_eip)
            return sp;
    }
    return NULL;
}
/*
 * SHACK_HASHTBL_INSERT()
 *  Add the entry into the hash table;
 */
void SHACK_HASHTBL_INSERT(CPUState *env, target_ulong guest_eip, unsigned long *host_eip)
{
    // TODO:
    unsigned int index = guest_eip % SHACK_HASHTBL_SIZE;
    list_t *head = &((list_t*)env->shadow_hash_table)[index];
    struct shadow_pair *sp = malloc(sizeof(struct shadow_pair));
    sp->guest_eip = guest_eip;
    sp->host_eip = host_eip;
    list_init(&sp->l);
    list_add(&sp->l, head);
}

/*
 * SHACK_HASHTBL_REMOVE()
 *  Remove the entry of hash table.
 */
void SHACK_HASHTBL_REMOVE(struct shadow_pair *sp)
{
    list_del(&sp->l);
}


/*
 * Indirect Branch Target Cache
 */
__thread int update_ibtc;

/*
 * helper_lookup_ibtc()
 *  Look up IBTC. Return next host eip if cache hit or
 *  back-to-dispatcher stub address if cache miss.
 */
void *helper_lookup_ibtc(target_ulong guest_eip)
{
    return optimization_ret_addr;
}

/*
 * update_ibtc_entry()
 *  Populate eip and tb pair in IBTC entry.
 */
void update_ibtc_entry(TranslationBlock *tb)
{
}

/*
 * ibtc_init()
 *  Create and initialize indirect branch target cache.
 */
static inline void ibtc_init(CPUState *env)
{
}

/*
 * init_optimizations()
 *  Initialize optimization subsystem.
 */
int init_optimizations(CPUState *env)
{
    shack_init(env);
    ibtc_init(env);

    return 0;
}

/*
 * vim: ts=8 sts=4 sw=4 expandtab
 */
