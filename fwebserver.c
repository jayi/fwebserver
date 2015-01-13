#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <arpa/inet.h>

#include "http.h"
#include "hash.h"

#define MAX_EPOLLSIZE 100000
#define ERROR_LOG perror

/*
 * process event
 * argument: new_fd is client socket file description
 * return:
 * -1 means format error
 * -2 means socket io error
 *  0 means process sucessful, and close connection
 *  1 means process sucessful, and keep alive
 */
int process(int new_fd)
{
	char buf[MAX_BUF_SIZE];
	int nbytes = 0;
	request_t request;
	response_t response;
	int ret = 1;

	bzero(buf, MAX_BUF_SIZE);

	do {
		if ((nbytes = read(new_fd, buf, MAX_BUF_SIZE)) == -1) {
			ret = -2;
			break;
		}

		if (read_http_request(&request, buf, nbytes) == -1) {
			ret = -1;
			break;
		}
		http_response(&response, &request);
		if (response.keep_alive == 0) {
			ret = 0;
		}
		if ((nbytes = write_http_response(buf, &response)) == -1) {
			ret = -2;
			break;
		}
	} while (0);

	if (ret == -1) {
		nbytes = http_response_400(buf);
	}

	if (ret != -2 && write(new_fd, buf, nbytes) == -1)
		ret = -2;

	return ret;
}

/* print help information, if get wrong arguments */
void print_help(char *program_name)
{
	printf("Usage:    %s [-h ip] [-p port]\n", program_name);
	printf("-h ip     specify the listening ip.\n");
	printf("          default is %s, which means listening all request from any ip.\n", DEFAULT_IP);
	printf("-p port   specify the listening port number.\n");
	printf("          default is %d.\n", DEFAULT_PORT);
}

/*
 * read arguments
 * arguments:
 * argc: number of arguments
 * argv: string array of arguments
 * ip: listening ip
 * port: listening port number
 * return:
 * 	1 means sucessful
 * 	other means have error
 */
int read_args(int argc, char **argv, char *ip, int *port)
{
	int i = 1;
	int j;
	int ret = 1;
	int len;
	int flag = 0;
	char *c;

	// strcpy(DEFAULT_IP, ip);
	strcpy(ip, DEFAULT_IP);
	*port = DEFAULT_PORT;

	for (; i < argc; ) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == 'h') {
				if (flag & 1) {
					printf("\nError: argument \"-h\" repeated\n");
					ret = 0;
					break;
				}
				flag |= 1;

				if (argv[i][2] != '\0') {
					printf("\nError: invalid arguments\n");
					ret = 0;
					break;
				}
				else {
					++i;
					if (i == argc) {
						printf("\nError: no value for argument \"-h\"\n");
						ret = 0;
						break;
					}

					len = 0;
					c = ip;
					for (j = 0; argv[i][j]; ++j) {
						++len;
						if (len > MAX_IP_LENGTH) {
							break;
						}
						*c = argv[i][j];
						++c;
					}
					*c = '\0';

					if (argv[i][j]) {
						ret = 0;
						break;
					}
					if (inet_addr(ip) < 0) {
						printf("\nError: invalid ip\n");
						ret = 0;
						break;
					}
				}
			}
			else if (argv[i][1] == 'p') {
				if (flag & 2) {
					printf("\nError: argument \"-p\" repeated\n");
					ret = 0;
					break;
				}
				flag |= 2;

				if (argv[i][2] != '\0') {
					printf("\nError: invalid arguments\n");
					ret = 0;
					break;
				}
				else {
					++i;
					if (i == argc) {
						printf("\nError: no value for argument \"-p\"\n");
						ret = 0;
						break;
					}

					*port = 0;
					for (j = 0; argv[i][j]; ++j) {
						if (!isdigit(argv[i][j])) {
							break;
						}
						*port = *port * 10 + argv[i][j] - '0';
					}
					if (argv[i][j]) {
						printf("\nError: invalid port\n");
						ret = 0;
						break;
					}
				}
			}
			else {
				printf("\nError: invalid arguments\n");
				ret = 0;
				break;
			}
		}
		else {
			printf("\nError: invalid arguments\n");
			ret = 0;
			break;
		}
		++i;
	}
	if (ret == 0) {
		print_help(argv[0]);
	}

	return ret;
}

struct event_info {
	time_t last_active;	/* last active time */
	int status;	/* event_info status. 1 means this event_info is effective. 0 means empty. */
	int fd;		/* socket file description */
} ei[MAX_EPOLLSIZE];	/* more event information */
int eip;	/* event_info array position, use it to find an empty event_info. */

struct hash_node event_table[HASH_SIZE];
#define CHECK_TIMEOUT_NUM 10000	// every second check some events if it is timeout or not.
#define TIMEOUT 60	// after 60 seconds inactive, close connection

