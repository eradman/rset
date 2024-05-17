/* miniquark: see LICENSE file for copyright and license details. */

#include <sys/stat.h>

#include <limits.h>

#define HEADER_MAX 4096
#define FIELD_MAX 200
#define TIMESTAMP_LEN 30

#define MIN(x,y)  ((x) < (y) ? (x) : (y))
#define LEN(x) (sizeof (x) / sizeof *(x))
#define RELPATH(x) ((!*(x) || !strcmp(x, "/")) ? "." : ((x) + 1))

enum req_field {
	REQ_RANGE,
	REQ_IF_MODIFIED_SINCE,
	REQ_AGENT,
	NUM_REQ_FIELDS,
};

extern const char *req_field_str[];

enum req_method {
	M_GET,
	M_HEAD,
	NUM_REQ_METHODS,
};

extern const char *req_method_str[];

struct request {
	enum req_method method;
	char target[PATH_MAX];
	char field[NUM_REQ_FIELDS][FIELD_MAX];
	unsigned long bytes_sent;
};

enum status {
	S_OK                    = 200,
	S_PARTIAL_CONTENT       = 206,
	S_MOVED_PERMANENTLY     = 301,
	S_NOT_MODIFIED          = 304,
	S_BAD_REQUEST           = 400,
	S_FORBIDDEN             = 403,
	S_NOT_FOUND             = 404,
	S_METHOD_NOT_ALLOWED    = 405,
	S_REQUEST_TIMEOUT       = 408,
	S_RANGE_NOT_SATISFIABLE = 416,
	S_REQUEST_TOO_LARGE     = 431,
	S_INTERNAL_SERVER_ERROR = 500,
	S_VERSION_NOT_SUPPORTED = 505,
};

extern const char *status_str[];

enum status http_send_status(int, enum status);
int http_get_request(int, struct request *);
enum status http_send_response(int, struct request *);
enum status resp_file(int, const char *, struct request *, const struct stat *, long, long);
