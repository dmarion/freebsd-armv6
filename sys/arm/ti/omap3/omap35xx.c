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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#define	_ARM32_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <arm/omap/omapvar.h>
#include <arm/omap/omap3/omap3var.h>
#include <arm/omap/omap3/omap35xx_reg.h>

struct omap3_softc *g_omap3_softc = NULL;


/*
 * Standard priority levels for the system -  0 has the highest priority and 63
 * is the lowest.  
 *
 * Currently these are all set to the same standard value.
 */
static const int omap35xx_irq_prio[96] =
{
	0,	/* MPU emulation(2) */
	0,	/* MPU emulation(2) */
	0,	/* MPU emulation(2) */
	0,	/* MPU emulation(2) */
	0,	/* Sidetone MCBSP2 overflow */
	0,	/* Sidetone MCBSP3 overflow */
	0,	/* MPU subsystem secure state-machine abort (2) */
	0,	/* External source (active low) */
	0,	/* RESERVED */
	0,	/* SMX error for debug */
	0,	/* SMX error for application */
	0,	/* PRCM module IRQ */
	0,	/* System DMA request 0(3) */
	0,	/* System DMA request 1(3) */
	0,	/* System DMA request 2 */
	0,	/* System DMA request 3 */
	0,	/* McBSP module 1 IRQ (3) */
	0,	/* McBSP module 2 IRQ (3) */
	0,	/* SmartReflex™ 1 */
	0,	/* SmartReflex™ 2 */
	0,	/* General-purpose memory controller module */
	0,	/* 2D/3D graphics module */
	0,	/* McBSP module 3(3) */
	0,	/* McBSP module 4(3) */
	0,	/* Camera interface request 0 */
	0,	/* Display subsystem module(3) */
	0,	/* Mailbox user 0 request */
	0,	/*  McBSP module 5 (3) */
	0,	/* IVA2 MMU */
	0,	/* GPIO module 1(3) */
	0,	/* GPIO module 2(3) */
	0,	/* GPIO module 3(3) */
	0,	/* GPIO module 4(3) */
	0,	/* GPIO module 5(3) */
	0,	/* GPIO module 6(3) */
	0,	/* USIM interrupt (HS devices only) (4) */
	0,	/* Watchdog timer module 3 overflow */
	0,	/* General-purpose timer module 1 */
	0,	/* General-purpose timer module 2 */
	0,	/* General-purpose timer module 3 */
	0,	/* General-purpose timer module 4 */
	0,	/* General-purpose timer module 5(3) */
	0,	/* General-purpose timer module 6(3) */
	0,	/* General-purpose timer module 7(3) */
	0,	/* General-purpose timer module 8(3) */
	0,	/* General-purpose timer module 9 */
	0,	/* General-purpose timer module 10 */
	0,	/* General-purpose timer module 11 */
	0,	/* McSPI module 4 */
	0,	/* SHA-1/MD5 crypto-accelerator 2 (HS devices only)(4) */
	0,	/* PKA crypto-accelerator (HS devices only) (4) */
	0,	/* SHA-2/MD5 crypto-accelerator 1 (HS devices only) (4) */
	0,	/* RNG module (HS devices only) (4) */
	0,	/* MG function (3) */
	0,	/* McBSP module 4 transmit(3) */
	0,	/* McBSP module 4 receive(3) */
	0,	/* I2C module 1 */
	0,	/* I2C module 2 */
	0,	/* HDQ / One-wire */
	0,	/* McBSP module 1 transmit(3) */
	0,	/* McBSP module 1 receive(3) */
	0,	/* I2C module 3 */
	0,	/* McBSP module 2 transmit(3) */
	0,	/* McBSP module 2 receive(3) */
	0,	/* PKA crypto-accelerator (HS devices only) (4) */
	0,	/* McSPI module 1 */
	0,	/* McSPI module 2 */
	0,	/* RESERVED */
	0,	/* RESERVED */
	0,	/* RESERVED */
	0,	/* RESERVED */
	0,	/* RESERVED */
	0,	/* UART module 1 */
	0,	/* UART module 2 */
	0,	/* UART module 3 (also infrared)(3) */
	0,	/* Merged interrupt for PBIASlite1 and 2 */
	0,	/* OHCI controller HSUSB MP Host Interrupt */
	0,	/* EHCI controller HSUSB MP Host Interrupt */
	0,	/* HSUSB MP TLL Interrupt */
	0,	/* SHA2/MD5 crypto-accelerator 1 (HS devices only) (4) */
	0,	/* Reserved */
	0,	/* McBSP module 5 transmit(3) */
	0,	/* McBSP module 5 receive(3) */
	0,	/* MMC/SD module 1 */
	0,	/* MS-PRO™ module */
	0,	/* Reserved */
	0,	/* MMC/SD module 2 */
	0,	/* MPU ICR */
	0,	/* RESERVED */
	0,	/* McBSP module 3 transmit(3) */
	0,	/* McBSP module 3 receive(3) */
	0,	/* McSPI module 3 */
	0,	/* High-Speed USB OTG controller */
	0,	/* High-Speed USB OTG DMA controller */
	0,	/* MMC/SD module 3 */
	0,	/* General-purpose timer module 12 */
};


