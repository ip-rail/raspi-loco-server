/*
 * uart.h
 *
 *  Created on: 02.01.2015
 *      Author: Michael Brunnbauer
 */

#ifndef UART_H_
#define UART_H_

extern void uart_init(void);
extern uint8_t uart_write(const void *data);
extern int uart_read(char *buffer);
extern void *uartlistener(void *ptr);

#endif /* UART_H_ */
