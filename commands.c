/*
 * commands.c
 *
 *  Created on: 25.08.2013
 *      Author: Michael Brunnbauer
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

#include "raspilokserver.h"
#include "commands.h"
#include "raspinetwork.h"
#include "ledc.h"
#include "uart.h"



int checkcmd(char *nettxt, int cmdorigin, char *cmdbuffer)
{
	char *start, *end;
	char command[UART_MAXCMDLEN+1];	// auszuwertender Befehl

	//printf("Text to check: '%s'\n", nettxt);
	bzero(command,UART_MAXCMDLEN+1);

	// cmdbuffer (=der aufghobene, unvollständige befehlstext) muss mit einem '<' anfangen! -> checken, alles davor entfernen
	start=strchr(cmdbuffer, '<');
	end=strchr(cmdbuffer, '>');


	if (start == NULL) // wenn kein Startzeichen vorhanden ist, ist der komplette Buffer zu löschen
	{
		if (cmdbuffer[0] != 0) { bzero(cmdbuffer,CMDBUFFER_SIZE); }
	}
	else
	{
		if (start > cmdbuffer)	// start ist nicht das erste Zeichen -> Mist vor dem Befehl vorhanden -> entfernen
		{
			int error = strmovetostart(cmdbuffer, start);
			if (error > 0) { printf("Fehler beim Säubern des gespeicherten Befehlstextes aufgetreten\n"); }
		}
	}

	if (strlen(cmdbuffer) + strlen(nettxt) < CMDBUFFER_SIZE) { strcat(cmdbuffer, nettxt); }	// neu empfangenen Text an aufgehobenen Befehlstext anhängen
	else { printf("Fehler: cmdbuffer ist zu klein!!!\n"); }	//TODO: was machen..

	int gefunden = getcmd(cmdbuffer, command);	// liefert einen Befehl in command
	while (gefunden)
	{
		int returnwert;

		if (cmdorigin == CMD_FROM_NETWORK) { returnwert = parsecmd(command); }
		else if (cmdorigin == CMD_FROM_UART) { returnwert = parse_mc_cmd(command); }


		if (returnwert > 0) { return returnwert; }	// Befehl ausführen, Returnwert 2 = Verbindung mit Client trennen, 1 = exit tcpserver
		else { gefunden = getcmd(cmdbuffer, command);	} // prüfen, ob noch ein vollständiger Befehl zur Ausführung vorhanden ist
	}

	return 0;

	/* Returnwert
	 * 0: Standardwert
	 * 1: exit tcpserver, von parsecmd()
	 * 2: Verbindung mit Client trennen (tcpserver), von parsecmd()
	 */
}

int getcmd(char *text, char *cmd)
{
	// liefert einen Befehl aus text in cmd. Returnwert 1, wenn ein Befehl gefunden wurde, sonst 0
	char *start, *end;
	int cmdlen;

	//printf("Text mit getcmd prüfen: %s\n", text);

	start=strchr(text, '<');

	if (start != NULL)
	{
		//printf("getcmd: start im text gefunden\n");
		end=strchr(text, '>');
		if (end != NULL)
		{
			//printf("getcmd: end im text gefunden\n");
			cmdlen = end-start;
			if (cmdlen > 0) { strncpy(cmd, start+1, end-start-1); }	// gültigen Befehl herauskopieren (ohne <>)
			else { printf("ausstieg cmdlen\n"); return 0; }

			//gültigen Befehl aus text entfernen (samt <>)
			if (strlen(text) > (unsigned int)cmdlen) { strmovetostart(text, end+1); }	// wenn noch weitere daten vorhanden , dann diese an den anfang verschieben
			else { bzero(text,CMDBUFFER_SIZE); }	// sonst text löschen
			//printf("getcmd: Befehl gefunden: %s\n", cmd);
			return 1;	// es wurde ein Befehl gefunden!
		}
	}
	//printf("getcmd: kein Befehl im text gefunden\n");
	return 0;	// es wurde kein Befehl gefunden!
}

