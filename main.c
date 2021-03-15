#include <sys/epoll.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <errno.h>
#define MAX_PKT 100000
static unsigned long long bytes = 0;

int nRecvBuf=512*1024*1024;//设置为32K
 void set_socket_orig_buffer(int * sockfd)
{
	int err = -1;        
    int s = *sockfd;            
    int snd_size = 0;  
    int rcv_size = 0;    
    socklen_t optlen;  

	optlen = sizeof(snd_size);
    err = getsockopt(s, SOL_SOCKET, SO_SNDBUF,&snd_size, &optlen);
    if(err<0){
        printf("getsockopt SO_SNDBUF error\n");
        //exit(1);
	return;
    }   

    optlen = sizeof(rcv_size);
    err = getsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcv_size, &optlen);
    if(err<0){
        printf("getsockopt SO_RCVBUF error\n");
        return;
    }
     
    printf(" Original snd_size: %d Byte\n",snd_size);
    printf(" Original rcv_size: %d Byte\n",rcv_size);
 
    /*snd_size = 1024*1024*32;    
    optlen = sizeof(snd_size);
    err = setsockopt(s, SOL_SOCKET, SO_SNDBUF, &snd_size, optlen);
    if(err<0){
        printf("setsockopt SOL_SOCKET error\n");
        exit(1);
    }*/
 
    rcv_size = nRecvBuf;    
    optlen = sizeof(rcv_size);
    err = setsockopt(s,SOL_SOCKET,SO_RCVBUF, (char *)&rcv_size, optlen);
    if(err<0){
        printf("setsockopt SO_RCVBUF error\n");
        return;
    }

    optlen = sizeof(snd_size);
    err = getsockopt(s, SOL_SOCKET, SO_SNDBUF,&snd_size, &optlen);
    if(err<0){
        printf("getsockopt SO_SNDBUF error\n");
        return;
    }   
 
    optlen = sizeof(rcv_size);
    err = getsockopt(s, SOL_SOCKET, SO_RCVBUF,(char *)&rcv_size, &optlen);
    if(err<0){
        printf("getsockopt SO_RCVBUF error\n");
        return;
    }
 
    printf("+++ Original snd_size: %d Byte +++\n",snd_size);
    printf("+++ Original rcv_size: %d Byte +++\n",rcv_size);
}
 int main(int argc, char *argv[])
 {
 
     int udpfd = 0;
     char arr_ip4_local[] = {"192.168.1.133"};
     char arr_ip4_dst[] = {"192.168.1.150"};
      
 
     struct sockaddr_in addr_dst;
     struct sockaddr_in addr_local;
     bzero(&addr_dst, sizeof(struct sockaddr_in));
     bzero(&addr_local, sizeof(struct sockaddr_in));
 
     addr_dst.sin_port   = htons(8756);
     addr_dst.sin_family = AF_INET;
    // addr_dst.sin_addr.s_addr = htonl(INADDR_ANY);
     inet_pton(AF_INET, arr_ip4_dst, &addr_dst.sin_addr.s_addr);
 
     addr_local.sin_family   = AF_INET;
     addr_local.sin_port     = htons(809);
     //inet_pton(AF_INET, arr_ip4_local, &addr_local.sin_addr.s_addr);
     addr_local.sin_addr.s_addr = inet_addr("192.168.1.133");
 
     // socket
     udpfd   = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
     if (0 > udpfd)
     {
        perror("created failed");
         return 0;
     }
 	//setsockopt(udpfd,SOL_SOCKET,SO_RCVBUF,(const char*)&nRecvBuf,sizeof(int));
 	set_socket_orig_buffer(&udpfd);
     // bind local address to recv and send data
     int ret_val = bind(udpfd, (struct sockaddr*)&addr_local, sizeof(addr_local));   
     if (0  != ret_val)
     {
         perror("bind error, id = " );
         close(udpfd);
         return 0;
     }
 
 
     // epoll operation
     struct epoll_event event;
     struct epoll_event event_wait;
 
     int epoll_fd = epoll_create(10);
     if ( -1 == epoll_fd )
     {
         perror("epoll_create");
         close(udpfd);
         return 0;
     }
 
     // 
     event.data.fd   = 0;
     event.events    = EPOLLIN;
 
     // register functions
     ret_val = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, 0, &event);
     if ( -1 == ret_val )
     {
         perror("epoll_ctl error, id = " );
         close(udpfd);
 
         return 0;
     }
 
     // 
     event.data.fd = udpfd;
     event.events = EPOLLIN;
     ret_val = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udpfd, &event);
     if ( -1 == ret_val )
     {
         perror( "epoll_ctl error, udpfd, id = ");
         close(udpfd);
 
         return 0;
     }
 
     // listen
     struct sockaddr_in addr_recv;
     char arr_recv[1024*32] = {0};
     int addr_recv_len = sizeof(addr_recv);
     bzero(&addr_recv, addr_recv_len);
     socklen_t recv_len   = sizeof(addr_recv);
 
     unsigned long int quit_index = 0;
     unsigned long int quit_count = 1000000;
 
 
     while (1)
     {
         //perror("wait for connecting: ");
         ret_val = epoll_wait(epoll_fd, &event_wait, 2, 5*1000);
         if (-1  == ret_val)
         {
             perror("error, while, id = " );
             close(udpfd);
             return 0;
         }
 
         else if (0 < ret_val)
         {
             if ( (udpfd == event_wait.data.fd) 
                 && 
                 (EPOLLIN == (event_wait.events & EPOLLIN)) )
             {
                 ret_val = recvfrom(udpfd, arr_recv, 8192*2, 0, (struct sockaddr*)&addr_recv, &recv_len);
                 //printf("\n recvfrom = %d\n",ret_val);
		     bytes+=ret_val;
                 bzero(&addr_recv, addr_recv_len);
                 quit_index ++;
#if 1
                 if ( quit_index >= quit_count )
		      {
		          //printf("\npktCnt=%d\tbytesLen=%lld\n",quit_index,bytes);
			    printf("\npktCnt=%d\n",quit_index);
			    quit_count += 1000000;
			    //totalIndex += quit_index;
			    //quit_index = 0;
			    //bytes = 0;
                      }
#endif
             }
 
         }
         else if (0 == ret_val)
         {
          printf("\nwhile timeout\n");
          printf("pktCnt=%d\tbytesLen=%lld\n",quit_index,bytes);
	    //if(quit_index > 0)
	    //   return 0;
	    quit_count = 1000000;
	    quit_index = 0;
	    bytes = 0;
	     
         }
         else
         {
 
         }
     }
     
     close(udpfd);
 
     return 0;
 }
