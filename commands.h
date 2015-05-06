/*
 * commands.h
 *
 *  Created on: 25.08.2013
 *      Author: Michael Brunnbauer
 */

#ifndef COMMANDS_H_
#define COMMANDS_H_


extern int checkcmd(char *nettxt);
extern int getcmd(char *text, char *cmd);
extern int parsecmd(char *singlecmd);
extern void sendsimplecmdtomc(char *cmddata);
extern int strmovetostart(char *oristring, char *newstart);

#endif /* COMMANDS_H_ */
