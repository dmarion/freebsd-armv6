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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <arm/omap/omapvar.h>
#include <arm/omap/omap_prcm.h>
#include <arm/omap/omap3/omap35xx_reg.h>



/*
 *	This file defines the clock configuration for the OMAP3xxx series of
 *	devices.
 *
 *	How This is Suppose to Work
 *	===========================
 *	- There is a top level omap_prcm module that defines all OMAP SoC drivers
 *	should use to enable/disable the system clocks regardless of the version
 *	of OMAP device they are running on.  This top level PRCM module is just
 *	a thin shim to chip specific functions that perform the donkey work of
 *	configuring the clock - this file is the 'donkey' for OMAP35xx devices.
 *
 *	- The key bit in this file is the omap_clk_devmap array, it's
 *	used by the omap_prcm driver to determine what clocks are valid and which
 *	functions to call to manipulate them.
 *
 *	- In essence you just need to define some callbacks for each of the
 *	clocks and then you're done.
 *
 *	- The other thing worth noting is that when the omap_prcm device
 *	is registered you typically pass in some memory ranges which are the
 *	SYS_MEMORY resources.  These resources are in turn allocated using 
 *	bus_allocate_resources(...) and the resource handles are passed to all
 *	individual clock callback handlers. 
 *
 *
 *
 *
 */


void
omap3_clk_init(device_t dev, int prio);

static int
omap3_clk_generic_activate(const struct omap_clock_dev *clkdev,
                           struct resource* mem_res[]);

static int
omap3_clk_generic_deactivate(const struct omap_clock_dev *clkdev,
                             struct resource* mem_res[]);

static int
omap3_clk_generic_accessible(const struct omap_clock_dev *clkdev,
                             struct resource* mem_res[]);

static int
omap3_clk_generic_set_source(const struct omap_clock_dev *clkdev, clk_src_t clksrc,
                             struct resource* mem_res[]);

static int
omap3_clk_generic_get_source_freq(const struct omap_clock_dev *clkdev,
                                  unsigned int *freq,
                                  struct resource* mem_res[]);


static int
omap3_clk_gptimer_get_source_freq(const struct omap_clock_dev *clkdev,
                                  unsigned int *freq,
                                  struct resource* mem_res[]);
static int
omap3_clk_gptimer_set_source(const struct omap_clock_dev *clkdev,
                             clk_src_t clksrc, struct resource* mem_res[]);



static int
omap3_clk_alwayson_null_func(const struct omap_clock_dev *clkdev,
                            struct resource* mem_res[]);



static int
omap3_clk_get_sysclk_freq(const struct omap_clock_dev *clkdev,
                          unsigned int *freq, struct resource* mem_res[]);

static int
omap3_clk_get_arm_fclk_freq(const struct omap_clock_dev *clkdev,
                            unsigned int *freq, struct resource* mem_res[]);



static int
omap3_clk_hsusbhost_activate(const struct omap_clock_dev *clkdev,
                             struct resource* mem_res[]);

static int
omap3_clk_hsusbhost_deactivate(const struct omap_clock_dev *clkdev,
                               struct resource* mem_res[]);




#define FREQ_96MHZ    96000000
#define FREQ_64MHZ    64000000
#define FREQ_48MHZ    48000000
#define FREQ_32KHZ    32000



/**
 *	Only one memory regions is needed for OMAP35xx clock control (unlike OMAP4)
 *	
 *	   CM Instance  -  0x4800 4000 : 0x4800 5500
 *	   PRM Instance -  0x4830 6000 : 0x4830 8000
 *
 */
#define CM_INSTANCE_MEM_REGION    0
#define PRM_INSTANCE_MEM_REGION   1

#define IVA2_CM_OFFSET            0x0000
#define OCP_SYSTEM_CM_OFFSET      0x0800
#define MPU_CM_OFFSET             0x0900
#define CORE_CM_OFFSET            0x0A00
#define SGX_CM_OFFSET             0x0B00
#define WKUP_CM_OFFSET            0x0C00
#define CLOCK_CTRL_CM_OFFSET      0x0D00
#define DSS_CM_OFFSET             0x0E00
#define CAM_CM_OFFSET             0x0F00
#define PER_CM_OFFSET             0x1000
#define EMU_CM_OFFSET             0x1100
#define GLOBAL_CM_OFFSET          0x1200
#define NEON_CM_OFFSET            0x1300
#define USBHOST_CM_OFFSET         0x1400

#define IVA2_PRM_OFFSET           0x0000
#define OCP_SYSTEM_PRM_OFFSET     0x0800
#define MPU_PRM_OFFSET            0x0900
#define CORE_PRM_OFFSET           0x0A00
#define SGX_PRM_OFFSET            0x0B00
#define WKUP_PRM_OFFSET           0x0C00
#define CLOCK_CTRL_PRM_OFFSET     0x0D00
#define DSS_PRM_OFFSET            0x0E00
#define CAM_PRM_OFFSET            0x0F00
#define PER_PRM_OFFSET            0x1000
#define EMU_PRM_OFFSET            0x1100
#define GLOBAL_PRM_OFFSET         0x1200
#define NEON_PRM_OFFSET           0x1300
#define USBHOST_PRM_OFFSET        0x1400






