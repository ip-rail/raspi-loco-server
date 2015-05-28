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
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

//#include "wiringPi.h"
//#include "wiringPiI2C.h"
#include "raspilokserver.h"
#include "commands.h"
#include "ledc.h"
#include "uart.h"



int checkcmd(char *nettxt)
{
	char *start, *end;
	char command[UART_MAXCMDLEN+1];	// auszuwertender Befehl

	printf("Text to check: '%s'\n", nettxt);
	bzero(command,UART_MAXCMDLEN);

	// cmdbuffer (der aufghobene, unvollständige befehlstext) muss mit einem '<' anfangen! -> checken, alles davor entfernen
	start=strchr(cmdbuffer, '<');
	end=strchr(cmdbuffer, '>');


	if (start == NULL) // wenn kein Startzeichen vorhanden ist, ist der komplette Buffer zu löschen
	{
		if (cmdbuffer[0] != 0) { bzero(cmdbuffer,256); }
	}
	else
	{
		if (start > cmdbuffer)	// start ist nicht das erste Zeichen -> Mist vor dem Befehl vorhanden -> entfernen
		{
			int error = strmovetostart(cmdbuffer, start);
			if (error > 0) { printf("Fehler beim Säubern des gespeicherten Befehlstextes aufgetreten\n"); }
		}
	}

	if (strlen(cmdbuffer) + strlen(nettxt) < 256) { strcat(cmdbuffer, nettxt); }	// neu empfangenen Text an aufgehobenen Befehlstext anhängen
	else { printf("Fehler: cmdbuffer ist zu klein!!!\n"); }	//TODO: was machen..

	int gefunden = getcmd(cmdbuffer, command);	// liefert einen Befehl in command
	while (gefunden)
	{
		//printf("Test3\n");
		int returnwert = parsecmd(command);

		if (returnwert > 0) { return returnwert; }	// Befehl ausführen, Returnwert 2 = Verbindung mit Client trennen, 1 = exit tcpserver
		else { gefunden = getcmd(cmdbuffer, command);	} // prüfen, ob noch ein vollständiger Befehl zur Ausführung vorhanden ist
	}

	return 0;
}

int getcmd(char *text, char *cmd)
{
	// liefert einen Befehl aus text in cmd. Returnwert 1, wenn ein Befehl gefunden wurde
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

			//gültigen Befehl aus cmdbuffer entfernen (samt <>)
			if (strlen(text) > (unsigned int)cmdlen) { strmovetostart(text, end+1); }	// wenn noch weitere daten vorhanden , dann diese an den anfang verschieben
			else { bzero(cmdbuffer,256); }	// sonst cmdbuffer löschen
			printf("getcmd: Befehl gefunden: %s\n", cmd);
			return 1;	// es wurde ein Befehl gefunden!
		}
	}
	printf("getcmd: kein Befehl im text gefunden\n");
	return 0;	// es wurde kein Befehl gefunden!
}

int parsecmd(char *singlecmd)
{
	//unsigned char sendcmd[UART_MAXCMDLEN] = "<";	// für Befehle, die zum Mikrocontroller weitergesendet werden sollen

	if(!strcmp(singlecmd, "stop"))	// <stop>
	{
		printf("Received STOP command\n");
		sendsimplecmdtomc(singlecmd);	// cmd an MC weitergeben
		return 0;	// Befehl wird hier abgearbeitet
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

	else if(!strcmp(singlecmd, "exit"))	// <exit>
	{
		printf("Received command to exit the server\n");
		return 1;
	}

	else if(!strcmp(singlecmd, "bye"))	// <bye> statt <dc>
	{
		printf("Received command to disconnect the client\n");
		return 2;
	}


	else if(!strcmp(singlecmd, "alive"))	// <alive>	// dummy-Befehl für akive-Meldungen von der Gegenstelle
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


// sendsimplecmdtomc: Command über UART an den Mikrocontroller weiterschicken - für einfache Befehle, die nur geklammert <>
// und nicht spezifisch zusammengesetzt werden müssen
void sendsimplecmdtomc(char *cmddata)
{
	char sendcmd[UART_MAXCMDLEN] = "<";	// für Befehle, die zum Mikrocontroller weitergesendet werden sollen

	strcat (sendcmd, cmddata);
	strcat (sendcmd, ">");

	printf("sendsimplecmdtomc: Send cmd to MC: %s\n", sendcmd);
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

