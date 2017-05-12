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
#ifdef ENABLE_OPTIMIZATION

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

#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, "[SHADOW STACK] shack_init\n");
    fprintf(stderr, "[SHADOW STACK] env->shack: %p\n", env->shack);
    fprintf(stderr, "[SHADOW STACK] env->shack_top: %p\n", env->shack_top);
    fprintf(stderr, "[SHADOW STACK] env->shack_end: %p\n", env->shack_end);
    fprintf(stderr, "[SHADOW STACK] env->shadow_hash_table: %p\n", env->shadow_hash_table);
    fprintf(stderr, "\n");
#endif
#endif
}

/*
 * shack_set_shadow()
 *  Insert a guest eip to host eip pair if it is not yet created.
 */
 void shack_set_shadow(CPUState *env, target_ulong guest_eip, unsigned long *host_eip)
{
#ifdef ENABLE_OPTIMIZATION
#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, "[SHADOW STACK] shack_set_shadow\n");
    fprintf(stderr, "               quest_eip: 0x%X\n", guest_eip);
    fprintf(stderr, "               host_eip: 0x%X\n", host_eip);
    fprintf(stderr, "\n");
#endif
    struct shadow_pair *sp = SHACK_HASHTBL_LOOKUP(env, guest_eip);
    if(!sp) {
        sp = SHACK_HASHTBL_INSERT(env, guest_eip, host_eip);
    }
    /*
    if(sp) {
        sp->host_eip = host_eip; // TODO:
    }
    */
#endif
}

/*
 * helper_shack_flush()
 *  Reset shadow stack.
 */
void helper_shack_flush(CPUState *env)
{
#ifdef ENABLE_OPTIMIZATION
    env->shack_top = env->shack;
#endif
}

void helper_push_shack(CPUState *env, target_ulong next_eip)
{
#ifdef ENABLE_OPTIMIZATION
    fprintf(stderr, "[SHADOW STACK] Helper Push()\n");
    fprintf(stderr, "               next_eip: 0x%lX\n", next_eip);
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
}

void helper_pop_shack(CPUState *env, target_ulong next_eip)
{
#ifdef ENABLE_OPTIMIZATION
    fprintf(stderr, "[SHADOW STACK] Helper Pop()\n");
    fprintf(stderr, "               next_eip: 0x%lX\n", next_eip);
    struct shadow_pair *sp = (struct shadow_pair*) env->shack_top;
    sp--;
    fprintf(stderr, "               top->guest_eip: 0x%X\n", sp->guest_eip);
    fprintf(stderr, "               top->host_eip: 0x%X\n", sp->host_eip);
    fprintf(stderr, "\n");
    fflush(stderr);
#endif
}


/*
 * push_shack()
 *  Push next guest eip into shadow stack.
 */