/**
 *	omap_clk_devmap - Array of clock devices available on OMAP3xxx devices
 *
 *	This map only defines which clocks are valid and the callback functions
 *	for clock activate, deactivate, etc.  It is used by the top level omap_prcm
 *	driver.
 *
 *	The actual details of the clocks (config registers, bit fields, sources,
 *	etc) are in the private g_omap3_clk_details array below.
 *
 */

#define OMAP3_GENERIC_CLOCK_DEV(i) \
	{	.id = (i), \
		.clk_activate = omap3_clk_generic_activate, \
		.clk_deactivate = omap3_clk_generic_deactivate, \
		.clk_set_source = omap3_clk_generic_set_source, \
		.clk_accessible = omap3_clk_generic_accessible, \
		.clk_get_source_freq = omap3_clk_generic_get_source_freq \
	}

#define OMAP3_GPTIMER_CLOCK_DEV(i) \
	{	.id = (i), \
		.clk_activate = omap3_clk_generic_activate, \
		.clk_deactivate = omap3_clk_generic_deactivate, \
		.clk_set_source = omap3_clk_gptimer_set_source, \
		.clk_accessible = omap3_clk_generic_accessible, \
		.clk_get_source_freq = omap3_clk_gptimer_get_source_freq \
	}

#define OMAP3_ALWAYSON_CLOCK_DEV(i) \
	{	.id = (i), \
		.clk_activate = omap3_clk_alwayson_null_func, \
		.clk_deactivate = omap3_clk_alwayson_null_func, \
		.clk_set_source = NULL, \
		.clk_accessible = omap3_clk_alwayson_null_func, \
		.clk_get_source_freq = NULL \
	}

#define OMAP3_HSUSBHOST_CLOCK_DEV(i) \
	{	.id = (i), \
		.clk_activate = omap3_clk_hsusbhost_activate, \
		.clk_deactivate = omap3_clk_hsusbhost_deactivate, \
		.clk_set_source = NULL, \
		.clk_accessible = omap3_clk_generic_accessible, \
		.clk_get_source_freq = NULL \
	}


const struct omap_clock_dev omap_clk_devmap[] = {

	/* System clock */
	{	.id                  = SYS_CLK,
		.clk_activate        = NULL,
		.clk_deactivate      = NULL,
		.clk_set_source      = NULL,
		.clk_accessible      = NULL,
		.clk_get_source_freq = omap3_clk_get_sysclk_freq,
	},
	/* MPU (ARM) core clocks */
	{	.id                  = MPU_CLK,
		.clk_activate        = NULL,
		.clk_deactivate      = NULL,
		.clk_set_source      = NULL,
		.clk_accessible      = NULL,
		.clk_get_source_freq = omap3_clk_get_arm_fclk_freq,
	},


	/* UART device clocks */
	OMAP3_GENERIC_CLOCK_DEV(UART1_CLK),
	OMAP3_GENERIC_CLOCK_DEV(UART2_CLK),
	OMAP3_GENERIC_CLOCK_DEV(UART3_CLK),
	OMAP3_GENERIC_CLOCK_DEV(UART4_CLK),

	/* Timer device source clocks */
	OMAP3_GPTIMER_CLOCK_DEV(GPTIMER1_CLK),
	OMAP3_GPTIMER_CLOCK_DEV(GPTIMER2_CLK),
	OMAP3_GPTIMER_CLOCK_DEV(GPTIMER3_CLK),
	OMAP3_GPTIMER_CLOCK_DEV(GPTIMER4_CLK),
	OMAP3_GPTIMER_CLOCK_DEV(GPTIMER5_CLK),
	OMAP3_GPTIMER_CLOCK_DEV(GPTIMER6_CLK),
	OMAP3_GPTIMER_CLOCK_DEV(GPTIMER7_CLK),
	OMAP3_GPTIMER_CLOCK_DEV(GPTIMER8_CLK),
	OMAP3_GPTIMER_CLOCK_DEV(GPTIMER9_CLK),
	OMAP3_GPTIMER_CLOCK_DEV(GPTIMER10_CLK),
	OMAP3_GPTIMER_CLOCK_DEV(GPTIMER11_CLK),
	
	/* MMC device clocks (MMC1 and MMC2 can have different input clocks) */
	OMAP3_GENERIC_CLOCK_DEV(MMC1_CLK),
	OMAP3_GENERIC_CLOCK_DEV(MMC2_CLK),
	OMAP3_GENERIC_CLOCK_DEV(MMC3_CLK),

	/* USB HS (high speed TLL, EHCI and OHCI) */
	OMAP3_GENERIC_CLOCK_DEV(USBTLL_CLK),
	OMAP3_HSUSBHOST_CLOCK_DEV(USBHSHOST_CLK),

	/* GPIO */
	OMAP3_GENERIC_CLOCK_DEV(GPIO1_CLK),
	OMAP3_GENERIC_CLOCK_DEV(GPIO2_CLK),
	OMAP3_GENERIC_CLOCK_DEV(GPIO3_CLK),
	OMAP3_GENERIC_CLOCK_DEV(GPIO4_CLK),
	OMAP3_GENERIC_CLOCK_DEV(GPIO5_CLK),
	OMAP3_GENERIC_CLOCK_DEV(GPIO6_CLK),
	
	/* I2C */
	OMAP3_GENERIC_CLOCK_DEV(I2C1_CLK),
	OMAP3_GENERIC_CLOCK_DEV(I2C2_CLK),
	OMAP3_GENERIC_CLOCK_DEV(I2C3_CLK),

	/* sDMA */
	OMAP3_ALWAYSON_CLOCK_DEV(SDMA_CLK),	

	{  INVALID_CLK_IDENT, NULL, NULL, NULL, NULL }
};






