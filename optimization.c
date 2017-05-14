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

#define SHACK_HASHTBL_SIZE 65536

list_t *shadow_hash_list;

static inline void shack_init(CPUState *env)
{
#ifdef ENABLE_OPTIMIZATION
#ifdef ENABLE_OPTIMIZATION_SHACK

    /* allocate shadow stack */
    // store guest return address
    env->shack = (struct shadow_pair**) malloc(SHACK_SIZE * sizeof(struct shadow_pair*));
    env->shack_top = env->shack;
    env->shack_end = env->shack + SHACK_SIZE * sizeof(struct shadow_pair*);

    // store a hash table
    env->shadow_hash_table = (list_t*) malloc(SHACK_HASHTBL_SIZE * sizeof(list_t));

    int i;
    for(i=0 ; i<SHACK_HASHTBL_SIZE ; ++i)
        list_init(&((list_t*)env->shadow_hash_table)[i]);

#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, "[SHADOW STACK] shack_init\n");
    fprintf(stderr, "\n");
    //dump_shack_structure(env);
    dump_shack(env);
#endif // ENABLE_OPTIMIZATION_DEBUG
#endif // ENABLE_OPTIMIZATION_SHACK
#endif // ENABLE_OPTIMIZATION
}

/*
 * shack_set_shadow()
 *  Insert a guest eip to host eip pair if it is not yet created.
 */
 void shack_set_shadow(CPUState *env, target_ulong guest_eip, unsigned long *host_eip)
{
#ifdef ENABLE_OPTIMIZATION
#ifdef ENABLE_OPTIMIZATION_SHACK
#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, "[SHADOW STACK] shack_set_shadow\n");
    fprintf(stderr, "               quest_eip: 0x%X\n", guest_eip);
    fprintf(stderr, "               host_eip: %p\n", host_eip);
    fprintf(stderr, "\n");
#endif // ENABLE_OPTIMIZATION_DEBUG
    struct shadow_pair *sp = SHACK_HASHTBL_LOOKUP(env, guest_eip);
    if(!sp) {
        sp = SHACK_HASHTBL_INSERT(env, guest_eip, host_eip);
    }
    else if(!sp->host_eip) {
        sp->host_eip = host_eip;
    }
#endif // ENABLE_OPTIMIZATION_SHACK
#endif // ENABLE_OPTIMIZATION
}

/*
 * helper_shack_flush()
 *  Reset shadow stack.
 */
void helper_shack_flush(CPUState *env)
{
#ifdef ENABLE_OPTIMIZATION
#ifdef ENABLE_OPTIMIZATION_SHACK
    env->shack_top = env->shack;
#endif // ENABLE_OPTIMIZATION_SHACK
#endif // ENABLE_OPTIMIZATION
}

void helper_push_shack(CPUState *env, target_ulong next_eip)
{
#ifdef ENABLE_OPTIMIZATION_SHACK
#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, "[SHADOW STACK] Helper Push()\n");
    fprintf(stderr, "               next_eip: 0x%X\n", next_eip);
    fprintf(stderr, "\n");
    //dump_shack_structure(env);
#endif // ENABLE_OPTIMIZATION_DEBUG

    // Check whether the stack is full
    if(env->shack_top == env->shack_end) {
        // flush the stack
        env->shack_top = env->shack;
    }

    // Push the shadow pair onto the stack
    struct shadow_pair *sp = SHACK_HASHTBL_LOOKUP(env, next_eip);
    if(!sp) {
        sp = SHACK_HASHTBL_INSERT(env, next_eip, NULL);
    }

    *((struct shadow_pair **)env->shack_top) = sp;
    env->shack_top += sizeof(struct shadow_pair*);

    //dump_shack(env);
#endif // ENABLE_OPTIMIZATION_SHACK
}

