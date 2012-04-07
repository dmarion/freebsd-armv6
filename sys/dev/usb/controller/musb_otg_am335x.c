/*-
 * Copyright (c) 2012 Damjan Marion <dmarion@Freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver for TI implementation of Mentor InventraTM MUSBHSFC USB 2.0
 * High-Speed Function Controller found in AM335x SoC
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bus.h"

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/controller/ehci.h>
#include <dev/usb/controller/ehcireg.h>

#include <arm/ti/ti_prcm.h>

#include <arm/debug.h> //FIXME

#define USBSS_OFFSET			0
#define USBSS_REVREG			(USBSS_OFFSET + 0x000)

#define USB_CTRL_OFFSET(p)		(0x1000 + (p * 0x800))
#define USB_REV(p)			(USB_CTRL_OFFSET(p) + 0x000)
#define USB_CTRL(p)			(USB_CTRL_OFFSET(p) + 0x014)
#define USB_STAT(p)			(USB_CTRL_OFFSET(p) + 0x018)

#define NUM_MEM_RESOURCES 5
#define NUM_IRQ_RES 4

#define	musb_read_4(reg)		\
	bus_space_read_4(sc->bst, sc->bsh, reg)
#define	musb_write_4(reg, val)	\
	bus_space_write_4(sc->bst, sc->bsh, reg, val)

struct musb_otg_am335x_softc {
	struct resource *	mem_res;
	struct resource *	irq_res[NUM_IRQ_RES];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	struct usb_bus sc_bus;
};

typedef struct musb_otg_am335x_softc musb_otg_am335x_softc_t;

static struct resource_spec musb_otg_am335x_mem_spec[] = {
	{ SYS_RES_MEMORY,   0,  RF_ACTIVE },
	{ -1,               0,  0 }
};

static struct resource_spec musb_otg_am335x_irq_spec[] = {
	{ SYS_RES_IRQ,      0,  RF_ACTIVE },
	{ SYS_RES_IRQ,      1,  RF_ACTIVE },
	{ SYS_RES_IRQ,      2,  RF_ACTIVE },
	{ SYS_RES_IRQ,      3,  RF_ACTIVE },
	{ -1,               0,  0 }
};

static int musb_otg_am335x_probe(device_t self);
static int musb_otg_am335x_attach(device_t self);
static int musb_otg_am335x_detach(device_t self);

static device_method_t musbhsfc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, musb_otg_am335x_probe),
	DEVMETHOD(device_attach, musb_otg_am335x_attach),
	DEVMETHOD(device_detach, musb_otg_am335x_detach),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),
	DEVMETHOD_END
};

static driver_t musbhsfc_driver = {
	"musbhsfc",
	musbhsfc_methods,
	sizeof(musb_otg_am335x_softc_t),
};

static devclass_t musbhsfc_devclass;

DRIVER_MODULE(musbhsfc, simplebus, musbhsfc_driver, musbhsfc_devclass, 0, 0);
MODULE_DEPEND(musbhsfc, usb, 1, 1, 1);


static int
musb_otg_am335x_probe(device_t self)
{

	if (!ofw_bus_is_compatible(self, "ti,am335x-musbhsfc"))
		return (ENXIO);

	device_set_desc(self, "Mentor Inventra MUSBHSFC USB 2.0 HS Function Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
musb_otg_am335x_attach(device_t self)
{
	musb_otg_am335x_softc_t *sc = device_get_softc(self);
	int err;
	uint32_t x;

	/* Request the memory resources */
	err = bus_alloc_resources(self, musb_otg_am335x_mem_spec,
		&sc->mem_res);
	if (err) {
		device_printf(self, "Error: could not allocate mem resources\n");
		return (ENXIO);
	}
	sc->bst = rman_get_bustag(sc->mem_res);
	sc->bsh = rman_get_bushandle(sc->mem_res);

	/* Request the IRQ resources */
	err = bus_alloc_resources(self, musb_otg_am335x_irq_spec,
		sc->irq_res);
	if (err) {
		device_printf(self, "Error: could not allocate irq resources\n");
		return (ENXIO);
	}

	/* Configure source and enable */
	err = ti_prcm_clk_enable(MUSB0_CLK);
	if (err) {
		device_printf(self, "Error: could not setup timer clock\n");
		return (ENXIO);
	}


	x = musb_read_4(USBSS_REVREG);
	device_printf(self, "Revision %u.%u\n",(x >> 8) & 0x7, x & 0x3F);

	sc->sc_bus.bdev = device_add_child(self, "usbus", -1);
	if (!sc->sc_bus.bdev) {
		device_printf(self, "Could not add USB device\n");
		goto error;
	}
	device_set_ivars(sc->sc_bus.bdev, &sc->sc_bus);


	return (0);
error:
	musb_otg_am335x_detach(self);
	return (ENXIO);
}

static int
musb_otg_am335x_detach(device_t self)
{
	//musb_otg_am335x_softc_t *sc = device_get_softc(self);
	return (0);
}
