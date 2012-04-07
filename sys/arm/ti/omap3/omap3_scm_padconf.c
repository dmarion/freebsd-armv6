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
#include <arm/omap/omap_scm.h>
#include <arm/omap/omap3/omap3var.h>
#include <arm/omap/omap3/omap35xx_reg.h>


/*
 *	This file defines the pin mux configuration for the OMAP4xxx series of
 *	devices.
 *
 *	How This is Suppose to Work
 *	===========================
 *	- There is a top level omap_scm module (System Control Module) that is
 *	the interface for all omap drivers, which can use it to change the mux
 *	settings for individual pins.  (That said, typically the pin mux settings
 *	are set to defaults by the 'hints' and then not altered by the driver).
 *
 *	- For this to work the top level driver needs all the pin info, and hence
 *	this is where this file comes in.  Here we define all the pin information
 *	that is supplied to the top level driver.
 *
 */

#define _OMAP_PINDEF(r, b, gp, gm, m0, m1, m2, m3, m4, m5, m6, m7) \
	{	.reg_off = r, \
		.ballname = b, \
		.gpio_pin = gp, \
		.gpio_mode = gm, \
		.muxmodes[0] = m0, \
		.muxmodes[1] = m1, \
		.muxmodes[2] = m2, \
		.muxmodes[3] = m3, \
		.muxmodes[4] = m4, \
		.muxmodes[5] = m5, \
		.muxmodes[6] = m6, \
		.muxmodes[7] = m7, \
	}

