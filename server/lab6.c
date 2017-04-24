#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <time.h>

#define MSG_SIZE 40
#define IP_MAX 10
#define IP_MIN 1
#define VOTE_TIMEOUT_SEC 5

// prototypes
int newMaster(void);
void getIPAddr(struct sockaddr_in *addr);
void socket_transciever(int sockfd, struct sockaddr_in *addr, int len);

// Globals
int VOTES[IP_MAX + 1] = { 0 };
int last_octet;
char ip_str[INET_ADDRSTRLEN];
char bcast_str[INET_ADDRSTRLEN];

void socket_transciever(int sockfd, struct sockaddr_in *addr, int len) {
  char recvbuf[MSG_SIZE];
  char msgbuf[MSG_SIZE];
  char tokbuf[MSG_SIZE] = { 0 };
  // Receieve
  while(1)
    {
      struct sockaddr_in from;
      socklen_t fromlen = sizeof(from);
      int n;
      bzero(&recvbuf, MSG_SIZE);
      bzero(&msgbuf, MSG_SIZE);

      // 
      n = recvfrom(sockfd, recvbuf, MSG_SIZE, 0, (struct sockaddr *)&from, fromlen);
      if(n < 0) {
	perror("Error receiving data\n"); }
      else 
      {
	printf("Received message\n");
      ('\n' == recvbuf[strlen(recvbuf)-1]) ? memcpy(msgbuf, recvbuf, strlen(recvbuf)-1) : memcpy(msgbuf, recvbuf, strlen(recvbuf));
      

      // Check if voting is occuring 
//      if('#' == msgbuf[0])
//	{
//	  tok = strtok(msgbuf, ". ");
//	  count = 6;
//	    while(tok != NULL)
//	      {
//		count--;
//		(1 == count
      printf("Message buffer %s\n", msgbuf);
      // Check for VOTE
      if(0 == strcmp(msgbuf, "VOTE") || 0 == strcmp(msgbuf, "vote"))
      {
	      char tmp[MSG_SIZE];
	      char voteBuf[MSG_SIZE] = "# ";
	      int r = rand() % (IP_MAX + 1 - IP_MIN) + IP_MIN;
	      sprintf(tmp, "%d", r);
	      
	      strcat(voteBuf, ip_str);
	      strcat(voteBuf, " ");
	      strcat(voteBuf, tmp);
	      printf("Vote buffer: %s\n", voteBuf);
      }
      } //else()
} // while
} // func()

int main(int argc, char **argv) {
    struct sockaddr_in server;
    int sockfd;
    int portnum = 2000;
    int length = sizeof(server);
    int boolval = 1;
    struct hostent *hp = NULL;
	
    // Check input args for port #
    if(argc < 2) {
	    fprintf(stderr, "Usage is %s hostname [port]\n", argv[0]);
	    return -1;
    }
    if(argc >= 3) {
	portnum = atoi(argv[2]);
    }
    hp = gethostbyname(argv[1]);
    
    srand(time(NULL)); // Random numbers seed

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0) {
	    fprintf(stderr, "socket() %s\n", strerror(errno));
    }

    // Socket info
    bzero(&server, length); // zero out struct
    server.sin_family = AF_INET;
    server.sin_port = htons(portnum);
    bcopy((char *)hp->h_addr, (char *)&server.sin_addr, hp->h_length);

    if( bind(sockfd, (struct sockaddr *)&server, length) < 0 ) {
	    fprintf(stderr, "bind() %s\n", strerror(errno));
    }

    if( setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &boolval, sizeof(boolval)) < 0 ) {
	    fprintf(stderr, "setsockopt() %s\n", strerror(errno));
    }

    getIPAddr(&server);
    printf("My ip: %s\n", ip_str);
    printf("Broadcast ip: %s\n", bcast_str);

    socket_transciever(sockfd, &server, length);
    
    return 0;
}

// Get the current machine ip and the broadcast ip as a string
// And the last octet as an integer. Put them all in global vars.
void getIPAddr(struct sockaddr_in *addr) { 
  char buf[INET_ADDRSTRLEN] = { 0 };
  if(NULL == addr) {
	  return; 
  }
  // Get ip address as string
  struct in_addr ipAddr = addr->sin_addr;
  inet_ntop(AF_INET, &ipAddr, ip_str, INET_ADDRSTRLEN);
  // Get broadcast address as string
  inet_ntop(AF_INET, &ipAddr, bcast_str, INET_ADDRSTRLEN);
  int len = strlen(bcast_str);
  bcast_str[len] = '5';
  bcast_str[len-1] = '5';
  bcast_str[len-2] = '2';
  memcpy(buf, ip_str, strlen(ip_str));
  char *my_ip = (char *)&buf;
  char *tok = strtok(my_ip, ". ");
  int count = 5;
  while(tok != NULL)
    {
      count--;
      if(1 == count)
	{
	  last_octet = atoi(tok);
	}
      tok = strtok(NULL, ". ");
    }
}

// Tally votes. Return 1 if this program is the new master, 0 otherwise;
int newMaster(void) {
  int i = 0;
  int max_votes = 0;
  int new_master = 0;
  for(i=1; i<=IP_MAX; i++)
    {
      printf("[%d]: %d\n", i, VOTES[i]);
      if(VOTES[i] > max_votes)
	{
	  new_master = i;
	  max_votes = VOTES[i];
	}
      else if(max_votes == VOTES[i])
	{
	  if(i > new_master)
	    {
	      new_master = i;
	    }
	}
    }
  if(new_master == last_octet)
    {
      return 1;
    }
  return 0;
}