/**
 *	g_omap3_clk_details - Stores details for all the different clocks supported
 *
 *	Whenever an operation on a clock is being performed (activated, deactivated,
 *	etc) this array is looked up to find the correct register and bit(s) we
 *	should be modifying.
 *
 */

struct omap3_clk_details {
	clk_ident_t id;
	int32_t     src_freq;
	
	/* The register offset from the CM module register region of the registers*/
	uint32_t    fclken_offset;
	uint32_t    iclken_offset;
	uint32_t    idlest_offset;
	
	/* The bit offset for the clock */
	uint32_t    bit_offset;
};

#define OMAP3_GENERIC_CLOCK_DETAILS(d, freq, base, fclk, iclk, idlest, bit) \
	{	.id = (d), \
		.src_freq = (freq), \
		.fclken_offset = ((base) + (fclk)), \
		.iclken_offset = ((base) + (iclk)), \
		.idlest_offset = ((base) + (idlest)), \
		.bit_offset = (bit), \
	}

static const struct omap3_clk_details g_omap3_clk_details[] = {

	/* UART */
	OMAP3_GENERIC_CLOCK_DETAILS(UART1_CLK, FREQ_48MHZ, CORE_CM_OFFSET,
		0x00, 0x10, 0x20, 13),
	OMAP3_GENERIC_CLOCK_DETAILS(UART2_CLK, FREQ_48MHZ, CORE_CM_OFFSET,
		0x00, 0x10, 0x20, 14),
	OMAP3_GENERIC_CLOCK_DETAILS(UART3_CLK, FREQ_48MHZ, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 11),

	/* General purpose timers */
	OMAP3_GENERIC_CLOCK_DETAILS(GPTIMER1_CLK,  -1, WKUP_CM_OFFSET,
		0x00, 0x10, 0x20, 3),
	OMAP3_GENERIC_CLOCK_DETAILS(GPTIMER2_CLK,  -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 3),
	OMAP3_GENERIC_CLOCK_DETAILS(GPTIMER3_CLK,  -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 4),
	OMAP3_GENERIC_CLOCK_DETAILS(GPTIMER4_CLK,  -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 5),
	OMAP3_GENERIC_CLOCK_DETAILS(GPTIMER5_CLK,  -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 6),
	OMAP3_GENERIC_CLOCK_DETAILS(GPTIMER6_CLK,  -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 7),
	OMAP3_GENERIC_CLOCK_DETAILS(GPTIMER7_CLK,  -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 8),
	OMAP3_GENERIC_CLOCK_DETAILS(GPTIMER8_CLK,  -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 9),
	OMAP3_GENERIC_CLOCK_DETAILS(GPTIMER9_CLK,  -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 10),
	OMAP3_GENERIC_CLOCK_DETAILS(GPTIMER10_CLK, -1, CORE_CM_OFFSET,
		0x00, 0x10, 0x20, 11),
	OMAP3_GENERIC_CLOCK_DETAILS(GPTIMER11_CLK, -1, CORE_CM_OFFSET,
		0x00, 0x10, 0x20, 12),

	/* HSMMC (MMC1 and MMC2 can have different input clocks) */
	OMAP3_GENERIC_CLOCK_DETAILS(MMC1_CLK, FREQ_96MHZ, CORE_CM_OFFSET,
		0x00, 0x10, 0x20, 24),
	OMAP3_GENERIC_CLOCK_DETAILS(MMC2_CLK, FREQ_96MHZ, CORE_CM_OFFSET,
		0x00, 0x10, 0x20, 25),
	OMAP3_GENERIC_CLOCK_DETAILS(MMC3_CLK, FREQ_96MHZ, CORE_CM_OFFSET,
		0x00, 0x10, 0x20, 30),
	
	/* USB HS (high speed TLL, EHCI and OHCI) */
	OMAP3_GENERIC_CLOCK_DETAILS(USBTLL_CLK, -1, CORE_CM_OFFSET,
		0x08, 0x18, 0x28, 2),
	OMAP3_GENERIC_CLOCK_DETAILS(USBHSHOST_CLK, -1, USBHOST_CM_OFFSET,
		0x00, 0x10, 0x20, 1),

	/* GPIO modules */
	OMAP3_GENERIC_CLOCK_DETAILS(GPIO1_CLK, -1, WKUP_CM_OFFSET,
		0x00, 0x10, 0x20, 3),
	OMAP3_GENERIC_CLOCK_DETAILS(GPIO2_CLK, -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 13),
	OMAP3_GENERIC_CLOCK_DETAILS(GPIO3_CLK, -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 14),
	OMAP3_GENERIC_CLOCK_DETAILS(GPIO4_CLK, -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 15),
	OMAP3_GENERIC_CLOCK_DETAILS(GPIO5_CLK, -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 16),
	OMAP3_GENERIC_CLOCK_DETAILS(GPIO6_CLK, -1, PER_CM_OFFSET,
		0x00, 0x10, 0x20, 17),
		
	/* I2C modules */
	OMAP3_GENERIC_CLOCK_DETAILS(I2C1_CLK, -1, CORE_CM_OFFSET,
		0x00, 0x10, 0x20, 15),
	OMAP3_GENERIC_CLOCK_DETAILS(I2C2_CLK, -1, CORE_CM_OFFSET,
		0x00, 0x10, 0x20, 16),
	OMAP3_GENERIC_CLOCK_DETAILS(I2C3_CLK, -1, CORE_CM_OFFSET,
		0x00, 0x10, 0x20, 17),


	{ INVALID_CLK_IDENT, 0, 0, 0, 0 },
};