const struct omap_scm_padconf omap_padconf_devmap[] = {
	_OMAP_PINDEF(0x0116, "ag17",  99, 4, "cam_d0", NULL, NULL, NULL, "gpio_99", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0118, "ah17", 100, 4, "cam_d1", NULL, NULL, NULL, "gpio_100", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x012a,  "b25", 109, 4, "cam_d10", NULL, NULL, NULL, "gpio_109", "hw_dbg8", NULL, "safe_mode"),
	_OMAP_PINDEF(0x012c,  "c26", 110, 4, "cam_d11", NULL, NULL, NULL, "gpio_110", "hw_dbg9", NULL, "safe_mode"),
	_OMAP_PINDEF(0x011a,  "b24", 101, 4, "cam_d2", NULL, NULL, NULL, "gpio_101", "hw_dbg4", NULL, "safe_mode"),
	_OMAP_PINDEF(0x011c,  "c24", 102, 4, "cam_d3", NULL, NULL, NULL, "gpio_102", "hw_dbg5", NULL, "safe_mode"),
	_OMAP_PINDEF(0x011e,  "d24", 103, 4, "cam_d4", NULL, NULL, NULL, "gpio_103", "hw_dbg6", NULL, "safe_mode"),
	_OMAP_PINDEF(0x0120,  "a25", 104, 4, "cam_d5", NULL, NULL, NULL, "gpio_104", "hw_dbg7", NULL, "safe_mode"),
	_OMAP_PINDEF(0x0122,  "k28", 105, 4, "cam_d6", NULL, NULL, NULL, "gpio_105", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0124,  "l28", 106, 4, "cam_d7", NULL, NULL, NULL, "gpio_106", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0126,  "k27", 107, 4, "cam_d8", NULL, NULL, NULL, "gpio_107", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0128,  "l27", 108, 4, "cam_d9", NULL, NULL, NULL, "gpio_108", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0114,  "c23",  98, 4, "cam_fld", NULL, "cam_global_reset", NULL, "gpio_98", "hw_dbg3", NULL, "safe_mode"),
	_OMAP_PINDEF(0x010c,  "a24",  94, 4, "cam_hs", NULL, NULL, NULL, "gpio_94", "hw_dbg0", NULL, "safe_mode"),
	_OMAP_PINDEF(0x0112,  "c27",  97, 4, "cam_pclk", NULL, NULL, NULL, "gpio_97", "hw_dbg2", NULL, "safe_mode"),
	_OMAP_PINDEF(0x0132,  "d25", 126, 4, "cam_strobe", NULL, NULL, NULL, "gpio_126", "hw_dbg11", NULL, "safe_mode"),
	_OMAP_PINDEF(0x010e,  "a23",  95, 4, "cam_vs", NULL, NULL, NULL, "gpio_95", "hw_dbg1", NULL, "safe_mode"),
	_OMAP_PINDEF(0x0130,  "b23", 167, 4, "cam_wen", NULL, "cam_shutter", NULL, "gpio_167", "hw_dbg10", NULL, "safe_mode"),
	_OMAP_PINDEF(0x0110,  "c25",  96, 4, "cam_xclka", NULL, NULL, NULL, "gpio_96", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x012e,  "b26", 111, 4, "cam_xclkb", NULL, NULL, NULL, "gpio_111", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0134, "ag19", 112, 4, "csi2_dx0", NULL, NULL, NULL, "gpio_112", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0138, "ag18", 114, 4, "csi2_dx1", NULL, NULL, NULL, "gpio_114", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0136, "ah19", 113, 4, "csi2_dy0", NULL, NULL, NULL, "gpio_113", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x013a, "ah18", 115, 4, "csi2_dy1", NULL, NULL, NULL, "gpio_115", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00da,  "e27",  69, 4, "dss_acbias", NULL, NULL, NULL, "gpio_69", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00dc, "ag22",  70, 4, "dss_data0", NULL, "uart1_cts", NULL, "gpio_70", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00de, "ah22",  71, 4, "dss_data1", NULL, "uart1_rts", NULL, "gpio_71", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00f0, "ad28",  80, 4, "dss_data10", NULL, NULL, NULL, "gpio_80", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00f2, "ad27",  81, 4, "dss_data11", NULL, NULL, NULL, "gpio_81", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00f4, "ab28",  82, 4, "dss_data12", NULL, NULL, NULL, "gpio_82", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00f6, "ab27",  83, 4, "dss_data13", NULL, NULL, NULL, "gpio_83", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00f8, "aa28",  84, 4, "dss_data14", NULL, NULL, NULL, "gpio_84", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00fa, "aa27",  85, 4, "dss_data15", NULL, NULL, NULL, "gpio_85", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00fc,  "g25",  86, 4, "dss_data16", NULL, NULL, NULL, "gpio_86", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00fe,  "h27",  87, 4, "dss_data17", NULL, NULL, NULL, "gpio_87", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0100,  "h26",  88, 4, "dss_data18", NULL, "mcspi3_clk", "dss_data0", "gpio_88", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0102,  "h25",  89, 4, "dss_data19", NULL, "mcspi3_simo", "dss_data1", "gpio_89", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0104,  "e28",  90, 4, "dss_data20", NULL, "mcspi3_somi", "dss_data2", "gpio_90", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0106,  "j26",  91, 4, "dss_data21", NULL, "mcspi3_cs0", "dss_data3", "gpio_91", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0108, "ac27",  92, 4, "dss_data22", NULL, "mcspi3_cs1", "dss_data4", "gpio_92", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x010a, "ac28",  93, 4, "dss_data23", NULL, NULL, "dss_data5", "gpio_93", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00e0, "ag23",  72, 4, "dss_data2", NULL, NULL, NULL, "gpio_72", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00e2, "ah23",  73, 4, "dss_data3", NULL, NULL, NULL, "gpio_73", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00e4, "ag24",  74, 4, "dss_data4", NULL, "uart3_rx_irrx", NULL, "gpio_74", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00e6, "ah24",  75, 4, "dss_data5", NULL, "uart3_tx_irtx", NULL, "gpio_75", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00e8,  "e26",  76, 4, "dss_data6", NULL, "uart1_tx", NULL, "gpio_76", "hw_dbg14", NULL, "safe_mode"),
	_OMAP_PINDEF(0x00ea,  "f28",  77, 4, "dss_data7", NULL, "uart1_rx", NULL, "gpio_77", "hw_dbg15", NULL, "safe_mode"),
	_OMAP_PINDEF(0x00ec,  "f27",  78, 4, "dss_data8", NULL, NULL, NULL, "gpio_78", "hw_dbg16", NULL, "safe_mode"),
	_OMAP_PINDEF(0x00ee,  "g26",  79, 4, "dss_data9", NULL, NULL, NULL, "gpio_79", "hw_dbg17", NULL, "safe_mode"),
	_OMAP_PINDEF(0x00d6,  "d26",  67, 4, "dss_hsync", NULL, NULL, NULL, "gpio_67", "hw_dbg13", NULL, "safe_mode"),
	_OMAP_PINDEF(0x00d4,  "d28",  66, 4, "dss_pclk", NULL, NULL, NULL, "gpio_66", "hw_dbg12", NULL, "safe_mode"),
	_OMAP_PINDEF(0x00d8,  "d27",  68, 4, "dss_vsync", NULL, NULL, NULL, "gpio_68", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x05d8, "af10",  12, 4, "etk_clk", "mcbsp5_clkx", "sdmmc3_clk", "hsusb1_stp", "gpio_12", "mm1_rxdp", "hsusb1_tll_stp", "hw_dbg0"),
	_OMAP_PINDEF(0x05da, "ae10",  13, 4, "etk_ctl", NULL, "sdmmc3_cmd", "hsusb1_clk", "gpio_13", NULL, "hsusb1_tll_clk", "hw_dbg1"),
	_OMAP_PINDEF(0x05dc, "af11",  14, 4, "etk_d0", "mcspi3_simo", "sdmmc3_dat4", "hsusb1_data0", "gpio_14", "mm1_rxrcv", "hsusb1_tll_data0", "hw_dbg2"),
	_OMAP_PINDEF(0x05de, "ag12",  15, 4, "etk_d1", "mcspi3_somi", NULL, "hsusb1_data1", "gpio_15", "mm1_txse0", "hsusb1_tll_data1", "hw_dbg3"),
	_OMAP_PINDEF(0x05f0,  "ae7",  24, 4, "etk_d10", NULL, "uart1_rx", "hsusb2_clk", "gpio_24", NULL, "hsusb2_tll_clk", "hw_dbg12"),
	_OMAP_PINDEF(0x05f2,  "af7",  25, 4, "etk_d11", NULL, NULL, "hsusb2_stp", "gpio_25", "mm2_rxdp", "hsusb2_tll_stp", "hw_dbg13"),
	_OMAP_PINDEF(0x05f4,  "ag7",  26, 4, "etk_d12", NULL, NULL, "hsusb2_dir", "gpio_26", NULL, "hsusb2_tll_dir", "hw_dbg14"),
	_OMAP_PINDEF(0x05f6,  "ah7",  27, 4, "etk_d13", NULL, NULL, "hsusb2_nxt", "gpio_27", "mm2_rxdm", "hsusb2_tll_nxt", "hw_dbg15"),
	_OMAP_PINDEF(0x05f8,  "ag8",  28, 4, "etk_d14", NULL, NULL, "hsusb2_data0", "gpio_28", "mm2_rxrcv", "hsusb2_tll_data0", "hw_dbg16"),
	_OMAP_PINDEF(0x05fa,  "ah8",  29, 4, "etk_d15", NULL, NULL, "hsusb2_data1", "gpio_29", "mm2_txse0", "hsusb2_tll_data1", "hw_dbg17"),
	_OMAP_PINDEF(0x05e0, "ah12",  16, 4, "etk_d2", "mcspi3_cs0", NULL, "hsusb1_data2", "gpio_16", "mm1_txdat", "hsusb1_tll_data2", "hw_dbg4"),
	_OMAP_PINDEF(0x05e2, "ae13",  17, 4, "etk_d3", "mcspi3_clk", "sdmmc3_dat3", "hsusb1_data7", "gpio_17", NULL, "hsusb1_tll_data7", "hw_dbg5"),
	_OMAP_PINDEF(0x05e4, "ae11",  18, 4, "etk_d4", "mcbsp5_dr", "sdmmc3_dat0", "hsusb1_data4", "gpio_18", NULL, "hsusb1_tll_data4", "hw_dbg6"),
	_OMAP_PINDEF(0x05e6,  "ah9",  19, 4, "etk_d5", "mcbsp5_fsx", "sdmmc3_dat1", "hsusb1_data5", "gpio_19", NULL, "hsusb1_tll_data5", "hw_dbg7"),
	_OMAP_PINDEF(0x05e8, "af13",  20, 4, "etk_d6", "mcbsp5_dx", "sdmmc3_dat2", "hsusb1_data6", "gpio_20", NULL, "hsusb1_tll_data6", "hw_dbg8"),
	_OMAP_PINDEF(0x05ea, "ah14",  21, 4, "etk_d7", "mcspi3_cs1", "sdmmc3_dat7", "hsusb1_data3", "gpio_21", "mm1_txen_n", "hsusb1_tll_data3", "hw_dbg9"),
	_OMAP_PINDEF(0x05ec,  "af9",  22, 4, "etk_d8", "sys_drm_msecure", "sdmmc3_dat6", "hsusb1_dir", "gpio_22", NULL, "hsusb1_tll_dir", "hw_dbg10"),
	_OMAP_PINDEF(0x05ee,  "ag9",  23, 4, "etk_d9", "sys_secure_indicator", "sdmmc3_dat5", "hsusb1_nxt", "gpio_23", "mm1_rxdm", "hsusb1_tll_nxt", "hw_dbg11"),
	_OMAP_PINDEF(0x007a,   "n4",  34, 4, "gpmc_a1", NULL, NULL, NULL, "gpio_34", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x008c,   "k3",  43, 4, "gpmc_a10", "sys_ndmareq3", NULL, NULL, "gpio_43", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x007c,   "m4",  35, 4, "gpmc_a2", NULL, NULL, NULL, "gpio_35", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x007e,   "l4",  36, 4, "gpmc_a3", NULL, NULL, NULL, "gpio_36", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0080,   "k4",  37, 4, "gpmc_a4", NULL, NULL, NULL, "gpio_37", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0082,   "t3",  38, 4, "gpmc_a5", NULL, NULL, NULL, "gpio_38", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0084,   "r3",  39, 4, "gpmc_a6", NULL, NULL, NULL, "gpio_39", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0086,   "n3",  40, 4, "gpmc_a7", NULL, NULL, NULL, "gpio_40", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0088,   "m3",  41, 4, "gpmc_a8", NULL, NULL, NULL, "gpio_41", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x008a,   "l3",  42, 4, "gpmc_a9", "sys_ndmareq2", NULL, NULL, "gpio_42", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00be,   "t4",  59, 4, "gpmc_clk", NULL, NULL, NULL, "gpio_59", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00a2,   "p1",  46, 4, "gpmc_d10", NULL, NULL, NULL, "gpio_46", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00a4,   "r1",  47, 4, "gpmc_d11", NULL, NULL, NULL, "gpio_47", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00a6,   "r2",  48, 4, "gpmc_d12", NULL, NULL, NULL, "gpio_48", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00a8,   "t2",  49, 4, "gpmc_d13", NULL, NULL, NULL, "gpio_49", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00aa,   "w1",  50, 4, "gpmc_d14", NULL, NULL, NULL, "gpio_50", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00ac,   "y1",  51, 4, "gpmc_d15", NULL, NULL, NULL, "gpio_51", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x009e,   "h2",  44, 4, "gpmc_d8", NULL, NULL, NULL, "gpio_44", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00a0,   "k2",  45, 4, "gpmc_d9", NULL, NULL, NULL, "gpio_45", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00c6,   "g3",  60, 4, "gpmc_nbe0_cle", NULL, NULL, NULL, "gpio_60", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00c8,   "u3",  61, 4, "gpmc_nbe1", NULL, NULL, NULL, "gpio_61", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00b0,   "h3",  52, 4, "gpmc_ncs1", NULL, NULL, NULL, "gpio_52", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00b2,   "v8",  53, 4, "gpmc_ncs2", NULL, NULL, NULL, "gpio_53", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00b4,   "u8",  54, 4, "gpmc_ncs3", "sys_ndmareq0", NULL, NULL, "gpio_54", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00b6,   "t8",  55, 4, "gpmc_ncs4", "sys_ndmareq1", "mcbsp4_clkx", "gpt9_pwm_evt", "gpio_55", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00b8,   "r8",  56, 4, "gpmc_ncs5", "sys_ndmareq2", "mcbsp4_dr", "gpt10_pwm_evt", "gpio_56", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00ba,   "p8",  57, 4, "gpmc_ncs6", "sys_ndmareq3", "mcbsp4_dx", "gpt11_pwm_evt", "gpio_57", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00bc,   "n8",  58, 4, "gpmc_ncs7", "gpmc_io_dir", "mcbsp4_fsx", "gpt8_pwm_evt", "gpio_58", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00ca,   "h1",  62, 4, "gpmc_nwp", NULL, NULL, NULL, "gpio_62", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00ce,   "l8",  63, 4, "gpmc_wait1", NULL, NULL, NULL, "gpio_63", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00d0,   "k8",  64, 4, "gpmc_wait2", NULL, NULL, NULL, "gpio_64", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x00d2,   "j8",  65, 4, "gpmc_wait3", "sys_ndmareq1", NULL, NULL, "gpio_65", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01c6,  "j25", 170, 4, "hdq_sio", "sys_altclk", "i2c2_sccbe", "i2c3_sccbe", "gpio_170", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01a2,  "t28", 120, 4, "hsusb0_clk", NULL, NULL, NULL, "gpio_120", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01aa,  "t27", 125, 4, "hsusb0_data0", NULL, "uart3_tx_irtx", NULL, "gpio_125", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01ac,  "u28", 130, 4, "hsusb0_data1", NULL, "uart3_rx_irrx", NULL, "gpio_130", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01ae,  "u27", 131, 4, "hsusb0_data2", NULL, "uart3_rts_sd", NULL, "gpio_131", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01b0,  "u26", 169, 4, "hsusb0_data3", NULL, "uart3_cts_rctx", NULL, "gpio_169", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01b2,  "u25", 188, 4, "hsusb0_data4", NULL, NULL, NULL, "gpio_188", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01b4,  "v28", 189, 4, "hsusb0_data5", NULL, NULL, NULL, "gpio_189", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01b6,  "v27", 190, 4, "hsusb0_data6", NULL, NULL, NULL, "gpio_190", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01b8,  "v26", 191, 4, "hsusb0_data7", NULL, NULL, NULL, "gpio_191", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01a6,  "r28", 122, 4, "hsusb0_dir", NULL, NULL, NULL, "gpio_122", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01a8,  "t26", 124, 4, "hsusb0_nxt", NULL, NULL, NULL, "gpio_124", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01a4,  "t25", 121, 4, "hsusb0_stp", NULL, NULL, NULL, "gpio_121", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01be, "af15", 168, 4, "i2c2_scl", NULL, NULL, NULL, "gpio_168", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01c0, "ae15", 183, 4, "i2c2_sda", NULL, NULL, NULL, "gpio_183", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01c2, "af14", 184, 4, "i2c3_scl", NULL, NULL, NULL, "gpio_184", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01c4, "ag14", 185, 4, "i2c3_sda", NULL, NULL, NULL, "gpio_185", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a00, "ad26",   0, 0, "i2c4_scl", "sys_nvmode1", NULL, NULL, NULL, NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a02, "ae26",   0, 0, "i2c4_sda", "sys_nvmode2", NULL, NULL, NULL, NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a24, "aa11",  11, 4, "jtag_emu0", NULL, NULL, NULL, "gpio_11", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a26, "aa10",  31, 4, "jtag_emu1", NULL, NULL, NULL, "gpio_31", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x018c,  "y21", 156, 4, "mcbsp1_clkr", "mcspi4_clk", NULL, NULL, "gpio_156", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0198,  "w21", 162, 4, "mcbsp1_clkx", NULL, "mcbsp3_clkx", NULL, "gpio_162", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0192,  "u21", 159, 4, "mcbsp1_dr", "mcspi4_somi", "mcbsp3_dr", NULL, "gpio_159", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0190,  "v21", 158, 4, "mcbsp1_dx", "mcspi4_simo", "mcbsp3_dx", NULL, "gpio_158", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x018e, "aa21", 157, 4, "mcbsp1_fsr", NULL, "cam_global_reset", NULL, "gpio_157", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0196,  "k26", 161, 4, "mcbsp1_fsx", "mcspi4_cs0", "mcbsp3_fsx", NULL, "gpio_161", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x013e,  "n21", 117, 4, "mcbsp2_clkx", NULL, NULL, NULL, "gpio_117", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0140,  "r21", 118, 4, "mcbsp2_dr", NULL, NULL, NULL, "gpio_118", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0142,  "m21", 119, 4, "mcbsp2_dx", NULL, NULL, NULL, "gpio_119", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x013c,  "p21", 116, 4, "mcbsp2_fsx", NULL, NULL, NULL, "gpio_116", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0170,  "af5", 142, 4, "mcbsp3_clkx", "uart2_tx", NULL, NULL, "gpio_142", "hsusb3_tll_data6", NULL, "safe_mode"),
	_OMAP_PINDEF(0x016e,  "ae6", 141, 4, "mcbsp3_dr", "uart2_rts", NULL, NULL, "gpio_141", "hsusb3_tll_data5", NULL, "safe_mode"),
	_OMAP_PINDEF(0x016c,  "af6", 140, 4, "mcbsp3_dx", "uart2_cts", NULL, NULL, "gpio_140", "hsusb3_tll_data4", NULL, "safe_mode"),
	_OMAP_PINDEF(0x0172,  "ae5", 143, 4, "mcbsp3_fsx", "uart2_rx", NULL, NULL, "gpio_143", "hsusb3_tll_data7", NULL, "safe_mode"),
	_OMAP_PINDEF(0x0184,  "ae1", 152, 4, "mcbsp4_clkx", NULL, NULL, NULL, "gpio_152", "hsusb3_tll_data1", "mm3_txse0", "safe_mode"),
	_OMAP_PINDEF(0x0186,  "ad1", 153, 4, "mcbsp4_dr", NULL, NULL, NULL, "gpio_153", "hsusb3_tll_data0", "mm3_rxrcv", "safe_mode"),
	_OMAP_PINDEF(0x0188,  "ad2", 154, 4, "mcbsp4_dx", NULL, NULL, NULL, "gpio_154", "hsusb3_tll_data2", "mm3_txdat", "safe_mode"),
	_OMAP_PINDEF(0x018a,  "ac1", 155, 4, "mcbsp4_fsx", NULL, NULL, NULL, "gpio_155", "hsusb3_tll_data3", "mm3_txen_n", "safe_mode"),
	_OMAP_PINDEF(0x0194,  "t21", 160, 4, "mcbsp_clks", NULL, "cam_shutter", NULL, "gpio_160", "uart1_cts", NULL, "safe_mode"),
	_OMAP_PINDEF(0x01c8,  "ab3", 171, 4, "mcspi1_clk", "sdmmc2_dat4", NULL, NULL, "gpio_171", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01ce,  "ac2", 174, 4, "mcspi1_cs0", "sdmmc2_dat7", NULL, NULL, "gpio_174", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01d0,  "ac3", 175, 4, "mcspi1_cs1", NULL, NULL, "sdmmc3_cmd", "gpio_175", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01d2,  "ab1", 176, 4, "mcspi1_cs2", NULL, NULL, "sdmmc3_clk", "gpio_176", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01d4,  "ab2", 177, 4, "mcspi1_cs3", NULL, "hsusb2_tll_data2", "hsusb2_data2", "gpio_177", "mm2_txdat", NULL, "safe_mode"),
	_OMAP_PINDEF(0x01ca,  "ab4", 172, 4, "mcspi1_simo", "sdmmc2_dat5", NULL, NULL, "gpio_172", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01cc,  "aa4", 173, 4, "mcspi1_somi", "sdmmc2_dat6", NULL, NULL, "gpio_173", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01d6,  "aa3", 178, 4, "mcspi2_clk", NULL, "hsusb2_tll_data7", "hsusb2_data7", "gpio_178", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01dc,   "y4", 181, 4, "mcspi2_cs0", "gpt11_pwm_evt", "hsusb2_tll_data6", "hsusb2_data6", "gpio_181", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01de,   "v3", 182, 4, "mcspi2_cs1", "gpt8_pwm_evt", "hsusb2_tll_data3", "hsusb2_data3", "gpio_182", "mm2_txen_n", NULL, "safe_mode"),
	_OMAP_PINDEF(0x01d8,   "y2", 179, 4, "mcspi2_simo", "gpt9_pwm_evt", "hsusb2_tll_data4", "hsusb2_data4", "gpio_179", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01da,   "y3", 180, 4, "mcspi2_somi", "gpt10_pwm_evt", "hsusb2_tll_data5", "hsusb2_data5", "gpio_180", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0144,  "n28", 120, 4, "sdmmc1_clk", NULL, NULL, NULL, "gpio_120", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0146,  "m27", 121, 4, "sdmmc1_cmd", NULL, NULL, NULL, "gpio_121", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0148,  "n27", 122, 4, "sdmmc1_dat0", NULL, NULL, NULL, "gpio_122", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x014a,  "n26", 123, 4, "sdmmc1_dat1", NULL, NULL, NULL, "gpio_123", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x014c,  "n25", 124, 4, "sdmmc1_dat2", NULL, NULL, NULL, "gpio_124", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x014e,  "p28", 125, 4, "sdmmc1_dat3", NULL, NULL, NULL, "gpio_125", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0150,  "p27", 126, 4, "sdmmc1_dat4", NULL, "sim_io", NULL, "gpio_126", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0152,  "p26", 127, 4, "sdmmc1_dat5", NULL, "sim_clk", NULL, "gpio_127", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0154,  "r27", 128, 4, "sdmmc1_dat6", NULL, "sim_pwrctrl", NULL, "gpio_128", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0156,  "r25", 129, 4, "sdmmc1_dat7", NULL, "sim_rst", NULL, "gpio_129", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0158,  "ae2", 130, 4, "sdmmc2_clk", "mcspi3_clk", NULL, NULL, "gpio_130", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x015a,  "ag5", 131, 4, "sdmmc2_cmd", "mcspi3_simo", NULL, NULL, "gpio_131", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x015c,  "ah5", 132, 4, "sdmmc2_dat0", "mcspi3_somi", NULL, NULL, "gpio_132", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x015e,  "ah4", 133, 4, "sdmmc2_dat1", NULL, NULL, NULL, "gpio_133", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0160,  "ag4", 134, 4, "sdmmc2_dat2", "mcspi3_cs1", NULL, NULL, "gpio_134", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0162,  "af4", 135, 4, "sdmmc2_dat3", "mcspi3_cs0", NULL, NULL, "gpio_135", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0164,  "ae4", 136, 4, "sdmmc2_dat4", "sdmmc2_dir_dat0", NULL, "sdmmc3_dat0", "gpio_136", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0166,  "ah3", 137, 4, "sdmmc2_dat5", "sdmmc2_dir_dat1", "cam_global_reset", "sdmmc3_dat1", "gpio_137", "hsusb3_tll_stp", "mm3_rxdp", "safe_mode"),
	_OMAP_PINDEF(0x0168,  "af3", 138, 4, "sdmmc2_dat6", "sdmmc2_dir_cmd", "cam_shutter", "sdmmc3_dat2", "gpio_138", "hsusb3_tll_dir", NULL, "safe_mode"),
	_OMAP_PINDEF(0x016a,  "ae3", 139, 4, "sdmmc2_dat7", "sdmmc2_clkin", NULL, "sdmmc3_dat3", "gpio_139", "hsusb3_tll_nxt", "mm3_rxdm", "safe_mode"),
	_OMAP_PINDEF(0x0262,  "ae3",   0, 0, "sdrc_cke0", NULL, NULL, NULL, NULL, NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0264,  "ae3",   0, 0, "sdrc_cke1", NULL, NULL, NULL, NULL, NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a0a, "ah26",   2, 4, "sys_boot0", NULL, NULL, NULL, "gpio_2", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a0c, "ag26",   3, 4, "sys_boot1", NULL, NULL, NULL, "gpio_3", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a0e, "ae14",   4, 4, "sys_boot2", NULL, NULL, NULL, "gpio_4", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a10, "af18",   5, 4, "sys_boot3", NULL, NULL, NULL, "gpio_5", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a12, "af19",   6, 4, "sys_boot4", "sdmmc2_dir_dat2", NULL, NULL, "gpio_6", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a14, "ae21",   7, 4, "sys_boot5", "sdmmc2_dir_dat3", NULL, NULL, "gpio_7", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a16, "af21",   8, 4, "sys_boot6", NULL, NULL, NULL, "gpio_8", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a1a, "ag25",  10, 4, "sys_clkout1", NULL, NULL, NULL, "gpio_10", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01e2, "ae22", 186, 4, "sys_clkout2", NULL, NULL, NULL, "gpio_186", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a06, "af25",   1, 4, "sys_clkreq", NULL, NULL, NULL, "gpio_1", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01e0, "af26",   0, 0, "sys_nirq", NULL, NULL, NULL, "gpio_0", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a08, "af24",  30, 4, "sys_nreswarm", NULL, NULL, NULL, "gpio_30", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0a18, "af22",   9, 4, "sys_off_mode", NULL, NULL, NULL, "gpio_9", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0180,   "w8", 150, 4, "uart1_cts", NULL, NULL, NULL, "gpio_150", "hsusb3_tll_clk", NULL, "safe_mode"),
	_OMAP_PINDEF(0x017e,  "aa9", 149, 4, "uart1_rts", NULL, NULL, NULL, "gpio_149", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0182,   "y8", 151, 4, "uart1_rx", NULL, "mcbsp1_clkr", "mcspi4_clk", "gpio_151", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x017c,  "aa8", 148, 4, "uart1_tx", NULL, NULL, NULL, "gpio_148", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0174, "ab26", 144, 4, "uart2_cts", "mcbsp3_dx", "gpt9_pwm_evt", NULL, "gpio_144", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0176, "ab25", 145, 4, "uart2_rts", "mcbsp3_dr", "gpt10_pwm_evt", NULL, "gpio_145", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x017a, "ad25", 147, 4, "uart2_rx", "mcbsp3_fsx", "gpt8_pwm_evt", NULL, "gpio_147", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x0178, "aa25", 146, 4, "uart2_tx", "mcbsp3_clkx", "gpt11_pwm_evt", NULL, "gpio_146", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x019a,  "h18", 163, 4, "uart3_cts_rctx", NULL, NULL, NULL, "gpio_163", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x019c,  "h19", 164, 4, "uart3_rts_sd", NULL, NULL, NULL, "gpio_164", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x019e,  "h20", 165, 4, "uart3_rx_irrx", NULL, NULL, NULL, "gpio_165", NULL, NULL, "safe_mode"),
	_OMAP_PINDEF(0x01a0,  "h21", 166, 4, "uart3_tx_irtx", NULL, NULL, NULL, "gpio_166", NULL, NULL, "safe_mode"),
	{  .ballname = NULL  },
};



/**
 *	omap4_pinmux_init - adds and initialise the OMAP SCM driver
 *	@dev: the parent device
 *	@prio: defines roughly the order with which to add the child to the parent
 * 
 *	Initialises the pinmux structure and add an instance of the omap_scm to
 *	the parent device with the correct memory regions assigned.
 *
 *
 */
void
omap3_padconf_init(device_t dev, int prio)
{
	device_t kid;
	struct omap_ivar *ivar;

	/* Start by adding the actual child to the parent (us) */
	kid = device_add_child_ordered(dev, prio, "omap_scm", 0);
	if (kid == NULL) {
	    printf("Can't add child omap_scm0 ordered\n");
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
	
	/* Initialise the resource list */
	resource_list_init(&ivar->resources);
		
	/* Assign the memory region to the resource list */
	bus_set_resource(kid, SYS_RES_MEMORY, 0, OMAP35XX_SCM_HWBASE,
	                 OMAP35XX_SCM_SIZE);
}


