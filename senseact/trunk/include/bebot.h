/*
    bebot.h - BeBot robot control interface

    Copyright (C) 2008
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

#ifndef BEBOT_H
#define BEBOT_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <poll.h>
#include <linux/senseact.h>

#define BEBOT_FD_COUNT			3
#define BEBOT_ACTION_COUNT		16

#define BEBOT_BRIGHTNESS_COUNT		12
#define BEBOT_SPEED_COUNT		4
#define BEBOT_POSITION_COUNT		2
#define BEBOT_ANGLE_COUNT		1
#define BEBOT_INCREMENT_COUNT		2

struct bebot {
	int fd[BEBOT_FD_COUNT];
	int brightness[BEBOT_BRIGHTNESS_COUNT];
	int speed[BEBOT_SPEED_COUNT];
	int position[BEBOT_POSITION_COUNT];
	int angle[BEBOT_ANGLE_COUNT];
	int increment[BEBOT_INCREMENT_COUNT];
};

static inline int bebot_init(struct bebot *bebot)
{
	char *name[BEBOT_FD_COUNT] = { "/dev/senseact/base", "/dev/senseact/ir0", "/dev/senseact/ir1" };;
	int i, j;

	bzero(bebot, sizeof(struct bebot));

	for (i = 0; i < BEBOT_FD_COUNT; i++) {
		bebot->fd[i] = open(name[i], O_RDWR | O_NONBLOCK);
		if (bebot->fd[i] == -1) {
			for (j = 0; j < i; j++)
				close(bebot->fd[j]);
			return -1;
		}
	}

	return 0;
}

static inline int bebot_release(struct bebot *bebot)
{
	int i;

	for (i = 0; i < BEBOT_FD_COUNT; i++)
		close(bebot->fd[i]);

	return 0;
}

static inline int bebot_update(struct bebot *bebot)
{
	static int brightness[BEBOT_BRIGHTNESS_COUNT];
	static int speed[BEBOT_SPEED_COUNT];
	static int position[BEBOT_POSITION_COUNT];
	static int angle[BEBOT_ANGLE_COUNT];
	static int increment[BEBOT_INCREMENT_COUNT];
	struct senseact_action actions[BEBOT_ACTION_COUNT];
	int i, j, n, offset, rc = 0;

	for (i = 0; i < BEBOT_FD_COUNT; i++) {
		n = read(bebot->fd[i], (void*)actions,
			 BEBOT_ACTION_COUNT * sizeof(struct senseact_action));

		offset = (i != 2) ? 0 :  BEBOT_BRIGHTNESS_COUNT / 2;

		if (n == -1)
			if (errno == EAGAIN)
				continue;
			else
				return -1;

		for (j = 0; j < n / sizeof(struct senseact_action); j++) {
			switch (actions[j].type) {
			case SENSEACT_TYPE_SPEED:
				if (actions[j].index < BEBOT_SPEED_COUNT)
					speed[actions[j].index] = actions[j].value;
				break;
			case SENSEACT_TYPE_POSITION:
				if (actions[j].index < BEBOT_POSITION_COUNT)
					position[actions[j].index] = actions[j].value;
				break;
			case SENSEACT_TYPE_ANGLE:
				if (actions[j].index < BEBOT_ANGLE_COUNT)
					angle[actions[j].index] = actions[j].value;
				break;
			case SENSEACT_TYPE_INCREMENT:
				if (actions[j].index < BEBOT_INCREMENT_COUNT)
					increment[actions[j].index] = actions[j].value;
				break;
			case SENSEACT_TYPE_BRIGHTNESS:
				if ((offset + actions[j].index) < BEBOT_BRIGHTNESS_COUNT)
					brightness[offset + actions[j].index] = actions[j].value;		
				break;
			case SENSEACT_TYPE_SYNC:
				if (actions[j].index == SENSEACT_SYNC_SENSOR) {
					rc |= 1 << i;

					if (i == 0) {
						memcpy(bebot->speed, speed, sizeof(bebot->speed)); 
						memcpy(bebot->position, position, sizeof(bebot->position)); 
						memcpy(bebot->angle, angle, sizeof(bebot->angle)); 
						memcpy(bebot->increment, increment, sizeof(bebot->increment)); 
					} else {
						memcpy(bebot->brightness + offset,
						       brightness + offset,
						       sizeof(bebot->brightness) / 2);
					}
				}
				break;
			}
		}
	}

	return rc;
}

static inline int bebot_poll(struct bebot *bebot, int timeout)
{
	struct pollfd fds[BEBOT_FD_COUNT];
	int i;

	for (i = 0; i < BEBOT_FD_COUNT; i++) {
		fds[i].fd = bebot->fd[i];
		fds[i].events = POLLIN;
	}

	return poll(fds, BEBOT_FD_COUNT, timeout);
}

static inline int bebot_set_speed(struct bebot *bebot, int left, int right)
{
	struct senseact_action actions[2];
	int rc;

	actions[0].type = SENSEACT_TYPE_SPEED;
	actions[0].index = 0;
	actions[0].value = left;

	actions[1].type = SENSEACT_TYPE_SPEED;
	actions[1].index = 1;
	actions[1].value = right;

	rc = write(bebot->fd[0], (void*)actions, 2 * sizeof(struct senseact_action));

	if (rc != 2 * sizeof(struct senseact_action))
		return -1;

	return 0;
}

static inline int bebot_get_brightness(struct bebot *bebot, unsigned int i)
{
	if (i < BEBOT_BRIGHTNESS_COUNT)
		return bebot->brightness[i];
	return 0;
}

static inline int bebot_get_speed(struct bebot *bebot, unsigned int i)
{
	if (i < BEBOT_SPEED_COUNT)
		return bebot->speed[i];
	return 0;
}

static inline int bebot_get_speed_left(struct bebot *bebot)
{
	return bebot_get_speed(bebot, 2);
}

static inline int bebot_get_speed_right(struct bebot *bebot)
{
	return bebot_get_speed(bebot, 3);
}

static inline int bebot_get_position(struct bebot *bebot, unsigned int i)
{
	if (i < BEBOT_POSITION_COUNT)
		return bebot->speed[i];
	return 0;
}

static inline int bebot_get_position_x(struct bebot *bebot)
{
	return bebot_get_position(bebot, 0);
}

static inline int bebot_get_position_y(struct bebot *bebot)
{
	return bebot_get_position(bebot, 01);
}

static inline int bebot_get_angle(struct bebot *bebot, unsigned int i)
{
	if (i < BEBOT_ANGLE_COUNT)
		return bebot->angle[i];
	return 0;
}

static inline int bebot_get_angle_alpha(struct bebot *bebot)
{
	return bebot_get_angle(bebot, 0);
}

static inline int bebot_get_increment(struct bebot *bebot, unsigned int i)
{
	if (i < BEBOT_INCREMENT_COUNT)
		return bebot->increment[i];
	return 0;
}

static inline int bebot_get_increment_left(struct bebot *bebot)
{
	return bebot_get_increment(bebot, 0);
}

static inline int bebot_get_increment_right(struct bebot *bebot)
{
	return bebot_get_increment(bebot, 1);
}

#endif /* BEBOT_H */
