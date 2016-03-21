/*
 * commands.h
 *
 *  Created on: 25.08.2013
 *      Author: Michael Brunnbauer
 */

#ifndef COMMANDS_H_
#define COMMANDS_H_


extern int checkcmd(char *nettxt, int cmdorigin, char *cmdbuffer);
extern int getcmd(char *text, char *cmd);
extern int parsecmd(char *singlecmd);
extern int parse_mc_cmd(char *singlecmd);
extern void sendCMDtoNet(char *cmddata);
extern void sendsimplecmdtomc(char *cmddata);
extern int strmovetostart(char *oristring, char *newstart);

#endif /* COMMANDS_H_ */
