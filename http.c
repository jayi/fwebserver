#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "http.h"
#include "hash.h"

int valid_status[STATUS_NUM] = {200, 301, 302, 400, 403, 404, 503};
char status_name[STATUS_NUM][MAX_STATUS_NAME_LENGTH] = {
	"OK",
	"Moved Permanently",
	"Found",
	"Bad Request",
	"Forbidden",
	"Not Found",
	"Service Unavailable"
};

char week[WEEK_NUM][ABBR_LENGTH] = {
	"Sun",
	"Mon",
	"Tue",
	"Wed",
	"Thu",
	"Fri",
	"Sat"
};

char month[MONTH_NUM][ABBR_LENGTH] = {
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

int is_valid_uri(char *uri)
{
	return strlen(uri) >= 1 &&
		uri[0] == '/';
}

int read_head(request_t *request, char *line)
{
	char *iter = line;
	char *part;

	if (line[strlen(line) - 1] != '\r') {
		return -1;
	}

	if ((part = strsep(&iter, " ")) == NULL) {
		return -1;
	}
	if (strcmp(part, "GET") == 0) {
		request->http_method = HTTP_METHOD_GET;
	}
	else if (strcmp(part, "HEAD") == 0) {
		request->http_method = HTTP_METHOD_HEAD;
	}
	else {
		return -1;
	}

	// read uri
	if ((part = strsep(&iter, " ")) == NULL) {
		return -1;
	}
	request->uri = (char *)malloc((strlen(part) + 1) * sizeof(char));
	strcpy(request->uri, part);
	if (!is_valid_uri(request->uri)) {
		return -1;
	}

	// read http version
	if ((part = strsep(&iter, " ")) == NULL) {
		return -1;
	}
	if (strcmp(part, "HTTP/1.0\r") == 0) {
		request->http_version = HTTP_VERSION_1_0;
		request->keep_alive = 0;
	}
	else if (strcmp(part, "HTTP/1.1\r") == 0) {
		request->http_version = HTTP_VERSION_1_1;
		request->keep_alive = 1;
	}
	else {
		puts("Error: have no http version");
		return -1;
	}

	if ((part = strsep(&iter, " ")) != NULL) {
		return -1;
	}
	return 1;
}

void init_request(request_t *request)
{
	request->http_version = HTTP_VERSION_UNSET;
	request->http_method = HTTP_METHOD_UNSET;
	request->uri = NULL;
	request->keep_alive = 0;
	request->expect_status = -1;
	request->expect_location = NULL;
	request->expect_length = 0;
	request->expect_header = NULL;
	request->other_header = NULL;
	request->flag = 0;
}

int is_valid_status(int status)
{
	int i;
	for (i = 0; i < STATUS_NUM; ++i) {
		if (status == valid_status[i]) {
			return 1;
		}
	}
	return 0;
}

struct hash_node hash_table[HASH_SIZE];
int read_http_request(request_t *request, char *str, int len)
{
	char *iter = str;
	char *line;
	char *pch;
	char *part;
	char *pe;
	int i;
	int location_len;
	header_t node;

	init_request(request);

	if ((line = strsep(&iter, "\n")) == NULL) {
		return -1;
	}

	if (read_head(request, line) == -1) {
		return -1;
	}

	while ((line = strsep(&iter, "\n")) != NULL) {
		if (strcmp(line, "\r") == 0) {
			break;
		}
		if (line[strlen(line) - 1] != '\r') {
			puts("Error: format error line[-1] != \\r");
			return -1;
		}
		if ((pch = strchr(line, ':')) == NULL) {
			return -1;
		}
		if (strncmp(line, "Expect-Status", strlen("Expect-Status")) == 0) {
			if (request->flag & EXPECT_STATUS_FLAG) {
				return -1;
			}
			request->flag |= EXPECT_STATUS_FLAG;

			request->expect_status = 0;
			for (i = pch - line + 2; isdigit(line[i]); ++i) {
				request->expect_status =
					request->expect_status * 10 + line[i] - '0';
			}
			if (!is_valid_status(request->expect_status)) {
				return -1;
			}
			if (line[i] != '\r') {
				return -1;
			}
		}
		else if (strncmp(line, "Expect-Location",
					strlen("Expect-Location")) == 0) {
			if (request->flag & EXPECT_LOCATION_FLAG) {
				return -1;
			}
			request->flag |= EXPECT_LOCATION_FLAG;

			location_len = strlen(line) - (pch - line) - 3;
			request->expect_location = (char *)malloc(location_len + 1);
			strncpy(request->expect_location, pch + 2, location_len);
			request->expect_location[location_len] = '\0';
		}
		else if (strncmp(line, "Expect-Header", strlen("Expect-Header")) == 0) {
			if (request->flag & EXPECT_HEADER_FLAG) {
				return -1;
			}
			request->flag |= EXPECT_HEADER_FLAG;

			init_hash_table(hash_table);
			pch += 2;
			part = strsep(&pch, ",");
			while (part) {
				while (*part == ' ') ++part;
				header_t node = (header_t)malloc(sizeof(struct header_node));
				if ((pe = strchr(part, '=')) == NULL) {
					return -1;
				}
				node->header = (char *)malloc(sizeof(char) * (pe - part + 1));
				strncpy(node->header, part, pe - part);
				node->header[pe - part] = '\0';

				elem_t hash_num = elf_hash(node->header);
				if (hash_get(hash_num, hash_table) != -1) {
					return -1;
				}
				else {
					hash_insert(hash_num, 0, hash_table);
				}
				node->value = (char *)malloc(sizeof(char)
						* (strlen(part) - (pe - part) + 1));
				strncpy(node->value, pe + 1, strlen(part) - (pe - part) - 1);
				node->value[strlen(part) - (pe - part)] = '\0';
				node->next = request->expect_header;
				request->expect_header = node;
				part = strsep(&pch, ",");
				if (part == NULL) {
					node->value[strlen(node->value) - 1] = '\0';
					break;
				}
			}
		}
		else if (strncmp(line,
					"Expect-Length",
					strlen("Expect-Length")) == 0) {
			if (request->flag & EXPECT_LENGTH_FLAG) {
				return -1;
			}
			request->flag |= EXPECT_LENGTH_FLAG;

			request->expect_length = 0;
			for (i = pch - line + 2; isdigit(line[i]); ++i) {
				request->expect_length =
					request->expect_length * 10 + line[i] - '0';
			}
			if (line[i] != '\r') {
				return -1;
			}
		}
		else if (strncmp(line,
				"Connection",
				strlen("Connection")) == 0) {
			if (request->flag & CONNECTION_FLAG) {
				return -1;
			}
			request->flag |= CONNECTION_FLAG;

			pch += 2;
			// FIXME: here must ignore case
			if (strncmp(pch,
					"close\r",
					strlen("close\r")) == 0) {
				request->keep_alive = 0;
			}
			else if (strncmp(pch,
					"keep-alive\r",
					strlen("keep-alive\r")) == 0) {
				request->keep_alive = 1;
			}
			else {
				return -1;
			}

		}
		else {
			node = (header_t)malloc(sizeof(struct header_node));
			node->header = (char *)malloc(sizeof(char) * (pch - line + 1));
			strncpy(node->header, line, pch - line);
			node->header[pch - line + 1] = '\0';
			node->value = (char *)malloc(sizeof(char)
					* (strlen(line) - (pch - line) - 1));
			strncpy(node->value, pch + 2, strlen(line) - (pch - line) - 3);
			node->value[strlen(line) - (pch - line) - 1] = '\0';
			node->next = request->other_header;
			request->other_header = node;
		}
	}
	if (!is_valid_status(request->expect_status)) {
		return -1;
	}
	return 1;
}

void init_response(response_t *response)
{
	response->http_version = HTTP_VERSION_UNSET;
	response->http_method = HTTP_METHOD_UNSET;
	response->uri = NULL;
	response->keep_alive = 0;
	response->status = -1;
	response->location = NULL;
	response->content_length = 0;
	response->other_header= NULL;
	response->t = time(0);
	response->server = (char *)malloc(sizeof(char) * (strlen(DEFAULT_SERVER) + 1));
	strcpy(response->server, DEFAULT_SERVER);
	response->time_str = (char *)malloc(sizeof(char) * MAX_TIME_STR_LEN);
	get_time_in_http_format(response->time_str, response->t);
}

void deep_copy(char **dest, char *src, int len)
{
	*dest = (char *)malloc(sizeof(char) * (len + 1));
	strncpy(*dest, src, len);
	(*dest)[len] = '\0';
}

int http_response(response_t *response, request_t *request)
{
	init_response(response);
	response->http_version = request->http_version;
	response->http_method = request->http_method;
	response->keep_alive = request->keep_alive;
	response->status = request->expect_status;
	response->content_length = request->expect_length;

	if (request->uri) {
		response->uri =
			(char *)malloc(sizeof(char)
					* (1 + strlen(request->uri)));
		strcpy(response->uri, request->uri);
	}
	else {
		return -1;
	}

	if (request->expect_location) {
		response->location =
			(char *)malloc(sizeof(char)
					* (1 + strlen(request->expect_location)));
		strcpy(response->location, request->expect_location);
	}

	header_t iter;
	header_t node = NULL;
	for (iter = request->expect_header; iter; iter = iter->next) {
		if (strcmp(iter->header, "Connection") == 0) {
			if (strcmp(iter->value, "keep-alive") == 0) {
				response->keep_alive = 1;
			}
			else if (strcmp(iter->value, "close") == 0) {
				response->keep_alive = 0;
			}
			else {
				return -1;
			}
		}
		else if (strcmp(iter->header, "Server") == 0) {
			deep_copy(&response->server, iter->value, strlen(iter->value));
		}
		else if (strcmp(iter->header, "Content-Length") == 0) {
			sscanf(iter->value, "%d", &response->content_length);
		}
		else if (strcmp(iter->header, "Date") == 0) {
			deep_copy(&response->time_str, iter->value, strlen(iter->value));
		}
		else {
			node = (header_t)malloc(sizeof(struct header_node));
			deep_copy(&node->header, iter->header, strlen(iter->header));
			deep_copy(&node->value, iter->value, strlen(iter->value));
			node->next = response->other_header;
			response->other_header = node;
		}
	}
	return 1;
}

void print_http_response(response_t *response)
{
	header_t iter;
	printf("*http version: %d\n", response->http_version);
	printf("*http method: %d\n", response->http_method);
	printf("*status: %d\n", response->status);
	printf("*Connection: %d\n", response->keep_alive);
	printf("*content_length: %d\n", response->content_length);
	printf("*uri: ");
	if (response->uri) {
		puts(response->uri);
	}
	else {
		puts("NULL");
	}
	printf("*location: ");
	if (response->location) {
		puts(response->location);
	}
	else {
		puts("NULL");
	}

	printf("*other_header*\n");
	for (iter = response->other_header; iter; iter = iter->next) {
		printf("%s: %s\n", iter->header, iter->value);
	}
	printf("**\n");
}

int get_status_name(char *name, int status)
{
	int i;
	for (i = 0; i < STATUS_NUM; ++i) {
		if (status == valid_status[i]) {
			strcpy(name, status_name[i]);
			return strlen(status_name[i]);
		}
	}
	return -1;
}

void get_time_in_http_format(char *str, time_t _time)
{
	struct tm *t = gmtime(&_time);
	sprintf(str,
		"%s, %d %s %d %d:%d:%d GMT",
		week[t->tm_wday],
		t->tm_mday,
		month[t->tm_mon],
		t->tm_year + START_YEAR,
		t->tm_hour,
		t->tm_min,
		t->tm_min);
}

int http_response_400(char *str)
{
	char time_str[MAX_TIME_STR_LEN];

	strcpy(str, HTTP_400);
	time_t t = time(0);
	get_time_in_http_format(time_str, t);
	strcat(str, "Date: ");
	strcat(str, time_str);
	strcat(str, "\r\n\r\n");
	return strlen(str);
}

int write_http_response(char *str, response_t *response)
{
	char line[MAX_BUF_SIZE];
	char buf[MAX_BUF_SIZE];
	header_t iter;
	int i;

	str[0] = line[0] = '\0';
	if (response->http_version == HTTP_VERSION_UNSET) {
		return -1;
	}
	else if (response->http_version == HTTP_VERSION_1_0) {
		strcat(line, "HTTP/1.0 ");
	}
	else {
		strcat(line, "HTTP/1.1 ");
	}

	sprintf(buf, "%d ", response->status);
	strcat(line, buf);
	get_status_name(buf, response->status);
	strcat(line, buf);
	strcat(line, "\r\n");
	strcat(str, line);
	sprintf(line, "Server: %s\r\n", response->server);
	strcat(str, line);

	line[0] = '\0';
	strcat(line, "Connection: ");
	if (response->keep_alive == 1) {
		strcat(line, "keep-alive\r\n");
	}
	else if (response->keep_alive == 0) {
		strcat(line, "close\r\n");
	}
	else {
		return -1;
	}
	strcat(str, line);

	sprintf(line, "Content-Length: %d\r\n", response->content_length);
	strcat(str, line);

	sprintf(line, "Date: %s\r\n", response->time_str);
	strcat(str, line);

	for (iter = response->other_header; iter; iter = iter->next) {
		sprintf(line, "%s: %s\r\n", iter->header, iter->value);
		strcat(str, line);
	}

	switch(response->status) {
		case 200:
			strcat(str, "\r\n");
			if (response->http_method == HTTP_METHOD_GET) {
				for (i = 0; i < response->content_length; ++i) {
					line[i] = '0' + i % 10;
				}
				line[i] = '\0';
				strcat(str, line);
			}
			break;
		case 301: case 302:
			if (response->location == NULL) {
				return -1;
			}
			strcpy(line, "Location: ");
			strcat(line, response->location);
			strcat(str, line);
			strcat(str, "\r\n");
			break;
		case 400: case 403: case 404: case 503:
			strcat(str, "\r\n");
			break;
		default:
			return -1;
			break;
	}

	return strlen(str);
}
