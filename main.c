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
		"	-m msg		Message to pass. Up to 5, in order.\n"
		"	-s speed	Scrolling speed of the next message. 1-5. Default 5.\n"
		"	-d device	Use device instead of /dev/ttyUSB0.\n",
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
}

int main(int argc, char **argv) {

	enum {
		MSG_MAX = 5
	};

	if (argc < 2)
		help(argv[0]);

	u32 cur = 0;
	u8 msg[MSG_MAX][256], speed[MSG_MAX];
	const char *dev = strdup("/dev/ttyUSB0");

	u32 i;
	u8 tmp;
	for (i = 0; i < MSG_MAX; i++) {
		memset(msg[i], 0, 256);
		speed[i] = 5;
	}

	while (1) {
		int c = getopt(argc, argv, "hm:s:");
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
			default:
				help(argv[0]);
		}
	}

	if (optind != argc)
		help(argv[0]);

	const int fd = open(dev, O_WRONLY);
	if (fd < 0)
		die("Failed to open %s (%s)\n", dev, strerror(errno));

	rates(fd, 1, B38400);

	// Build each message
	for (i = 0; i < cur; i++) {
		msg[i][0] = speed[i] + '0';
		msg[i][1] = i + 1 + '0';
		msg[i][2] = 'B';
	}

	// Send the initialization byte
	i = 0;
	write(fd, &i, 1);

	// Send each message as 69-byte packets
	for (i = 0; i < cur; i++) {
		u32 j;
		for (j = 0; j < 4; j++) {
			u8 packet[69];
			packet[0] = 2;
			packet[1] = 0x31;
			packet[2] = 6;
			packet[3] = j * 64;
			memcpy(packet + 4, msg[i] + j*64, 64);

			// Checksum
			u32 k;
			u32 sum = 0;
			for (k = 1; k < 68; k++)
				sum += packet[k];

			packet[68] = sum % 256;

			write(fd, packet, 69);
		}
	}

	// Bye message
	tmp = 2;
	write(fd, &tmp, 1);
	tmp = 0x33;
	write(fd, &tmp, 1);
	switch (cur) {
		case 1:
			tmp = 1;
		break;
		case 2:
			tmp = 3;
		break;
		case 3:
			tmp = 7;
		break;
		case 4:
			tmp = 15;
		break;
		case 5:
			tmp = 0x1f;
		break;
	}
	write(fd, &tmp, 1);

	close(fd);
	free((char *) dev);
	return 0;
}
