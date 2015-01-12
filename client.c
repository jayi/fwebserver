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
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_BUF_SIZE 32768

int main(int argc, char **argv) {
	int sockfd;
	char request_str[MAX_BUF_SIZE];
	char response_str[MAX_BUF_SIZE];
	char line[MAX_BUF_SIZE];
	struct sockaddr_in server_addr;
	struct hostent *host;
	int port, nbytes;
	FILE *fp, *output_fp;

	if ((host = gethostbyname("192.168.56.101")) == NULL) {
		fprintf(stderr, "Gethostname error\n");
		exit(1);
	}
	port = 80;

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family=AF_INET;
	server_addr.sin_port=htons(port);
	server_addr.sin_addr=*((struct in_addr *)host->h_addr);

	if ((fp = fopen("input.txt", "r")) == NULL) {
		printf("File Error:%s\a\n", strerror(errno));
		exit(1);
	}

	if ((output_fp = fopen("output.txt", "w")) == NULL) {
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
		puts(">>>");
		request_str[0] = '\0';
		while (1) {
			strncat(request_str, line, strlen(line));
			if (strcmp(line, "\r\n") == 0) {
				break;
			}
			if (!fgets(line, MAX_BUF_SIZE, fp)) {
				printf("EOF\n");
				break;
//				exit(1);
			}
		}

		printf("%s|%zd\n", request_str, strlen(request_str));
		puts("****");

		if (write(sockfd, request_str, strlen(request_str)) == -1) {
			printf("Write Error:%s\n", strerror(errno));
			break;
		}

		if ((nbytes = read(sockfd, response_str, MAX_BUF_SIZE)) == -1) {
			printf("Read Error:%s\n", strerror(errno));
			break;
		}
		response_str[nbytes] = '\0';
		fprintf(output_fp, "%s\n", response_str);
//		sleep(5);
		close(sockfd);
	}
//	close(sockfd);
	fclose(fp);
	fclose(output_fp);

	return 0;
}