/**
 *	MAX_MODULE_ENABLE_WAIT - the number of loops to wait for the module to come
 *	alive.
 *
 */
#define MAX_MODULE_ENABLE_WAIT    1000

	
/**
 *	ARRAY_SIZE - Macro to return the number of elements in a static const array.
 *
 */
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))




/**
 *	omap3_clk_wait_on_reg - loops for MAX_MODULE_ENABLE_WAIT times or until
 *	                        register bit(s) change.
 *	@mem_res: memory resource of the register to read
 *	@off: offset of the register within mem_res
 *	@mask: the mask to bitwise AND with the register
 *	@cmp: if this value matches the register value after the mask is applied
 *	      the function returns with 0.
 *
 *
 *	RETURNS:
 *	Returns 0 on success or ETIMEDOUT on failure.
 */
static int
omap3_clk_wait_on_reg(struct resource* mem_res, bus_size_t off, uint32_t mask,
                      uint32_t cmp)
{
	unsigned int i;
	for (i = 0; i < MAX_MODULE_ENABLE_WAIT; i++) {
		if ((bus_read_4(mem_res, off) & mask) == cmp)
			return (0);
	}

	return (ETIMEDOUT);
}





/**
 *	omap3_clk_details - returns a pointer to the generic clock details
 *	@id: The ID of the clock to get the details for
 *
 *	This function iterates over the g_omap3_clk_details array and returns a
 *	pointer to the entry that matches the supplied ID, or NULL if no entry
 *	is found.
 *
 *	RETURNS:
 *	Pointer to clock details or NULL on failure
 */
static const struct omap3_clk_details*
omap3_clk_details(clk_ident_t id)
{
	const struct omap3_clk_details *walker;

	for (walker = g_omap3_clk_details; walker->id != INVALID_CLK_IDENT; walker++) {
		if (id == walker->id)
			return (walker);
	}

	return NULL;
}




/**
 *	omap3_clk_alwayson_null_func - dummy function for always on clocks
 *	@clkdev: pointer to the clock device structure (ignored)
 *	@mem_res: array of memory resources mapped when PRCM driver attached (ignored)
 *	
 *	
 *
 *	LOCKING:
 *	Inherits the locks from the omap_prcm driver, no internal locking.
 *
 *	RETURNS:
 *	Returns 0 on success or a positive error code on failure.
 */
static int
omap3_clk_alwayson_null_func(const struct omap_clock_dev *clkdev,
                             struct resource* mem_res[])
{
	return (0);
}





/**
 *	omap3_clk_get_sysclk_freq - gets the sysclk frequency
 *	@sc: pointer to the clk module/device context
 *
 *	Read the clocking information from the power-control/boot-strap registers,
 *  and stored in two global variables.
 *
 *	RETURNS:
 *	nothing, values are saved in global variables
 */
static int
omap3_clk_get_sysclk_freq(const struct omap_clock_dev *clkdev,
                          unsigned int *freq, struct resource* mem_res[])
{
	uint32_t clksel;
	uint32_t clknsel;
	unsigned int oscclk;
	unsigned int sysclk;
	
	/* Read the input clock freq from the configuration register */
	clknsel = bus_read_4(mem_res[PRM_INSTANCE_MEM_REGION], CLOCK_CTRL_PRM_OFFSET + 0x40);
	switch (clknsel & 0x7) {
		case 0x0:
			/* 12Mhz */
			oscclk = 12000000;
			break;
		case 0x1:
			/* 13Mhz */
			oscclk = 13000000;
			break;
		case 0x2:
			/* 19.2Mhz */
			oscclk = 19200000;
			break;
		case 0x3:
			/* 26Mhz */
			oscclk = 26000000;
			break;
		case 0x4:
			/* 38.4Mhz */
			oscclk = 38400000;
			break;
		case 0x5:
			/* 16.8Mhz */
			oscclk = 16800000;
			break;
		default:
			panic("%s: Invalid clock freq", __func__);
	}

	/* Read the value of the clock divider used for the system clock */
	clksel = bus_read_4(mem_res[PRM_INSTANCE_MEM_REGION], GLOBAL_PRM_OFFSET + 0x70);
	switch (clksel & 0xC0) {
		case 0x40:
			sysclk = oscclk;
			break;
		case 0x80:
			sysclk = oscclk / 2;
			break;
		default:
			panic("%s: Invalid system clock divider", __func__);
	}

	/* Return the value */
	if (freq)
		*freq = sysclk;

	return (0);
}