static const struct omap_cpu_dev omap35xx_devs[] =
{
	/**	
	 *	OMAP35xx - General Purpose Timers
	 *	This module provides interfaces to the general purpose timers.
	 */
	{	.name      = "omap_gptimer",
		.unit      = 0,
		.mem       = { { OMAP35XX_GPTIMER1_HWBASE,  OMAP35XX_GPTIMER_SIZE },
		               { OMAP35XX_GPTIMER2_HWBASE,  OMAP35XX_GPTIMER_SIZE },
		               { OMAP35XX_GPTIMER3_HWBASE,  OMAP35XX_GPTIMER_SIZE },
		               { OMAP35XX_GPTIMER4_HWBASE,  OMAP35XX_GPTIMER_SIZE },
		               { OMAP35XX_GPTIMER5_HWBASE,  OMAP35XX_GPTIMER_SIZE },
		               { OMAP35XX_GPTIMER6_HWBASE,  OMAP35XX_GPTIMER_SIZE },
		               { OMAP35XX_GPTIMER7_HWBASE,  OMAP35XX_GPTIMER_SIZE },
		               { OMAP35XX_GPTIMER8_HWBASE,  OMAP35XX_GPTIMER_SIZE },
		               { OMAP35XX_GPTIMER9_HWBASE,  OMAP35XX_GPTIMER_SIZE },
		               { OMAP35XX_GPTIMER10_HWBASE, OMAP35XX_GPTIMER_SIZE },
		               { OMAP35XX_GPTIMER11_HWBASE, OMAP35XX_GPTIMER_SIZE },
		               { 0,  0 }
		             },
		.irqs      = {   OMAP35XX_IRQ_GPT1,
		                 OMAP35XX_IRQ_GPT2,
		                 OMAP35XX_IRQ_GPT3,
		                 OMAP35XX_IRQ_GPT4,
		                 OMAP35XX_IRQ_GPT5,
		                 OMAP35XX_IRQ_GPT6,
		                 OMAP35XX_IRQ_GPT7,
		                 OMAP35XX_IRQ_GPT8,
		                 OMAP35XX_IRQ_GPT9,
		                 OMAP35XX_IRQ_GPT10,
		                 OMAP35XX_IRQ_GPT11,
		                 -1,
		              },
	},

	/**	
	 *	OMAP35xx - DMA
	 *	This module provides interfaces to the direct memory access controller
	 */
	{	.name      = "omap_dma",
		.unit      = 0,
		.mem       = { { OMAP35XX_SDMA_HWBASE,  OMAP35XX_SDMA_SIZE },
		               { 0,  0 }
		             },
		.irqs      = {   OMAP35XX_IRQ_SDMA0,
		                 OMAP35XX_IRQ_SDMA1,
		                 OMAP35XX_IRQ_SDMA2,
		                 OMAP35XX_IRQ_SDMA3,
		                 -1,
		              },
	},

	/**	
	 *	OMAP35xx - I2C
	 *	This module provides interfaces to the I2C controller
	 *	Note: generally this should be the first function sub-device because
	 *	      it's used for the TWL power control device.
	 */
	{	.name      = "omap_i2c",
		.unit      = 0,
		.mem       = { { OMAP35XX_I2C1_HWBASE,  OMAP35XX_I2C1_SIZE },
		               { 0,  0 }
		             },
		.irqs      = {   OMAP35XX_IRQ_I2C1,
		                 -1,
		              },
	},
	
	/**	
	 *	OMAP35xx - GPIO
	 *	There are 6 GPIO register sets, with each set representing 32 GPIO
	 *	pins.
	 */
	{	.name      = "gpio",
		.unit      = 0,
		.mem       = { { OMAP35XX_GPIO1_HWBASE,   OMAP35XX_GPIO1_SIZE },
		               { OMAP35XX_GPIO2_HWBASE,   OMAP35XX_GPIO2_SIZE },
		               { OMAP35XX_GPIO3_HWBASE,   OMAP35XX_GPIO3_SIZE },
		               { OMAP35XX_GPIO4_HWBASE,   OMAP35XX_GPIO4_SIZE },
		               { OMAP35XX_GPIO5_HWBASE,   OMAP35XX_GPIO5_SIZE },
		               { OMAP35XX_GPIO6_HWBASE,   OMAP35XX_GPIO6_SIZE },
		               { 0,  0 }
		             },
		.irqs      = {   OMAP35XX_IRQ_GPIO1_MPU,
		                 OMAP35XX_IRQ_GPIO2_MPU,
		                 OMAP35XX_IRQ_GPIO3_MPU,
		                 OMAP35XX_IRQ_GPIO4_MPU,
		                 OMAP35XX_IRQ_GPIO5_MPU,
		                 OMAP35XX_IRQ_GPIO6_MPU,
		                 -1,
		              },
	},

	/**	
	 *	OMAP35xx - MMC/SDIO
	 *	There are a total of 3 MMC modules on OMAP3
	 */
	{	.name      = "omap_mmc",
		.unit      = 0,
		.mem       = { { OMAP35XX_MMCHS1_HWBASE,  OMAP35XX_MMCHS_SIZE },
		               { 0,  0 }
		             },
		.irqs      = {   OMAP35XX_IRQ_MMC1,
		                 -1,
		              },
	},

	/**	
	 *	OMAP35xx - USB EHCI
	 *	The OMAP EHCI driver expects three different register sets, one for
	 *	the actual EHCI registers and the other two control the interface.
	 */
	{	.name      = "ehci",
		.unit      = 0,
		.mem       = { { OMAP35XX_USB_EHCI_HWBASE,  OMAP35XX_USB_EHCI_SIZE },
		               { OMAP35XX_USB_UHH_HWBASE,   OMAP35XX_USB_UHH_SIZE },
		               { OMAP35XX_USB_TLL_HWBASE,   OMAP35XX_USB_TLL_SIZE },
		               { 0,  0 }
		             },
		.irqs      = {   OMAP35XX_IRQ_EHCI,
		                 -1,
		              },
	},

	{	0, 0, { { 0, 0 } }, { -1 } }
};



