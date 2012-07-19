#include <unistd.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include "log.h"

struct log_t{
    int  level;  /* log level */
    int  logfile;     /* log file descriptor */
    int  nerror; /* # log error */

};

char *cpystrn(char *dst, const char *src, size_t dst_size)
{
    char *d, *end;
    if (dst_size == 0) {
	return (dst);
    }
    d = dst;
    end = dst + dst_size - 1;

    for (; d < end; ++d, ++src) {
	if (!(*d = *src)) {
	    return (d);
	}
    }
    *d = '\0';	/* always null terminate */
    return (d);
}

/*
 * This function provides a way to parse a generic argument string
 * into a standard argv[] form of argument list. It respects the 
 * usual "whitespace" and quoteing rules. In the future this could
 * be expanded to include support for the apr_call_exec command line
 * string processing (including converting '+' to ' ' and doing the 
 * url processing. It does not currently support this function.
 *
 *    arg_str:       Input argument string for conversion to argv[].
 *    argv_out:      Output location. This is a pointer to an array
 *                   of pointers to strings (ie. &(char *argv[]).
 *                   This value will be allocated from the contexts
 *                   pool and filled in with copies of the tokens
 *                   found during parsing of the arg_str. 
 */

static int tokenize_to_argv(const char *arg_str, 
	char ***argv_out)
{
    const char *cp;
    const char *ct;
    char *cleaned, *dirty;
    int escaped;
    int isquoted, numargs = 0, argnum;

#define SKIP_WHITESPACE(cp) \
    for ( ; *cp == ' ' || *cp == '\t'; ) { \
	cp++; \
    };

#define CHECK_QUOTATION(cp,isquoted) \
    isquoted = 0; \
    if (*cp == '"') { \
	isquoted = 1; \
	cp++; \
    } \
    else if (*cp == '\'') { \
	isquoted = 2; \
	cp++; \
    }

    /* DETERMINE_NEXTSTRING:
     * At exit, cp will point to one of the following:  NULL, SPACE, TAB or QUOTE.
     * NULL implies the argument string has been fully traversed.
     */
#define DETERMINE_NEXTSTRING(cp,isquoted) \
    for ( ; *cp != '\0'; cp++) { \
	if (   (*cp == '\\' && (*(cp+1) == ' ' || *(cp+1) == '\t' || \
			*(cp+1) == '"' || *(cp+1) == '\''))) { \
	    cp++; \
	    continue; \
	} \
	if (   (!isquoted && (*cp == ' ' || *cp == '\t')) \
		|| (isquoted == 1 && *cp == '"') \
		|| (isquoted == 2 && *cp == '\'')                 ) { \
	    break; \
	} \
    }

    /* REMOVE_ESCAPE_CHARS:
     * Compresses the arg string to remove all of the '\' escape chars.
     * The final argv strings should not have any extra escape chars in it.
     */
#define REMOVE_ESCAPE_CHARS(cleaned, dirty, escaped) \
    escaped = 0; \
    while(*dirty) { \
	if (!escaped && *dirty == '\\') { \
	    escaped = 1; \
	} \
	else { \
	    escaped = 0; \
	    *cleaned++ = *dirty; \
	} \
	++dirty; \
    } \
    *cleaned = 0;        /* last line of macro... */

    cp = arg_str;
    SKIP_WHITESPACE(cp);
    ct = cp;

    /* This is ugly and expensive, but if anyone wants to figure a
     * way to support any number of args without counting and 
     * allocating, please go ahead and change the code.
     *
     * Must account for the trailing NULL arg.
     */
    numargs = 1;
    while (*ct != '\0') {
	CHECK_QUOTATION(ct, isquoted);
	DETERMINE_NEXTSTRING(ct, isquoted);
	if (*ct != '\0') {
	    ct++;
	}
	numargs++;
	SKIP_WHITESPACE(ct);
    }
    *argv_out = (char **)calloc(sizeof(char*),numargs);

    /*  determine first argument */
    for (argnum = 0; argnum < (numargs-1); argnum++) {
	SKIP_WHITESPACE(cp);
	CHECK_QUOTATION(cp, isquoted);
	ct = cp;
	DETERMINE_NEXTSTRING(cp, isquoted);
	cp++;
	(*argv_out)[argnum] = (char *)malloc(cp - ct);
	cpystrn((*argv_out)[argnum], ct, cp - ct);
	cleaned = dirty = (*argv_out)[argnum];
	REMOVE_ESCAPE_CHARS(cleaned, dirty, escaped);
    }
    (*argv_out)[argnum] = NULL;

    return 0;
}

