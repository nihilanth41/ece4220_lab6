#include <stdio.h>
#include <stdlib.h>

/* TODO
   Essentially same code as lab5 -- determine master/slave relatoinship.

   This time the messages are @A,B,C,D,E
   if(messageReceived)
   {
   	if(Master)
	{
		Change frequency
		 - Write message to RTFIFO 
		 - Trigger software interrupt handler by writing to appropriate register
		Broadcast new note to slaves
	}
	else if(Slave)
	{
		Change frequency
	}
}
