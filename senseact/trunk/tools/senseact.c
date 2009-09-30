#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/senseact.h>

static char *device;

static void usage(int argc, char **argv)
{
	printf("Usage: %s [options]\n\n"
	       "Version 0.1\n"
	       "Options:\n"
	       "-d | --device name   Senseact device name [%s]\n"
	       "-h | --help          Print this message\n"
	       "-r | --read          Read from the device [default]\n"
	       "-w | --write         Write to the device\n"
	       "-t | --type          Set action type\n"
	       "-i | --index         Set action index\n"
	       "-v | --value         Set action value\n"
	       "",
	       argv[0], device);
}

static void print(struct senseact_action *action)
{
	char *type[SENSEACT_TYPE_CNT] = {
		"sync",
		"brightness",
		"enable",
		"speed",
		"position",
		"angle",
		"increment",
		"unknown",
	};

	char *prefix[16] = {
		"",
		"k",
		"M",
		"G",
		"T",
		"P",
		"E",
		"Z",
		"y",
		"z",
		"a",
		"f",
		"p",
		"n",
		"u",
		"m",
	};

	if (action->type > SENSEACT_TYPE_MAX)
		return;

	printf("%s%i = %i %s\n",
	       type[action->type],
	       action->index,
	       action->value,
	       prefix[action->prefix & 0xf]);
}

static const char short_options[] = "d:hrwt:i:v:";

static const struct option long_options[] = {
	{ "device", required_argument, NULL, 'd' },
	{ "help",   no_argument,       NULL, 'h' },
	{ "read",   no_argument,       NULL, 'r' },
	{ "write",  no_argument,       NULL, 'w' },
	{ "type",   required_argument, NULL, 't' },
	{ "index",  required_argument, NULL, 'i' },
	{ "value",  required_argument, NULL, 'v' },
	{ 0, 0, 0, 0 }
};

int main(int argc, char **argv)
{
	int fd, i, n;
	int dir = 1;
	struct senseact_action actions[20];

	device = "/dev/senseact0";

	for (;;) {
		int idx;
		int c;

		c = getopt_long(argc, argv,
				short_options, long_options, &idx);

		if (c == -1)
			break;

		switch (c) {
		case 0: /* getopt_long() flag */
			break;

		case 'd':
			device = optarg;
			break;

		case 'h':
			usage(argc, argv);
			exit(EXIT_SUCCESS);

		case 'r':
			dir = 1;
			break;

		case 'w':
			dir = 0;
			break;

		case 't':
			actions[0].type = strtol(optarg, NULL, 0);
			break;

		case 'i':
			actions[0].index = strtol(optarg, NULL, 0);
			break;

		case 'v':
			actions[0].value = strtol(optarg, NULL, 0);
			break;

		default:
			usage(argc, argv);
			exit(EXIT_FAILURE);
		}
	}

	fd = open(device, O_RDWR);
	if (fd <= 0)
		exit(EXIT_FAILURE);

	if (dir == 0) {
		print(&actions[0]);
		n = write(fd, &actions, sizeof(struct senseact_action));
	} else {
		do {
			n = read(fd, &actions, 20 * sizeof(struct senseact_action));

			for (i = 0; i < (n / sizeof(struct senseact_action)); i++)
			{
				print(&actions[i]);
			}
		} while (n > 0);
	}

	close(fd);
	return 0;
}