void push_shack(CPUState *env, TCGv_ptr cpu_env, target_ulong next_eip)
{
#ifdef ENABLE_OPTIMIZATION
    gen_helper_push_shack(cpu_env, tcg_const_i32(next_eip));
    /*
     * tcg_shack_top = ld_ptr cpu_env, offsetof(CPUState, shack_top)
     * tcg_shack_end = ld_ptr cpu_env, offsetof(CPUState, shack_end)
     * bne tcg_shack_top, tcg_shack_end, label_shack_not_full
     *
     * tcg_shack = ld_ptr cpu_env, offsetof(CPUState, shack)
     * st tcg_shack, cpu_env, offsetof(CPUState, shack_top)
     *
     * label_shack_not_full:
     */
    TCGv_ptr tcg_shack_top = tcg_temp_new_ptr();
    TCGv_ptr tcg_shack_end = tcg_temp_new_ptr();
    tcg_gen_ld_ptr(tcg_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(tcg_shack_end, cpu_env, offsetof(CPUState, shack_end));

    int label_shack_not_full = gen_new_label();
    tcg_gen_brcond_tl(TCG_COND_NE, tcg_shack_top, tcg_shack_end, label_shack_not_full);

    TCGv_ptr tcg_shack = tcg_temp_new_ptr();
    tcg_gen_ld_ptr(tcg_shack, cpu_env, offsetof(CPUState, shack));
    tcg_gen_st_ptr(tcg_shack, cpu_env, offsetof(CPUState, shack_top));

    gen_set_label(label_shack_not_full);

    struct shadow_pair *sp = SHACK_HASHTBL_LOOKUP(env, next_eip);
    if(!sp) {
        sp = SHACK_HASHTBL_INSERT(env, next_eip, NULL);
    }

    /*
     * st tcg_const_ptr(sp), tcg_shack_top, 0
     * tcg_shack_top = add tcg_shack_top, sizeof(void*)
     * st tcg_shack_top, cpu_env, offsetof(CPUState, shack_top)
     */
    tcg_gen_ld_ptr(tcg_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_st_ptr(tcg_const_ptr((tcg_target_long)sp), tcg_shack_top, 0); // TODO:

    tcg_gen_addi_ptr(tcg_shack_top, tcg_shack_top, sizeof(void*));
    tcg_gen_st_ptr(tcg_shack_top, cpu_env, offsetof(CPUState, shack_top));

    // TODO: free
    tcg_temp_free_ptr(tcg_shack_top);
    tcg_temp_free_ptr(tcg_shack_end);
    tcg_temp_free_ptr(tcg_shack);

#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, "\n");
    fprintf(stderr, "               guest_eip: 0x%X\n", sp->guest_eip);
    fprintf(stderr, "               host_eip: 0x%X\n", sp->host_eip);
    fprintf(stderr, "[SHADOW STACK] end of Push (after HASHTBL_LOOKUP)\n");
    fprintf(stderr, "\n");
#endif

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
#endif
}

/*
 * pop_shack()
 *  Pop next host eip from shadow stack.
 */
void pop_shack(TCGv_ptr cpu_env, TCGv next_eip)
{
#ifdef ENABLE_OPTIMIZATION
    gen_helper_pop_shack(cpu_env, next_eip);

    TCGv_ptr tcg_shack_top = tcg_temp_new_ptr();
    TCGv_ptr tcg_shack= tcg_temp_new_ptr();
    tcg_gen_ld_ptr(tcg_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(tcg_shack, cpu_env, offsetof(CPUState, shack));

    int label_exit = gen_new_label();
    tcg_gen_brcond_tl(TCG_COND_EQ, tcg_shack_top, tcg_shack, label_exit);

    tcg_gen_addi_ptr(tcg_shack_top, tcg_shack_top, -sizeof(void*));
    tcg_gen_st_ptr(tcg_shack_top, cpu_env, offsetof(CPUState, shack_top));

    TCGv_ptr tcg_sp = tcg_temp_new_ptr();
    TCGv tcg_sp_guest_eip = tcg_temp_new();
    TCGv_ptr tcg_sp_host_eip = tcg_temp_new_ptr();

    tcg_gen_ld_ptr(tcg_sp, tcg_shack_top, 0);
    tcg_gen_ld_tl(tcg_sp_guest_eip, tcg_sp, offsetof(struct shadow_pair, guest_eip));
    tcg_gen_ld_ptr(tcg_sp_host_eip, tcg_sp, offsetof(struct shadow_pair, host_eip));

    int label_if_fail = gen_new_label();
    tcg_gen_brcond_tl(TCG_COND_NE, tcg_sp_guest_eip, next_eip, label_if_fail);
    tcg_gen_brcond_ptr(TCG_COND_EQ, tcg_sp_host_eip, tcg_const_ptr((tcg_target_long)NULL), label_if_fail);

    *gen_opc_ptr++ = INDEX_op_jmp;
    *gen_opparam_ptr++ = tcg_sp_host_eip;

    gen_set_label(label_if_fail);
    gen_set_label(label_exit);

    // TODO: free
    tcg_temp_free_ptr(tcg_shack_top);
    tcg_temp_free_ptr(tcg_shack);
    tcg_temp_free_ptr(tcg_sp);
    tcg_temp_free(tcg_sp_guest_eip);
    tcg_temp_free_ptr(tcg_sp_host_eip);

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
#endif
}

/*
 * SHACK_HASHTBL_LOOKUP()
 *  Lookup the hash table to find whether the guest_eip is in the hash table.
 */
struct shadow_pair* SHACK_HASHTBL_LOOKUP(CPUState *env, target_ulong guest_eip)
{
#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, "[SHADOW STACK] Hash Table Lookup(0x%X)\n", guest_eip);
#endif
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
struct shadow_pair* SHACK_HASHTBL_INSERT(CPUState *env, target_ulong guest_eip, unsigned long *host_eip)
{
#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, "[SHADOW STACK] >> Hash Table Insert(0x%X, %p)\n", guest_eip, host_eip);
#endif
    // TODO:
    unsigned int index = guest_eip % SHACK_HASHTBL_SIZE;
    list_t *head = &((list_t*)env->shadow_hash_table)[index];
    struct shadow_pair *sp = malloc(sizeof(struct shadow_pair));
    sp->guest_eip = guest_eip;
    sp->host_eip = host_eip;
    list_init(&sp->l);
    list_add(&sp->l, head);
    return sp;
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
