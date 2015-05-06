/*
 * uart.c
 *
 *  Created on: 02.01.2015
 *      Author: Michael Brunnbauer
 *      based on this: http://www.raspberry-projects.com/pi/programming-in-c/uart-serial-port/using-the-uart
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>			//Used for UART
#include <fcntl.h>			//Used for UART
#include <termios.h>		//Used for UART
#include <stdint.h>
#include <string.h>

#include "uart.h"
#include "raspilokserver.h"
#include "raspinetwork.h"

void uart_init(void)
{
	// INFOS:	https://www.cmrr.umn.edu/~strupp/serial.html
	//			http://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c

	//-------------------------
	//----- SETUP USART 0 -----
	//-------------------------
	//At bootup, pins 8 and 10 are already set to UART0_TXD, UART0_RXD (ie the alt0 function) respectively
	uart0_filestream = -1;

	//OPEN THE UART
	//The flags (defined in fcntl.h):
	//	Access modes (use 1 of these):
	//		O_RDONLY - Open for reading only.
	//		O_RDWR - Open for reading and writing.
	//		O_WRONLY - Open for writing only.
	//
	//	O_NDELAY / O_NONBLOCK (same function) - Enables nonblocking mode. When set read requests on the file can return immediately with a failure status
	//											if there is no input immediately available (instead of blocking). Likewise, write requests can also return
	//											immediately with a failure status if the output can't be written immediately.
	//
	//	O_NOCTTY - When set and path identifies a terminal device, open() shall not cause the terminal device to become the controlling terminal for the process.

	uart0_filestream = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY);		//Open in blocking read/write mode
	//uart0_filestream = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	//VMIN and VTIME: Timeouts are ignored (in canonical input mode or) when the NDELAY option is set on the file via open or fcntl.


	if (uart0_filestream == -1)
	{
		//ERROR - CAN'T OPEN SERIAL PORT
		printf("Error - Unable to open UART.  Ensure it is not in use by another application\n");
	}

	//CONFIGURE THE UART
	//The flags (defined in /usr/include/termios.h - see http://pubs.opengroup.org/onlinepubs/007908799/xsh/termios.h.html):
	//	Baud rate:- B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000
	//	CSIZE:- CS5, CS6, CS7, CS8
	//	CLOCAL - Ignore modem status lines
	//	CREAD - Enable receiver
	//	IGNPAR = Ignore characters with parity errors
	//	ICRNL - Map CR to NL on input (Use for ASCII comms where you want to auto correct end of line characters - don't use for bianry comms!)
	//	PARENB - Parity enable
	//	PARODD - Odd parity (else even)
	struct termios options;
	int initerror = 0;
	initerror = tcgetattr(uart0_filestream, &options);
	if (initerror != 0) { printf("Error UART: tcgetattr\n"); }
	options.c_cflag = B38400 | CS8 | CLOCAL | CREAD;		//<Set baud rate
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	options.c_cc[VMIN]  = 0;     // don't wait for a special number of characters
	options.c_cc[VTIME] = 5;     // 0.5 seconds read timeout
	tcflush(uart0_filestream, TCIFLUSH);
	initerror = tcsetattr(uart0_filestream, TCSANOW, &options);
	if (initerror != 0) { printf("Error UART: tcsetattr\n"); }

}

// Schreiben auf UART0
uint8_t uart_write(const void *data)
{

	//printf("uart_write:\n");
	uint8_t error = 0;
	uint8_t datalen = strlen(data);

	if (uart0_filestream != -1)
	{
		int count = write(uart0_filestream, data, datalen);		//Filestream, bytes to write, number of bytes to write

		if (count < 0)
		{
			printf("UART TX error\n");
			error = -2;
		}
		else if (count < datalen)
		{
			printf("UART: zu wenig Daten gesendet\n");
			error = -3;
		}
	}
	else { error = -1; }

	return error;
}

// liest von UART0, wartet nicht, falls keine Daten vorhanden sind
int uart_read(char *buffer)
{
	int rx_length =0;

	//----- CHECK FOR ANY RX BYTES -----
	if (uart0_filestream != -1)
	{
		// Read up to 255 characters from the port if they are there
		rx_length = read(uart0_filestream, (void*)buffer, 255);		//Filestream, buffer to store in, number of bytes to read (max)

		if (rx_length < 0)
		{
			//An error occured (will occur if there are no bytes)
		}
		else if (rx_length == 0)
		{
			//No data waiting
		}
		else	//Bytes received
		{
			buffer[rx_length] = '\0';
		}

	}

	return rx_length;

}

// uartlistener wartet auf Daten der UART-Verbindung zum Mikrocontroller und wertet sie aus
// TODO: später soll das auch laufen, wenn keine Netzwerk-Verbindugn besteht!
void *uartlistener(void *ptr)
{
	char data[256];
	uint8_t end = 0;	// 1 bedeutet Ausstieg aus der Server-Schleife
	int testcount;

	printf("Starting UARTlistener...\n");

	while (!getNWThreadEnd())	// mit den Network-Threads beenden
	{

		testcount = uart_read(data);	// wartet jeweils 0,5s auf Daten (siehe uart_init(), VTIME), data wird überschrieben!!
		if (testcount > 0) { printf("UART data: %s\n", data); }

		end = getNWThreadEnd();
		if (!end)
		{
			if (testcount > 0) { tcp_send_safe(data); } //TODO: vorerst einfach an Controller weitersenden
		}
	}
	printf("Exiting UARTlistener...\n");

	return NULL;
}

