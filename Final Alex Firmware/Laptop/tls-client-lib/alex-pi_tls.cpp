
// Routines to create a TLS client
#include "make_tls_client.h"

// Network packet types
#include "netconstants.h"

// Packet types, error codes, etc.
#include "constants.h"

//Addtional libraries from Alex_working
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <ncurses.h>
#include "unistd.h"

int mode = 0;

char d = 'a';
char command = 'x';
char prevcommand = 'x';
char finalcommand = 'x';
int count = 0;
int state = 0;
int commandflag = 0;
int ok_flag = 1;
int quit = 0;

// Tells us that the network is running.
static volatile int networkActive = 0;

void handleError(const char* buffer)
{
	switch (buffer[1])
	{
	case RESP_OK:
		ok_flag = 1;
		printf("Command / Status OK\n");
		break;

	case RESP_BAD_PACKET:
		printf("BAD MAGIC NUMBER FROM ARDUINO\n");
		break;

	case RESP_BAD_CHECKSUM:
		printf("BAD CHECKSUM FROM ARDUINO\n");
		break;

	case RESP_BAD_COMMAND:
		printf("PI SENT BAD COMMAND TO ARDUINO\n");
		break;

	case RESP_BAD_RESPONSE:
		printf("PI GOT BAD RESPONSE FROM ARDUINO\n");
		break;

	default:
		printf("PI IS CONFUSED!\n");
	}
}

void handleStatus(const char* buffer)
{
	int32_t data[16];
	memcpy(data, &buffer[1], sizeof(data));

	printf("\n ------- ALEX STATUS REPORT ------- \n\n");
	printf("Left Forward Ticks:\t\t%d\n", data[0]);
	printf("Right Forward Ticks:\t\t%d\n", data[1]);
	printf("Left Reverse Ticks:\t\t%d\n", data[2]);
	printf("Right Reverse Ticks:\t\t%d\n", data[3]);
	printf("Left Forward Ticks Turns:\t%d\n", data[4]);
	printf("Right Forward Ticks Turns:\t%d\n", data[5]);
	printf("Left Reverse Ticks Turns:\t%d\n", data[6]);
	printf("Right Reverse Ticks Turns:\t%d\n", data[7]);
	printf("Forward Distance:\t\t%d\n", data[8]);
	printf("Reverse Distance:\t\t%d\n", data[9]);
	printf("\n---------------------------------------\n\n");
}

void handleMessage(const char* buffer)
{
	printf("MESSAGE FROM ALEX: %s\n", &buffer[1]);
}

void handleCommand(const char* buffer)
{
	// We don't do anything because we issue commands
	// but we don't get them. Put this here
	// for future expansion
}

void handleNetwork(const char* buffer, int len)
{
	// The first byte is the packet type
	int type = buffer[0];

	switch (type)
	{
	case NET_ERROR_PACKET:
		handleError(buffer);
		break;

	case NET_STATUS_PACKET:
		printf("handling status\n");
		handleStatus(buffer);
		break;

	case NET_MESSAGE_PACKET:
		handleMessage(buffer);
		break;

	case NET_COMMAND_PACKET:
		printf("sent command\n");
		handleCommand(buffer);
		break;
	}
}

void sendData(void* conn, const char* buffer, int len)
{
	int c;
	printf("\nSENDING %d BYTES DATA\n\n", len);
	if (networkActive)
	{
		/* TODO: Insert SSL write here to write buffer to network */

		c = sslWrite(conn, buffer, len);
		ok_flag = 0;
		/* END TODO */
		networkActive = (c > 0);
	}
}

void* readerThread(void* conn)
{
	char buffer[128];
	int len;

	while (networkActive)
	{
		/* TODO: Insert SSL read here into buffer */

		printf("read %d bytes from server.\n", len);
		len = sslRead(conn, buffer, sizeof(buffer));

		/* END TODO */

		networkActive = (len > 0);

		if (networkActive)
			handleNetwork(buffer, len);
	}

	printf("Exiting network listener thread\n");

	/* TODO: Stop the client loop and call EXIT_THREAD */
	stopClient();
	EXIT_THREAD(conn);
	/* END TODO */
}

