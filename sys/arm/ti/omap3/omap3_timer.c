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

/**
 * To be confusing, there are two timers setup here, one is the system ticks
 * that is suppose to go off 'hz' number of times a second. The other is a
 * general purpose counter that is just used to provide a count to the system.
 *
 * For the system tick timer we use GPTIMER10, this has an acurate 1ms mode
 * which is designed for system tick generation, however at the moment we don't
 * use that feature, instead we just run it as a normal 32KHz timer.
 *
 * For the other timer we use GPTIMER11, for no special reason other than it
 * comes after 10 and they are both in the core power domain. It's clocked
 * of the SYS_CLK which (on the beagleboard at least) is 13MHz.
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/timetc.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <arm/omap/omap3/omap35xx_reg.h>
#include <arm/omap/omap3/omap3var.h>

#include <arm/omap/omap_gptimer.h>
#include <arm/omap/omap_prcm.h>

//#define CPU_CLOCKSPEED			500000000

/**
 * The pin to use for the heartbeat, the pin will be toggled every second. If
 * you don't want a heartbeat, don't define the following.
 * TODO: Move this to make options
 */
//#define OMAP3_HEARTBEAT_GPIO	150


#define TICKTIMER_GPTIMER       10
#define TIMECOUNT_GPTIMER       11



#define __omap3_delay(i)                                     \
	do {                                                     \
		unsigned int cnt = (i);                              \
		__asm __volatile("1:  subs   %0, %0, 1\n"            \
		                 "    bne    1b\n"                   \
		                 : "+r" (cnt) : : "cc");             \
	} while(0)


static unsigned int delay_loops_per_us = 100;



/**
 *	omap4_timer_get_timecount - returns the count in GPTIMER11, the system counter
 *	@tc: pointer to the timecounter structure used to register the callback
 *
 *	
 *
 *	RETURNS:
 *	the value in the counter
 */
static unsigned
omap3_timer_get_timecount(struct timecounter *tc)
{
	uint32_t count;
	omap_gptimer_read_count(TIMECOUNT_GPTIMER, &count);
	return(count);
}

static struct timecounter g_omap3_timecounter = {
	.tc_get_timecount  = omap3_timer_get_timecount,	/* get_timecount */
	.tc_poll_pps       = NULL,						/* no poll_pps */
	.tc_counter_mask   = ~0u,						/* counter_mask */
	.tc_frequency      = 0,							/* frequency */
	.tc_name           = "OMAP3 Timer",				/* name */
	.tc_quality        = 1000,						/* quality */
};



/**
 *	omap3_calibrate_delay_loop - uses the setup timecounter to configure delay
 *
 *	This is not very scientfic, basically just use the timecount to measure the
 *	time to do 1000 delay loops (for loop with 1024 loops).
 *
 *
 */
static int
omap3_calibrate_delay_loop(struct timecounter *tc)
{
	u_int oldirqstate;
	unsigned int start, end;
	uint64_t nanosecs;
	
	/* Disable interrupts to ensure they don't mess up the calculation */
	oldirqstate = disable_interrupts(I32_bit);

	start = omap3_timer_get_timecount(tc);
	__omap3_delay(10240);
	end = omap3_timer_get_timecount(tc);

	restore_interrupts(oldirqstate);

	/* Calculate the number of loops in 1us */
	nanosecs = ((uint64_t)(end - start) * 1000000000ULL) / tc->tc_frequency;
	delay_loops_per_us = (unsigned int)((uint64_t)(10240 * 1000) / nanosecs);

	printf("OMAP: delay loop calibrated to %u cycles\n", delay_loops_per_us);
	
	return (0);
}




/**
 *	omap3_clk_intr - interrupt handler for the tick timer (GPTIMER10)
 *	@arg: the trapframe, needed for the hardclock system function.
 *
 *	This interrupt is triggered every hz times a second.  It's role is basically
 *	to just clear the interrupt status and set it up for triggering again, plus
 *	tell the system a tick timer has gone off by calling the hardclock()
 *	function from the kernel API.
 *
 *	RETURNS:
 *	Always returns FILTER_HANDLED. 
 */
static int
omap3_timer_tick_intr(void *arg)
{
	struct trapframe *frame = arg;
#if defined(OMAP3_HEARTBEAT_GPIO)
	static int heartbeat_cnt = 0;
#endif

	/* Acknowledge the interrupt */
	omap_gptimer_intr_filter_ack(TICKTIMER_GPTIMER);

	/* Heartbeat */
#if defined(OMAP3_HEARTBEAT_GPIO)
	if (heartbeat_cnt++ >= (hz/2)) {
		//printf("[BRG] ** tick **\n");
		omap3_gpio_pin_toggle(OMAP3_HEARTBEAT_GPIO);
		heartbeat_cnt = 0;
	}
#endif

	/* Do what we came here for */
	hardclock(TRAPF_USERMODE(frame), TRAPF_PC(frame));
	
	/* Indicate we've handed the interrupt */
	return (FILTER_HANDLED);
}




