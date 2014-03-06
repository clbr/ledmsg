#define _GNU_SOURCE

#include "lrtypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void help(const char name[]) {

	printf("\nUsage: %s -m \"message 1\" -m \"Another\"\n\n"
		"	-m msg		Message to pass. Up to 5, in order.\n"
		"	-s speed	Scrolling speed of the next message. 1-5.\n",
		name);

	exit(1);
}

int main(int argc, char **argv) {

	enum {
		MSG_MAX = 5
	};

	if (argc < 2)
		help(argv[0]);

	u32 curmsg = 0;
	u8 msg[MSG_MAX], speed[MSG_MAX];

	while (1) {
		int c = getopt(argc, argv, "hm:s:");
		if (c == -1) break;

		switch (c) {
			case 'm':
			break;
			case 's':
			break;
			default:
				help(argv[0]);
		}
	}

	if (optind != argc)
		help(argv[0]);


	return 0;
}
