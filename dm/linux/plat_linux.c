// The Linux host's thread slots (dm.h dm_slot_get): four words of the host's initial-exec TLS, which rewritten code (DM_TARGET=linux, dm/tools/nso_asm.py) finds
// at tpidr_el0 + each slot's key page value. glibc's thread pointer is the C library's own and is never changed by the guest, whose msr / mrs tpidr_el0 are
// rewritten to slot KTLS; an initial-exec variable of the executable sits at the same offset from it in every thread. The x18 block of Android (plat_android.c)
// does not fit here: desktop Linux libraries are not built with x18 reserved, so a call from the host into one may change it.
// Everything else of the platform layer is plat_android.c, which the Linux build compiles too.
#include "dm.h"
#include <sys/mman.h>

static __thread uint64_t t_slots[4] __attribute__((tls_model("initial-exec"), aligned(8)));   // K18, KTLS, KTLR, KGATE (loader.c create_keys)

int64_t dm_slot_keypage_value(pthread_key_t k) { return (int64_t)((uintptr_t)&t_slots[k] - (uintptr_t)__builtin_thread_pointer()); }
void *dm_slot_get(pthread_key_t k) { return (void *)(uintptr_t)t_slots[k]; }
void dm_slot_set(pthread_key_t k, const void *v) { t_slots[k] = (uint64_t)(uintptr_t)v; }
void dm_slots_thread_init(void) {}    // every thread has its TLS from its start
void dm_slots_x18_restore(void) {}    // x18 is not used for the slots

// Address space below 2^39, where a Switch's addresses stay: desktop arm64 kernels have 48-bit address spaces and hand mappings out top down, far above it (most
// Android phones have 39-bit ones). The first free place from 4 GiB up, in 4 GiB steps; MAP_FIXED_NOREPLACE keeps what is mapped there, and on a kernel older than
// 4.17, which takes the address only as a hint, a mapping elsewhere is given back. NULL when nothing below 2^39 is free
void *dm_linux_map_low(size_t len, int prot, int flags) {
    const uintptr_t step = (uintptr_t)1 << 32, top = (uintptr_t)1 << 39;
    for (uintptr_t a = step; a + len <= top; a += step) {
        void *p = mmap((void *)a, len, prot, flags | MAP_FIXED_NOREPLACE, -1, 0);
        if (p == (void *)a) return p;
        if (p != MAP_FAILED) munmap(p, len);
    }
    return NULL;
}
