
static __inline void
dump_packet(char *pa, unsigned int len)
{
	int i;
	for (i=0;i<len;i++) {
		if (!(i % 16)) { printf("\n%06u ", i);}
		printf("%02x ", *(pa+i));
	}
	printf("\n");
}

/*
 *  Early Print
 */

#define LSR_THRE    0x20        /* Xmit holding register empty */
#if defined(SOC_TI_AM335X)
#define EARLY_UART_VA_BASE      0xE4E09000
#define EARLY_UART_PA_BASE      0x44E09000
#elif defined(SOC_OMAP4)
#define EARLY_UART_VA_BASE      0xE8020000
#define EARLY_UART_PA_BASE      0x48020000
#else
#error "Unknown SoC"
#endif


static __inline void
early_putc(unsigned char c)
{
	volatile uint8_t *p_lsr = (volatile uint8_t*) (EARLY_UART_VA_BASE + 0x14);
	volatile uint8_t *p_thr = (volatile uint8_t*) (EARLY_UART_VA_BASE + 0x00);

	while ((*p_lsr & LSR_THRE) == 0);
	*p_thr = c;

	if (c == '\n')
	{
		while ((*p_lsr & LSR_THRE) == 0);
		*p_thr = '\r';
	}
}


static __inline void
early_putstr(unsigned char *str)
{
        do {
                early_putc(*str);
        } while (*++str != '\0');
}

#if (STARTUP_PAGETABLE_ADDR < PHYSADDR) || \
    (STARTUP_PAGETABLE_ADDR > (PHYSADDR + (64 * 1024 * 1024)))
#error STARTUP_PAGETABLE_ADDR is not within init. MMU table, early print support not possible
#endif

static __inline void
early_print_init(void)
{
        volatile uint32_t *mmu_tbl = (volatile uint32_t*)STARTUP_PAGETABLE_ADDR;
        mmu_tbl[(EARLY_UART_VA_BASE >> L1_S_SHIFT)] = L1_TYPE_S | L1_S_AP(AP_KRW) | (EARLY_UART_PA_BASE & L1_S_FRAME);
        __asm __volatile ("mcr  p15, 0, r0, c8, c7, 0");        /* invalidate I+D TLBs */
        __asm __volatile ("mcr  p15, 0, r0, c7, c10, 4");       /* drain the write buffer */
        early_putstr("Early printf initialised\n");
}

#ifdef _VA_LIST_DECLARED
#define _EPRINTF
static __inline void
eprintf(const char *fmt,...)
{
	va_list ap;
	const char *hex = "0123456789abcdef";
	char buf[10];
	char *s;
	unsigned u;
	int c;

	va_start(ap, fmt);
	while ((c = *fmt++)) {
		if (c == '%') {
			c = *fmt++;
			switch (c) {
			case 'c':
				early_putc(va_arg(ap, int));
				continue;
			case 's':
				for (s = va_arg(ap, char *); *s; s++)
					early_putc(*s);
				continue;
			case 'd':       /* A lie, always prints unsigned */
			case 'u':
				u = va_arg(ap, unsigned);
				s = buf;
				do
					*s++ = '0' + u % 10U;
				while (u /= 10U);
				dumpbuf:;
				while (--s >= buf)
					early_putc(*s);
				continue;
			case 'x':
				u = va_arg(ap, unsigned);
				s = buf;
				do
					*s++ = hex[u & 0xfu];
				while (u >>= 4);
				goto dumpbuf;
			}
		}
		early_putc(c);
	}
	va_end(ap);

	return;
}
#endif

#ifdef _EPRINTF
static __inline void
dump_l2pagetable(uint32_t pta, uint32_t l1)
{
        int i;
        volatile uint32_t *pt = (volatile uint32_t*)pta;

        for (i=0; i<256;i++) {
                switch (pt[i] & 0x3) {
                        case 1:
                                eprintf("0x%x -> 0x%x 64K ",(i<<12) | l1,
                                        pt[i]&0xFFFF0000);
                                eprintf("l2pt[0x%x]=0x%x ",i, pt[i]);
                                eprintf("s=%u ",        (pt[i]>>10) &0x1);
                                eprintf("apx=%u ",      (pt[i]>> 9) &0x1);
                                eprintf("tex=%u ",      (pt[i]>>12) &0x7);
                                eprintf("ap=%u ",       (pt[i]>> 4) &0x3);
                                eprintf("c=%u ",        (pt[i]>> 3) &0x1);
                                eprintf("b=%u\n",       (pt[i]>> 2) &0x1);
                                break;
                        case 2:
                        case 3:
                                eprintf("0x%x -> 0x%x  4K ",(i<<12) | l1,
                                        pt[i]&0xFFFFF000);
                                eprintf("l2pt[0x%x]=0x%x ",i, pt[i]);
                                eprintf("s=%u ",        (pt[i]>>10) &0x1);
                                eprintf("apx=%u ",      (pt[i]>> 9) &0x1);
                                eprintf("tex=%u ",      (pt[i]>> 6) &0x7);
                                eprintf("ap=%u ",       (pt[i]>> 4) &0x3);
                                eprintf("c=%u ",        (pt[i]>> 3) &0x1);
                                eprintf("b=%u\n",       (pt[i]>> 2) &0x1);
                                break;
                }
        }
}
#endif

