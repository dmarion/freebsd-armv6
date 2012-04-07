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
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/ic/ns16550.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include <arm/omap/omap3/omap3var.h>
#include <arm/omap/omap3/omap35xx_reg.h>

#include "uart_if.h"

#define OMAP35XX_UART_FREQ  48000000	/* 48Mhz clock for all uarts (techref 17.3.1.1) */


extern SLIST_HEAD(uart_devinfo_list, uart_devinfo) uart_sysdevs;


/**
 *	omap3_uart_attach - attach function for the driver
 *	@dev: uart device handle
 *
 *	Driver initialisation ...
 *
 *	RETURNS:
 *	Returns 0 on success or an error code on failure.
 */
static int
omap3_uart_attach(device_t dev)
{
	int unit = device_get_unit(dev);

	device_printf(dev, "Attaching device %d\n", unit);

	/* TODO: ensure the interface clocks are running */
	
	/* TODO: ensure the pin mappings are configured correctly */

	return uart_bus_attach(dev);
}


/**
 *	omap3_mmc_detach - dettach function for the driver
 *	@dev: mmc device handle
 *
 *	Shutdowns the controll and release resources allocated by the driver.
 *
 *	RETURNS:
 *	Always returns 0.
 */
static int
omap3_uart_detach(device_t dev)
{
	int unit = device_get_unit(dev);

	device_printf(dev, "Detaching device %d\n", unit);

	return uart_bus_detach(dev);
}



/**
 *	omap3_uart_probe - probe function for the driver
 *	@dev: uart device handle
 *
 *
 *
 *	RETURNS:
 *	0 on success, error code on failure.
 */
static int
omap3_uart_probe(device_t dev)
{
	struct uart_softc *sc;
	int unit = device_get_unit(dev);
	int rclk;

	device_printf(dev, "Probing device %d\n", unit);

	sc = device_get_softc(dev);
	sc->sc_class = &uart_ns8250_class;
   
	if (resource_int_value("omap_uart", unit, "rclk", &rclk))
		rclk = OMAP35XX_UART_FREQ;
	
	// if (bootverbose)
		device_printf(dev, "rclk %u\n", rclk);

	
	/* Depending on the device we need to assign it a register range and an
	 * IRQ number.
	 */
	//bus_set_resource(dev, SYS_RES_IRQ, 0, OMAP35XX_IRQ_UART1, 1);
	//bus_set_resource(dev, SYS_RES_MEMORY, 0, OMAP35XX_UART3_HWBASE, OMAP35XX_UART3_SIZE);

	sc->sc_sysdev = SLIST_FIRST(&uart_sysdevs);
	bcopy(&sc->sc_sysdev->bas, &sc->sc_bas, sizeof(sc->sc_bas));


	return uart_bus_probe(dev, 2, rclk, 0, device_get_unit(dev));
}


static device_method_t omap3_uart_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		omap3_uart_probe),
	DEVMETHOD(device_attach,	omap3_uart_attach),
	DEVMETHOD(device_detach,	omap3_uart_detach),
	{ 0, 0 }
};


static driver_t omap3_uart_driver = {
	uart_driver_name,
	omap3_uart_methods,
	sizeof(struct uart_softc),
};

DRIVER_MODULE(uart, omap, omap3_uart_driver, uart_devclass, 0, 0);