void init_event_info()
{
	int i;

	eip = 0;
	for (i = 0; i < MAX_EPOLLSIZE; ++i) {
		ei[i].last_active = 0;
		ei[i].status = 0;
		ei[i].fd = 0;
	}
}

// find an empty event_info and save a new socket fd
void add_event(int fd)
{
	for (; ei[eip].status; ++eip) {}
	ei[eip].fd = fd;
	ei[eip].status = 1;
	ei[eip].last_active = time(0);
	hash_insert(fd, eip, event_table);
}

// if socked is active, update its event_info
void update_event(int fd) {
	int idx = hash_get(fd, event_table);
	ei[idx].status = 1;
	ei[idx].last_active = time(0);
}

// if a socket connection closed, delete it
void del_event(int fd) {
	int idx = hash_get(fd, event_table);
	close(fd);
	ei[idx].status = 0;
}

int init_socket(uint32_t ip, int port)
{
	struct sockaddr_in service_addr;
	int listen_fd;
	int tmp = 1;
	int backlog = 5;

	if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		ERROR_LOG("Cannot create socket\n");
		return -1;
	}
	fcntl(listen_fd, F_SETFL, fcntl(listen_fd, F_GETFD, 0)|O_NONBLOCK);
	bzero(&service_addr, sizeof(service_addr));
	service_addr.sin_family		= AF_INET;
	service_addr.sin_port		= htons(port);
	service_addr.sin_addr.s_addr	= ip;

	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(tmp));
	if (bind(listen_fd, (struct sockaddr *)&service_addr,
				sizeof(struct sockaddr)) == -1) {
		ERROR_LOG("Cannot bind\n");
		return -1;
	}

	if (listen(listen_fd, backlog) == -1) {
		ERROR_LOG("Cannot listen\n");
		return -1;
	}

	return listen_fd;
}

void epoll_loop(int listen_fd)
{
	struct epoll_event events[MAX_EPOLLSIZE];
	struct epoll_event ev;
	struct rlimit rt;
	socklen_t sock_len;
	int epfd;
	int curfds;
	int check_pos = 0;

	rt.rlim_max = rt.rlim_cur = MAX_EPOLLSIZE;
	if (setrlimit(RLIMIT_NOFILE, &rt) == -1) {
		ERROR_LOG("Cannot set rlimit\n");
		return;
	}

	epfd = epoll_create(MAX_EPOLLSIZE);
	sock_len = sizeof(struct sockaddr_in);
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = listen_fd;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
		ERROR_LOG("epoll set insertion error");
		return;
	}

	curfds = 1;
	check_pos = 0;
	init_event_info();
	init_hash_table(event_table);
	while (1) {
		time_t now = time(0);
		int nfds;
		int i;

		for (i = 0; i < CHECK_TIMEOUT_NUM ; ++i, ++check_pos) {
			int duration;

			if (check_pos == MAX_EPOLLSIZE)
				check_pos = 0;
			if (ei[check_pos].status != 1)
				continue;
			duration = now - ei[check_pos].last_active;
			if (duration >= TIMEOUT) {
				del_event(ei[check_pos].fd);
				epoll_ctl(epfd, EPOLL_CTL_DEL, ei[check_pos].fd,&ev);
			}
		}

		nfds = epoll_wait(epfd, events, curfds, 1);
		if (nfds == -1) {
			ERROR_LOG("epoll_wait");
			break;
		}
		for (i = 0; i < nfds; ++i) {
			if (events[i].data.fd == listen_fd) {
				int new_fd;
				struct sockaddr_in client_addr;

				new_fd = accept(listen_fd,
						(struct sockaddr *)&client_addr,
						&sock_len);
				if (new_fd < 0)
					continue;
				else
					add_event(new_fd);

				fcntl(new_fd, F_SETFL,
					fcntl(new_fd, F_GETFD, 0) | O_NONBLOCK);
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = new_fd;
				if (epoll_ctl(epfd, EPOLL_CTL_ADD, new_fd, &ev) < 0) {
					ERROR_LOG("add socket to epoll unsucessful!!!");
					return;
				}
				curfds++;
			} else {
				int ret = process(events[i].data.fd);
				update_event(events[i].data.fd);
				if (ret != 1 && errno != 11) {
					del_event(events[i].data.fd);
					epoll_ctl(epfd, EPOLL_CTL_DEL,
							events[i].data.fd, &ev);
					curfds--;
				}
			}
		}
	}
}

int main(int argc, char **argv)
{
	int listen_fd;
	int port;
	char ip[MAX_IP_LENGTH];

	if (read_args(argc, argv, ip, &port) != 1) {
		exit(1);
	}

	if (fork() != 0) {
		exit(0);
	}

	if ((listen_fd = init_socket(inet_addr(ip), port)) < 0) {
		exit(1);
	}

	epoll_loop(listen_fd);
	close(listen_fd);
	return 0;
}