#ifdef _EPRINTF
static __inline void
dump_l1pagetable(uint32_t pta)
{
        int i;
        eprintf("L1 pagetable starts at 0x%x\n",pta);
        volatile uint32_t *pt = (volatile uint32_t*)pta;
        for (i=0; i<4096;i++) {
                switch (pt[i] & 0x3) {
                        case 1:
                                eprintf("0x%x ->             L2 ",i<<20);
                                eprintf("l1pt[0x%x]=0x%x ",i, pt[i]);
                                eprintf("l2desc=0x%x ",pt[i] & 0xFFFFFC00);
                                eprintf("p=%u ",(pt[i]>>9) &0x1);
                                eprintf("domain=0x%x\n",(pt[i]>>5) &0xF);
                                dump_l2pagetable(pt[i] & 0xFFFFFC00, i<<20);
                                break;
                        case 2:
                                if (pt[i] &0x40000) {
                                        eprintf("0x%x -> 0x%x 16M ",i<<20, pt[i] & 0xFF000000);
                                        eprintf("l1pt[0x%x]=0x%x ",i, pt[i]);
                                        eprintf("base=0x%x ", ((pt[i]>>24)));
                                } else {
                                        eprintf("0x%x -> 0x%x  1M ",i<<20, pt[i] & 0xFFF00000);
                                        eprintf("l1pt[0x%x]=0x%x ",i, pt[i]);
                                        eprintf("base=0x%x ", (pt[i]>>20));
                                }
                                eprintf("nG=%u ",       (pt[i]>>17) &0x1);
                                eprintf("s=%u ",        (pt[i]>>16) &0x1);
                                eprintf("apx=%u ",      (pt[i]>>15) &0x1);
                                eprintf("tex=%u ",      (pt[i]>>12) &0x7);
                                eprintf("ap=%u ",       (pt[i]>>10) &0x3);
                                eprintf("p=%u ",        (pt[i]>> 9) &0x1);
                                eprintf("domain=0x%x ", (pt[i]>> 5) &0xF);
                                eprintf("xn=%u ",       (pt[i]>> 4) &0x1);
                                eprintf("c=%u ",        (pt[i]>> 3) &0x1);
                                eprintf("b=%u\n",       (pt[i]>> 2) &0x1);
                                break;
                        case 3:
                                eprintf("pt[0x%x] 0x%x RESV\n",i, pt[i]);
                                break;
                }
        }
}

#endif


/*
 * CPSW driver debug routines
 */

#define DUMP_RXBD(p)  do {							\
		struct cpsw_cpdma_bd mybd;					\
		cpsw_cpdma_read_rxbd(p, &mybd);					\
		printf("%s: RXBD[%3u] next=0x%08x bufptr=0x%08x bufoff=0x%04x "	\
			"buflen=0x%04x pktlen=0x%04x flags=0x%04x head=%i "	\
			"tail=%i %s %s %s %s\n", __func__, p, mybd.next,	\
			mybd.bufptr, mybd.bufoff, mybd.buflen, mybd.pktlen,	\
			mybd.flags, sc->rxbd_head, sc->rxbd_tail,		\
			(mybd.flags & (1<<15)?"SOP":"   "),			\
			(mybd.flags & (1<<14)?"EOP":"   "),			\
			(mybd.flags & (1<<13)?"OWN":"   "),			\
			(mybd.flags & (1<<12)?"EOQ":"   "));			\
	} while(0)

#define DUMP_TXBD(p)  do {							\
		struct cpsw_cpdma_bd mybd;					\
		cpsw_cpdma_read_txbd(p, &mybd);					\
		printf("%s: TXBD[%3u] next=0x%08x bufptr=0x%08x bufoff=0x%04x "	\
			"buflen=0x%04x pktlen=0x%04x flags=0x%04x "		\
			"%s %s %s %s\n", __func__, p, mybd.next,		\
			mybd.bufptr, mybd.bufoff, mybd.buflen, mybd.pktlen,	\
			mybd.flags,						\
			(mybd.flags & (1<<15)?"SOP":"   "),			\
			(mybd.flags & (1<<14)?"EOP":"   "),			\
			(mybd.flags & (1<<13)?"OWN":"   "),			\
			(mybd.flags & (1<<12)?"EOQ":"   "));			\
	} while(0)
