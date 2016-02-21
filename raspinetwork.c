/*
 * raspinetwork.c
 *
 *  Created on: 25.08.2013
 *      Author: Michael Brunnbauer
 */

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/route.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "raspilokserver.h"
#include "commands.h"
#include "raspinetwork.h"

struct nwthreadargs	// Argumente zur Übergabe an den udp_alive thread
	{
		int sock;
		int port;
	};

struct sendlist	// für tcp-sendeliste
{
	char *msg;
	struct sendlist *next;
};


//struct sendlist	tcpsendlist;
time_t lasttcp_data;
time_t lastdata;		// Zeitpunkt, an dem die letzen tcp-Daten vom Controller empfangen wurden (zur Erkennung von Verbindungsabbrüchen)
pthread_mutex_t mutex_udpsend = PTHREAD_MUTEX_INITIALIZER;	// mutex für udp-senden (damit sich die 2 udp-threads nicht gegenseitig dazwischensenden)
pthread_mutex_t mutex_lasttcp_data = PTHREAD_MUTEX_INITIALIZER;	// mutex für lasttcp_data
//pthread_mutex_t mutex_tcpsendlist = PTHREAD_MUTEX_INITIALIZER;	// mutex für tcpsendlist
pthread_t thread_tcp_clientloop;

void *tcpserver(void *ptr)
{
	// TODO: achtung, cmdtext kann vollaufen, wenn zu viel mist ohne commands (<xyz>) gesendet wird -> checken und löschen, wenn maximale befehlslänge überschritten wird
	// tcp vars
	int listen_desc, conn_desc; // main listening descriptor and connected descriptor
	struct sockaddr_in serv_addr, client_addr;
	socklen_t clilen;
	uint8_t end = 0;	// 1 bedeutet Ausstieg aus der Server-Schleife
	int *tcpport;
	in_addr_t clientip;	// zur Ermittlung der Client-IP
	int treadrc;
	pthread_t thread_check_every_s;	// thread für check_every_second

	tcpport = (int *) ptr;

	printf("Starting tcpserver on port %i...\n", *tcpport);

	listen_desc = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_desc < 0) { printf("Failed creating socket\n"); return NULL; }

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(*tcpport);

	int binderror = bind(listen_desc, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if ( binderror < 0)
	{
		printf("Failed to bind. Error %i\n", binderror);
		return NULL;
	}

	listen(listen_desc, 1);	// nur 1 Client zulassen

	while(!end) // main server loop
	{
		printf("entering main server loop..\n");

		clilen = sizeof(client_addr);

		// ------- auf Clientverbindung warten -------------------------------------------------
		conn_desc = accept(listen_desc, (struct sockaddr *) &client_addr, &clilen);
		if (conn_desc < 0) {  printf("ERROR on accept"); return NULL; }

		tcpconnectionsock = conn_desc;	// in globaler Variable speichern (für tcp_send())

		setConnected();	// jetzt ist ein Client verbunden

		printf("Starting tcp_clientloop thread...\n");
		treadrc = pthread_create(&thread_tcp_clientloop, NULL, tcp_clientloop, (void*)&conn_desc);
		if (treadrc !=0) { printf("Error creating tcp_clientloop thread.\n"); }

		printf("Starting check_every_second thread...\n");
		treadrc = pthread_create(&thread_check_every_s, NULL, check_every_second, (void*)&conn_desc);
		if (treadrc !=0) { printf("Error creating check_every_second thread.\n"); }

		clientip = htonl(client_addr.sin_addr.s_addr);	// Bytereihenfolge berichtigen

		unsigned char bytes[4];
		bytes[0] = clientip & 0xFF;
		bytes[1] = (clientip >> 8) & 0xFF;
		bytes[2] = (clientip >> 16) & 0xFF;
		bytes[3] = (clientip >> 24) & 0xFF;
		printf("Client IP-Address = %d.%d.%d.%d\n", bytes[3], bytes[2], bytes[1], bytes[0]);

		pthread_mutex_lock(&mutex_lasttcp_data);
		time(&lasttcp_data); 	// falls keine Daten kommen, eine Anfangszeit für den Abbruch einstellen!
		pthread_mutex_unlock(&mutex_lasttcp_data);

		pthread_join(thread_tcp_clientloop, NULL);	// auf thread-Beendigung warten (wird für jede Controller-Verbindung neu gestartet)
		printf("tcp_clientloop thread ended.\n");

		pthread_join(thread_check_every_s, NULL);	// auf thread-Beendigung warten (wird für jede Controller-Verbindung neu gestartet)
		printf("check_every_second thread ended.\n");

		close(conn_desc);
		tcpconnectionsock = 0;
		setDisconnected();	// jetzt ist kein Client mehr verbunden

		if (getNWThreadEnd()) { end = 1; }

	} // End main listening loop

	printf("exiting main server loop..\n");
	close(listen_desc);
	return NULL;
}

