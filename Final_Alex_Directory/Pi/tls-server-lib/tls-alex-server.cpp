#include "make_tls_server.h"
#include "tls_common_lib.h"
#include "netconstants.h"
#include "constants.h"
#include "packet.h"
#include "serial.h"
#include "serialize.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

/* TODO: Set PORT_NAME to the port name of your Arduino */
#define PORT_NAME			"/dev/ttyACM0"
/* END TODO */

#define BAUD_RATE			B9600

// TLS Port Number
#define SERVER_PORT			5000

/* TODO: #define constants for the  filenames for Alex's private key, certificate, CA certificate name,
        and the Common Name for your laptop */
#define KEY_FNAME                      "alex.key"
#define CERT_FNAME                     "alex.crt"
#define CA_CERT_FNAME                  "signing.pem"
#define CLIENT_NAME                    "laptop.epp.com"


/* END TODO */

// Our network buffer consists of 1 byte of packet type, and 128 bytes of data
#define BUF_LEN				129

// This variable shows whether a network connection is active
// We will also use this variable to prevent the server from serving
// more than one connection, to keep connection management simple.

static volatile int networkActive;

// This variable is used by sendNetworkData to send back responses
// to the TLS connection.  It is sent by handleNetworkData

static void *tls_conn = NULL;

/*

	Alex Serial Routines to the Arduino

	*/

int toggle = 0;
int sleepMode = 0;
int busy = 0;

// Prototype for sendNetworkData
void sendNetworkData(const char *, int);

void handleErrorResponse(TPacket *packet)
{
	printf("UART ERROR: %d\n", packet->command);
	char buffer[2];
	buffer[0] = NET_ERROR_PACKET;
	buffer[1] = packet->command;
	sendNetworkData(buffer, sizeof(buffer));
}

void handleMessage(TPacket *packet)
{
	char data[33];
	printf("UART MESSAGE PACKET: %s\n", packet->data);
	data[0] = NET_MESSAGE_PACKET;
	memcpy(&data[1], packet->data, sizeof(packet->data));
	sendNetworkData(data, sizeof(data));
}

void handleStatus(TPacket *packet)
{
	char data[65];
	printf("UART STATUS PACKET\n");
	data[0] = NET_STATUS_PACKET;
	memcpy(&data[1], packet->params, sizeof(packet->params));
	sendNetworkData(data, sizeof(data));
}

void uartSendPacket(TPacket *);

void sendOK(){
	TPacket okpacket;
	okpacket.packetType = PACKET_TYPE_RESPONSE;
	okpacket.command = RESP_OK;
	uartSendPacket(&okpacket);
}

void handleResponse(TPacket *packet)
{
	// The response code is stored in command
	switch(packet->command)
	{
		case RESP_OK:
			char resp[2];
			printf("Command OK\n");
			sendOK();
			resp[0] = NET_ERROR_PACKET;
			resp[1] = RESP_OK;
			sendNetworkData(resp, sizeof(resp));
		break;

		case RESP_STATUS:
			handleStatus(packet);
		break;

		default:
		printf("Boo\n");
	}
}

void rplidarSleep(){
	int sender_fd, ret;
	char buffer[1];
	//Every time user presses corresponding key, rplidar should toggle between sleep mode and active mode
	toggle = 1-toggle;
	buffer[0] = toggle ? 's': 'x';
	ret = chdir("/home/pi/030401rox/Final_Alex_Directory/Pi/slam/src/rplidar_ros/");
	sender_fd = open("stop.bin", O_RDWR|O_CREAT,0777);
	if(!sender_fd) printf("File cannot be created or be written to!\n");
	//Write to intermediate file "stop.bin" to toggle operating mode of LIDAR
	write(sender_fd, buffer, 1);
	printf("wrote!\n");
}

