/*
 * ledc.h
 *
 *  Created on: 04.05.2015
 *      Author: Michael Brunnbauer
 */

#ifndef LEDC_H_
#define LEDC_H_

#define PCA9622_I2C_RESET_ADDR   	0x03	//  Software Reset Adresse der PLED01 (PCA9622) LED-Controller
#define PCA9622_I2C_ALLCALL_ADDR	0x70	// "All Call"-Adresse des PCA9622. Über diese Adresse nimmt den Befehl jedes am I2C-Bus hängende LED-Controller-Modul des gleichen Typs entgegen.
#define PCA9622_AUTOINC_NO	0x00	// no Auto-Increment
#define PCA9622_AUTOINC_ALL	0x80	// Auto-Increment all
// Adressen der Register im PCA9622
#define PCA9622_ADDR_LEDOUT0		0x14	// LED driver output state register (LED 0 - 3)
#define PCA9622_ADDR_LEDOUT1		0x15	// LED driver output state register (LED 4 - 7)
#define PCA9622_ADDR_LEDOUT2		0x16	// LED driver output state register (LED 8 - 11)
#define PCA9622_ADDR_LEDOUT3		0x17	// LED driver output state register (LED 12 - 15)
#define PCA9622_ADDR_MODE1			0x00
#define PCA9622_ADDR_MODE2			0x01
#define PCA9622_ADDR_PWM0			0x02	// PWM (Helligkeits)-Register 1. LED
#define PCA9622_ADDR_GRPPWM			0x12	// Gruppen-PWM (Helligkeit)
#define PCA9622_ADDR_GRPFREQ		0x13	// Gruppen-Blink-Frequenz
#define I2CSLAVE_ADDR_PLED01 		0x0e 	// Std.-Adresse des (1.) LED-Controllers


extern void i2c_init();
extern int i2c_open_slave(int slaveaddr);
extern void ledc_init(int slaveaddr);
extern void ledc_reset();
extern void ledc_led_setpwm(int slaveaddr, uint8_t lednumber, uint8_t value);

#endif // LEDC_H_