/**
 *	omap3_clk_get_arm_fclk_freq - gets the MPU clock frequency
 *	@clkdev: ignored
 *	@freq: pointer which upon return will contain the freq in hz
 *	@mem_res: array of allocated memory resources
 *
 *	Reads the frequency setting information registers and returns the value
 *	in the freq variable.
 *
 *	RETURNS:
 *	returns 0 on success, a positive error code on failure.
 */
static int
omap3_clk_get_arm_fclk_freq(const struct omap_clock_dev *clkdev,
                            unsigned int *freq, struct resource* mem_res[])
{
	unsigned int sysclk;
	unsigned int coreclk;
	unsigned int mpuclk;
	uint32_t clksel;
	uint32_t clkout;


	/* Get the SYSCLK freq */
	omap3_clk_get_sysclk_freq(clkdev, &sysclk, mem_res);
	
	
	/* First get the freq of the CORE_CLK (feed from DPLL3) */
	clksel = bus_read_4(mem_res[CM_INSTANCE_MEM_REGION], CLOCK_CTRL_CM_OFFSET + 0x40);
	clkout = (sysclk * ((clksel >> 16) & 0x7FF)) / (((clksel >> 8) & 0x7F) + 1);
	coreclk = clkout / (clksel >> 27);


	/* Next get the freq for the MPU_CLK */
	clksel = bus_read_4(mem_res[CM_INSTANCE_MEM_REGION], MPU_CM_OFFSET + 0x40);
	mpuclk = (coreclk * ((clksel >> 8) & 0x7FF)) / ((clksel & 0x7F) + 1);


	/* Return the value */
	if (freq)
		*freq = mpuclk;

	return (0);
}







	
/**
 *	omap3_clk_generic_activate - activates a modules iinterface and func clock
 *	@clkdev: pointer to the clock device structure.
 *	@mem_res: array of memory resources mapped when PRCM driver attached
 *	
 *	
 *
 *	LOCKING:
 *	Inherits the locks from the omap_prcm driver, no internal locking.
 *
 *	RETURNS:
 *	Returns 0 on success or a positive error code on failure.
 */
static int
omap3_clk_generic_activate(const struct omap_clock_dev *clkdev,
                           struct resource* mem_res[])
{
	const struct omap3_clk_details* clk_details = omap3_clk_details(clkdev->id);
	struct resource* clk_mem_res = mem_res[CM_INSTANCE_MEM_REGION];
	uint32_t fclken, iclken;

	if (clk_details == NULL)
		return (ENXIO);
	if (clk_mem_res == NULL)
		return (ENOMEM);
	
	
	/* All the 'generic' clocks have a FCLKEN, ICLKEN and IDLEST register which
	 * is for the functional, interface and clock status regsters respectively.
	 */
	 
	/* Enable the interface clock */
	iclken = bus_read_4(clk_mem_res, clk_details->iclken_offset);
	iclken |= (1UL << clk_details->bit_offset);
	bus_write_4(clk_mem_res, clk_details->iclken_offset, iclken);
	
	/* Read back the value to ensure the write has taken place ... needed ? */
	iclken = bus_read_4(clk_mem_res, clk_details->iclken_offset);


	/* Enable the functional clock */
	fclken = bus_read_4(clk_mem_res, clk_details->fclken_offset);
	fclken |= (1UL << clk_details->bit_offset);
	bus_write_4(clk_mem_res, clk_details->fclken_offset, fclken);

	/* Read back the value to ensure the write has taken place ... needed ? */
	fclken = bus_read_4(clk_mem_res, clk_details->fclken_offset);


	/* Now poll on the IDLEST register to tell us if the module has come up.
	 * TODO: We need to take into account the parent clocks.
	 */
	
	/* Try MAX_MODULE_ENABLE_WAIT number of times to check if enabled */
	if (omap3_clk_wait_on_reg(clk_mem_res, clk_details->idlest_offset,
	                          (1UL << clk_details->bit_offset), 0) != 0) {
		printf("Error: failed to enable module with clock %d\n", clkdev->id);
		return (ETIMEDOUT);
	}
	
	return (0);
}



/**
 *	omap3_clk_generic_deactivate - deactivates a modules clock
 *	@clkdev: pointer to the clock device structure.
 *	@mem_res: array of memory resources mapped when PRCM driver attached
 *	
 *	
 *
 *	LOCKING:
 *	Inherits the locks from the omap_prcm driver, no internal locking.
 *
 *	RETURNS:
 *	Returns 0 on success or a positive error code on failure.
 */