int readLidar(){
	int reader_fd, ret;
	char buffer[1];
	ret = chdir("/home/pi/030401rox/Final_Alex_Directory/Pi/slam/src/rplidar_ros/");
	reader_fd = open("stop.bin", O_RDWR|O_CREAT,0777);
	if(!reader_fd) printf("File cannot be created or be written to!\n");
	//Write to intermediate file "stop.bin" to toggle operating mode of LIDAR
	
	read(reader_fd, buffer, 1);
	printf("character is %c\n", buffer[0]);
	if(buffer[0] == 'g') return 1;
	else return 0;
}

void handleCommand(TPacket *packet)
{
	switch(packet->command){
		case COMMAND_RPLIDAR_SLEEP:
				rplidarSleep();
		break;
	}
}


void handleUARTPacket(TPacket *packet)
{
	switch(packet->packetType)
	{
		case PACKET_TYPE_COMMAND:

				// Only we send command packets, so ignore
			break;

		case PACKET_TYPE_RESPONSE:
				handleResponse(packet);
			break;

		case PACKET_TYPE_ERROR:
				handleErrorResponse(packet);
			break;

		case PACKET_TYPE_MESSAGE:
				handleMessage(packet);
			break;
	}
}


void uartSendPacket(TPacket *packet)
{
	char buffer[PACKET_SIZE];
	int len = serialize(buffer, packet, sizeof(TPacket));

	serialWrite(buffer, len);
}

void handleError(TResult error)
{
	switch(error)
	{
		case PACKET_BAD:
			printf("ERROR: Bad Magic Number\n");
			break;

		case PACKET_CHECKSUM_BAD:
			printf("ERROR: Bad checksum\n");
			break;

		default:
			printf("ERROR: UNKNOWN ERROR\n");
	}
}

void *uartReceiveThread(void *p)
{
	char buffer[PACKET_SIZE];
	int len;
	TPacket packet;
	TResult result;
	int counter=0;

	while(1)
	{
		len = serialRead(buffer);
		counter+=len;
		if(len > 0)
		{
			result = deserialize(buffer, len, &packet);

			if(result == PACKET_OK)
			{
				counter=0;
				handleUARTPacket(&packet);
			}
			else 
				if(result != PACKET_INCOMPLETE)
				{
					printf("PACKET ERROR\n");
					handleError(result);
				} // result
		} // len > 0
	} // while
}

/*

	Alex Network Routines

	*/


void sendNetworkData(const char *data, int len)
{
	// Send only if network is active
	if(networkActive)
	{
        // Use this to store the number of bytes actually written to the TLS connection.
        int c = len;

		printf("WRITING TO CLIENT\n");
 
        if(tls_conn != NULL) {
            /* TODO: Implement SSL write here to write data to the network. Note that
              handleNetworkData should already have set tls_conn to point to the TLS
              connection we want to write to. */
		 sslWrite(tls_conn, data, c);
            /* END TODO */

        }

		// Network is still active if we can write more then 0 bytes.
		networkActive = (c > 0);
	}
}

