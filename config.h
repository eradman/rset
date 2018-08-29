/*
 * config.h
 */

/* templates */
#define WEB_SERVER "darkhttpd %s --port %d --addr 127.0.0.1 --maxconn 4"
#define REMOTE_TMP_PATH "/tmp/rset_staging_%d"
#define LOCAL_SOCKET_PATH "/tmp/rset_control_%s"

/* defaults */
#define INSTALL_PORT 6000
#define ROUTES_FILE "routes.pln"
#define REPLICATED_DIRECTORY "_rutils"
#define SSH_OPTIONS "-o", "UserKnownHostsFile=/dev/null", "-o", "StrictHostKeyChecking=no"

/* option defaults */
#define EXECUTE_WITH ""
#define INSTALL_URL "http://localhost:6000"
#define INTERPRETER "/bin/sh"

/* colors */
#define HL_REVERSE "\x1b[7m"
#define HL_RESET   "\x1b[0m"
#define HL_HOST    "\x1b[33m" /* yellow */
#define HL_LABEL   "\x1b[36m" /* cyan */