static int
omap3_clk_generic_deactivate(const struct omap_clock_dev *clkdev,
                             struct resource* mem_res[])
{
	const struct omap3_clk_details* clk_details = omap3_clk_details(clkdev->id);
	struct resource* clk_mem_res = mem_res[CM_INSTANCE_MEM_REGION];
	uint32_t fclken, iclken;

	if (clk_details == NULL)
		return (ENXIO);
	if (clk_mem_res == NULL)
		return (ENOMEM);
	
	
	/* All the 'generic' clocks have a FCLKEN, ICLKEN and IDLEST register which
	 * is for the functional, interface and clock status regsters respectively.
	 */
	 
	/* Disable the interface clock */
	iclken = bus_read_4(clk_mem_res, clk_details->iclken_offset);
	iclken &= ~(1UL << clk_details->bit_offset);
	bus_write_4(clk_mem_res, clk_details->iclken_offset, iclken);

	/* Disable the functional clock */
	fclken = bus_read_4(clk_mem_res, clk_details->fclken_offset);
	fclken &= ~(1UL << clk_details->bit_offset);
	bus_write_4(clk_mem_res, clk_details->fclken_offset, fclken);

	
	return (0);
}


/**
 *	omap3_clk_generic_set_source - checks if a module is accessible
 *	@clkdev: pointer to the clock device structure.
 *	
 *	
 *
 *	LOCKING:
 *	Inherits the locks from the omap_prcm driver, no internal locking.
 *
 *	RETURNS:
 *	Returns 0 on success or a positive error code on failure.
 */
static int
omap3_clk_generic_set_source(const struct omap_clock_dev *clkdev,
                             clk_src_t clksrc, struct resource* mem_res[])
{


	return (0);
}

/**
 *	omap3_clk_generic_accessible - checks if a module is accessible
 *	@clkdev: pointer to the clock device structure.
 *	
 *	
 *
 *	LOCKING:
 *	Inherits the locks from the omap_prcm driver, no internal locking.
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code on failure.
 */
static int
omap3_clk_generic_accessible(const struct omap_clock_dev *clkdev,
                             struct resource* mem_res[])
{
	const struct omap3_clk_details* clk_details = omap3_clk_details(clkdev->id);
	struct resource* clk_mem_res = mem_res[CM_INSTANCE_MEM_REGION];
	uint32_t idlest;

	if (clk_details == NULL)
		return (ENXIO);
	if (clk_mem_res == NULL)
		return (ENOMEM);
	
	idlest = bus_read_4(clk_mem_res, clk_details->idlest_offset);
		
	/* Check the enabled state */
	if ((idlest & (1UL << clk_details->bit_offset)) == 0)
		return (0);
	
	return (1);
}


/**
 *	omap3_clk_generic_get_source_freq - checks if a module is accessible
 *	@clkdev: pointer to the clock device structure.
 *	
 *	
 *
 *	LOCKING:
 *	Inherits the locks from the omap_prcm driver, no internal locking.
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code on failure.
 */
static int
omap3_clk_generic_get_source_freq(const struct omap_clock_dev *clkdev,
                                  unsigned int *freq,
                                  struct resource* mem_res[])
{
	const struct omap3_clk_details* clk_details = omap3_clk_details(clkdev->id);

	if (clk_details == NULL)
		return (ENXIO);
	
	/* Simply return the stored frequency */
	if (freq)
		*freq = (unsigned int)clk_details->src_freq;
	
	return (0);
}



/**
 *	omap3_clk_gptimer_set_source - sets the source clock for one of the GP timers
 *	@clkdev: pointer to the clock device structure.
 *	@clksrc: enum describing the clock source.
 *	@mem_res: an array of memory handles for the device.
 *	
 *
 *	LOCKING:
 *	Inherits the locks from the omap_prcm driver, no internal locking.
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code on failure.
 */
static int
omap3_clk_gptimer_set_source(const struct omap_clock_dev *clkdev,
                             clk_src_t clksrc, struct resource* mem_res[])
{
	const struct omap3_clk_details* clk_details = omap3_clk_details(clkdev->id);
	struct resource* clk_mem_res = mem_res[CM_INSTANCE_MEM_REGION];
	uint32_t bit, regoff;
	uint32_t clksel;

	if (clk_details == NULL)
		return (ENXIO);
	if (clk_mem_res == NULL)
		return (ENOMEM);
	
	/* Set the source freq by writing the clksel register */
	switch (clkdev->id) {
		case GPTIMER1_CLK:
			bit = 0;
			regoff = WKUP_CM_OFFSET + 0x40;
			break;
		case GPTIMER2_CLK ... GPTIMER9_CLK:
			bit = (clkdev->id - GPTIMER2_CLK);
			regoff = PER_CM_OFFSET + 0x40;
			break;
		case GPTIMER10_CLK ... GPTIMER11_CLK:
			bit = 6 + (clkdev->id - GPTIMER10_CLK);
			regoff = CORE_CM_OFFSET + 0x40;
			break;
		default:
			return (EINVAL);
	}
	
	/* Set the CLKSEL bit if then the SYS_CLK is the source */
	clksel = bus_read_4(clk_mem_res, regoff);

	if (clksrc == SYSCLK_CLK)
		clksel |= (0x1UL << bit);
	else
		clksel &= ~(0x1UL << bit);

	bus_write_4(clk_mem_res, regoff, clksel);

	/* Read back the value to ensure the write has taken place ... needed ? */
	clksrc = bus_read_4(clk_mem_res, regoff);

	return (0);
}