void handleCommand(void *conn, const char *buffer)
{
	// The first byte contains the command
	char cmd = buffer[1];
	uint32_t cmdParam[2];

	// Copy over the parameters.
	memcpy(cmdParam, &buffer[2], sizeof(cmdParam));

	TPacket commandPacket;

	commandPacket.packetType = PACKET_TYPE_COMMAND;
	commandPacket.params[0] = cmdParam[0];
	commandPacket.params[1] = cmdParam[1];

	printf("COMMAND RECEIVED: %c %d %d\n", cmd, cmdParam[0], cmdParam[1]);
	
	switch(cmd)
	{
		case 'z':
		case 'Z':
			rplidarSleep();
			break;
			
		case 'o':
		case 'O':
			sleepMode = 1- sleepMode;
			if (sleepMode) printf("sleepMode activated!\n");
			else printf("sleepMode deactivated!");
			break;

		case 'f':
		case 'F':
			if(toggle && sleepMode) {
				rplidarSleep();
				while(readLidar() == 0){
					busy = 1;
					usleep(3000000);
					busy = 0;
				}
			}
			commandPacket.command = COMMAND_FORWARD;
			uartSendPacket(&commandPacket);
			break;

		case 'b':
		case 'B':
			if(toggle && sleepMode) {
				rplidarSleep();
				while(readLidar() == 0){
					usleep(3000000);
				}
			}
			commandPacket.command = COMMAND_REVERSE;
			uartSendPacket(&commandPacket);
			break;

		case 'l':
		case 'L':
			commandPacket.command = COMMAND_TURN_LEFT;
			uartSendPacket(&commandPacket);
			break;

		case 'r':
		case 'R':
			commandPacket.command = COMMAND_TURN_RIGHT;
			uartSendPacket(&commandPacket);
			break;

		case 's':
		case 'S':
			commandPacket.command = COMMAND_STOP;
			uartSendPacket(&commandPacket);
			usleep(1000000);
			if((!toggle) && sleepMode) rplidarSleep();
			break;

		case 'c':
		case 'C':
			commandPacket.command = COMMAND_CLEAR_STATS;
			commandPacket.params[0] = 0;
			uartSendPacket(&commandPacket);
			break;

		case 'g':
		case 'G':
			commandPacket.command = COMMAND_GET_STATS;
			uartSendPacket(&commandPacket);
			break;

		default:
			printf("Bad command\n");

	}
}

void handleNetworkData(void *conn, const char *buffer, int len)
{
    /* Note: A problem with our design is that we actually get data to be written
        to the SSL network from the serial port. I.e. we send a command to the Arduino,
        get back a status, then write to the TLS connection.  So we do a hack:
        we assume that whatever we get back from the Arduino is meant for the most
        recent client, so we just simply store conn, which contains the TLS
        connection, in a global variable called tls_conn */

        tls_conn = conn; // This is used by sendNetworkData

	if(buffer[0] == NET_COMMAND_PACKET)
		handleCommand(conn, buffer);
}

void *worker(void *conn)
{
	int len;

	char buffer[BUF_LEN];
	
	while(networkActive)
	{
		/* TODO: Implement SSL read into buffer */
		len = sslRead(conn, buffer, sizeof(buffer));

		/* END TODO */
		// As long as we are getting data, network is active
		networkActive=(len > 0);

		if(len > 0){
			while(busy);
			handleNetworkData(conn, buffer, len);
		}
		else
			if(len < 0)
				perror("ERROR READING NETWORK: ");
	}

    // Reset tls_conn to NULL.
    tls_conn = NULL;
    EXIT_THREAD(conn);
}


void sendHello()
{
	// Send a hello packet
	TPacket helloPacket;

	helloPacket.packetType = PACKET_TYPE_HELLO;
	uartSendPacket(&helloPacket);
}

int main()
{
	// Start the uartReceiveThread. The network thread is started by
    // createServer

	pthread_t serThread;

	printf("\nALEX REMOTE SUBSYSTEM\n\n");

	printf("Opening Serial Port\n");
	// Open the serial port
	startSerial(PORT_NAME, BAUD_RATE, 8, 'N', 1, 5);
	printf("Done. Waiting 3 seconds for Arduino to reboot\n");
	sleep(3);

	printf("DONE. Starting Serial Listener\n");
	pthread_create(&serThread, NULL, uartReceiveThread, NULL);

    printf("Starting Alex Server\n");

    networkActive = 1;

    /* TODO: Call createServer with the necessary parameters to do client authentication and to send
        Alex's certificate. Use the #define names you defined earlier  */
	createServer(KEY_FNAME, CERT_FNAME, SERVER_PORT, &worker, CA_CERT_FNAME, CLIENT_NAME, 1);
    /* TODO END */

	printf("DONE. Sending HELLO to Arduino\n");
	sendHello();
	printf("DONE.\n");


    // Loop while the server is active
    while(server_is_running());
}
