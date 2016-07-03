#include <sys/param.h>
#include "isa_stdports.h"
#include "rtc.h"

#define	ISA "/dev/isa"
u_char rtcin(u_char  adr);

/* convert 2 digit BCD number */
bcd(i)
int i;
{
	return ((i >> 4) * 10 + (i & 0xF));
}

main() {
	int isa;

	/* allow I/O instructions */
	if ((isa = open (ISA, 2)) < 0) {
		perror("cmos:");
		return (1);
	}

	getdate();
	return (0);
}


getdate() {
	int statusa;
	/*
	 * Synchonize on edge to gaurantee correspondance with internal
	 * RTC counters and (to be set) kernel interrupt interval timer.
	 * (also, time is gauranteed to be consistant)
	 */

	do
		statusa = rtcin(RTC_STATUSA);
	while ((statusa & RTCSA_UIP) == 0);
	do
		statusa = rtcin(RTC_STATUSA);
	while ((statusa & RTCSA_UIP) != 0);
	

	/* year */
	printf("%02d%02d", bcd(rtcin(RTC_CENTURY)), bcd(rtcin(RTC_YEAR)));

	/* month & day */
	printf("%02d%02d", bcd(rtcin(RTC_MONTH)), bcd(rtcin(RTC_DAY)));

	/* hours & minutes */
	printf("%02d%02d", bcd(rtcin(RTC_HRS)), bcd(rtcin(RTC_MIN)));

	/* seconds */
	printf(".%02d\n", bcd(rtcin(RTC_SEC)));
}

/*
 * Read RTC atomically
 */
u_char
rtcin(u_char  adr)
{
	int x;

	/*
	 * Some RTC's (dallas smart watch) attempt to foil unintentioned
	 * operation of the RTC by requiring back-to-back i/o instructions
	 * to reference the RTC; any interviening i/o operations cancel
	 * the reference to the clock (e.g. the NOP).
	 */
	asm volatile ("out%z0 %b0, %2 ; xorl %0, %0 ; in%z0 %3, %b0"
		: "=a"(adr) : "0"(adr), "i"(IO_RTC), "i"(IO_RTC + 1));
	return (adr);
}

#ifdef notyet
/*
 * Write RTC atomically.
 */
void
rtcout(u_char adr, u_char val)
{
	int x;

	/*
	 * Some RTC's (dallas smart watch) attempt to foil unintentioned
	 * operation of the RTC by requiring back-to-back i/o instructions
	 * to reference the RTC; any interviening i/o operations cancel
	 * the reference to the clock (e.g. the NOP).
	 */
	asm volatile ("out%z0 %b0, %2 ; movb %1, %b0 ; out%z0 %b0, %3"
		:: "a"(adr), "g"(val), "i"(IO_RTC), "i"(IO_RTC+1));
}
#endif
