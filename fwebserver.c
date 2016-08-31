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

#define MAX_EPOLLSIZE 100000
#define ERROR_LOG perror

/*
 * process event, read http request and response
 *
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
                // read data from socket
		if ((nbytes = recv(new_fd, buf, MAX_BUF_SIZE, 0)) == -1) {
			ret = -2;
			break;
		}

                // parse data to http request
		if (read_http_request(&request, buf, nbytes) == -1) {
			ret = -1;
			break;
		}
                // get response from request
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

	if (ret != -2 && send(new_fd, buf, nbytes, 0) == -1) {
		ret = -2;
        }

	return ret;
}

/*
 * print help information
 */
void usage(char *program_name)
{
	printf("Usage:    %s [-h ip] [-p port]\n", program_name);
	printf("-h ip     specify the listening ip.\n");
	printf("          default is %s, which means listening all request from any ip.\n", DEFAULT_IP);
	printf("-p port   specify the listening port number.\n");
	printf("          default is %d.\n", DEFAULT_PORT);
}

struct session {
	time_t last_active;		/* last active time */
	int status;			/* event_info status. 1 means this
					 * event_info is effective. 0 means empty.
					 */
	int fd;				/* file description */
	struct session *next;		/* for recycle list, active_list */
	struct session *prev;
};

struct session *recycle_list = NULL;
struct session active_list = {
	.next = &active_list,
	.prev = &active_list,
};

void list_add(struct session *head, struct session *new)
{
	new->prev = head;
	new->next = head->next;
	head->next->prev = new;
	head->next = new;
}

void list_del(struct session *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->prev = entry->next = NULL;
}

struct session *new_session(int fd)
{
	struct session *ret;

	if (recycle_list == NULL) {
		ret = malloc(sizeof(struct session));
	} else {
		ret = recycle_list;
		recycle_list = recycle_list->next;
	}

	ret->last_active = time(0);
	ret->fd = fd;
	list_add(&active_list, ret);

	return ret;
}

void end_session(struct session *session)
{
	list_del(session);
	session->next = recycle_list;
	recycle_list = session;
	close(session->fd);
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

/* every second check some events if it is timeout or not. */
#define CHECK_TIMEOUT_NUM 10000
/* inactive more than 60 seconds, close connection. */
#define TIMEOUT 60
void del_timeout_sessions(int epfd)
{
	time_t now = time(0);
	struct session *iter;
	int cnt = 0;

	/* Because active_list is sorted by timestamp,
	 * we can delete timeout sessions from tail to head
	 */
	for (iter = active_list.prev;
			iter != &active_list && (++cnt) < CHECK_TIMEOUT_NUM;
			iter = iter->prev) {
		int duration = now - iter->last_active;
		if (duration >= TIMEOUT) {
			end_session(iter);
			epoll_ctl(epfd, EPOLL_CTL_DEL, iter->fd, NULL);
		}
	}
}

int accept_conn(int epfd, int listen_fd)
{
	struct sockaddr_in client_addr;
	struct epoll_event ev;
	struct session *session;
	socklen_t sock_len;
	int new_fd;

	new_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &sock_len);
	if (new_fd < 0)
		return new_fd;

	fcntl(new_fd, F_SETFL, fcntl(new_fd, F_GETFD, 0) | O_NONBLOCK);
	ev.events = EPOLLIN | EPOLLET;
	session = new_session(new_fd);
	ev.data.ptr = session;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, new_fd, &ev) < 0) {
		ERROR_LOG("add socket to epoll unsucessful!!!");
		return -1;
	}

	return new_fd;
}

void epoll_loop(int listen_fd)
{
	struct epoll_event events[MAX_EPOLLSIZE];
	struct epoll_event ev;
	struct rlimit rt;
	socklen_t sock_len;
	int epfd;
	int curfds;

	/* set max open fd limit */
	rt.rlim_max = rt.rlim_cur = MAX_EPOLLSIZE;
	if (setrlimit(RLIMIT_NOFILE, &rt) == -1) {
		ERROR_LOG("Cannot set rlimit\n");
		return;
	}

	epfd = epoll_create(MAX_EPOLLSIZE);
	sock_len = sizeof(struct sockaddr_in);
	ev.events = EPOLLIN | EPOLLET;
	ev.data.ptr = NULL;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
		ERROR_LOG("epoll set insertion error");
		return;
	}
	curfds = 1;

	while (1) {
		int nfds;
		int i;

		del_timeout_sessions(epfd);

		nfds = epoll_wait(epfd, events, curfds, 1);
		if (nfds == -1) {
			ERROR_LOG("epoll_wait");
			break;
		}
		for (i = 0; i < nfds; ++i) {
			if (events[i].data.ptr == NULL) {
				if (accept_conn(epfd, listen_fd) > 0) {
					curfds++;
				}
			} else {
				struct session *session = events[i].data.ptr;
				int ret = process(session->fd);
				session->last_active = time(0);
				/* move session to head of list,
				 * so the list is sorted by timestamp.
				 */
				list_del(session);
				list_add(&active_list, session);
				if (ret != 1 && errno != 11) {
					end_session(session);
					epoll_ctl(epfd, EPOLL_CTL_DEL, session->fd, NULL);
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
	uint32_t ip = 0;
	int opt;

	while ((opt = getopt(argc, argv, "h:p:")) != -1) {
		switch(opt) {
			case 'h':
				ip = inet_addr(optarg);
				if (ip == -1) {
					usage(argv[0]);
					return 0;
				}
				break;
			case 'p':
				port = atoi(optarg);
				break;
			default:
				usage(argv[0]);
				return 0;
		}
	}

	if (fork() != 0) {
		exit(0);
	}

	if ((listen_fd = init_socket(ip, port)) < 0) {
		exit(1);
	}

	epoll_loop(listen_fd);
	close(listen_fd);
	return 0;
}

