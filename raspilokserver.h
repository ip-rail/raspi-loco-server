/*
 * raspilokserver.h
 *
 *  Created on: 25.08.2013
 *      Author: Michael Brunnbauer
 */

#ifndef RASPILOKSERVER_H_
#define RASPILOKSERVER_H_

#define UART_MAXCMDLEN 64		// maximal erlaubte Länge eines Befehls "<*>" incl. <> Wert muss mit der gleichnamigen Definition in lokserver identisch sein!!

#define SERVERMODE_TRANSPARENT	0

#define CMD_FROM_NETWORK	1	// für die Befehlsauswertung zur Unterscheidung Befehl vom Netzwerk (Controller) oder MC
#define CMD_FROM_UART		2

#define CMDBUFFER_SIZE		1024		// Buffer-Größe für die einzelnen Befehls-Buffer mit dem noch nicht ausgewerteten Rest (unvollständige Befehle)

// extern verwendete Variablen
extern char tcp_cmdbuffer[CMDBUFFER_SIZE];	// nur für tcpserver (eigener thread) empfangener Befehltext der aufgehoben weil noch unvollständig ist
extern char uart_cmdbuffer[CMDBUFFER_SIZE];	// selbes für UART-cmds
extern char lokname[41];
extern char cmd_iam[UART_MAXCMDLEN];
extern uint8_t threadend;			// 1 = alle threads beenden
extern uint8_t connected;			// 1 = client connected
extern pthread_mutex_t mutex_end;	// mutex für threadend
extern pthread_mutex_t mutex_connected;	// mutex für connected
extern pthread_mutex_t mutex_tcpsend;	// mutex für tcp senden an den controller
extern int8_t i2c_active;			// -1: i2c nicht aktiv. 0/1: aktive i2c-Bus-Nummer
extern uint8_t PLED01_ok;		// erster PLED01 ist ok? 0: nicht ok, 1:ok
extern int uart0_filestream;		// für UART Kommunikation
extern int alivecheck;				// aus dem .cfg file -> soll geprüft werden, ob die gegenstelle noch aktiv ist?
extern int servermode;				//aus dem .cfg file -> 0: transparenter Modus / 1: Raspi-Modus
extern int tcpconnectionsock;		// zum Schreiben an den Controller (Gegenstelle der tcp-Verbindung)
extern char ipadress_eth0[INET_ADDRSTRLEN];	// eigenen eth0 IP-Adresse
extern in_addr_t eth0ip;		// eigenen eth0 IP-Adresse als Zahl

#endif /* RASPILOKSERVER_H_ */
