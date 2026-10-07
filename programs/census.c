/* census -- print the service's errand counter and ask the VMM for its slow-exit census (outb to 0x505). Root only. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include "libErrand.h"
#include "tlskey_abi.h"

int
main(int argc, char **argv)
{
	unsigned int tag = argc > 1 ? (unsigned int)strtoul(argv[1], NULL, 0) : 0;

	/* the service's errand count first (one more errand, itself included), then the VMM's census */
	printf("census[%u]: tlskey errands served %lu\n", tag, errand_call(TLSKEY_SLOT, TLSKEY_OP_COUNTERS, 0, 0, 0));
	if (iopl(3)) { perror("iopl"); return 1; }
	outb((unsigned char)tag, TLSKEY_CENSUS_PORT);

	return 0;
}