#define SHELL_PATH "/bin/sh"
static int log_child(const char *progname,
	int *fpin)
{
    /* Child process code for 'ErrorLog "|..."';
     * may want a common framework for this, since I expect it will
     * be common for other foo-loggers to want this sort of thing...
     */
    pid_t pid;

    char **args;
    const char *pname;

    tokenize_to_argv(progname, &args);
    pname = strdup(args[0]);
    int filedes[2];
    if (pipe(filedes) == -1) {
	return errno; 
    }   

    if ((pid = fork()) < 0) {   
	return errno;
    }
    else if(pid == 0) {
	//child
	dup2(filedes[0], STDIN_FILENO); 
	close(filedes[0]);
	close(filedes[1]);
	signal(SIGCHLD, SIG_DFL);
	int onearg_len = 0;
	const char *newargs[4];

	newargs[0] = SHELL_PATH;
	newargs[1] = "-c";

	int i = 0;
	while (args[i]) {
	    onearg_len += strlen(args[i]);
	    onearg_len++; /* for space delimiter */
	    i++;
	}

	switch(i) {
	    case 0:
		/* bad parameters; we're doomed */
		break;
	    case 1:
		/* no args, or caller already built a single string from
		 *                  * progname and args
		 *                                   */
		newargs[2] = args[0];
		break;
	    default:
		{
		    char *ch, *onearg;
		    ch = onearg = (char *)malloc(onearg_len);
		    i = 0;
		    while (args[i]) {
			size_t len = strlen(args[i]);
			memcpy(ch, args[i], len);
			ch += len;
			*ch = ' ';
			++ch;
			++i;
		    }
		    --ch; /* back up to trailing blank */
		    *ch = '\0';
		    newargs[2] = onearg;
		}
	}

	newargs[3] = NULL;
	execv(SHELL_PATH, (char * const *)newargs);
	_exit(-1);
    }

    close(filedes[0]);
    *fpin = filedes[1];
    return 0;
}

int log_open(log_t **log,const char *filename)
{
    int rc;
    *log = NULL;
    (*log) = (log_t *)calloc(1,sizeof(log_t));
    log_level_set(*log,LOG_ERR);
    if(!filename){
        return -1;
    }

    if (*filename == '|') {
        rc = log_child(filename + 1, &((*log)->logfile));
        log_error(*log,
                "Start ErrorLog process!");
        if(rc != 0)
            return -1;
    }
    else{
        (*log)->logfile = open(filename,O_CREAT|O_RDWR|O_APPEND|O_LARGEFILE,0644);
        if((*log)->logfile != -1)
        {
            return 0;
        }
    }

    return 0;
}

static int write_full(int thefile, const void *buf, size_t nbytes)
{
    ssize_t amt = nbytes;

    do {
        amt = write(thefile, buf, amt);
        if(amt > 0)
        {
            buf = (char *)buf + amt;
            nbytes -= amt;
        }
    } while ((amt >= 0) && (nbytes > 0));

    return 0;
}

typedef struct time_exp_t time_exp_t;

struct time_exp_t {
    /** microseconds past tm_sec */
    int32_t tm_usec;
    /** (0-61) seconds past tm_min */
    int32_t tm_sec;
    /** (0-59) minutes past tm_hour */
    int32_t tm_min;
    /** (0-23) hours past midnight */
    int32_t tm_hour;
    /** (1-31) day of the month */
    int32_t tm_mday;
    /** (0-11) month of the year */
    int32_t tm_mon;
    /** year since 1900 */
    int32_t tm_year;
    /** (0-6) days since sunday */
    int32_t tm_wday;
    /** (0-365) days since jan 1 */
    int32_t tm_yday;
    /** daylight saving time */
    int32_t tm_isdst;
    /** seconds east of UTC */
    int32_t tm_gmtoff;
};

