/* census -- ask the VMM to print its slow-exit census: outb(tag, 0x505). Root only (iopl). */
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include "tlskey_abi.h"

int
main(int argc, char **argv)
{
	unsigned int tag = argc > 1 ? (unsigned int)strtoul(argv[1], NULL, 0) : 0;

	if (iopl(3)) { perror("iopl"); return 1; }
	outb((unsigned char)tag, TLSKEY_CENSUS_PORT);
	printf("census: dump requested, tag %u\n", tag);

	return 0;
}