void *tcp_clientloop(void *ptr)
{
	uint8_t end = 0, hangup = 0;	// 1 bedeutet: bei hangup: Ausstieg aus der Client-Verbindung, end: Ausstieg aus der Server-Schleife (tcpserver thread)
	char buffer[256];
	int n;
	int *conn_desc;

	conn_desc = (int *) ptr;

	while(!hangup)
	{
		printf("entering client loop..\n");
		bzero(buffer,256);
		n = read(*conn_desc,(void*)buffer,255);
		// blocking read ist eine Katastrophe, wenn Client nicht mehr reagiert
		// tcpserver bleibt beim read hängen, außer die Verbindung wird vom Client ordnungsgemäß geschlossen (tcp-intern)
		// non-blocking hat auch Nachteile -> daher Clientloop in eigenen thread auslagern und bei Bedarf killen!!!
		// -> pthread_cancel (thread);
		// http://openbook.galileocomputing.de/linux_unix_programmierung/Kap10-007.htm#RxxKap10007040003211F03E100

		if (n > 0)
		{
			pthread_mutex_lock(&mutex_lasttcp_data);
			time(&lasttcp_data); 	// für Verbindungskontrolle Zeit des Datenempfangs merken
			pthread_mutex_unlock(&mutex_lasttcp_data);

			//printf("tcp Text empfangen: '%s'\n",buffer);



			int ergebnis = checkcmd(buffer);	// Befehl auswerten

			switch(ergebnis)
			{
			case 1: 	// tcp-Server beenden + Programmausstieg
				end = 1;
				hangup = 1;
				break;
			case 2:
				hangup = 1;	// Client-Verbindung beenden
				break;
			} //Ende switch
		}
		else if (n <= 0)
		{
			// -1 bedeutet error
			// 0 kommt zurück, wenn Gegenstelle getrennt hat - dann wird nicht mehr geblockt!
			printf("ERROR reading from tcp-socket\n");
			hangup = 1;
		}

		//n = write(conn_desc,"I got your message",18);
		//if (n < 0) { printf("ERROR writing to socket"); return; }

		//exit check: threadsichere Abfrage/Setzen der threadend Varable
		pthread_mutex_lock( &mutex_end );
		if (end) { threadend = 1;}			// den anderen Threads das Beenden signalisieren
		if (threadend) { hangup = 1; end = 1; }
		pthread_mutex_unlock( &mutex_end );

		if (hangup == 1) { setDisconnected(); }	//

	}
	printf("exiting client loop..\n");

	return NULL;
}


void *udpserver(void *ptr)
{
	int *udpport;
	int sock, length;
	struct sockaddr_in server;
	int end = 0;		// 1= Haupt-Schleife beenden
	int broadcast = 1;
	int treadrc;

	pthread_t thread_udp_reader;	// thread für upd_alive Meldungen, der läuft, solange kein Controller verbunden ist
	struct nwthreadargs aliveargs;	// Argumente zur Übergabe an den udp_alive thread


	udpport = (int *) ptr;

	printf("Starting udpserver on port %i...\n", *udpport);

	sock=socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		printf("Error Opening socket\n");
		return NULL;
	}
	length = sizeof(server);
	bzero(&server,length);
	server.sin_family=AF_INET;
	server.sin_addr.s_addr=INADDR_ANY;
	server.sin_port=htons(*udpport);

	if (bind(sock,(struct sockaddr *)&server,length)<0)
	{
		printf("Error on binding\n");
		return NULL;
	}

	// broadcasts ermöglichen
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof broadcast) == -1)
	{
		printf("Error on setsockopt (SO_BROADCAST)");
		return NULL;
	}

	// subthread für udp-alive Meldungen (alle 10s), wenn kein Client verbunden ist (Achtung: wenn ein Client verbunden ist, werden sekündliche  tcp-alive-Meldungen benötigt!)

	aliveargs.port = *udpport;
	aliveargs.sock = sock;

	printf("Starting udp_reader thread...\n");
	treadrc = pthread_create(&thread_udp_reader, NULL, udp_reader, (void*)&aliveargs);
	if (treadrc !=0) { printf("Error creating udp_reader thread.\n"); }

	while (!end)
	{
		printf("udp alive check\n");
		// nur senden, wenn kein Controller verbunden ist
		if (!getConnectionStatus()) { sendabroadcast(sock, *udpport, "<iam:1:raspilok1>"); }
		sleep(10);	//10s warten
		if (getNWThreadEnd()) //exit check (wird threadsicher abgefragt)
		{
			end = 1;
			pthread_cancel(thread_udp_reader);	// den am recvfrom() wartenden udp_reader-thread abbrechen
		}
	}

	printf("waiting for udp_reader exit before exit udpserver\n");
	pthread_join(thread_udp_reader, NULL);	// auf thread_udp_alive-Beendigung warten

	close(sock);
	printf("exit udpserver\n");
	return NULL;
}



