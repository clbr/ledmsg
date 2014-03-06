/*
Copyright (C) 2014 Lauri Kasanen

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, version 3 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE

#include "lrtypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

static void die(const char msg[], ...) {

	va_list ap;
	va_start(ap, msg);

	vfprintf(stderr, msg, ap);

	va_end(ap);
	exit(1);
}

static void help(const char name[]) {

	die("\nUsage: %s -m \"message 1\" -m \"Another\"\n\n"
		"	-m msg		Message to pass. Up to 6, in order.\n"
		"	-s speed	Scrolling speed of the next message. 1-5. Default 5.\n"
		"	-e effect	Effect of the next message. Default scroll.\n"
		"			Effects: hold, scroll, snow, flash, frame.\n"
		"\n"
		"	-d device	Use device instead of /dev/ttyUSB0.\n"
		"	-w wait		Wait this long between packets, default 200ms.\n"
		"\n"
		"Example: %1$s -s2 -m \"Slow\" -s5 -m \"Fast message\"\n\n",
		name);
}

static void rates(const int fd, const u8 parity, const u32 speed) {

	struct termios tty;
	memset(&tty, 0, sizeof(struct termios));
	if (tcgetattr(fd, &tty) != 0)
		die("tcgetattr\n");

	cfsetspeed(&tty, speed);

	if (parity)
		tty.c_cflag |= PARENB;
	else
		tty.c_cflag &= ~PARENB;

	if (tcsetattr(fd, TCSANOW, &tty) != 0)
		die("tcsetattr");
}

int main(int argc, char **argv) {

	enum {
		MSG_MAX = 6
	};

	if (argc < 2)
		help(argv[0]);

	u32 cur = 0;
	u32 wait = 200000;
	u8 msg[MSG_MAX][256], speed[MSG_MAX], effect[MSG_MAX];
	const char *dev = strdup("/dev/ttyUSB0");

	u32 i;
	for (i = 0; i < MSG_MAX; i++) {
		memset(msg[i], 0, 256);
		speed[i] = 5;
		effect[i] = 'B';
	}

	while (1) {
		int c = getopt(argc, argv, "hm:s:w:e:");
		if (c == -1) break;
		u32 len;

		switch (c) {
			case 'm':
				len = strlen(optarg);
				if (len >= 250)
					die("Message %u too long (%u)\n", cur, len);
				memcpy(msg[cur] + 4, optarg, len);
				msg[cur][3] = len;
				cur++;
				if (cur > MSG_MAX)
					die("Too many messages\n");
			break;
			case 's':
				speed[cur] = atoi(optarg);
				if (speed[cur] > 5)
					die("Speed %u too high\n", speed[cur]);
			break;
			case 'd':
				free((char *) dev);
				dev = strdup(optarg);
			break;
			case 'w':
				wait = atoi(optarg) * 1000;
			break;
			case 'e':
				#define arg(a) if (!strcmp(optarg, a))

				arg("hold")
					effect[cur] = 'A';
				else arg("scroll")
					effect[cur] = 'B';
				else arg("snow")
					effect[cur] = 'C';
				else arg("flash")
					effect[cur] = 'D';
				else arg("frame")
					effect[cur] = 'E';
				else die("Unknown effect %s\n", optarg);

				#undef arg
			break;
			default:
				help(argv[0]);
		}
	}

	if (optind != argc)
		help(argv[0]);

	const int fd = open(dev, O_WRONLY);
	if (fd < 0)
		die("Failed to open %s (%s)\n", dev, strerror(errno));

	rates(fd, 0, B38400);

	// Build each message
	for (i = 0; i < cur; i++) {
		msg[i][0] = speed[i] + '0';
		msg[i][1] = i + 1 + '0';
		msg[i][2] = effect[i];
	}

	/*
        HOLD      = 'A'
        SCROLL    = 'B'
        SNOW      = 'C'
        FLASH     = 'D'
        HOLDFRAME = 'E'
	*/

	// Send the initialization byte
	i = 0;
	write(fd, &i, 1);
	usleep(wait);

	// Send each message as 69-byte packets
	for (i = 0; i < cur; i++) {
		u32 j;
		for (j = 0; j < 4; j++) {
			u8 packet[69];
			packet[0] = 2;
			packet[1] = 0x31;
			packet[2] = 6 + i;
			packet[3] = j * 64;
			memcpy(packet + 4, msg[i] + j*64, 64);

			// Checksum
			u32 k;
			u32 sum = 0;
			for (k = 1; k < 68; k++)
				sum += packet[k];

			packet[68] = sum % 256;

			write(fd, packet, 69);

			usleep(wait);
		}

		usleep(wait);
	}

	// Bye message
	u8 end[3];
	end[0] = 2;
	end[1] = 0x33;
	switch (cur) {
		case 1:
			end[2] = 1;
		break;
		case 2:
			end[2] = 3;
		break;
		case 3:
			end[2] = 7;
		break;
		case 4:
			end[2] = 15;
		break;
		case 5:
			end[2] = 0x1f;
		break;
		case 6:
			end[2] = 0x3f;
		break;
	}
	write(fd, end, 3);

	close(fd);
	free((char *) dev);
	return 0;
}