//Befehle aus dem Netzwerk auswerten
int parsecmd(char *singlecmd)
{
	if(!strcmp(singlecmd, "exit"))	// <exit>
	{
		printf("Received command to exit the server\n");
		return 1;
	}

	else if(!strcmp(singlecmd, "bye"))	// <bye> statt <dc>
	{
		printf("Received command to disconnect the client\n");
		return 2;
	}

	// --- vor dieser Zeile Befehle auswerten, die auch im transparenten Modus (servermode 0) am Raspi verarbeitet werden sollen ----------------------------------

	if (servermode == 0) 	// im servermode 0 ALLE weitern Befehle direkt an den MC senden
	{
		sendsimplecmdtomc(singlecmd);	// cmd an MC weitergeben
		return 0;	// Befehl wird hier abgearbeitet
	}


	if(!strcmp(singlecmd, "stop"))	// <stop>
	{
		printf("Received STOP command\n");
		sendsimplecmdtomc(singlecmd);	// cmd an MC weitergeben
		return 0;
	}

	else if(!strncmp(singlecmd, "sd:", 3))	// <sd:nnn>  Geschwindgkeitsbefehl
	{
		printf("Received sd command\n");
		sendsimplecmdtomc(singlecmd);	// cmd an MC weitergeben
		return 0;
	}

	else if(!strncmp(singlecmd, "richtung:", 9))	// <richtung:ss>  Richtungswechsel
	{
		sendsimplecmdtomc(singlecmd);	// cmd an MC weitergeben
		return 0;
	}

	else if(!strncmp(singlecmd, "ping", 4))	// <ping>
	{
		sendsimplecmdtomc(singlecmd);	// cmd an MC weitergeben
		return 0;
	}

	else if(!strcmp(singlecmd, "alive"))	// <alive>	TODO: weg damit! // dummy-Befehl für aktive-Meldungen von der Gegenstelle
	{
		printf("Alive!\n");
		return 0;
	}

	else if(!strncmp(singlecmd, "l1:", 3))	// <l1:*nummer*> Licht einschalten: Licht *Nummer* 1-16
	{
		int lednummer = 0;
		long long_val;
		char test[UART_MAXCMDLEN+1];

		if (PLED01_ok)	// wenn der LED-Controller aktiv ist
		{
			bzero(test,UART_MAXCMDLEN+1);
			printf("LEDc: singlecmd+3=%s\n",singlecmd+3);
			strncpy(test, singlecmd+3, strlen(singlecmd+3));		// die Zahl rauskopiern test sollte leer sein
			printf("LEDc: Teststring mit LED-Nummer=%s\n",test);

			long_val = strtol(test, NULL, 10);
			printf("LEDc: LED-Nummer long=%lu\n",long_val);
			lednummer = (int) long_val;
			printf("LEDc: LED-Nummer=%i\n",lednummer);
			if ((lednummer > 0) && (lednummer < 17)) { ledc_led_setpwm(I2CSLAVE_ADDR_PLED01, lednummer, 255);  }	//TODO: LED vorerst voll aufdrehen
			else { printf("Error cmd getcmd: <l1:n> LED-number %i not valid!\n", lednummer);}
		}
		else { sendsimplecmdtomc(singlecmd); }	// cmd an MC weitergeben

		return 0;
	}

	else if(!strncmp(singlecmd, "l0:", 3))	// <l0:*name*> Licht ausschalten: Licht *Nummer* 1-16
	{
		int lednummer = 0;
		long long_val;
		char test[UART_MAXCMDLEN+1];

		if (PLED01_ok)	// wenn der LED-Controller aktiv ist
		{
			bzero(test,UART_MAXCMDLEN+1);
			strncpy(test, singlecmd+3, strlen(singlecmd+3));		// die Zahl rauskopiern test sollte leer sein
			long_val = strtol(test, NULL, 10);
			lednummer = (int) long_val;
			if ((lednummer > 0) && (lednummer < 17)) { ledc_led_setpwm(I2CSLAVE_ADDR_PLED01, lednummer, 0);  }
			else { printf("Error cmd getcmd: <l1:n> LED-number %i not valid!\n", lednummer);}
		}
		else { sendsimplecmdtomc(singlecmd); }	// cmd an MC weitergeben

		return 0;
	}

	else if(!strcmp(singlecmd, "lr"))	// <lr>	// TODO: LED-controller reset - nur für Test
		{
			ledc_reset();
			return 0;
		}




	return 0;
}


// Befehle vom Mikrocontroller auswerten und wenn nötig an Netzwerk weiterleiten
int parse_mc_cmd(char *singlecmd)
{

	if(!strncmp(singlecmd, "iam:1:", 6))	// <iam:1:name>  Lok meldet ihren Namen -> speichern in lokname[41];
	{
		bzero(lokname,sizeof(lokname));
		//strlcpy(lokname, singlecmd+6, sizeof(lokname)); // Befehl gibt's hier nicht?
		strncpy (lokname, singlecmd+6, strlen(singlecmd+6));	// TODO: Achtung: Länge des Loknamens ist nicht abgesichert -> ändern
		printf("Received Loknamen: %s\n", lokname);
		sprintf(cmd_iam, "<iam:1:%s>",lokname);		// Befehlstext für iam-Meldung ändern
		// TODO: iam Befehl weitersenden (bei bestehender Verbindung mit tcp, bei aktivem Netzwerk mit udp, sonst nicht)
		return 0;
	}
	else	// Befehl nicht gefunden
	{
		if (servermode == SERVERMODE_TRANSPARENT)
		{
			sendCMDtoNet(singlecmd);	// im transparenten Modus unbekannte Befehle an Controller weitersenden
		}

		return 0;
	}
}


// TODO: parse_udp_cmd() -> Meldungen für alle auswerten


//Befehlstext wieder mit spitzen Klammern versehen und +ber tcp-Verbindung senden
void sendCMDtoNet(char *cmddata)
{
	char sendcmd[UART_MAXCMDLEN+1] = "<";

	if (strlen(cmddata) <= (UART_MAXCMDLEN-2))
	{
		strcat (sendcmd, cmddata);
		strcat (sendcmd, ">");
		tcp_send_safe(sendcmd);
	}
}


// sendsimplecmdtomc: Command über UART an den Mikrocontroller weiterschicken - für einfache Befehle, die nur geklammert <>
// und nicht spezifisch zusammengesetzt werden müssen
void sendsimplecmdtomc(char *cmddata)
{
	char sendcmd[UART_MAXCMDLEN] = "<";	// für Befehle, die zum Mikrocontroller weitergesendet werden sollen

	strcat (sendcmd, cmddata);
	strcat (sendcmd, ">");

	//printf("sendsimplecmdtomc: Send cmd to MC: %s\n", sendcmd);
	uart_write(sendcmd);	// cmd an MC weitergeben
}

int strmovetostart(char *oristring, char *newstart)
{
	// entfernt alle Zeichen vor newstart in einem String
	char *tempstring;

	tempstring = strdup(newstart);

	if (tempstring == NULL) { printf("Fehler beim Umkopieren !\n"); return 1;  }

	bzero(oristring,strlen(oristring));
	strncpy(oristring, tempstring, strlen(tempstring));
	free(tempstring);	// Speicher muss wieder freigegeben werden!

	return 0;
}