void *udp_reader(void *ptr)
{
	int end = 0;	// end = 1 -> funktion (thread) benden
	int n;
	int udpport, sock;
	struct nwthreadargs *myargs;
	struct sockaddr_in from;
	socklen_t fromlen;
	in_addr_t fromip;	// zur Ermittlung der Client-IP
	char buf[1024];

	myargs = (struct nwthreadargs *)ptr;
	udpport = myargs->port;
	sock = myargs->sock;

	printf("udp_reader started on port %i...\n", udpport);

	fromlen = sizeof(struct sockaddr_in);
	while (!end) {
		printf("udp reader loop\n");
		n = recvfrom(sock,buf,1024,0,(struct sockaddr *)&from,&fromlen);	// blockt !!
		if (n < 0) { printf("Error on receiving UDP\n"); }
		else //Message erhalten
		{
			fromip= htonl(from.sin_addr.s_addr);	// Bytereihenfolge berichtigen
			unsigned char bytes[4];
			bytes[0] = fromip & 0xFF;
			bytes[1] = (fromip >> 8) & 0xFF;
			bytes[2] = (fromip >> 16) & 0xFF;
			bytes[3] = (fromip >> 24) & 0xFF;
			printf("UDP-Message received from IP-Address = %d.%d.%d.%d\n", bytes[3], bytes[2], bytes[1], bytes[0]);
		}

		//sendabroadcast(sock, *udpport, "<iarcs:raspicamserver1>");	// nur Test

		if (getNWThreadEnd()) { end = 1; }	//exit check (wird threadsicher abgefragt)
	}

	printf("exit udp_reader");

	return NULL;
}





// Thread: checks, die die 1x in der Sekunde durchgeführt werden sollen. soll sich beenden, wenn kein client verbunden ist
void *check_every_second(void *ptr)
{
	int *connsd;	// arg aus dem thread-start
	int end = 0;	// end = 1 -> funktion (thread) beenden

	connsd = (int *) ptr;

	printf("check_every_second started\n");

	while (!end)
	{
		printf("1s loop\n");
		// Aktionen ausführen, wenn ein Controller verbunden ist
		if (getConnectionStatus())
		{
			//hangup detection, falls die Gegenseite "unhöflich" die Verbindung trennt (also einfach trennt, ohne den Trennbefehl zu senden
			if ((getTimeWoData() > 2) && (alivecheck == 1))
			{
				printf("Zu lange keine Daten empfangen!\n");
				end = 1;
				pthread_cancel(thread_tcp_clientloop);	// den am read() wartenden client-thread abbrechen
			};

			//if (!end) { tcp_send_safe("<s:0>"); }	//Statusmeldungen senden // TODO: vorerst wird speed vom MC einfach weitergeleitet
		}
		else { end = 1; }

		if (getNWThreadEnd()) {	end = 1; }	//exit check (wird threadsichere abgefragt)
		if (!end) { sleep(1); } //1s warten
	}

	printf("exit check_every_second\n");

	return NULL;
}


// Senden an den Controller (die eine/std. tcp-Verbindung!!!)
// Achtung: tcpconnectionsock wird aus globaler Variable geholt
void tcp_send(char *msg)
{
	ssize_t n, data_sent, data_tosend;

	if (getConnectionStatus())	// nur senden, wenn lt. Program eine Verbindung besteht
	{
		data_tosend = strlen(msg);
		data_sent = 0;

		//printf("tcp_send starting\n");

		while (data_sent < data_tosend)	// es könnte sein, dass nicht die ganze msg in einem Aufruf gesendet wird
		{
			n = write(tcpconnectionsock, msg, strlen(msg));
			if (n < 0) { printf("Error writing tcp data\n"); }
			else
			{
				data_sent += n;
				msg += n;
			}
		}
	}
	//printf("tcp_send finishing\n");
}


//abgesichert, dass nicht aus 2 threads gleichzeitig gesendet wird
void tcp_send_safe(char *msg)
{
	pthread_mutex_lock(&mutex_tcpsend);
	tcp_send(msg);
	pthread_mutex_unlock(&mutex_tcpsend);
}


void sendabroadcast(int broadcastSock, int port, const char *msg)
{
	struct sockaddr_in s;

	if (broadcastSock < 0) { return; }

	memset(&s, '\0', sizeof(struct sockaddr_in));
	s.sin_family = AF_INET;
	s.sin_port = (in_port_t)htons(port);
	s.sin_addr.s_addr = htonl(INADDR_BROADCAST);	// TODO: durch richtige Broadcast-Adresse (zB 10.0.0.255) ersetzen!!


	pthread_mutex_lock(&mutex_udpsend);

	if(sendto(broadcastSock, msg, strlen(msg), 0, (struct sockaddr *)&s, sizeof(struct sockaddr_in)) < 0)
	{
		printf("Error on sendto broadcast");
	}

	pthread_mutex_unlock(&mutex_udpsend);
}


