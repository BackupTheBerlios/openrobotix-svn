/*
    fpga.c - FPGA demo

    Copyright (C) 2009
    Heinz Nixdorf Institute - University of Paderborn
    Department of System and Circuit Technology
    Stefan Herbrechtsmeier <hbmeier@hni.uni-paderborn.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void usage(int argc, char **argv, char *device)
{
	printf("Usage: %s ADDR [VALUE]\n"
		"Read or write VALUE at ADDR on %s.\n\n",
		argv[0], device);
}

int main(int argc, char *argv[])
{
	char *device = "/dev/fpga";
	unsigned int value, addr;
	int rc, fd, c, index;

	if (argc > 1) {
		if (argc > 2)
			value = strtol(argv[2], NULL, 0);
		addr = strtol(argv[1], NULL, 0);
	} else {
		usage(argc, argv, device);
		exit(1);
	}

	/* open fpga device */
	fd = open(device, O_RDWR);
	if (fd < 0) {
		printf("Can't open device %s\n", device);	  
		perror(strerror(errno));
		exit(1);
	}

	if(argc > 2) {
		/* write stream*/
		rc = pwrite(fd, &value, 4, addr);
		if (rc != 4) {
			printf("Can't write value 0x%08x at addr 0x%08x - %d\n",
			       value, addr, rc);
			exit(1);
		}
	} else {
		/* read stream */
		rc = pread(fd, &value, 4, addr);
		if (rc != 4) {
			printf("Can't read at addr 0x%08x - %d\n",
			       addr, rc);
			exit(1);
		}
		printf("0x%08x\n", value);
	}

	/* close fpga */
	close(fd);

	return 0;
}


