# raspi-loco-server
Raspberry Pi network server software for controlling G-scale train engines over wifi

##Voraussetzungen
* Linux: Raspbian Wheezy
* serielle Schnittstelle: Konsolenoutput deaktivieren (/etc/inittab, /boot/cmdline.txt)
* I2C aktivieren https://learn.adafruit.com/adafruits-raspberry-pi-lesson-4-gpio-setup/configuring-i2c
    andere I2C-Anleitung: http://skpang.co.uk/blog/archives/575
* benötigte Bibliothek: http://www.hyperrealm.com/libconfig/
* nicht mehr benötigte Bibliothek: WiringPi http://wiringpi.com/
* apt-get install libi2c-dev
* Libraries werden nicht gefunden - Abhilfe: sudo ldconfig -v

## Installation am Raspi
* Source in ein (User-) Verzeichnis kopieren
* kompilieren: gcc -Wall -o raspilokserver raspilokserver.c commands.c uart.c ledc.c raspinetwork.c raspispec.c -lconfig -lpthread
* läuft bisher nur unter root

## Funktionalität
* übernimmt WLAN-Kommunikation für ATMega2561 board
* i2c LED-Controller
* Konfiguration über lokserver.cfg (im Programmverzeichnis)
