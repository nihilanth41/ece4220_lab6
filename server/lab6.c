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
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define MSG_SIZE 40
#define VOTE_MAX 10
#define VOTE_MIN 1
#define FIFO_READ 1 // Opposite of kernel module
#define FIFO_WRITE 0

// prototypes
void getIPAddr(struct sockaddr_in *addr);
void socket_transciever(int sockfd);

// Globals
int last_octet;
char ip_str[INET_ADDRSTRLEN];
char bcast_str[INET_ADDRSTRLEN];
unsigned long *VIC2SoftInt = NULL; 

void socket_transciever(int sockfd) {
  char recvbuf[MSG_SIZE];
  char msgbuf[MSG_SIZE];
  struct sockaddr_in from;
  socklen_t fromlen = sizeof(from);
  int n, isMaster = 0, my_vote;
  // Receieve
  while(1)
  {

    bzero(&recvbuf, MSG_SIZE);
    bzero(&msgbuf, MSG_SIZE);

    n = recvfrom(sockfd, recvbuf, MSG_SIZE, 0, (struct sockaddr *)&from, (socklen_t *)&fromlen);
    if(n < 0) {
      perror("Error receiving data\n"); 
      continue;
    }
    if(recvbuf[strlen(recvbuf)-1] == '\n') {
      recvbuf[strlen(recvbuf)-1] = '\0';
    }
    memcpy(msgbuf, recvbuf, strlen(recvbuf));
    printf("Received message: %s\n", msgbuf);
    fflush(stdout);

    // Check for @<note> 
    if('@' == msgbuf[0])
    {
      const char notes[] = "ABCDE";
      char c = msgbuf[1];
      c = toupper(c);
      int i;
      for(i=0; i<5; i++)
      {
        if(notes[i] == c) { // Message received
          // Write i to fifo -- change frequency
          int fd = open("/dev/rtf/0", O_WRONLY); // FIFO_WRITE
          if(fd < 0) {
            fprintf(stderr, "Error open() rtfifo %s\n", strerror(errno));
            return;
          }
          if( write(fd, (void *)&i, sizeof(i)) < 0 ) {
            fprintf(stderr, "Error write() rtfifo %s\n", strerror(errno));
          }
          // Set software interrupt
          *VIC2SoftInt |= 1 << 31; // Set MSB
          if(1 == isMaster) { 
            // Broadcast note to slaves if master
            printf("Forwarding message: %s\n", msgbuf);
            from.sin_addr.s_addr = inet_addr(bcast_str); // set broadcast
            if( sendto(sockfd, msgbuf, MSG_SIZE, 0, (struct sockaddr *)&from, fromlen) < 0 ) {
              fprintf(stderr, "sendto() %s\n", strerror(errno));
            }
          }
          break;
        }
      }
    }

    // Check if voting is occuring 
    else if('#' == msgbuf[0])
    {
      int count, vote_val, from_ip;
      char *tok = strtok(msgbuf, ". ");
      count = 6;
      while(tok != NULL)
      {
        printf("%s\n", tok);
        count--;
        if(1 == count) {
          from_ip = atoi(tok);
        }
        if(0 == count) {
          vote_val = atoi(tok);
          break;
        }
        tok = strtok(NULL, " .");
      }

      //printf("From ip: %d\n", from_ip);
      //printf("Vote val: %d\n", vote_val);
      //printf("My vote: %d\n", my_vote);

      if(vote_val > my_vote) {
        isMaster = 0;
      }
      else if(vote_val == my_vote) {
        if(from_ip > last_octet) {
          isMaster = 0;
        }
      }
    }

    // Check for VOTE
    else if(0 == strncmp(msgbuf, "VOTE", 4) || 0 == strncmp(msgbuf, "vote", 4))
    {
      isMaster = 1;
      char tmp[MSG_SIZE];
      char voteBuf[MSG_SIZE] = "# ";
      int r = rand() % (VOTE_MAX + 1 - VOTE_MIN) + VOTE_MIN;
      my_vote = r;
      sprintf(tmp, "%d", r);
      strcat(voteBuf, ip_str);
      strcat(voteBuf, " ");
      strcat(voteBuf, tmp);
      //printf("Vote buffer: %s\n", voteBuf);
      // Set broadcast ip
      from.sin_addr.s_addr = inet_addr(bcast_str);
      printf("Sending message: %s\n", voteBuf);
      if( sendto(sockfd, voteBuf, MSG_SIZE, 0, (struct sockaddr *)&from, fromlen) < 0 ) {
        fprintf(stderr, "sendto() %s\n", strerror(errno));
      }
    }
    else if( (0 == strncmp(msgbuf, "WHOIS", 5) || 0 == strncmp(msgbuf, "whois", 5)) & (1 == isMaster) )
    {
      char master_str[MSG_SIZE];
      sprintf(master_str, "Zach on board %s is the master", ip_str);
      from.sin_addr.s_addr = inet_addr(bcast_str); // set broadcast
      printf("Sending message: %s\n", master_str);
      if( sendto(sockfd, master_str, MSG_SIZE, 0, (struct sockaddr *)&from, fromlen) < 0 ) {
        fprintf(stderr, "sendto() %s\n", strerror(errno));
      }
    }

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

  printf("Portnum: %d\n", portnum);
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
  getIPAddr(&server);
  printf("My ip: %s\n", ip_str);
  printf("Broadcast ip: %s\n", bcast_str);
  server.sin_addr.s_addr = INADDR_ANY;

  if( bind(sockfd, (struct sockaddr *)&server, length) < 0 ) {
    fprintf(stderr, "bind() %s\n", strerror(errno));
  }

  if( setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &boolval, sizeof(boolval)) < 0 ) {
    fprintf(stderr, "setsockopt() %s\n", strerror(errno));
  }

  int fd = open("/dev/mem", O_RDWR|O_SYNC, 0);
  if(fd < 0) {
    printf("Error opening /dev/mem. Are you root?\n");
    return -1; // failed open
  }
  unsigned long *BasePtr = mmap(NULL, getpagesize(), PROT_READ|PROT_WRITE,MAP_SHARED,fd,0x800C0000);
  if(MAP_FAILED == BasePtr) { 
    printf("Unable to map baseptr\n");
    return -1;
  }
  VIC2SoftInt = (unsigned long *)((char *)BasePtr + 0x18);

  socket_transciever(sockfd);

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

// vim: ts=2 sw=2 et 
