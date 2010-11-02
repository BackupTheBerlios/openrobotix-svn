/*
    wiibot.c - Bebot WiiMote remote control

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
#include <stdlib.h>
#include <time.h>

#include <bluetooth/bluetooth.h>
#include <cwiid.h>

#include <bebot.h>

#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define limit(a,b, c) min(max(a,b), c)

/* 00:19:1D:DA:E2:03 */

void set_led(int value)
{
	FILE *file;

	file = fopen("/sys/class/leds/bebot\:red\:error/brightness", "w");
	fprintf(file, "%d\n", value);
	fclose(file);
}

int main(int argc, char **argv)
{
	cwiid_wiimote_t *wiimote;
	struct cwiid_state state;
	struct acc_cal acc_cal;
	bdaddr_t bdaddr;
	struct bebot bebot;
	int buttons, speed = 150;
	int ahead, turn, left, right;
	int i;
	struct timespec t;

	while (1) {
		if (argc > 1) {
			str2ba(argv[1], &bdaddr);
		} else {
			bdaddr = *BDADDR_ANY;
		}

		set_led(1);
		printf("Put Wiimote in discoverable mode now (press 1+2)...\n");
	        while(!(wiimote = cwiid_open(&bdaddr, 0)));
		set_led(0);

		if (bebot_init(&bebot) < 0) {
			printf("Unable to init bebot\n");
			exit(1);
		}

		cwiid_set_rpt_mode(wiimote, CWIID_RPT_ACC | CWIID_RPT_BTN);

		cwiid_set_led(wiimote, CWIID_LED1_ON);

		cwiid_set_rumble(wiimote, 0);

		cwiid_get_state(wiimote, &state);

		cwiid_get_acc_cal(wiimote, CWIID_EXT_NONE, &acc_cal);

		while (!(state.buttons & CWIID_BTN_HOME) && bebot_poll(&bebot, -1) > 0) {
		       cwiid_get_state(wiimote, &state);
		       bebot_update(&bebot);

#if 0
			rumble = 0;
			for (i = 0; i < BEBOT_BRIGHTNESS_COUNT; i++) {
				if (bebot_get_brightness(&bebot, i) > 200)
					rumble = 1;
			}
			cwiid_set_rumble(wiimote, rumble);
#endif

			if (state.buttons & ~buttons & CWIID_BTN_PLUS)
				speed = min(speed + 50, 300);

			if (state.buttons & ~buttons & CWIID_BTN_MINUS)
				speed = max(speed - 50, 50);

			buttons = state.buttons;

			if (state.buttons & CWIID_BTN_B) {
				ahead = limit(-10,state.acc[CWIID_Y] - acc_cal.zero[CWIID_Y], 10);
				turn = limit(-10, state.acc[CWIID_X] - acc_cal.zero[CWIID_X], 10);
//				printf("Acc: x=%d y=%d z=%d\n", state.acc[CWIID_X],
//				       state.acc[CWIID_Y], state.acc[CWIID_Z]);
			} else {
				if (state.buttons & CWIID_BTN_UP)
					ahead = 5;
				else if (state.buttons & CWIID_BTN_DOWN)
					ahead = -5;
				else
					ahead = 0;
				if (state.buttons & CWIID_BTN_RIGHT)
					turn = 5;
				else if (state.buttons & CWIID_BTN_LEFT)
					turn = -5;
				else
					turn = 0;
			}
//			printf("ahead: %d - turn: %d\n", ahead, turn);

			left = limit(-300, ahead * speed / 10 + turn * speed / 15, 300);
			right = limit(-300, ahead * speed / 10 - turn * speed / 15, 300);

//			printf("left: %d - right: %d\n", left, right);

			bebot_set_speed(&bebot, left, right);
	
			t.tv_sec = 0;
			t.tv_nsec = 50000000;
			nanosleep(&t, NULL);
		}

		bebot_release(&bebot);
		cwiid_close(wiimote);
	}

	return 0;
}
