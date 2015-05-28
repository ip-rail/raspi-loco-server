/*
 * raspispec.c
 *
 * Rasperry Pi system specific functions
 *
 *  Created on: 07.05.2015
 *      Author: Michel Brunnbauer
 */


#include <fcntl.h> // for open
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h> // for close

#include "raspispec.h"


struct rpidata {
	unsigned long revision;		// RevisionsID
	char model[15];		// Board (A,B,A+,B+,2B..)
	int ram;				// RAM in MB (256/512/1024)
	char pcbrev[4];		// PCB Revision
	uint8_t i2cbusnr;	// I2C Bus Nummer am GPIO
	char serial[17];		// Pi serial number
} raspidata;

const char *raspitemppath = "/sys/class/thermal/thermal_zone0/temp";

// Raspi Board identifizieren
/*
 *  RevisionsID
 *  Board
 *  RAM
 *  PCB-Revision -> I2C-Bus Nummer
 *
 *  Info: http://www.makelinux.net/alp/050
 */
void raspi_id()
{
	FILE* fp;
	char buffer[1024];
	size_t bytes_read;
	char* match;
	char testref[16];

	// Read the entire contents of /proc/cpuinfo into the buffer.
	fp = fopen ("/proc/cpuinfo", "r");
	bytes_read = fread (buffer, 1, sizeof (buffer), fp);
	fclose (fp);

	// Bail if read failed or if buffer isn't big enough
	if (bytes_read == 0) { printf("Reading /proc/cpuinfo failed!\n"); return; }
	if (bytes_read == sizeof (buffer)) { printf("Reading /proc/cpuinfo: filebuffer not big enough!\n"); return;}

	// NUL-terminate the text
	buffer[bytes_read] = '\0';

	// looking for this:
	//Hardware        : BCM2708
	//Revision        : 000f
	//Serial          : 00000000b43b4b37



	// Locate the line that starts with "Revision".
	match = strstr(buffer, "Revision");

	if (match == NULL)
	{
		printf("Reading /proc/cpuinfo: Hardware data not found!\n");
		return;
	}

	//sscanf (match, "Revision        : %ul", &raspidata.revision);
	sscanf(match, "Revision        : %s", testref);

	raspidata.revision = strtoul(testref, NULL, 16);




	if (raspidata.revision > 0x10000000)
	{
		printf("oO - Raspberry Pi has been over-volted!\n");
		raspidata.revision =  raspidata.revision - 0x10000000;
	}

	match = strstr (buffer, "Serial");

	if (match == NULL)
	{
		printf("Reading /proc/cpuinfo: Serial data not found!\n");
		return;
	}

	sscanf (match, "Serial          : %s", raspidata.serial);

	/*
	Info: http://elinux.org/RPi_HardwareHistory

	Revision 	Release Date 	Model 	PCB Revision 	Memory 	Notes
	0002 		Q1 2012 	B 	1.0 	256MB
	0003 		Q3 2012 	B (ECN0001) 	1.0 	256MB 	Fuses mod and D14 removed
	0004 		Q3 2012 	B 	2.0 	256MB 	(Mfg by Sony)
	0005 		Q4 2012 	B 	2.0 	256MB 	(Mfg by Qisda)
	0006 		Q4 2012 	B 	2.0 	256MB 	(Mfg by Egoman)
	0007 		Q1 2013 	A 	2.0 	256MB 	(Mfg by Egoman)
	0008 		Q1 2013 	A 	2.0 	256MB 	(Mfg by Sony)
	0009 		Q1 2013 	A 	2.0 	256MB 	(Mfg by Qisda)
	000d 		Q4 2012 	B 	2.0 	512MB 	(Mfg by Egoman)
	000e 		Q4 2012 	B 	2.0 	512MB 	(Mfg by Sony)
	000f 		Q4 2012 	B 	2.0 	512MB 	(Mfg by Qisda)
	0010 		Q3 2014 	B+ 	1.0 	512MB 	(Mfg by Sony)
	0011 		Q2 2014 	Compute Module 	1.0 	512MB 	(Mfg by Sony)
	0012 		Q4 2014 	A+ 	1.0 	256MB 	(Mfg by Sony)
	a21041 		Q1 2015 	2 Model B 	1.1 	1GB 	(Mfg by Embest, China)

	https://www.raspberrypi.org/forums/viewtopic.php?f=63&t=98367&start=250 -> neues Schema
	*/


	switch(raspidata.revision)
	{
	      case 2:
	      case 3:
	    	  printf("Das war eins \n");
	    	  //raspidata.model = "B";
	    	  strncpy(raspidata.model, "B", 1);
	    	  raspidata.ram = 256;
	    	  //raspidata.pcbrev = "1.0";
	    	  strncpy(raspidata.pcbrev, "1.0", 3);
	    	  raspidata.i2cbusnr = 0;
	          break;

	      case 4:
	      case 5:
	      case 6:
	    	  //raspidata.model = "B";
	    	  strncpy(raspidata.model, "B", 1);
	    	  raspidata.ram = 256;
	    	  //raspidata.pcbrev = "2.0";
	    	  strncpy(raspidata.pcbrev, "2.0", 3);
	    	  raspidata.i2cbusnr = 1;
	          break;

	      case 7:
	      case 8:
	      case 9:
	    	  //raspidata.model = "A";
	    	  strncpy(raspidata.model, "A", 1);
	    	  raspidata.ram = 256;
	    	  //raspidata.pcbrev = "2.0";
	    	  strncpy(raspidata.pcbrev, "2.0", 3);
	    	  raspidata.i2cbusnr = 1;
	          break;

	      case 0xd:
	      case 0xe:
	      case 0xf:
	    	  //raspidata.model = "B";
	    	  strncpy(raspidata.model, "B", 1);
	    	  raspidata.ram = 512;
	    	  //raspidata.pcbrev = "2.0";
	    	  strncpy(raspidata.pcbrev, "2.0", 3);
	    	  raspidata.i2cbusnr = 1;
	          break;

	      case 0x10:
	    	  //raspidata.model = "B+";
	    	  strncpy(raspidata.model, "B+", 2);
	    	  raspidata.ram = 512;
	    	  //raspidata.pcbrev = "1.0";
	    	  strncpy(raspidata.pcbrev, "1.0", 3);
	    	  raspidata.i2cbusnr = 1;
	          break;

	      case 0x11:
	    	  //raspidata.model = "Compute Module";
	    	  strncpy(raspidata.model, "Compute Module", 14);
	    	  raspidata.ram = 512;
	    	  //raspidata.pcbrev = "1.0";
	    	  strncpy(raspidata.pcbrev, "1.0", 3);
	    	  raspidata.i2cbusnr = 255;	// TODO: ist nicht feststellbar
	    	  break;

	      case 0x12:
	    	  //raspidata.model = "A+";
	    	  strncpy(raspidata.model, "A+", 2);
	    	  raspidata.ram = 256;
	    	  //raspidata.pcbrev = "1.0";
	    	  strncpy(raspidata.pcbrev, "1.0", 3);
	    	  raspidata.i2cbusnr = 1;
	    	  break;

	      case 0xa21041:
	    	  //raspidata.model = "2B";
	    	  strncpy(raspidata.model, "2B", 2);
	    	  raspidata.ram = 1024;
	    	  //raspidata.pcbrev = "1.1";
	    	  strncpy(raspidata.pcbrev, "1.1", 3);
	    	  raspidata.i2cbusnr = 1;
	      	  break;


	} // Ende switch

	printf("Raspi board info: \n");
	printf("Revision = %lx\n", raspidata.revision);
	printf("Model = %s\n", raspidata.model);
	printf("RAM = %i\n", raspidata.ram);
	printf("I2C-Bus = %i\n", raspidata.i2cbusnr);
	printf("Serial = %s\n", raspidata.serial);


}

// Temp auslesen!!!
int getRaspiTemp()
{
	int fd, readcount;
	char buffer[10];
	int temp = 0;

	if ((fd = open(raspitemppath, O_RDONLY)) != -1)
	{
		readcount = read(fd, buffer, 9);	// max 9 Zeichen lesen (es sollten 5 sein)
		if (readcount >= 2) { temp = strtol(buffer, NULL, 10); }
		close(fd);
	}

	return temp;
}

