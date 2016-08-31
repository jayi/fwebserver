/*
 * =====================================================================================
 *
 *       Filename:  client.c
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  2012年07月20日 10时27分43秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  jayi (), hjy322@gmail.com
 *        Company:
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_BUF_SIZE 32768
#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8080

#define NR_TEST_CASES 1
#define TEST_CASES_PATH "cases"

int main(int argc, char **argv) {
    int sockfd;
    char request_str[MAX_BUF_SIZE];
    char response_str[MAX_BUF_SIZE];
    char line[MAX_BUF_SIZE];
    struct sockaddr_in server_addr;
    struct hostent *host;
    int port, nbytes;
    FILE *fp, *output_fp;
    int i;
    char filename[64];

    if ((host = gethostbyname(SERVER_HOST)) == NULL) {
        fprintf(stderr, "Gethostname error\n");
        exit(1);
    }
    port = SERVER_PORT;

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(port);
    server_addr.sin_addr=*((struct in_addr *)host->h_addr);

    for (i = 1; i <= NR_TEST_CASES; ++i) {
        sprintf(filename, "%s/%d.in", TEST_CASES_PATH, i);
        printf("testing file %s ...\n", filename);
        if ((fp = fopen(filename, "r")) == NULL) {
            printf("File Error:%s\a\n", strerror(errno));
            exit(1);
        }

        //	if ((sockfd=socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        //		printf("Socket Error: %s\a\n", strerror(errno));
        //		exit(1);
        //	}
        //	if (connect(sockfd, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr)) == -1) {
        //		printf("Connect Error:%s\a\n", strerror(errno));
        //		exit(1);
        //	}

        while (fgets(line, MAX_BUF_SIZE, fp)) {
            if ((sockfd=socket(AF_INET, SOCK_STREAM, 0)) == -1) {
                printf("Socket Error: %s\a\n", strerror(errno));
                exit(1);
            }
            if (connect(sockfd, (struct sockaddr *)(&server_addr), sizeof(struct sockaddr)) == -1) {
                printf("Connect Error:%s\a\n", strerror(errno));
                exit(1);
            }
            /* puts(">>>"); */
            request_str[0] = '\0';
            while (1) {
                strncat(request_str, line, strlen(line));
                if (strcmp(line, "\r\n") == 0) {
                    break;
                }
                if (!fgets(line, MAX_BUF_SIZE, fp)) {
                    /* printf("EOF\n"); */
                    break;
                    //				exit(1);
                }
            }

            /* printf("%s|%zd\n", request_str, strlen(request_str)); */
            /* puts("****"); */

            if (send(sockfd, request_str, strlen(request_str), 0) == -1) {
                printf("Write Error:%s\n", strerror(errno));
                break;
            }

            if ((nbytes = recv(sockfd, response_str, MAX_BUF_SIZE, 0)) == -1) {
                printf("Read Error:%s\n", strerror(errno));
                break;
            }
            response_str[nbytes] = '\0';
            /* fprintf(output_fp, "%s\n", response_str); */
            //		sleep(5);
            close(sockfd);

            sprintf(filename, "%s/%d.out", TEST_CASES_PATH, i);
            if ((output_fp = fopen(filename, "r")) == NULL) {
                printf("File Error:%s\a\n", strerror(errno));
                exit(1);
            }

            if (fread(line, sizeof(char), nbytes, output_fp) != nbytes
                    || strncmp(line, response_str, nbytes) != 0) {
                printf("#### TEST %d\n", i);
                line[nbytes] = '\0';
                printf("EXPECT:\n%s\nRECEIVED:\n%s\n", line, response_str);
            }
        }
        //	close(sockfd);
        fclose(fp);
        fclose(output_fp);
    }

    return 0;
}