void* helper_pop_shack(CPUState *env, target_ulong next_eip)
{
    void *host_eip = NULL;
#ifdef ENABLE_OPTIMIZATION_SHACK
    if(env->shack_top != env->shack) {
        env->shack_top -= sizeof(struct shadow_pair*);
        struct shadow_pair *sp = *((struct shadow_pair**) env->shack_top);
        if(sp->guest_eip == next_eip && sp->host_eip != NULL) {
            host_eip = sp->host_eip;
        }
#ifdef ENABLE_OPTIMIZATION_DEBUG
        fprintf(stderr, "[SHADOW STACK] Helper Pop()\n");
        fprintf(stderr, "               next_eip: 0x%X\n", next_eip);
        fprintf(stderr, "               top->guest_eip: 0x%X\n", sp->guest_eip);
        fprintf(stderr, "               top->host_eip: %p\n", sp->host_eip);
        fprintf(stderr, "\n");
        //dump_shack_structure(env);
#endif
    }
#ifdef ENABLE_OPTIMIZATION_DEBUG
    else {
        fprintf(stderr, "[SHADOW STACK] Helper Pop()\n");
        fprintf(stderr, "               Stack is empty.\n");
        fprintf(stderr, "\n");
        //dump_shack_structure(env);
    }
#endif

    //dump_shack(env);

#endif // ENABLE_OPTIMIZATION_SHACK
    return host_eip;
}

void helper_shack_debug(CPUState *env)
{
#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, "############# Debug ##############\n");
    dump_shack(env);
#endif // ENABLE_OPTIMIZATION_DEBUG
}

void helper_shack_debug2(target_ulong data)
{
#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, "Debug2: 0x%X\n", data);
#endif // ENABLE_OPTIMIZATION_DEBUG
}

/*
 * push_shack()
 *  Push next guest eip into shadow stack.
 */