/**
 *	omap3_clk_gptimer_get_source_freq - gets the source freq of the clock
 *	@clkdev: pointer to the clock device structure.
 *	@clksrc: enum describing the clock source.
 *	@mem_res: an array of memory handles for the device.
 *	
 *	
 *
 *	LOCKING:
 *	Inherits the locks from the omap_prcm driver, no internal locking.
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code on failure.
 */
static int
omap3_clk_gptimer_get_source_freq(const struct omap_clock_dev *clkdev,
                                  unsigned int *freq,
                                  struct resource* mem_res[])
{
	const struct omap3_clk_details* clk_details = omap3_clk_details(clkdev->id);
	struct resource* clk_mem_res = mem_res[CM_INSTANCE_MEM_REGION];
	uint32_t bit, regoff;
	uint32_t clksel;
	unsigned int src_freq;

	if (clk_details == NULL)
		return (ENXIO);
	if (clk_mem_res == NULL)
		return (ENOMEM);
	
	/* Determine the source freq by reading the clksel register */
	switch (clkdev->id) {
		case GPTIMER1_CLK:
			bit = 0;
			regoff = WKUP_CM_OFFSET + 0x40;
			break;
		case GPTIMER2_CLK ... GPTIMER9_CLK:
			bit = (clkdev->id - GPTIMER2_CLK);
			regoff = PER_CM_OFFSET + 0x40;
			break;
		case GPTIMER10_CLK ... GPTIMER11_CLK:
			bit = 6 + (clkdev->id - GPTIMER10_CLK);
			regoff = CORE_CM_OFFSET + 0x40;
			break;
		default:
			return (EINVAL);
	}


	/* If the CLKSEL bit is set then the SYS_CLK is the source */
	clksel = bus_read_4(clk_mem_res, regoff);
	if (clksel & (0x1UL << bit))
		omap3_clk_get_sysclk_freq(NULL, &src_freq, mem_res);
	else
		src_freq = FREQ_32KHZ;

	/* Return the frequency */
	if (freq)
		*freq = src_freq;
	
	return (0);
}




/**
 *	omap3_clk_setup_dpll5 - setup DPLL5 which is needed for the 120M_FCLK
 *	@dev: prcm device handle
 *
 *	Sets up the DPLL5 at the frequency specified by the mul and div arguments
 *	(the source clock is SYS_CLK).
 *
 *	LOCKING:
 *	None
 *
 *	RETURNS:
 *	Returns 0 on success otherwise ETIMEDOUT if DPLL failed to lock.
 */
static int
omap3_clk_setup_dpll5(struct resource* cm_mem_res, uint32_t mul, uint32_t div)
{
	uint32_t val;
	
	/* DPPL5 uses DPLL5_ALWON_FCLK as it's reference clock, this is just SYS_CLK
	 * which on the beagleboard is 13MHz.
	 */
	
	/* Set the multipler and divider values for the PLL.  We want 120Mhz so take
	 * the system clock (13Mhz) divide by that then multiple by 120.
	 */
	val = ((mul & 0x7ff) << 8) | ((div - 1) & 0x7f);
	bus_write_4(cm_mem_res, CLOCK_CTRL_CM_OFFSET + 0x4C, val);
	
	/* This is the clock divider from the PLL into the 120Mhz clock supplied to
	 * the USB module. */
	val = 0x01;
	bus_write_4(cm_mem_res, CLOCK_CTRL_CM_OFFSET + 0x50, val);

	/* PERIPH2_DPLL_FREQSEL = 0x7   (1.75 MHz—2.1 MHz)
	 * EN_PERIPH2_DPLL = 0x7        (Enables the DPLL5 in lock mode)
	 */
	val = (7 << 4) | (7 << 0);
	bus_write_4(cm_mem_res, CLOCK_CTRL_CM_OFFSET + 0x04, val);


	/* Disable auto-idle */
	bus_write_4(cm_mem_res, CLOCK_CTRL_CM_OFFSET + 0x34, 0x00);


	/* Wait until the DPLL5 is locked and there is clock activity */
	return (omap3_clk_wait_on_reg(cm_mem_res, (CLOCK_CTRL_CM_OFFSET + 0x24),
	                              0x01, 0x01));
}



/**
 *	omap3_clk_usbhost_activate - activates a modules iinterface and func clock
 *	@clkdev: pointer to the clock device structure.
 *	@mem_res: array of memory resources mapped when PRCM driver attached
 *	
 *	
 *
 *	LOCKING:
 *	Inherits the locks from the omap_prcm driver, no internal locking.
 *
 *	RETURNS:
 *	Returns 0 on success or a positive error code on failure.
 */
