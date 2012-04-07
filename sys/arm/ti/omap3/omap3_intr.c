/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/intr.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <arm/omap/omap3/omap3var.h>
#include <arm/omap/omap3/omap35xx_reg.h>



/*
 * There are a number of ways that interrupt handling is implemented in
 * the various ARM platforms, the PXA has the neatest way, it creates another
 * device driver that handles everything. However IMO this is rather heavy-
 * weight for playing with IRQs which should be quite fast ... so I've
 * gone for something similar to the IXP425, which just directly plays with
 * registers. This assumes that the interrupt control registers are already
 * mapped in virtual memory at a fixed virtual address ... simplies.
 * 
 * The intcps (OMAP3 interrupt controller) has some nice registers, were
 * you write a bit to clear or set the mask register ... I think in theory
 * that means that you don't need to disable interrupts while doing this,
 * because it is an atomic operation.
 *
 * TODO: check this.
 *
 */



#define	INTCPS_SYSCONFIG        0x10
#define	INTCPS_SYSSTATUS        0x14
#define	INTCPS_SIR_IRQ          0x40
#define	INTCPS_SIR_FIQ          0x44
#define	INTCPS_CONTROL          0x48
#define	INTCPS_PROTECTION       0x4C
#define	INTCPS_IDLE             0x50
#define	INTCPS_IRQ_PRIORITY     0x60
#define	INTCPS_FIQ_PRIORITY     0x64
#define	INTCPS_THRESHOLD        0x68
#define INTCPS_ITR(n)           (0x80 + (0x20 * (n)))
#define INTCPS_MIR(n)           (0x84 + (0x20 * (n)))
#define INTCPS_MIR_CLEAR(n)     (0x88 + (0x20 * (n)))
#define INTCPS_MIR_SET(n)       (0x8C + (0x20 * (n)))
#define INTCPS_ISR_SET(n)       (0x90 + (0x20 * (n)))
#define INTCPS_ISR_CLEAR(n)     (0x94 + (0x20 * (n)))
#define INTCPS_PENDING_IRQ(n)   (0x98 + (0x20 * (n)))
#define INTCPS_PENDING_FIQ(n)   (0x9C + (0x20 * (n)))
#define INTCPS_ILR(m)           (0x100 + (0x4 * (m)))




/**
 *	omap3_post_filter_intr - called after the IRQ has been filtered
 *	@arg: the IRQ number
 *
 *	Called after the interrupt handler has done it's stuff, can be used to
 *	clean up interrupts that haven't been handled properly.
 *
 *
 *	RETURNS:
 *	nothing
 */
void
omap3_post_filter_intr(void *arg)
{
	/* uintptr_t irq = (uintptr_t) arg; */
	
	/* data synchronization barrier */
	cpu_drain_writebuf();
}





/**
 *	arm_mask_irq - masks an IRQ (disables it)
 *	@nb: the number of the IRQ to mask (disable)
 *
 *	Disables the interrupt at the HW level.
 *
 *
 *	RETURNS:
 *	nothing
 */
void
arm_mask_irq(uintptr_t nb)
{
	bus_space_write_4(g_omap3_softc->sc_iotag, g_omap3_softc->sc_intcps_ioh,
	                  INTCPS_MIR_SET(nb >> 5), 1UL << (nb & 0x1F));
}


/**
 *	arm_unmask_irq - unmasks an IRQ (enables it)
 *	@nb: the number of the IRQ to unmask (enable)
 *
 *	Enables the interrupt at the HW level.
 *
 *
 *	RETURNS:
 *	nothing
 */
void
arm_unmask_irq(uintptr_t nb)
{
	// printf("[BRG] unmasking IRQ %d (off %d, bit %d)\n", nb, (nb >> 5), (nb & 0x1F));

	bus_space_write_4(g_omap3_softc->sc_iotag, g_omap3_softc->sc_intcps_ioh,
	                  INTCPS_MIR_CLEAR(nb >> 5), 1UL << (nb & 0x1F));
}



/**
 *	arm_get_next_irq - gets the next tripped interrupt
 *	@last_irq: the number of the last IRQ processed
 *
 *	Enables the interrupt at the HW level.
 *
 *
 *	RETURNS:
 *	nothing
 */
int
arm_get_next_irq(int last_irq)
{
	uint32_t active_irq;
	
	/* clean-up the last IRQ */
	if (last_irq != -1) {
		
		/* clear the interrupt status flag */
		bus_space_write_4(g_omap3_softc->sc_iotag, g_omap3_softc->sc_intcps_ioh,
		                  INTCPS_ISR_CLEAR(last_irq >> 5),
		                  1UL << (last_irq & 0x1F));
	
		/* tell the interrupt logic we've dealt with the interrupt */
		bus_space_write_4(g_omap3_softc->sc_iotag, g_omap3_softc->sc_intcps_ioh,
		                  INTCPS_CONTROL, 1);
	}
	
	/* Get the next active interrupt */
	active_irq = bus_space_read_4(g_omap3_softc->sc_iotag,
	                              g_omap3_softc->sc_intcps_ioh, INTCPS_SIR_IRQ);

	/* Check for spurious interrupt */
	if ((active_irq & 0xffffff80) == 0xffffff80) {
		device_printf(g_omap3_softc->sc_dev, "Spurious interrupt detected "
		              "(0x%08x)\n", active_irq);
		return -1;
	}

	/* Just get the active IRQ part */
	active_irq &= 0x7F;
	
	/* Return the new IRQ if it is different from the previous */
	if (active_irq != last_irq)
		return active_irq;
	else
		return -1;
}


/**
 *	omap3_setup_intr_controller - configures and enables the OMAP3 interrupt
 *                                controller (INTCPS)
 *
 *
 *
 *	RETURNS:
 *	nothing
 */
int
omap3_setup_intr_controller(struct omap3_softc *sc, const const int *irqs)
{
	uint32_t syscfg;
	uint32_t i;
	
	if (sc != g_omap3_softc)
		panic("Invalid omap3 soft context\n");


	/* Reset the interrupt controller */
	bus_space_write_4(g_omap3_softc->sc_iotag, g_omap3_softc->sc_intcps_ioh,
	                  INTCPS_SYSCONFIG, 0x2);
	
	/* Loop a number of times to check if the INTCPS has come out of reset */
	for (i = 0; i < 10000; i++) {
		syscfg = bus_space_read_4(g_omap3_softc->sc_iotag,
		                          g_omap3_softc->sc_intcps_ioh, INTCPS_SYSCONFIG);
		if (syscfg & 0x1UL)
			break;
	}
	


	return 0;
}

