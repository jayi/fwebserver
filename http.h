/*
 * =====================================================================================
 *
 *       Filename:  fwebserver.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/19/2012 01:18:40 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  jayi (), hjy322@gmail.com
 *        Company:  chinanetcenter
 *
 * =====================================================================================
 */
#ifndef _H_HTTP
#define _H_HTTP

#define DEFAULT_PORT 80
#define DEFAULT_IP "0.0.0.0"
#define MAX_BUF_SIZE 32768
#define MAX_IP_LENGTH 32
#define MAX_TIME_STR_LEN 64
#define START_YEAR 1970

#define STATUS_NUM 7
#define MAX_STATUS_NAME_LENGTH 32

#define WEEK_NUM 7
#define ABBR_LENGTH 4
#define MONTH_NUM 12

#define HTTP_400 "HTTP/1.0 400 Bad Request\r\n\
Server: FakeWebServer\r\n\
Connection: close\r\n\
Content-Length: 0\r\n"

#define DEFAULT_SERVER "FakeWebServer"

#define EXPECT_STATUS_FLAG 1
#define EXPECT_LOCATION_FLAG 2
#define EXPECT_LENGTH_FLAG 4
#define EXPECT_HEADER_FLAG 8
#define CONNECTION_FLAG 16

typedef enum {
	HTTP_VERSION_UNSET = -1,
	HTTP_VERSION_1_0,
	HTTP_VERSION_1_1
} http_version_t;

typedef enum {
	HTTP_METHOD_UNSET = -1,
	HTTP_METHOD_GET,
	HTTP_METHOD_HEAD
} http_method_t;

struct header_node {
	char *header;
	char *value;
	struct header_node *next;
};
typedef struct header_node * header_t;

typedef struct {
	http_version_t http_version;
	http_method_t http_method;
	char *uri;
	int keep_alive;
	int expect_status;
	char *expect_location;
	int expect_length;
        int flag;
	header_t expect_header;
	header_t other_header;
} request_t;

typedef struct {
	http_version_t http_version;
	http_method_t http_method;
	char *uri;
        char *server;
	int keep_alive;
	int status;
	char *location;
	int content_length;
	time_t t;
	header_t other_header;
        char *time_str;
} response_t;

int read_head(request_t *request, char *line);

void init_request(request_t *request);

int is_valid_status(int status);

int read_http_request(request_t *request, char *str, int len);

void init_response(response_t *response);

void deep_copy(char **dest, char *src, int len);

int http_response(response_t *response, request_t *request);

void print_http_response(response_t *response);

int get_status_name(char *name, int status);

void get_time_in_http_format(char *str, time_t _time);

int http_response_400(char *str);

int write_http_response(char *str, response_t *response);

#endif