static int
omap3_clk_hsusbhost_activate(const struct omap_clock_dev *clkdev,
                             struct resource* mem_res[])
{
	const struct omap3_clk_details* clk_details = omap3_clk_details(clkdev->id);
	struct resource* clk_mem_res = mem_res[CM_INSTANCE_MEM_REGION];
	uint32_t fclken, iclken;
	unsigned int sysclk;

	if (clk_details == NULL)
		return (ENXIO);
	if (clk_mem_res == NULL)
		return (ENOMEM);
	if (clkdev->id != USBHSHOST_CLK)
		return (EINVAL);
	
	/* First ensure DPLL5 is setup and running, this provides the 120M clock */

	/* SYS_CLK feeds the DPLL so need that to calculate the mul & div */
	omap3_clk_get_sysclk_freq(NULL, &sysclk, mem_res);

	/* Activate DPLL5 and therefore the 120M clock */
	if (omap3_clk_setup_dpll5(clk_mem_res, 120, 13) != 0)
		return (ETIMEDOUT);

	
	/* All the 'generic' clocks have a FCLKEN, ICLKEN and IDLEST register which
	 * is for the functional, interface and clock status regsters respectively.
	 */
	 
	/* Enable the interface clock */
	iclken = bus_read_4(clk_mem_res, clk_details->iclken_offset);
	iclken |= 0x1;
	bus_write_4(clk_mem_res, clk_details->iclken_offset, iclken);

	/* Read back the value to ensure the write has taken place ... needed ? */
	iclken = bus_read_4(clk_mem_res, clk_details->iclken_offset);


	/* Enable the functional clock */
	fclken = bus_read_4(clk_mem_res, clk_details->fclken_offset);
	fclken |= 0x03;
	bus_write_4(clk_mem_res, clk_details->fclken_offset, fclken);

	/* Read back the value to ensure the write has taken place ... needed ? */
	fclken = bus_read_4(clk_mem_res, clk_details->fclken_offset);


	/* Now poll on the IDLEST register to tell us if the module has come up.
	 * TODO: We need to take into account the parent clocks.
	 */
	
	if (omap3_clk_wait_on_reg(clk_mem_res, clk_details->idlest_offset, 0x02,
	    0x00) != 0) {
		printf("Error: failed to enable module with USB clock %d\n", clkdev->id);
		return (ETIMEDOUT);
	}
	
	return (0);
}



/**
 *	omap3_clk_hsusbhost_deactivate - deactivates a modules clock
 *	@clkdev: pointer to the clock device structure.
 *	@mem_res: array of memory resources mapped when PRCM driver attached
 *	
 *	
 *
 *	LOCKING:
 *	Inherits the locks from the omap_prcm driver, no internal locking.
 *
 *	RETURNS:
 *	Returns 0 on success or a positive error code on failure.
 */
static int
omap3_clk_hsusbhost_deactivate(const struct omap_clock_dev *clkdev,
                               struct resource* mem_res[])
{
	const struct omap3_clk_details* clk_details = omap3_clk_details(clkdev->id);
	struct resource* clk_mem_res = mem_res[CM_INSTANCE_MEM_REGION];
	uint32_t fclken, iclken;

	if (clk_details == NULL)
		return (ENXIO);
	if (clk_mem_res == NULL)
		return (ENOMEM);
	
	
	/* All the 'generic' clocks have a FCLKEN, ICLKEN and IDLEST register which
	 * is for the functional, interface and clock status regsters respectively.
	 */
	 
	/* Disable the interface clock */
	iclken = bus_read_4(clk_mem_res, clk_details->iclken_offset);
	iclken &= ~(1UL << clk_details->bit_offset);
	bus_write_4(clk_mem_res, clk_details->iclken_offset, iclken);

	/* Disable the functional clock */
	fclken = bus_read_4(clk_mem_res, clk_details->fclken_offset);
	fclken &= ~(1UL << clk_details->bit_offset);
	bus_write_4(clk_mem_res, clk_details->fclken_offset, fclken);

	
	return (0);
}




/**
 *	omap3_clk_init - add a child item to the root omap3 device
 *	@dev: the parent device
 *	@prio: defines roughly the order with which to add the child to the parent
 * 
 *	Initialises the clock structure and add an instance of the omap_prcm to
 *	the parent device with the correct memory regions assigned.
 *
 *
 */
void
omap3_clk_init(device_t dev, int prio)
{
	device_t kid;
	struct omap_ivar *ivar;

	/* Start by adding the actual child to the parent (us) */
	kid = device_add_child_ordered(dev, prio, "omap_prcm", 0);
	if (kid == NULL) {
	    printf("Can't add child omap_prcm0 ordered\n");
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
		
	/* Assign the memory region to the resource list */
	bus_set_resource(kid, SYS_RES_MEMORY, CM_INSTANCE_MEM_REGION,
	                 OMAP35XX_CM_HWBASE,  0x2000);
	bus_set_resource(kid, SYS_RES_MEMORY, PRM_INSTANCE_MEM_REGION,
	                 OMAP35XX_PRM_HWBASE, 0x2000);
}


