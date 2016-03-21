/* raspilokserver
 * raspilokserver.c
 *
 *	Version: 0.2
 *  Created on: 14.12.2014 - 06.05.2015
 *  Author: Michael Brunnbauer
 */

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "libconfig.h"
#include "commands.h"
#include "ledc.h"
#include "raspinetwork.h"
#include "raspilokserver.h"
#include "uart.h"
#include "raspispec.h"


char tcp_cmdbuffer[CMDBUFFER_SIZE];		// nur für tcpserver (eigener thread) empfangener Befehltext der aufgehoben weil noch unvollständig ist
char uart_cmdbuffer[CMDBUFFER_SIZE];	// selbes für UART-cmds
char lokname[41] = "raspilok1";			// zum zwischenspeichern des Loknamens
char cmd_iam[UART_MAXCMDLEN] = "<iam:1:raspilok1>";	// Text für iam-Lok-Meldung (wird bei Namensänderung geändert
uint8_t threadend = 0;		// 1 = alle threads beenden (für Zugriff mutex_end verwenden!!!!)
uint8_t connected = 0;		// zeigt an, ob eine Netzwerkverbindung besteht (info zwischen den Netzwerk-Threads). 1=connected, 0= not connected
pthread_mutex_t mutex_end = PTHREAD_MUTEX_INITIALIZER;	// mutex für threadend
pthread_mutex_t mutex_connected = PTHREAD_MUTEX_INITIALIZER;	// mutex für connected
pthread_mutex_t mutex_tcpsend = PTHREAD_MUTEX_INITIALIZER;	// mutex für tcp senden an den controller (damit immer nur 1 thread sendet
int8_t i2c_active = -1;		// -1: i2c nicht aktiv. 0/1: aktive i2c-Bus-Nummer
uint8_t PLED01_ok = 0;		// erster PLED01 ist ok? 0: nicht ok, 1:ok
int uart0_filestream;	// für UART Kommunikation
int alivecheck = 0;		// aus dem .cfg file -> soll geprüft werden, ob die gegenstelle noch aktiv ist?
int servermode = 1;		// aus dem .cfg file -> 0: transparenter Modus (ALLE Daten werden weitergereicht, Befehle werden am Raspi ignoriert) als Wiznet-Ersatz für die Tests
						// 1: Standardmodus: Raspi reicht nur bestimmte Befehle an den MC weiter (in commands.c definiert)
int tcpconnectionsock = -1;	// zum Schreiben an den Controller (Gegenstelle der tcp-Verbindung)
char ipadress_eth0[INET_ADDRSTRLEN];	// eigenen eth0 IP-Adresse
in_addr_t eth0ip;		// eigenen eth0 IP-Adresse als Zahl


int main() {

	//cfg vars
	config_t cfg;               // all parameters from the config-file will be stored in this structure
	//config_setting_t *cfgsetting;
	//const char *str1, *str2;
	int tcpport, udpport, servertcpport, useserver, loglevel;

	const char *config_file_name = "/home/pi/raspilokserver/lokserver.cfg";

	// thread vars
	pthread_t thread_tcp, thread_udp, thread_uart;
	int treadrc;

	printf("Starting raspilokserver\n");
	config_init(&cfg); // libconfig Initialization
	bzero(tcp_cmdbuffer, CMDBUFFER_SIZE); //Befehlspuffer löschen
	bzero(uart_cmdbuffer, CMDBUFFER_SIZE);

	// Read the config-file. If there is an error, report it and exit.
	if (!config_read_file(&cfg, config_file_name))
	{
		printf("\n%s:%d - %s", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return -1;
	}

	// Get the configuration settings
	if (config_lookup_int(&cfg, "tcpport", &tcpport)) { printf("tcpport = %i\n", tcpport); }
	else { printf("No 'tcpport' setting in configuration file.\n"); }

	if (config_lookup_int(&cfg, "udpport", &udpport)) { printf("udpport = %i\n", udpport); }
	else { printf("No 'udpport' setting in configuration file.\n"); }

	if (config_lookup_int(&cfg, "alivecheck", &alivecheck)) { printf("alivecheck = %i\n", alivecheck); }
	else { printf("No 'alivecheck' setting in configuration file.\n"); }

	if (config_lookup_int(&cfg, "servermode", &servermode)) { printf("servermode = %i\n", servermode); }
	else { printf("No 'servermode' setting in configuration file.\n"); }


	printLocalIPs();	// TODO: test - die richtige IP muss noch ausgewählt werden (damit man zB die eigenen UDP-Meldungen ignorieren kann)

	uart_init();	//uart0 Konfiguration

	raspi_id();		// Raspi board info auslesen



	// wiringPiSetup();	// wird eventuell noch für andere Dinge gebraucht
	// i2cslave = wiringPiI2CSetup(I2CSLAVEID);	// bricht mit Fehler ab (!!!), falls i2c nicht existiert ("no such file or directory")
	// daher i2c ohne wiringPi verwenden! (wiringPi wird überhaupt nicht mehr benötigt)

	i2c_init();	// i2c bus check
	ledc_init(I2CSLAVE_ADDR_PLED01);	// (1.) LED-Controller initialisieren


	// start threads
	treadrc = pthread_create(&thread_uart, NULL, uartlistener, NULL);
	if (treadrc !=0) { printf("Error creating uartlistener thread.\n"); }

	treadrc = pthread_create(&thread_tcp, NULL, tcpserver, (void*)&tcpport);
	if (treadrc !=0) { printf("Error creating tcp server thread.\n"); }

	treadrc = pthread_create(&thread_udp, NULL, udpserver, (void*)&udpport);
	if (treadrc !=0) { printf("Error creating udp server thread.\n"); }

	pthread_join(thread_tcp, NULL);	// tcp: auf thread-Beendigung warten
	printf("tcp thread ended.\n");
	pthread_join(thread_udp, NULL);	// udp: auf thread-Beendigung warten
	printf("udp thread ended.\n");
	pthread_join(thread_uart, NULL);	// uart: auf thread-Beendigung warten
	printf("uart thread ended.\n");
	printf("Threads wurden beendet - Programmende.\n");

	return 0;
}


