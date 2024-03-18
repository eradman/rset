/*
 * config.h
 */

/* templates */
#define REMOTE_TMP_PATH "/tmp/rset_staging_%d"
#define LOCAL_SOCKET_PATH "/tmp/rset_control_%s"
#define LOG_TIMESTAMP_FORMAT "%F %T%z"
#define WORKER_TIMESTAMP_FORMAT "%F_%H%M%S"

/* defaults */
#define INSTALL_PORT 6000
#define ROUTES_FILE "routes.pln"
#define REPLICATED_DIRECTORY "_rutils"
#define PUBLIC_DIRECTORY "_sources"
#define DEFAULT_LABEL_PATTERN "^[0-9a-z]"

/* option defaults */
#define EXECUTE_WITH ""
#define INSTALL_URL "http://127.0.0.1:6000"
#define INTERPRETER "/bin/sh"
#define LOCAL_INTERPRETER "/bin/sh"
#define ENVIRONMENT ""
#define ENVIRONMENT_FILE "/dev/null"
#define MAX_WORKERS 20

/* colors */
#define HL_REVERSE  "\x1b[7m"
#define HL_RESET    "\x1b[0m"
#define HL_HOST     "\x1b[33m" /* yellow */
#define HL_LABEL    "\x1b[36m" /* cyan */
#define HL_ERROR    "\x1b[31m" /* red */

/* options */
#if defined(_MACOS_PORT)
#define TAR_OPTIONS "--no-xattrs"
#else
#define TAR_OPTIONS ""
#endif
