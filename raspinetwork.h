/*
 * raspinetwork.h
 *
 *  Created on: 25.08.2013
 *      Author: ratz
 */

#ifndef RASPINETWORK_H_
#define RASPINETWORK_H_



extern time_t lasttcp_data;		// Zeitpunkt, an dem die letzen tcp-Daten vom Controller empfangen wurden (zur Erkennung von Verbindungsabbrüchen)

extern pthread_mutex_t mutex_udpsend;	// mutex für udp-senden
extern pthread_mutex_t mutex_lasttcp_data;	// mutex für lasttcp_data

extern pthread_t thread_tcp_clientloop;

extern void *tcpserver(void *ptr);
extern void *tcp_clientloop(void *ptr);
extern void *udpserver(void *ptr);
extern void *udp_reader(void *ptr);
extern void *check_every_second(void *ptr);
extern void tcp_send(char *msg);
extern void tcp_send_safe(char *msg);
extern void sendabroadcast(int broadcastSock, int port, const char *msg);
extern void printLocalIPs();
extern int getTimeWoData();
extern void setConnected();
extern void setDisconnected();
extern uint8_t getConnectionStatus();
extern uint8_t getNWThreadEnd();

#endif /* RASPINETWORK_H_ */