/**
 *	omap_sdram_size - called from machdep to get the total memory size
 * 
 *	Since this function is called very early in the boot, there is none of the
 *	bus handling code setup. However the boot device map will be setup, so
 *	we can directly access registers already mapped.
 *
 *	This is a bit ugly, but since we need this information early on and the
 *	only way to get it (appart from hardcoding it or via kernel args) is to read
 *	from the EMIF_SRAM registers.
 *
 *	RETURNS:
 *	The size of memory in bytes.
 */
unsigned int
omap_sdram_size(void)
{
	uint32_t size;
	uint32_t sdrc_mcfg_0, sdrc_mcfg_1;

	sdrc_mcfg_0 = *((volatile uint32_t *)(OMAP35XX_SDRC_MCFG(0)));
	sdrc_mcfg_1 = *((volatile uint32_t *)(OMAP35XX_SDRC_MCFG(1)));

	/* The size is given in bits 17:8 in 2MB chunk sizes */
	size  = ((sdrc_mcfg_0 >> 8) & 0x3FF) * (2 * 1024 * 1024);
	size += ((sdrc_mcfg_1 >> 8) & 0x3FF) * (2 * 1024 * 1024);
	
printf("[BRG] omap_sdram_size : size = %u\n", size);

	return (size);
}




/**
 *	omap35xx_add_child - add a child item to the root omap device
 *	@dev: the parent device
 *	@order: defines roughly the order with which to add the child to the parent
 *	@name: the name to give to the child item
 *	@unit: the unit number for the device
 *	@addr: the base address of the register set for device
 *	@size: the number of a bytes in register set
 *	@irq: the IRQ number(s) for the device
 * 
 *	Adds a child to the omap base device.
 *
 */
