/*
 * config.h
 */

/* templates */
#define WEB_SERVER "darkhttpd _sources --port %d --addr 127.0.0.1 --maxconn 4"
#define REMOTE_TMP_PATH "/tmp/rset_staging_%d"
#define REMOTE_SCRIPT_PATH "/tmp/rset_staging_%d/_script"
#define LOCAL_SOCKET_PATH "/tmp/rset_control_%s"

/* defaults */
#define WEB_SERVER_STARTUP 1 /* lines to strip: copyright messages, etc. */
#define INSTALL_PORT 6000
#define ROUTES_FILE "routes.pln"
#define REPLICATED_DIRECTORY "_rutils"
#define PUBLIC_DIRECTORY "_sources"
#define DEFAULT_LABEL_PATTERN "^[0-9a-z]"

/* option defaults */
#define EXECUTE_WITH ""
#define INSTALL_URL "http://127.0.0.1:6000"
#define INTERPRETER "/bin/sh"

/* colors */
#define HL_REVERSE  "\x1b[7m"
#define HL_RESET    "\x1b[0m"
#define HL_HOST     "\x1b[33m" /* yellow */
#define HL_LABEL    "\x1b[36m" /* cyan */
#define HL_HTTP_LOG "\x1b[40m" /* gray background */

/* checks */
#define REQUIRE_SSH_AGENT