void flushInput()
{
	char c;

	while ((c = getchar()) != '\n' && c != EOF);
}

void getParams(int32_t* params)
{
	printf("Enter distance/angle in cm/degrees (e.g. 50) and power in %% (e.g. 75) separated by space.\n");
	printf("E.g. 50 75 means go at 50 cm at 75%% power for forward/backward, or 50 degrees left or right turn at 75%%  power\n");
	scanf("%d %d", &params[0], &params[1]);
	flushInput();
}


//Transferred from alex_working
void getParamsAuto(char dir, int32_t* params)
{
	switch (dir) {
	case 'f':
		params[0] = 0;
		params[1] = 65;
		break;
	case 'b':
		params[0] = 0;
		params[1] = 65;
		break;
	case 'l':
		params[0] = 5;
		params[1] = 85;
		break;
	case 'r':
		params[0] = 5;
		params[1] = 85;
		break;
	}
}

//Our augment to this command is the accomodation of wasd keys being asserted, 
//in which we will feed custom parameter values that is best compatible with the movement mode.
//In this case, we do not require the user to input any parameter values at all.
//For wasd keys, we hijack the workflow such that we input the conventional f, b, l, r letters
// to set buffer[1], but we assign the parameter values automatically.
void sendCommand(char ch, void* conn)
{
	char buffer[10];
	int32_t params[2];

	buffer[0] = NET_COMMAND_PACKET;

	switch (ch)
	{

	case 'c':
	case 'C':
	case 'g':
	case 'G':
		params[0]=0;
		params[1]=0;
		memcpy(&buffer[2], params, sizeof(params));
		buffer[1] = ch;
		sendData(conn, buffer, sizeof(buffer));
	break;

	case 'x':
	case 'X':
		params[0]=0;
		params[1]=0;
		buffer[1] = 's';
		memcpy(&buffer[2], params, sizeof(params));
		sendData(conn, buffer, sizeof(buffer));
	break;



	case 'p':
	case 'P':
		mode = (mode == 1) ? 0 : 1;
		break;

	case 'f':
	case 'F':
	case 'b':
	case 'B':
	case 'l':
	case 'L':
	case 'r':
	case 'R':
		getParams(params);
		buffer[1] = ch;
		memcpy(&buffer[2], params, sizeof(params));
		sendData(conn, buffer, sizeof(buffer));
		break;


	case 'w':
	case 'W':
		buffer[1] = 'f';
		getParamsAuto('f', params);
		memcpy(&buffer[2], params, sizeof(params));
		sendData(conn, buffer, sizeof(buffer));
		break;

	case 'a':
	case 'A':
		buffer[1] = 'l';
		getParamsAuto('l', params);
		memcpy(&buffer[2], params, sizeof(params));
		sendData(conn, buffer, sizeof(buffer));
		break;

	case 's':
	case 'S':
		buffer[1] = 'b';
		getParamsAuto('b', params);
		memcpy(&buffer[2], params, sizeof(params));
		sendData(conn, buffer, sizeof(buffer));
		break;

	case 'd':
	case 'D':
		buffer[1] = 'r';
		getParamsAuto('r', params);
		memcpy(&buffer[2], params, sizeof(params));
		sendData(conn, buffer, sizeof(buffer));
		break;

	case 'q':
	case 'Q':
		quit = 1;
		break;

	default:
		printf("BAD COMMAND\n"); 
	}
}

	//In order to not spam Serial.write or the program, multiple characters input by default from holding down a key
	//is now reduced to a single sent command. That is, only when there is a change in direction of movement then a new
	//command is sent.

	//Also, algorithm takes into account that the command is sent only if the Arduino has received the previous packet 
	//in full, and hence is ready to accept the next packet of command. This overcomes the problem of Magic number error.
	void* movement_change_thread(void *conn) {
		char prev = 'x';
		int count = 0;
		while (1) {
			if (command == 'a' || command == 'd') {
				command = count? command: 's';
				count = 1-count;
				usleep(100000);
			}
			
			if (command != prev) {
				prev = command;
				finalcommand = command;
				if (ok_flag) {
					sendCommand(finalcommand, conn);			
					printw("command is %c\n", finalcommand);
				}
			}
		}
	}


	int kbhit(void){
		d = getch();
		if(d != (char)255){
			ungetch(d);
			return 1;
		}else{
			return 0;
		}
	}


	void* writerThread(void* conn)
	{
		char ch;
		int start = 0;
		int i = 0, j = 0;
		int _count = 0;
		pthread_t commandthread;
		pthread_create(&commandthread, NULL, movement_change_thread, (void*) conn);

		while (!quit)
		{
		switch (mode) {
		case 0:
				endwin();
				printf("###################################\r\n");
				printf("## Welcome to Alex's TLS Client! ##\r\n");
				printf("###################################\r\r\n\n");
				printf("Command (p=toggle to easy mode, f=forward, b=reverse, l=turn left, r=turn right, x=stop, c=clear stats, g=get stats q=exit)\n\r");
				printf("Easy Mode (w=forward, s=reverse, a=turn left, d=turn right, x = stop, c=clear stats, g=get stats, q=exit)\n\r");
				scanf("%c", &ch);
				// Purge extraneous characters from input stream
				flushInput();
				sendCommand(ch, conn);
				break;
			
		//The crux of the algorithm is that it samples for input from a keyboard key hold and assigns the appropriate movement command. 
		//If there is no input for a certain period of time, then a stop movement will automatically be assigned.
		//This is to as best as we could, simulate gaming controls to allow us to navigate the map with precision and ease.
		case 1: clear();
			if (!start) {
				initscr();
				cbreak();
				noecho();
				nodelay(stdscr, TRUE);
				scrollok(stdscr, TRUE);
			}
			start = 1;
			if (j > 0) {
				command = (d == -1 || d == (char)255) ? prevcommand : d;
				prevcommand = command;
				if (_count <= 1) usleep(470000);
			}
			else if (i % 3 == 0) {
				command = 'x';
				prevcommand = command;
			}
			if (ok_flag) printw("Ready!\n");
			else {
				printw("Busy!\n");
				getch();
			}

			if (kbhit()) {
				getch();
				i = 0;
				j++;
				_count++;
				refresh();
			}
			else {
				_count = 0;
				i++;
				j = 0;
				refresh();
				usleep(70000);
			}
			break;
		}
	}
		endwin();
		printf("Exiting keyboard thread\n");
		/* TODO: Stop the client loop and call EXIT_THREAD */
		stopClient();
		EXIT_THREAD(conn);
		/* END TODO */
}


	/* TODO: #define filenames for the client private key, certificatea,
	   CA filename, etc. that you need to create a client */
#define SERVER_NAME		"192.168.43.186"
#define PORT_NUM			5000
#define CA_CERT_FNAME		"signing.pem"
#define CLIENT_CERT_FNAME	"laptop.crt"
#define CLIENT_KEY_FNAME	"laptop.key"
#define SERVER_NAME_ON_CERT	"alex.epp.com" 

	   /* END TODO */
	void connectToServer(const char* serverName, int portNum)
	{
		/* TODO: Create a new client */
		createClient(SERVER_NAME, PORT_NUM, 1, CA_CERT_FNAME, SERVER_NAME_ON_CERT,
			1, CLIENT_CERT_FNAME, CLIENT_KEY_FNAME, readerThread, writerThread);
		/* END TODO */
	}

	int main(int ac, char** av)
	{
		if (ac != 3)
		{
			fprintf(stderr, "\n\n%s <IP address> <Port Number>\n\n", av[0]);
			exit(-1);
		}

		networkActive = 1;
		connectToServer(av[1], atoi(av[2]));

		/* TODO: Add in while loop to prevent main from exiting while the
		client loop is running */
		while (client_is_running());


		/* END TODO */
		printf("\nMAIN exiting\n\n");
}
