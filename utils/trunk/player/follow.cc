/*
    follow.c - Bebot camera based follower

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

/*
colors.txt
[Colors]
(  0,   0,   0) 0.0 10 Black

[Thresholds]
(  0: 40,   0:250,   0:255)

bebot_cam.cfg
driver
(
  name "cmvision"
  provides ["blobfinder:0"]
  requires ["camera:1"]
  colorfile "colors.txt"
)
*/
#include <stdio.h>
#include <iostream>
#include <time.h>
#include <math.h>
#include <signal.h>

#include <libplayerc++/playerc++.h>

using namespace PlayerCc;

/* datamode PLAYER_DATAMODE_PUSH or PLAYER_DATAMODE_PULL */
#define MODE	PLAYER_DATAMODE_PUSH

#undef DEBUG

static int running = 1;
static PlayerClient *client;
static Position2dProxy *position;
static IrProxy *ir;
static BlobfinderProxy *blobfinder;
static double speed = 0.0, turnrate = 0.3;
static int count;

double inline limit(double value, double vmin, double vmax)
{
	return min(max(vmin, value), vmax);
}

void stop(int sig)
{
	running = 0;
}

void avoid(void)
{
	speed = (min(min(ir->GetRange(0), ir->GetRange(1)),
		     min(ir->GetRange(count - 2), ir->GetRange(count - 1))
		     ) - 0.12) * 3;

	turnrate = (ir->GetRange(0) - ir->GetRange(count - 1)) * 20
		 + (ir->GetRange(1) - ir->GetRange(count - 2)) * 10
		 + (ir->GetRange(3) - ir->GetRange(count - 4)) * 5;

	turnrate = (speed < 0.01 && fabs(turnrate) < 0.1) ? 0.3 : turnrate;

#ifdef DEBUG
	printf("<< speed s: %f  t: %f >>\n", speed, turnrate);
#endif
}

void follow(void)
{
	if (blobfinder->GetCount()) {
		int index = 0;
		for (int i = 1; i < blobfinder->GetCount(); i++) {
			if (blobfinder->GetBlob(i).y > 120)
				if (blobfinder->GetBlob(i).area >
				    blobfinder->GetBlob(index).area)
					index = i;
		}

		static int xt = 0, at = 13000;
		int d, a;

		if (blobfinder->GetBlob(index).y > 120 &&
		    blobfinder->GetBlob(index).area > 400) {
			int x = blobfinder->GetWidth() / 2
			      - blobfinder->GetBlob(index).x;
			int dx = x - xt;

			turnrate = static_cast<double>(x) / 125.0;
#ifdef DEBUG
			printf("turnrate %f (%i - %i)\n", turnrate, x, dx);
#endif		
//			turnrate = static_cast<double>(x) / 125.0
//			         + static_cast<double>(dx) / 85.0;
			xt = x;

			if (ir->GetRange(0) > 0.1 &&
			    ir->GetRange(1) > 0.08 &&
			    ir->GetRange(count - 2) > 0.08 &&
			    ir->GetRange(count - 1) > 0.1) {
				int area = (3 * at + blobfinder->GetBlob(index).area) / 4;
#ifdef DEBUG
				printf("area %i\n", area);
#endif
				at = area;
				speed = (13000.0 - static_cast<double>(at)) / 22500.0;
			} else {
				speed = 0.0;
			}
		} else {
			speed = 0.0;
		}
	} else {
		speed = 0.0;
	}
}

void run(void)
{
	printf("Starting follow robot\n");

	client->Read();

	ir->RequestGeom();

	count = ir->GetPoseCount();

	while(running) {
		if ((MODE == PLAYER_DATAMODE_PUSH) || client->Peek()) {
			client->Read();

			if (ir->GetCount() == 0)
				continue;

			if (!blobfinder)
				avoid();
			else
				follow();

			speed = limit(speed, 0.0, 0.05);
			turnrate = limit(turnrate, -M_PI / 4.0, M_PI / 4.0);
#ifdef DEBUG
			printf("<< speed s: %f  t: %f >>\n", speed, turnrate);
#endif
			position->SetSpeed(speed, turnrate);
		}
	}

	position->SetSpeed(0.0, 0.0);
	usleep(500*1000);
}

int main(int argc, char *argv[])
{
       char *host = "127.0.0.1";
       int port = 6665;

       if (argc > 1)
		host = argv[1];
       
       if (argc > 2)
		port = atoi(argv[2]);

	// handle user interupt (CTRL+C)
	signal(SIGINT,stop);

	printf("Connect to device %s:%i\n", host, port);
	try {
		client = new PlayerClient(host, port);
		client->SetDataMode(MODE);
		client->SetReplaceRule(true, PLAYER_MSGTYPE_DATA , 1, -1);

		position = new Position2dProxy(client, 0);
		ir = new IrProxy(client, 0);
	}
	catch(...) {
		puts("Unhandled exception during connect!\n");
		exit(1);
	}

	try {
		blobfinder = new BlobfinderProxy(client, 0);

		/* fix camera bug */
		usleep(500*1000);
		delete blobfinder;
		usleep(500*1000);
		blobfinder = new BlobfinderProxy(client, 0);
	}
	catch(...) {
		puts("No Blobfinder!\n");
		blobfinder = NULL;
	}

	run();

	printf ("Disconnect from device\n");
	try {
		delete position;
		delete ir;
		delete client;
	}
	catch(...) {
		puts("Unhandled exception during disconnect!\n");
		exit(1);
	}

	return 0;
}

