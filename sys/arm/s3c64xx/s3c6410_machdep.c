/*-
 * Copyright (c) 2011 Damjan Marion.
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/mv/mv_machdep.c
 */

#include "opt_ddb.h"
#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/cons.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/kdb.h>
#include <sys/msgbuf.h>
#include <machine/reg.h>
#include <machine/cpu.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_map.h>
#include <vm/vnode_pager.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/pcb.h>
#include <machine/undefined.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/armreg.h>
#include <machine/bus.h>
#include <sys/reboot.h>


/*
 * This is the number of L2 page tables required for covering max
 * (hypothetical) memsize of 4GB and all kernel mappings (vectors, msgbuf,
 * stacks etc.), uprounded to be divisible by 4.
 */
#define KERNEL_PT_MAX	78

/* Define various stack sizes in pages */
#define IRQ_STACK_SIZE	1
#define ABT_STACK_SIZE	1
#define UND_STACK_SIZE	1

/* Physical and virtual addresses for some global pages */
vm_paddr_t phys_avail[10];
vm_paddr_t dump_avail[4];
struct pv_addr systempage;

#define EARLY_UART_PA_BASE 0x7f005000
#define EARLY_UART_VA_BASE 0xef005000

void
early_print_init()
{
        volatile uint32_t *mmu_tbl;
#if (STARTUP_PAGETABLE_ADDR < PHYSADDR) || (STARTUP_PAGETABLE_ADDR > (PHYSADDR + (64 * 1024 * 1024)))
#error STARTUP_PAGETABLE_ADDR is not withing initial MMU table, early print support not possible
#endif
	mmu_tbl = (volatile uint32_t*)STARTUP_PAGETABLE_ADDR;

	mmu_tbl[(EARLY_UART_VA_BASE >> L1_S_SHIFT)] =
		L1_TYPE_S | L1_S_AP(AP_KRW) | (EARLY_UART_PA_BASE & L1_S_FRAME);

	__asm __volatile ("mcr  p15, 0, r0, c8, c7, 0");        /* invalidate I+D TLBs */
	__asm __volatile ("mcr  p15, 0, r0, c7, c10, 4");       /* drain the write buffer */
}

void early_putstr(unsigned char *str)
{
	volatile uint8_t *p_utrstat = (volatile uint8_t*) (EARLY_UART_VA_BASE + 0x10);
	volatile uint8_t *p_utxh = (volatile uint8_t*) (EARLY_UART_VA_BASE + 0x20);

	do {
		while ((*p_utrstat & 0x2) == 0);
		*p_utxh = *str;

		if (*str == '\n')
		{
			while ((*p_utrstat & 0x2) == 0);
			*p_utxh = '\r';
		}
	} while (*++str != '\0');
}

void *
initarm(void *mdp, void *unused __unused)
{
	early_print_init();
	early_putstr("initarm()\r\n");
	while(1);
}

struct arm32_dma_range *
bus_dma_get_range(void)
{
	return (NULL);
}

int
bus_dma_get_range_nb(void)
{
	return(0);
}