#define USEC_PER_SEC (int64_t)(1000000)

static void explode_time(time_exp_t *xt, int64_t t,
	int32_t offset, int use_localtime)
{
    struct tm tm;
    time_t tt = (t / USEC_PER_SEC) + offset;
    xt->tm_usec = t % USEC_PER_SEC;

    if (use_localtime)
	localtime_r(&tt, &tm);
    else
	gmtime_r(&tt, &tm);

    xt->tm_sec  = tm.tm_sec;
    xt->tm_min  = tm.tm_min;
    xt->tm_hour = tm.tm_hour;
    xt->tm_mday = tm.tm_mday;
    xt->tm_mon  = tm.tm_mon;
    xt->tm_year = tm.tm_year;
    xt->tm_wday = tm.tm_wday;
    xt->tm_yday = tm.tm_yday;
    xt->tm_isdst = tm.tm_isdst;
    xt->tm_gmtoff = tm.tm_gmtoff;
}

int time_exp_lt(time_exp_t *result,int64_t input)
{
    explode_time(result, input, 0, 1);
    return 0;
}

const char month_snames[12][4] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
const char day_snames[7][4] =
{
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

int log_ctime(char *date_str, int64_t t)
{
    time_exp_t xt;
    const char *s;
    int real_year;

    /* example: "Wed Jun 30 21:49:08 1993" */
    /*           123456789012345678901234  */

    time_exp_lt(&xt, t);
    s = &day_snames[xt.tm_wday][0];
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = ' ';
    s = &month_snames[xt.tm_mon][0];
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = ' ';
    *date_str++ = xt.tm_mday / 10 + '0';
    *date_str++ = xt.tm_mday % 10 + '0';
    *date_str++ = ' ';
    *date_str++ = xt.tm_hour / 10 + '0';
    *date_str++ = xt.tm_hour % 10 + '0';
    *date_str++ = ':';
    *date_str++ = xt.tm_min / 10 + '0';
    *date_str++ = xt.tm_min % 10 + '0';
    *date_str++ = ':';
    *date_str++ = xt.tm_sec / 10 + '0';
    *date_str++ = xt.tm_sec % 10 + '0';
    *date_str++ = ' ';
    real_year = 1900 + xt.tm_year;
    *date_str++ = real_year / 1000 + '0';
    *date_str++ = real_year % 1000 / 100 + '0';
    *date_str++ = real_year % 100 / 10 + '0';
    *date_str++ = real_year % 10 + '0';
    *date_str++ = 0;

    return 0;
}

/* NB NB NB NB This returns GMT!!!!!!!!!! */
int64_t time_now(void)
{
    struct timeval tv;              
    gettimeofday(&tv, NULL);
    return tv.tv_sec * USEC_PER_SEC + tv.tv_usec;
}

#define EOL_STR 	"\n"
#define CTIME_LEN	25
static void log_error_core(int logfile,const char *file, int line,
	const char *fmt, va_list args)
{
    char errstr[MAX_LOG_LEN];
    memset(errstr,0,sizeof(errstr));
    size_t len=0;

    const char *p;
    /* On Unix, __FILE__ may be an absolute path in a
     * VPATH build. */
    if (file[0] == '/' && (p = strrchr(file, '/')) != NULL) {
	file = p + 1;
    }
    char time_str[CTIME_LEN];
    log_ctime(time_str,time_now());
    len += snprintf(errstr + len, MAX_LOG_LEN - len,
	    "[%s]%s(%d): ",time_str,file, line);

    len += vsnprintf(errstr+len, MAX_LOG_LEN-len, fmt, args);

    if (logfile) {
	/* Truncate for the terminator (as apr_snprintf does) */
	if (len > MAX_LOG_LEN - sizeof(EOL_STR)) {
	    len = MAX_LOG_LEN - sizeof(EOL_STR);
	}
	strcpy(errstr + len, EOL_STR);
	size_t len;
	len = strlen(errstr);
	write_full(logfile, errstr, len); 
    }
}

void _log(log_t *log,const char *file, 
		             int line,
                             const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_error_core(log->logfile,file, line,fmt, args);
    va_end(args);
}

void log_close(log_t *log)
{
    if(log)
    {
	close(log->logfile);
	free(log);
    }
}

void
log_level_up(log_t *log)
{
    struct log_t *l = log;

    if (l->level < LOG_PVERB) {
        l->level++;
        loga(l,"up log level to %d", l->level);
    }
}

void
log_level_down(log_t *log)
{
    struct log_t *l = log;

    if (l->level > LOG_EMERG) {
        l->level--;
        loga(l,"down log level to %d", l->level);
    }
}

void
log_level_set(log_t *log,int level)
{
    struct log_t *l = log;

    l->level = MAX(LOG_EMERG, MIN(level, LOG_PVERB));
    loga(l,"set log level to %d", l->level);
}

int
log_loggable(log_t *log,int level)
{
    struct log_t *l = log;

    if (level > l->level) {
        return 0;
    }

    return 1;
}

void
_log_stderr(const char *fmt, ...)
{
    int len, size;
    char buf[MAX_LOG_LEN];
    va_list args;
    ssize_t n;

    len = 0;                /* length of output buffer */
    size = MAX_LOG_LEN; /* size of output buffer */

    va_start(args, fmt);
    len += vsnprintf(buf, size, fmt, args);
    va_end(args);

    buf[len++] = '\n';

    n = write(STDERR_FILENO, buf, len);
}

/*
 * Hexadecimal dump in the canonical hex + ascii display
 * See -C option in man hexdump
 */
void
_log_hexdump(log_t *log,char *data, int datalen)
{
    struct log_t *l = log;
    char buf[MAX_LOG_LEN];
    int i, off, len, size, errno_save;

    if (l->logfile < 0) {
        return;
    }

    /* log hexdump */
    errno_save = errno;
    off = 0;                  /* data offset */
    len = 0;                  /* length of output buffer */
    size = MAX_LOG_LEN;   /* size of output buffer */

    while (datalen != 0 && (len < size - 1)) {
        char *save, *str;
        unsigned char c;
        int savelen;

        len += snprintf(buf + len, size - len, "%08x  ", off);

        save = data;
        savelen = datalen;

        for (i = 0; datalen != 0 && i < 16; data++, datalen--, i++) {
            c = (unsigned char)(*data);
            str = (char *)((i == 7) ? "  " : " ");
            len += snprintf(buf + len, size - len, "%02x%s", c, str);
        }
        for ( ; i < 16; i++) {
            str = (char *)((i == 7) ? "  " : " ");
            len += snprintf(buf + len, size - len, "  %s", str);
        }

        data = save;
        datalen = savelen;

        len += snprintf(buf + len, size - len, "  |");

        for (i = 0; datalen != 0 && i < 16; data++, datalen--, i++) {
            c = (unsigned char)(isprint(*data) ? *data : '.');
            len += snprintf(buf + len, size - len, "%c", c);
        }
        len += snprintf(buf + len, size - len, "|\n");

        off += 16;
    }

	write_full(l->logfile, buf, len); 
    errno = errno_save;
}

/*
#include <stdio.h>
#include "log.h"
int main(void)
{
    log_t *log;
    const char *str = "hexdump test!";
    //log_open(&log,"|/usr/local/sbin/cronolog logs/%Y-%m-%d.%H.log");
    log_open(&log,"|/usr/sbin/rotatelogs logs/log.%Y-%m-%d-%H_%M_%S 100M");
    //log_open(&log,"log.txt");
    while(1)
    {
        //log_error(log,"testlog:%d",1);
        loga_hexdump(log,str,strlen(str),"hexdump");
    }
    log_close(log);
    return 0;
}
*/