// TODO: Hilfsfunktionen, noch checken und anpassen

//==============================================================================
// function: IP2Broadcast
// description: Takes a subnet mask and IP returns a subnet broadcast address
//==============================================================================

// http://openbook.rheinwerk-verlag.de/linux_unix_programmierung/Kap11-008.htm#RxxKap110080400038D1F039100

in_addr_t IP2Broadcast(in_addr_t IP, in_addr_t Mask)
{
	in_addr_t bcIP;
	in_addr_t bit;
	in_addr_t notbit;
	int i;

	bit = 0x80000000;
	notbit = 0xFFFFFFFF;
	bcIP = 0;
	for ( i=0; i<31; i++)
	{
		if ((bit & Mask) == 0)
			break;

		bcIP |= (IP & bit);
		notbit &= ~bit;
		bit = bit >> 1;
	}
	//bcIP != notbit;
	//printf("bcIP = %s\n",inet_ntoa(inet_makeaddr(bcIP,0)));
	return bcIP;
}


//==============================================================================
// function: GetLocalIP
// description: retrieve current network ip and mask
// TODO: so nicht verwenden, subnetmask wird auch nicht geholt!!
//==============================================================================
int GetIP(char *ifname, in_addr_t *ip, in_addr_t *mask)
{
	struct ifreq ifr;
	int sock = socket(AF_INET,SOCK_DGRAM,0);

	if (sock < 0)
	{
	    perror("socket");
	    return 0;
	  }

	// Get the interface IP address
	strcpy( ifr.ifr_name, ifname );
	ifr.ifr_addr.sa_family = AF_INET;

	if (ioctl( sock, SIOCGIFADDR, &ifr ) < 0) { perror( "SIOCGIFADDR" ); }

	*ip = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;
	shutdown(sock,SHUT_RDWR);

	return 0;
}

// ermittelt lokale ip-adressen: funktioniert! name (eth0..) + adresse
//http://stackoverflow.com/questions/212528/get-the-ip-address-of-the-machine
void printLocalIPs()
{
	struct ifaddrs * ifAddrStruct=NULL;
	    struct ifaddrs * ifa=NULL;
	    void * tmpAddrPtr=NULL;

	    getifaddrs(&ifAddrStruct);

	    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
	        if (!ifa->ifa_addr) {
	            continue;
	        }
	        if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
	            // is a valid IP4 Address
	            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
	            char addressBuffer[INET_ADDRSTRLEN];
	            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
	            printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
	        } else if (ifa->ifa_addr->sa_family == AF_INET6) { // check it is IP6
	            // is a valid IP6 Address
	            tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
	            char addressBuffer[INET6_ADDRSTRLEN];
	            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
	            printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
	        }
	    }
	    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);

}


// liefert Anzahl Sekunden seit dem letzten tcp-Datenempfang
int getTimeWoData()
{
	time_t now, lastdata;
	int time_wo_data;

	time(&now);

	pthread_mutex_lock(&mutex_lasttcp_data);
	lastdata = lasttcp_data;
	pthread_mutex_unlock(&mutex_lasttcp_data);

	time_wo_data = ((int) now - lastdata);

	return time_wo_data;
}


void setConnected()
{
	pthread_mutex_lock(&mutex_connected);
	connected = 1;
	pthread_mutex_unlock(&mutex_connected);
}

void setDisconnected()
{
	pthread_mutex_lock(&mutex_connected);
	connected = 0;
	pthread_mutex_unlock(&mutex_connected);
}

uint8_t getConnectionStatus()
{
	uint8_t status = 0;
	pthread_mutex_lock(&mutex_connected);
	status = connected;
	pthread_mutex_unlock(&mutex_connected);

	return status;
}

uint8_t getNWThreadEnd()
{
	uint8_t ende = 0;
	pthread_mutex_lock( &mutex_end );
	if (threadend) { ende = 1; }
	pthread_mutex_unlock( &mutex_end );

	return ende;
}

/*
void addToSendlist(char *msg)
{
	pthread_mutex_lock( &mutex_tcpsendlist );

	struct sendlist *zeiger;

	zeiger = tcpsendlist;

	if (zeiger != NULL)	// bereits Einträge vorhanden
	{
		while(zeiger->next != NULL) { zeiger=zeiger->next; }
	}

	zeiger=malloc(sizeof(struct sendlist));
	strcpy(zeiger->msg, msg);
	zeiger->next = NULL;

	pthread_mutex_unlock( &mutex_tcpsendlist );
} */



