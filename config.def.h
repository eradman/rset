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

/* option defaults */
#define EXECUTE_WITH ""
#define INSTALL_URL "http://localhost:6000"
#define INTERPRETER "/bin/sh"

/* colors */
#define ANSI_RESET   "\x1b[0m"
#define ANSI_REVERSE "\x1b[7m"

#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN    "\x1b[36m"