/**
 *	cpu_initclocks - function called by the system in init the tick clock/timer
 *
 *	This is where both the timercount and system ticks timer are started.
 *
 *	RETURNS:
 *	nothing
 */
void
cpu_initclocks(void)
{
	u_int oldirqstate;
	unsigned int timer_freq;

	oldirqstate = disable_interrupts(I32_bit);

	/* Number of microseconds between interrupts */
	tick = 1000000 / hz;

	/* Next setup one of the timers to be the system tick timer */
	if (omap_gptimer_activate(TICKTIMER_GPTIMER, OMAP_GPTIMER_PERIODIC_FLAG,
	                          tick, NULL, NULL)) {
		panic("Error: failed to activate system tick timer\n");
	}
	
	/* Setup an interrupt filter for the timer */
	if (omap_gptimer_set_intr_filter(TICKTIMER_GPTIMER, omap3_timer_tick_intr))
		panic("Error: failed to start system tick timer\n");
	
	/* Lastly start the tick timer */
	if (omap_gptimer_start(TICKTIMER_GPTIMER))
		panic("Error: failed to start system tick timer\n");

	omap_gptimer_get_freq(TICKTIMER_GPTIMER, &timer_freq);
printf("tick: timer_freq = %u\n", timer_freq);



	/* Setup another timer to be the timecounter */
	if (omap_gptimer_activate(TIMECOUNT_GPTIMER, OMAP_GPTIMER_PERIODIC_FLAG, 0,
	                        NULL, NULL)) {
		printf("Error: failed to activate system tick timer\n");
	} else if (omap_gptimer_start(TIMECOUNT_GPTIMER)) {
		printf("Error: failed to start system tick timer\n");
	}



	/* Save the system clock speed */
	omap_gptimer_get_freq(TIMECOUNT_GPTIMER, &timer_freq);
	g_omap3_timecounter.tc_frequency = timer_freq;
	
	/* Setup the time counter */
	tc_init(&g_omap3_timecounter);

	/* Calibrate the delay loop */
	omap3_calibrate_delay_loop(&g_omap3_timecounter);

	/* Restore interrupt state */
	restore_interrupts(oldirqstate);
}


/**
 *	DELAY - Delay for at least N microseconds.
 *	@n: number of microseconds to delay by
 *
 *	This function is called all over the kernel and is suppose to provide a
 *	consistent delay.  It is a busy loop and blocks polling a timer when called.
 *
 *	RETURNS:
 *	nothing
 */
void
DELAY(int usec)
{
	if (usec <= 0)
		return;

	for (; usec > 0; usec--) {
		__omap3_delay(delay_loops_per_us);
	}

}

#if 0
void
DELAY(int n)
{
	int32_t counts_per_usec;
	int32_t counts;
	uint32_t first, last;
	
	if (n <= 0)
		return;
	
	/* Check the timers are setup, if not just use a for loop for the meantime */
//	if (g_omap3_timecounter.tc_frequency == 0) {
	if (1) {

		/* If the CPU clock speed is defined we use that via the 'cycle count'
		 * register, this should give us a pretty accurate delay value.  If not
		 * defined we use a basic for loop with a very simply calculation.
		 */
#if defined(CPU_CLOCKSPEED)
		counts_per_usec = (CPU_CLOCKSPEED / 1000000);
		counts = counts_per_usec * 1000;
		
		__asm __volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (first));
		while (counts > 0) {
			__asm __volatile("mrc p15, 0, %0, c9, c13, 0" : "=r" (last));
			counts -= (int32_t)(last - first);
			first = last;
		}
#else
		uint32_t val;
		for (; n > 0; n--)
			for (val = 1000; val > 0; val--)
				continue;
#endif		
		return;
	}
	
	/* Get the number of times to count */
	counts_per_usec = ((g_omap3_timecounter.tc_frequency / 1000000) + 1);
	
//printf("[BRG] %s : %d\n", __FUNCTION__, __LINE__);	

	/*
	 * Clamp the timeout at a maximum value (about 32 seconds with
	 * a 66MHz clock). *Nobody* should be delay()ing for anywhere
	 * near that length of time and if they are, they should be hung
	 * out to dry.
	 */
	if (n >= (0x80000000U / counts_per_usec))
		counts = (0x80000000U / counts_per_usec) - 1;
	else
		counts = n * counts_per_usec;
	
	first = omap3_timer_get_timecount(NULL);
	
	while (counts > 0) {
		last = omap3_timer_get_timecount(NULL);
		counts -= (int32_t)(last - first);
		first = last;
	}
}
#endif


/**
 *	cpu_startprofclock - Starts the profile clock
 *
 *
 *	RETURNS:
 *	nothing
 */
void
cpu_startprofclock(void)
{
	/* TODO: implement */
}


/**
 *	cpu_startprofclock - Stop the profile clock
 *
 *
 *	RETURNS:
 *	nothing
 */
void
cpu_stopprofclock(void)
{
	/* TODO: implement */
}