void push_shack(CPUState *env, TCGv_ptr cpu_env, target_ulong next_eip)
{
#ifdef ENABLE_OPTIMIZATION
#ifdef ENABLE_OPTIMIZATION_SHACK
    //gen_helper_push_shack(cpu_env, tcg_const_tl(next_eip));
    struct shadow_pair *sp = SHACK_HASHTBL_LOOKUP(env, next_eip);
    if(!sp) {
        sp = SHACK_HASHTBL_INSERT(env, next_eip, NULL);
    }
    
    // if(env->shack_top == env->shack_end) {
    TCGv_ptr tcg_shack_top = tcg_temp_new_ptr();
    TCGv_ptr tcg_shack_end = tcg_temp_new_ptr();
    tcg_gen_ld_ptr(tcg_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(tcg_shack_end, cpu_env, offsetof(CPUState, shack_end));

    int label_push = gen_new_label();
    tcg_gen_brcond_tl(TCG_COND_NE, tcg_shack_top, tcg_shack_end, label_push);

    //   env->shack_top = env->shack;
    TCGv_ptr tcg_shack = tcg_temp_new_ptr();
    tcg_gen_ld_ptr(tcg_shack, cpu_env, offsetof(CPUState, shack));
    tcg_gen_st_ptr(tcg_shack, cpu_env, offsetof(CPUState, shack_top));

    // }
    gen_set_label(label_push);

    // *((struct shadow_pair **)env->shack_top) = sp;
    tcg_gen_ld_ptr(tcg_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_st_ptr(tcg_const_ptr(sp), tcg_shack_top, 0);

    // env->shack_top += sizeof(struct shadow_pair*);
    tcg_gen_addi_ptr(tcg_shack_top, tcg_shack_top, sizeof(struct shadow_pair*));
    tcg_gen_st_ptr(tcg_shack_top, cpu_env, offsetof(CPUState, shack_top));

    // free
    tcg_temp_free_ptr(tcg_shack_top);
    tcg_temp_free_ptr(tcg_shack_end);
    tcg_temp_free_ptr(tcg_shack);
#endif // ENABLE_OPTIMIZATION_SHACK
#endif // ENABLE_OPTIMIZATION
}

/*
 * pop_shack()
 *  Pop next host eip from shadow stack.
 */
void pop_shack(TCGv_ptr cpu_env, TCGv next_eip)
{
#ifdef ENABLE_OPTIMIZATION
#ifdef ENABLE_OPTIMIZATION_SHACK
    //TCGv_ptr tcg_host_eip = tcg_temp_new_ptr();
    //gen_helper_pop_shack(tcg_host_eip, cpu_env, next_eip);

    TCGv_ptr tcg_shack_top = tcg_temp_local_new_ptr();
    TCGv_ptr tcg_shack = tcg_temp_local_new_ptr();
    TCGv_ptr tcg_sp = tcg_temp_local_new_ptr();
    TCGv_ptr tcg_sp_guest_eip = tcg_temp_local_new_ptr();
    TCGv_ptr tcg_sp_host_eip = tcg_temp_local_new_ptr();

    TCGv tcg_next_eip = tcg_temp_local_new();
    tcg_gen_mov_tl(tcg_next_eip, next_eip);

    int label_exit = gen_new_label();

    tcg_gen_ld_ptr(tcg_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(tcg_shack, cpu_env, offsetof(CPUState, shack));
    tcg_gen_brcond_ptr(TCG_COND_EQ, tcg_shack_top, tcg_shack, label_exit);

    tcg_gen_addi_ptr(tcg_shack_top, tcg_shack_top, -sizeof(struct shadow_pair*));
    tcg_gen_st_ptr(tcg_shack_top, cpu_env, offsetof(CPUState, shack_top));
    tcg_gen_ld_ptr(tcg_sp, tcg_shack_top, 0);
    tcg_gen_ld_ptr(tcg_sp_guest_eip, tcg_sp, offsetof(struct shadow_pair, guest_eip));
    tcg_gen_brcond_ptr(TCG_COND_NE, tcg_sp_guest_eip, tcg_next_eip, label_exit);

    tcg_gen_ld_ptr(tcg_sp_host_eip, tcg_sp, offsetof(struct shadow_pair, host_eip));
    tcg_gen_brcond_ptr(TCG_COND_EQ, tcg_sp_host_eip, tcg_const_ptr(NULL), label_exit);

    *gen_opc_ptr++ = INDEX_op_jmp;
    *gen_opparam_ptr++ = tcg_sp_host_eip;


    gen_set_label(label_exit);

    // free
    tcg_temp_free_ptr(tcg_shack_top);
    tcg_temp_free_ptr(tcg_shack);
    tcg_temp_free_ptr(tcg_sp);
    tcg_temp_free_ptr(tcg_sp_guest_eip);
    tcg_temp_free_ptr(tcg_sp_host_eip);
    tcg_temp_free_ptr(tcg_next_eip);
#endif // ENABLE_OPTIMIZATION_SHACK
#endif // ENABLE_OPTIMIZATION
}

/*
 * dump_shack_structure()
 *  Dump the shadow stack.
 */
void dump_shack_structure(CPUState *env)
{
#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    fprintf(stderr, "> Dump shack_structure\n");
    fprintf(stderr, ">     env->shack: %p\n", env->shack);
    fprintf(stderr, ">     env->shack_top: %p\n", env->shack_top);
    fprintf(stderr, ">     env->shack_end: %p\n", env->shack_end);
    fprintf(stderr, ">     env->shadow_hash_table: %p\n", env->shadow_hash_table);
    fprintf(stderr, ">   Now the stack has %d entries.\n",
            (env->shack_top-env->shack)/sizeof(struct shadow_pair*));
    fprintf(stderr, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
#endif // ENABLE_OPTIMIZATION_DEBUG
}

/*
 * dump_shack()
 *  Dump the shadow stack.
 */
void dump_shack(CPUState *env)
{
#ifdef ENABLE_OPTIMIZATION_DEBUG
    dump_shack_structure(env);
    fprintf(stderr, "+-----------------------------\n");
    fprintf(stderr, "| Shadow Stack Dump\n");
    struct shadow_pair **iter = env->shack_top;
    fprintf(stderr, "|         stack )             sp: (guest_eip, host_eip)\n");
    while(iter!= env->shack) {
        --iter;
        struct shadow_pair *sp = *iter;
        fprintf(stderr, "|     %p)     %p: (0x%X, %p)\n",
                iter, sp, sp->guest_eip, sp->host_eip);
    }
    fprintf(stderr, "+-----------------------------\n");
#endif // ENABLE_OPTIMIZATION_DEBUG
}


/*
 * SHACK_HASHTBL_DUMP()
 *  Dump the hash table.
 */
void SHACK_HASHTBL_DUMP(CPUState *env)
{
#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, "##############################\n");
    fprintf(stderr, "# Hash Table Dump\n");
    int index;
    for(index=0 ; index<SHACK_HASHTBL_SIZE ; ++index) {
        list_t *head = &((list_t*)env->shadow_hash_table)[index];
        if(list_empty(head))
            continue;

        fprintf(stderr, "# 0x%X: \n", index);
        list_t *l = head->next;
        while(l!=head) {
            struct shadow_pair *sp = container_of(l, struct shadow_pair, l);
            fprintf(stderr, "#     (0x%X, %p)\n", sp->guest_eip, sp->host_eip);
            l = l->next;
        }
    }
    fprintf(stderr, "##############################\n");
#endif // ENABLE_OPTIMIZATION_DEBUG
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
        struct shadow_pair *sp = container_of(l, struct shadow_pair, l);
        if(sp->guest_eip == guest_eip)
            return sp;
        l = l->next;
    }
    return NULL;
}
/*
 * SHACK_HASHTBL_INSERT()
 *  Add the entry into the hash table;
 */
struct shadow_pair* SHACK_HASHTBL_INSERT(CPUState *env, target_ulong guest_eip, unsigned long *host_eip)
{
    unsigned int index = guest_eip % SHACK_HASHTBL_SIZE;
    list_t *head = &((list_t*)env->shadow_hash_table)[index];
    struct shadow_pair *sp = malloc(sizeof(struct shadow_pair));
    sp->guest_eip = guest_eip;
    sp->host_eip = host_eip;
    list_init(&sp->l);
    list_add(&sp->l, head);
    //SHACK_HASHTBL_DUMP(env);
#ifdef ENABLE_OPTIMIZATION_DEBUG
    fprintf(stderr, "[SHADOW STACK] >> Hash Table Insert(0x%X, %p) @ %p\n",
            guest_eip, host_eip, sp);
#endif
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
struct ibtc_table *ibtc_tbl;
target_ulong ibtc_cache_guest_eip;

/*
 * helper_lookup_ibtc()
 *  Look up IBTC. Return next host eip if cache hit or
 *  back-to-dispatcher stub address if cache miss.
 */
void *helper_lookup_ibtc(target_ulong guest_eip)
{
#ifdef ENABLE_OPTIMIZATION_IBTC
    // Cache the guest_eip for the update_ibtc_entry()
    //fprintf(stderr, "Lookup IBTC.\n");
    ibtc_cache_guest_eip = guest_eip;

    unsigned index = guest_eip & IBTC_CACHE_MASK;
    if(ibtc_tbl->htable[index].guest_eip == guest_eip
            && ibtc_tbl->htable[index].tb != NULL) {
        // Cache hit
        // Return the context pointer of the related translation block.
        //fprintf(stderr, "[IBTC] Cache hit!\n");
        return ibtc_tbl->htable[index].tb->tc_ptr;
    }
    
    // Cache miss
    // Enable update_ibtc_entry to update ibtc.
    update_ibtc = 1;
#endif ENABLE_OPTIMIZATION_IBTC
    return optimization_ret_addr;
}

/*
 * update_ibtc_entry()
 *  Populate eip and tb pair in IBTC entry.
 */
void update_ibtc_entry(TranslationBlock *tb)
{
#ifdef ENABLE_OPTIMIZATION_IBTC
    //fprintf(stderr, "update_ibtc_entry\n");
    // Get the guest_eip from ibtc_cache_guest_eip,
    //  which was set by helper_lookup_ibtc().
    unsigned index = ibtc_cache_guest_eip & IBTC_CACHE_MASK;
    ibtc_tbl->htable[index].guest_eip = ibtc_cache_guest_eip;
    ibtc_tbl->htable[index].tb = tb;

    // Let QEMU will not call this again.
    update_ibtc = 0;
#endif ENABLE_OPTIMIZATION_IBTC
}

/*
 * ibtc_init()
 *  Create and initialize indirect branch target cache.
 */
static inline void ibtc_init(CPUState *env)
{
#ifdef ENABLE_OPTIMIZATION_IBTC
    ibtc_tbl = (struct ibtc_table*) malloc(sizeof(struct ibtc_table));
    memset(ibtc_tbl, 0, sizeof(struct ibtc_table));
#endif ENABLE_OPTIMIZATION_IBTC
    update_ibtc = 0;
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
