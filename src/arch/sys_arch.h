#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include <stddef.h>
#include "lwip/arch.h"
#include "lwip/err.h"

typedef u8_t sys_sem_t;
typedef u8_t sys_mutex_t;
typedef u8_t sys_mbox_t;
typedef u8_t sys_prot_t;
typedef u8_t sys_thread_t;

static inline sys_prot_t sys_arch_protect(void) {
    return 0;
}

static inline void sys_arch_unprotect(sys_prot_t p) {
    (void)p;
}

static inline err_t sys_mutex_new(sys_mutex_t *mutex) {
    (void)mutex;
    return ERR_OK;
}

static inline void sys_mutex_lock(sys_mutex_t *mutex) {
    (void)mutex;
}

static inline void sys_mutex_unlock(sys_mutex_t *mutex) {
    (void)mutex;
}

static inline void sys_mutex_free(sys_mutex_t *mutex) {
    (void)mutex;
}

static inline int sys_mutex_valid(sys_mutex_t *mutex) {
    (void)mutex;
    return 0;
}

static inline void sys_mutex_set_invalid(sys_mutex_t *mutex) {
    (void)mutex;
}

static inline err_t sys_sem_new(sys_sem_t *sem, u8_t count) {
    (void)sem;
    (void)count;
    return ERR_OK;
}

static inline void sys_sem_signal(sys_sem_t *sem) {
    (void)sem;
}

static inline void sys_sem_wait(sys_sem_t *sem) {
    (void)sem;
}

static inline u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) {
    (void)sem;
    (void)timeout;
    return 0;
}

static inline void sys_sem_free(sys_sem_t *sem) {
    (void)sem;
}

static inline int sys_sem_valid(sys_sem_t *sem) {
    (void)sem;
    return 0;
}

static inline void sys_sem_set_invalid(sys_sem_t *sem) {
    (void)sem;
}

static inline err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
    (void)mbox;
    (void)size;
    return ERR_OK;
}

static inline void sys_mbox_fetch(sys_mbox_t *mbox, void **msg) {
    (void)mbox;
    (void)msg;
}

static inline u32_t sys_mbox_tryfetch(sys_mbox_t *mbox, void **msg) {
    (void)mbox;
    (void)msg;
    return 0;
}

static inline void sys_mbox_post(sys_mbox_t *mbox, void *msg) {
    (void)mbox;
    (void)msg;
}

static inline err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg) {
    (void)mbox;
    (void)msg;
    return ERR_OK;
}

static inline void sys_mbox_free(sys_mbox_t *mbox) {
    (void)mbox;
}

static inline int sys_mbox_valid(sys_mbox_t *mbox) {
    (void)mbox;
    return 0;
}

static inline void sys_mbox_set_invalid(sys_mbox_t *mbox) {
    (void)mbox;
}

#endif