static void
omap35xx_add_child(device_t dev, int prio, const char *name, int unit,
                   const struct omap_mem_range mem[], const int irqs[])
{
	device_t kid;
	struct omap_ivar *ivar;
	unsigned int i;
	
	/* Start by adding the actual child to the parent (us) */
	kid = device_add_child_ordered(dev, prio, name, unit);
	if (kid == NULL) {
	    printf("Can't add child %s%d ordered\n", name, unit);
	    return;
	}
	
	/* Allocate some memory for the omap_ivar structure */ 
	ivar = malloc(sizeof(*ivar), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ivar == NULL) {
		device_delete_child(dev, kid);
		printf("Can't add alloc ivar\n");
		return;
	}
	
	/* Assign the ivars to the child item and populate with the device resources */
	device_set_ivars(kid, ivar);
	
	/* Assign the IRQ(s) in the resource list */
	resource_list_init(&ivar->resources);
	if (irqs) {
		for (i = 0; *irqs != -1; i++, irqs++) {
			bus_set_resource(kid, SYS_RES_IRQ, i, *irqs, 1);
		}
	}
	
	/* Assign the memory region to the resource list */
	if (mem) {
		for (i = 0; mem->base != 0; i++, mem++) {
			bus_set_resource(kid, SYS_RES_MEMORY, i, mem->base, mem->size);
		}
	}
}


/**
 *	omap35xx_cpu_add_builtin_children - adds the SoC child components
 *	@dev: this device, the one we are adding to
 * 
 *	Adds child devices from the omap35xx_devs list.
 *
 */
static void
omap35xx_cpu_add_builtin_children(device_t dev)
{
	int i;
	const struct omap_cpu_dev *walker;
	
	/* Setup all the clock devices - this is not the tick timers, rather it's
	 * the individual functional and interface clocks for the SoC modules.
	 */
	omap3_clk_init(dev, 1);
	
	/* Setup the system control module driver, which basically is just the
	 * padconf (pinmux) for the OMAP35xx devices.
	 */
	omap3_padconf_init(dev, 1);

	/* Add the rest of the children from the array above */
	for (i = 5, walker = omap35xx_devs; walker->name; i++, walker++) {
		omap35xx_add_child(dev, i, walker->name, walker->unit,
		                   walker->mem, walker->irqs);
	}
}


/**
 *	omap35xx_identify - adds the SoC child components
 *	@dev: this device, the one we are adding to
 * 
 *	Adds a child to the omap3 base device.
 *
 */
static void
omap35xx_identify(driver_t *drv, device_t parent)
{
	/* Add the resources for this "omap35xx" device */
	omap35xx_add_child(parent, 0, "omap35xx", 0, NULL, NULL);
	
	/* Add the other SOC components */
	omap35xx_cpu_add_builtin_children(parent);
}

/**
 *	omap35xx_probe - called when the device is probed
 *	@dev: the new device
 * 
 *	All we do in this routine is set the description of this device
 *
 */
static int
omap35xx_probe(device_t dev)
{
	device_set_desc(dev, "TI OMAP35XX");
	return (0);
}

/**
 *	omap35xx_attach - called when the device is attached
 *	@dev: the new device
 * 
 *	All we do in this routine is set the description of this device
 *
 */
static int
omap35xx_attach(device_t dev)
{
	struct omap_softc *omapsc = device_get_softc(device_get_parent(dev));
	struct omap3_softc *sc = device_get_softc(dev);

	sc->sc_iotag = omapsc->sc_iotag;
	sc->sc_dev = dev;


	/* Map in the interrupt controller register set */
	if (bus_space_map(sc->sc_iotag, OMAP35XX_INTCPS_HWBASE,
	                  OMAP35XX_INTCPS_SIZE, 0, &sc->sc_intcps_ioh)) {
		panic("%s: Cannot map registers", device_get_name(dev));
	}


	/* Save the device structure globally for the IRQ handling */
	g_omap3_softc = sc;

	/* TODO: Revisit - Install an interrupt post filter */
	arm_post_filter = omap3_post_filter_intr;
	
	/* Setup the OMAP3 interrupt controller */
	omap3_setup_intr_controller(g_omap3_softc, omap35xx_irq_prio);

	return (0);
}



static device_method_t omap35xx_methods[] = {
	DEVMETHOD(device_probe, omap35xx_probe),
	DEVMETHOD(device_attach, omap35xx_attach),
	DEVMETHOD(device_identify, omap35xx_identify),
	{0, 0},
};

static driver_t omap35xx_driver = {
	"omap35xx",
	omap35xx_methods,
	sizeof(struct omap3_softc),
};

static devclass_t omap35xx_devclass;

DRIVER_MODULE(omap35xx, omap, omap35xx_driver, omap35xx_devclass, 0, 0);
