
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

// Flags "WASD" Mode if set to 1. 
int mode = 0;
// Flags Sleep Mode if set to 1.
int sleepMode = 0;

// "WASD" mode global variables.
char d = 'a';
char command = 'x';
char prevcommand = 'x';
char finalcommand = 'x';

// Set to 1 when Arduino received the previous command in full and the TLS Client
// Received the acknowledgement packet.
int ok_flag = 1;

// Set to 1 to quit the Client.
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


// The main idea is to waive the requirement imposed on the operator to enter a parameter value.
// This function automatically assigns parameter values to command packets sent due to "WASD" mode.
// Substitutes getParams() from the studio implementation.
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
	
	// Get and Clear odometry data from the Arduino.
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

	// Operator can turn off the RPLidar manually when operating in "Studio" mode.
	// Sends a data packet to TLS Client to shut off the RPLidar motor.
	case 'z':
	case 'Z':
		params[0]=0;
		params[1]=0;
		memcpy(&buffer[2], params, sizeof(params));
		buffer[1] = ch;
		sendData(conn, buffer, sizeof(buffer));
		// Set flag to allow next operator command to be processed.
		ok_flag = 1;
	break;

	// Toggle Sleep Mode between ON and OFF.
	// Sends the data packet to the TLS server
	// to turn off the RPLidar motor.
	case 'o':
	case 'O':
		sleepMode = 1- sleepMode;
		params[0]=0;
		params[1]=0;
		buffer[1] = 'o';
		memcpy(&buffer[2], params, sizeof(params));
		sendData(conn, buffer, sizeof(buffer));
		// Set flag to allow next operator command to be processed.
		ok_flag = 1;
	break;

	// Alex stop command
	case 'x':
	case 'X':
		params[0]=0;
		params[1]=0;
		buffer[1] = 's';
		memcpy(&buffer[2], params, sizeof(params));
		sendData(conn, buffer, sizeof(buffer));
	break;

	// Toggle "WASD" mode between ON and OFF.
	case 'p':
	case 'P':
		mode = (mode == 1) ? 0 : 1;
		break;

	// Movement Commands for "Studio" Mode
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

	// Movement Commands for "WASD" mode.
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

	// Quit Client
	case 'q':
	case 'Q':
		quit = 1;
		break;

	default:
		printf("BAD COMMAND\n"); 
		break;
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
			// This if condition enforces a step-by-step movement in turning.
			// When 'a' and 'd' is held for an extensive amount of time,
			// Command sent alternates between the turn movement command,
			// and the stop command. 
			// The robot hence turns in a sequence of movement and stop. 
			if (command == 'a' || command == 'd') {
				command = count? command: 'x';
				count = 1-count;
				usleep(100000);
			}
			
			// If a new command is sent such that it is different from the previous command,
			// Send the new command. Otherwise do nothing.
			if (command != prev) {
				prev = command;
				finalcommand = command;
				if (ok_flag) {
					sendCommand(finalcommand, conn);	
					// Feedback to operator behind TLS client the command that is sent.		
					printw("command is %c\n", finalcommand);
				}
			}
		}
	}

	// This function flags a key register input from the user by returning 1, otherwise return 0.
	int kbhit(void){
		d = getch();
		// If key input is valid
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
		// int i is monitored to determine when to send a stop command.
		// int j is monitored to determine when to send a new command.
		int i = 0, j = 0;
		int _count = 0;

		// Set up parallel execution thread to handle approved key registers.
		pthread_t commandthread;
		pthread_create(&commandthread, NULL, movement_change_thread, (void*) conn);

		while (!quit)
		{
		switch (mode) {
		
		// Studio mode.
		case 0:
				endwin();
                printf("###################################\r\n");
				printf("## Welcome to Alex's TLS Client! ##\r\n");
				printf("###################################\r\r\n\n");
				printf("Command (p=toggle to easy mode, f=forward, b=reverse, l=turn left, r=turn right, x=stop, c=clear stats, g=get stats, q=exit, z=put rplidar to sleep!)\n\n\r\r");
				printf("Easy Mode (w=forward, s=reverse, a=turn left, d=turn right, other commands remain the same!)\n\n\r");
				printf("in WASD mode, press 'O' to activate sleep mode!\n\n\r\r");
				// Purge extraneous characters from input stream
				scanf("%c", &ch);
				flushInput();
				sendCommand(ch, conn);
				break;
			
		//The crux of the algorithm is that it samples for input from a keyboard key hold and assigns the appropriate movement command. 
		//If there is no input for a certain period of time, then a stop movement will automatically be assigned.
		//This is to as best as we could, simulate gaming controls to allow us to navigate the map with precision and ease.
		case 1: clear();
			if (!start) {
				// Configuration commands to set up ncurses.h. Only execute once when the mode first switches to "WASD".
				initscr();
				cbreak();
				noecho();
				nodelay(stdscr, TRUE);
				scrollok(stdscr, TRUE);
			}
			start = 1;

			// Only set command when received character is valid.
			// If invalid character is recognised, set to previous command, indicating no change in command.
			if (j > 0) {
				command = (d == -1 || d == (char)255) ? prevcommand : d;
				prevcommand = command;
				// Sleep is applied to ignore the first NULL key register.
				if (_count <= 1) usleep(470000);
			}

			// Listens for sequence of NULL key registers that persisted beyond a suitable threshold time.
			else if (i % 3 == 0) {
				command = 'x';
				prevcommand = command;
			}

			// If the Arduino has finished processing the previous command and a corresponding acknowledgement packet is received.
			if (ok_flag && !sleepMode) printw("Ready!\n");

			// Indicates change in mode to sleep mode.
			else if (ok_flag && sleepMode) printw("Sleep Mode activated!\n");

			// If acknowledgement packet is not received, discard all inputs in input stream.
			else if (!ok_flag) {
				printw("Busy!\n");
				getch();
			}

			// If a key register is detected, get character in input stream.
			if (kbhit()) {
				getch();
				i = 0;
				j++;
				_count++;
				refresh();
			}

			// If no key register is detected, kbhit will return 0. Register a NULL key register.
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
#define SERVER_NAME		"192.168.43.186" // Need to change if using a different network.
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
