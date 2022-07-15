
/* auto-generated by gen_syscalls.py, don't edit */
#ifndef Z_INCLUDE_SYSCALLS_RAND32_H
#define Z_INCLUDE_SYSCALLS_RAND32_H


#include <tracing/tracing_syscall.h>

#ifndef _ASMLANGUAGE

#include <syscall_list.h>
#include <syscall.h>

#include <linker/sections.h>


#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic push
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#if !defined(__XCC__)
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t z_impl_sys_rand32_get(void);

__pinned_func
static inline uint32_t sys_rand32_get(void)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		/* coverity[OVERRUN] */
		return (uint32_t) arch_syscall_invoke0(K_SYSCALL_SYS_RAND32_GET);
	}
#endif
	compiler_barrier();
	return z_impl_sys_rand32_get();
}

#if (CONFIG_TRACING_SYSCALL == 1)
#ifndef DISABLE_SYSCALL_TRACING

#define sys_rand32_get() ({ 	uint32_t retval; 	sys_port_trace_syscall_enter(K_SYSCALL_SYS_RAND32_GET, sys_rand32_get); 	retval = sys_rand32_get(); 	sys_port_trace_syscall_exit(K_SYSCALL_SYS_RAND32_GET, sys_rand32_get, retval); 	retval; })
#endif
#endif


extern void z_impl_sys_rand_get(void * dst, size_t len);

__pinned_func
static inline void sys_rand_get(void * dst, size_t len)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		/* coverity[OVERRUN] */
		arch_syscall_invoke2(*(uintptr_t *)&dst, *(uintptr_t *)&len, K_SYSCALL_SYS_RAND_GET);
		return;
	}
#endif
	compiler_barrier();
	z_impl_sys_rand_get(dst, len);
}

#if (CONFIG_TRACING_SYSCALL == 1)
#ifndef DISABLE_SYSCALL_TRACING

#define sys_rand_get(dst, len) do { 	sys_port_trace_syscall_enter(K_SYSCALL_SYS_RAND_GET, sys_rand_get, dst, len); 	sys_rand_get(dst, len); 	sys_port_trace_syscall_exit(K_SYSCALL_SYS_RAND_GET, sys_rand_get, dst, len); } while(false)
#endif
#endif


extern int z_impl_sys_csrand_get(void * dst, size_t len);

__pinned_func
static inline int sys_csrand_get(void * dst, size_t len)
{
#ifdef CONFIG_USERSPACE
	if (z_syscall_trap()) {
		/* coverity[OVERRUN] */
		return (int) arch_syscall_invoke2(*(uintptr_t *)&dst, *(uintptr_t *)&len, K_SYSCALL_SYS_CSRAND_GET);
	}
#endif
	compiler_barrier();
	return z_impl_sys_csrand_get(dst, len);
}

#if (CONFIG_TRACING_SYSCALL == 1)
#ifndef DISABLE_SYSCALL_TRACING

#define sys_csrand_get(dst, len) ({ 	int retval; 	sys_port_trace_syscall_enter(K_SYSCALL_SYS_CSRAND_GET, sys_csrand_get, dst, len); 	retval = sys_csrand_get(dst, len); 	sys_port_trace_syscall_exit(K_SYSCALL_SYS_CSRAND_GET, sys_csrand_get, dst, len, retval); 	retval; })
#endif
#endif


#ifdef __cplusplus
}
#endif

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#endif

#endif
#endif /* include guard */
