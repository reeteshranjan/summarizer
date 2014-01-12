/*
 * daemontest.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "daemon.h"

int main(int argc, char *argv[])
{
    int sockfd, n;
    float r;
    struct sockaddr_in serv_addr;
    request_header_t req;
    error_header_t rep;
    uint32_t summary_len;
    char buffer[256];

    if (argc < 3) {
        fprintf(stderr,"usage %s file ratio\n", argv[0]);
        exit(0);
    }

    /* Create a socket point */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        exit(1);
    }

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &serv_addr.sin_addr);
    serv_addr.sin_port = htons(SUMMARIZERD_PORT);

    /* Now connect to the server */
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
    {
        perror("ERROR connecting");
        close(sockfd);
        exit(1);
    }

    req.proto = htons(SUMMARIZERD_PROTO);
    req.ver = htons(SUMMARIZERD_VERSION);
    r = atof(argv[2]);
    req.ratio = *((uint32_t*)(void*)&r);
    req.ratio = htonl(req.ratio);
    req.filename_len = htonl(strlen(argv[1]) + 1);

    /* Send message to the server */
    n = send(sockfd,&req,sizeof(req),0);
    if (n < 0)
    {
        perror("ERROR writing to socket");
        close(sockfd);
        exit(1);
    }
    n = send(sockfd,argv[1],strlen(argv[1])+1,0);
    if (n < 0)
    {
        perror("ERROR writing to socket");
        close(sockfd);
        exit(1);
    }
    /* Now read server response */
    n = recv(sockfd,&rep,sizeof(rep),0);
    if (n < 0)
    {
        perror("ERROR reading from socket");
        close(sockfd);
        exit(1);
    }
    rep.proto = ntohs(rep.proto);
    rep.ver = ntohs(rep.ver);
    rep.status = ntohl(rep.status);
    printf("response header - proto %x ver %x status %u\n",
           rep.proto, rep.ver, rep.status);

    if(REP_SUMMARY == rep.status) {
        n = recv(sockfd,&summary_len,sizeof(uint32_t),0);
        if (n < 0)
        {
            perror("ERROR reading from socket");
            close(sockfd);
            exit(1);
        }
        summary_len = ntohl(summary_len);
        printf("Receiving summary of %u bytes", summary_len);
        while(summary_len > 0) {
            n = recv(sockfd,buffer,256,0);
            if (n < 0)
            {
                perror("ERROR reading from socket");
                close(sockfd);
                exit(1);
            }
            buffer[n] = 0;
            printf("%s",buffer);
            summary_len-=n;
        }
    }

    close(sockfd);
    return 0;
}
