/*
    avoid.c - Bebot avoid obstacle

    Copyright (C) 2010
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
#include <bebot.h>

#define SCALE	20

int avoid(struct bebot *bebot)
{
	int scale[2][12] = {{ -8, -4, -2, -1,  0,  0,  0,  0, -1, -2, -4, -8},
			    { -8, -4, -2, -1,  0,  0,  0,  0,  1,  2,  4,  8}};  
	int i, value, x, y, z, alpha, right, left;

	x = 3000;
	y = 0;

	for (i = 0; i < BEBOT_BRIGHTNESS_COUNT; i++) {
		value = bebot_get_brightness(bebot, i);

		if (value < 150 || value > 1000)
			value = 0;

		x += scale[0][i] * value;
		y += scale[1][i] * value;
	}

	if (x > 0)
		z = x / SCALE;
	else
		z = 0;

	alpha = y / SCALE;

	left = z - alpha;
	right = z + alpha;

//	printf("x %5i : y %5i - z %3i : a %3i - left %3i : right %3i\n", x, y, z, alpha, left, right);
	bebot_set_speed(bebot, left, right);
}


int main(int argc, char *argv[])
{
	struct bebot bebot;
	int run = 0;

	bebot_init(&bebot);

	while(bebot_poll(&bebot, -1) > 0) {
		if (bebot_update(&bebot) > 0) {
			if (run) {
				if (bebot_get_brightness(&bebot, 0) > 650 &&
				    bebot_get_brightness(&bebot, 11) > 650) {
					bebot_set_speed(&bebot, 0, 0);
					run = 0;
				} else {
					avoid(&bebot);
				}
			} else {
				if (bebot_get_brightness(&bebot, 5) > 650 &&
				    bebot_get_brightness(&bebot, 6) > 650) {
					run = 1;
				}
			}
		}
	}

	bebot_release(&bebot);
}
