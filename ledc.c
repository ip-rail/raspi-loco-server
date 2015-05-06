/*
 * ledc.c
 *
 * Funktionen für PLED01 I2C LED Controller Modul (PCA9622)
 *
 *  Created on: 04.05.2015
 *      Author: Michael Brunnbauer
 */

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "raspilokserver.h"
#include "ledc.h"


// I2C Zugriff initialisieren
// es wird der aktive i2c-Bus ermittelt und in i2c_active gespeichert
// Infos: http://www.netzmafia.de/skripten/hardware/RasPi/RasPi_I2C.html
// https://xgoat.com/wp/2007/11/11/using-i2c-from-userspace-in-linux/
// https://www.kernel.org/doc/Documentation/i2c/dev-interface

const char *i2c_path[2] = {"/dev/i2c-0", "/dev/i2c-1"};

void i2c_init()
{
	printf("Checking /dev/i2c-0 .. ");
	// checken, ob i2c aktiv ist, aktiven i2c-bus merken (0 oder 1)
	if( access(i2c_path[0], F_OK) != -1) { i2c_active = 0; printf("ok\n"); }
	else { i2c_active = 0; printf("not active\n"); }
	printf("Checking /dev/i2c-1 .. ");
	if( access(i2c_path[1], F_OK) != -1) { i2c_active = 1; printf("ok\n");}
	else { i2c_active = 0; printf("not active\n"); }
}


// per ioctl() Kommunikation mit einem i2c Slave freigeben
// gibt filedescriptor zurück - muss nach Beendigung des i2c-Zugriffs wieder geschlossen werden
// -> die Verbindung slave-Adresse <-> filedescriptor stellt nur der ioctl-Aufruf her!!
int i2c_open_slave(int slaveaddr)
{
	int i2c_fd = -1;

	printf("Trying to open I2C address %x on i2c bus nr %i\n", slaveaddr, i2c_active);

	if (i2c_active >= 0)
	{
		i2c_fd = open(i2c_path[i2c_active], O_RDWR);

		if (i2c_fd >= 0)
		{
			if (ioctl(i2c_fd, I2C_SLAVE, slaveaddr) < 0)
			{
				printf("Failed to acquire bus access and/or talk to slave addr %x\n", slaveaddr);
			}
		}
		else
		{
			printf("Failed to open i2c bus: %s\n", i2c_path[i2c_active]);
		}

	}
	return(i2c_fd);
}



/*
 * https://www.kernel.org/doc/Documentation/i2c/dev-interface
 * http://www.netzmafia.de/skripten/hardware/RasPi/RasPi_I2C.html

i2c_smbus_write_quick(int devicehandle, __u8 value)
i2c_smbus_read_byte(int devicehandle)
i2c_smbus_write_byte(int devicehandle, __u8 value)
i2c_smbus_read_byte_data(int devicehandle, __u8 command)
i2c_smbus_write_byte_data(int devicehandle, __u8 command, __u8 value)
i2c_smbus_read_word_data(int devicehandle, __u8 command)
i2c_smbus_write_word_data(int devicehandle, __u8 command, __u16 value)
i2c_smbus_read_block_data(int devicehandle, __u8 command, __u8 *values)
i2c_smbus_write_block_data(int devicehandle, __u8 command, __u8 length, __u8 *values)
i2c_smbus_read_i2c_block_data(int devicehandle, __u8 command, __u8 *values)
i2c_smbus_write_i2c_block_data(int devicehandle, __u8 command, __u8 length, __u8 *values)
i2c_smbus_process_call(int devicehandle, __u8 command, __u16 value)
i2c_smbus_block_process_call(int devicehandle, __u8 command, __u8 length, __u8 *values)

zB:
i2c_smbus_write_byte_data(fd,reg,value);	// -1 (<0) bedeutet Fehler
if((value = i2c_smbus_read_byte_data(fd, reg)) < 0)	// <0 = Fehler

Einige Lese-Funktionen liefern einen Long-Wert (32 Bit) zurück, es ist also gegebenenfalls ein Typecast notwendig. Hier hilft ein Blick in die Headerdatei linux/i2c-dev.h, wo alles gut kommentiert ist.

All these transactions return -1 on failure; you can read errno to see what happened. The 'write' transactions return 0 on success; the
'read' transactions return the read value, except for read_block, which returns the number of values read. The block buffers need not be longer
than 32 bytes.
*/

//grundlegende Einstellungen für PCA9622
void ledc_init(int slaveaddr)
{
	int i2c_fd;
	const __u8 leddata[] = { 0xff, 0xff, 0xff, 0xff };

	i2c_fd = i2c_open_slave(slaveaddr);

	// Zuerst wird der allgemeine Betriebsmodus konfiguriert: Mode1-Register einstellen (sleep auf off)
	if (i2c_smbus_write_byte_data(i2c_fd, PCA9622_ADDR_MODE1, 0x81) < 0)
	{
		printf("Error: LED-Controller: setting Mode1 register failed! LED-Controller is not active!\n");
		PLED01_ok = 0;
	}
	else { PLED01_ok = 1; }	// PLED01 funktioniert

	// LED driver output state register: fix alles aufdrehen (0xff)
	/*
    	0x00 für LEDs aus
    	0x55 für LEDs an auf 100%
    	0xaa für LEDs an mit individuellen Helligkeit des jeweiligen LED-Kanals
    	0xff für LEDs an mit individuellen Helligkeit des jeweiligen LED-Kanals samt Gesamthelligkeit oder Blinken
	 */

	if (PLED01_ok)	// nur machen, wenn der Chip ansprechbar ist
	{
		if (i2c_smbus_write_i2c_block_data(i2c_fd, PCA9622_ADDR_LEDOUT0 + PCA9622_AUTOINC_ALL, 4, leddata) < 0)	// <0 bedeutet Fehler
		{
			printf("Error: ledcontroller: setting LED driver output state registers failed!\n");
		}
	}

	close(i2c_fd);
}

// Software Reset für alle PCA9622 am i2c Bus
// i2cset -y 0 0x03 0xa5 0x5a i
void ledc_reset()
{
	int i2c_fd;
	const __u8 resetdata[] = { 0x5a };

	i2c_fd = i2c_open_slave(PCA9622_I2C_RESET_ADDR);

	if (i2c_smbus_write_i2c_block_data(i2c_fd, 0xa5, 1, resetdata) < 0)	// <0 bedeutet Fehler
	{
		printf("Error: ledcontrol_reset failed!\n");
	}
	close(i2c_fd);

}

void ledc_led_setpwm(int slaveaddr, uint8_t lednumber, uint8_t value)
{
	int i2c_fd;

	i2c_fd = i2c_open_slave(slaveaddr);

	if (i2c_smbus_write_byte_data(i2c_fd, PCA9622_ADDR_PWM0+lednumber-1, value) < 0)
	{
		printf("Error: ledc_led_setpwm failed!\n");
	}
	close(i2c_fd);
}



