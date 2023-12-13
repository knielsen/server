/* Copyright (C) 2019, Alexey Botchkov and MariaDB Corporation

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA */


#define PLUGIN_VERSION 0x0204
#define PLUGIN_STR_VERSION "2.4.1"

#define _my_thread_var loc_thread_var

#include <my_config.h>
#include <assert.h>


#ifndef _WIN32
#define DO_SYSLOG
#include <syslog.h>
#else
#define syslog(PRIORITY, FORMAT, INFO, MESSAGE_LEN, MESSAGE) do {}while(0)
static void closelog() {}
#define openlog(IDENT, LOG_NOWAIT, LOG_USER)  do {}while(0)

/* priorities */
#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */

#define LOG_MAKEPRI(fac, pri)   (((fac) << 3) | (pri))

/* facility codes */
#define LOG_KERN        (0<<3)  /* kernel messages */
#define LOG_USER        (1<<3)  /* random user-level messages */
#define LOG_MAIL        (2<<3)  /* mail system */
#define LOG_DAEMON      (3<<3)  /* system daemons */
#define LOG_AUTH        (4<<3)  /* security/authorization messages */
#define LOG_SYSLOG      (5<<3)  /* messages generated internally by syslogd */
#define LOG_LPR         (6<<3)  /* line printer subsystem */
#define LOG_NEWS        (7<<3)  /* network news subsystem */
#define LOG_UUCP        (8<<3)  /* UUCP subsystem */
#define LOG_CRON        (9<<3)  /* clock daemon */
#define LOG_AUTHPRIV    (10<<3) /* security/authorization messages (private) */
#define LOG_FTP         (11<<3) /* ftp daemon */
#define LOG_LOCAL0      (16<<3) /* reserved for local use */
#define LOG_LOCAL1      (17<<3) /* reserved for local use */
#define LOG_LOCAL2      (18<<3) /* reserved for local use */
#define LOG_LOCAL3      (19<<3) /* reserved for local use */
#define LOG_LOCAL4      (20<<3) /* reserved for local use */
#define LOG_LOCAL5      (21<<3) /* reserved for local use */
#define LOG_LOCAL6      (22<<3) /* reserved for local use */
#define LOG_LOCAL7      (23<<3) /* reserved for local use */

#endif /*!_WIN32*/

/*
   Defines that can be used to reshape the pluging:
   #define MARIADB_ONLY
   #define USE_MARIA_PLUGIN_INTERFACE
*/

#if !defined(MYSQL_DYNAMIC_PLUGIN) && !defined(MARIADB_ONLY)
#include <typelib.h>
#define MARIADB_ONLY
#endif /*MYSQL_PLUGIN_DYNAMIC*/

#ifndef MARIADB_ONLY
#define MYSQL_SERVICE_LOGGER_INCLUDED
#endif /*MARIADB_ONLY*/

#include <my_global.h>
#include <my_base.h>
#include <typelib.h>
#include <mysql_com.h>  /* for enum enum_server_command */
#include <mysql/plugin.h>
#include <mysql/plugin_audit.h>
#include <string.h>
#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT NULL
#endif

LEX_STRING * thd_query_string (MYSQL_THD thd);
unsigned long long thd_query_id(const MYSQL_THD thd);
size_t thd_query_safe(MYSQL_THD thd, char *buf, size_t buflen);
const char *thd_priv_user(MYSQL_THD thd,  size_t *length);
const char *thd_priv_host(MYSQL_THD thd,  size_t *length);
const char *thd_user_name(MYSQL_THD thd);
const char *thd_client_host(MYSQL_THD thd);
const char *thd_client_ip(MYSQL_THD thd);
LEX_CSTRING *thd_current_db(MYSQL_THD thd);
int thd_current_status(MYSQL_THD thd);
enum enum_server_command thd_current_command(MYSQL_THD thd);
size_t thd_query_table_list(MYSQL_THD thd,
   MYSQL_CONST_LEX_STRING *db_table_names, size_t max_names);

int maria_compare_hostname(const char *wild_host, long wild_ip, long ip_mask,
                         const char *host, const char *ip);
void maria_update_hostname(const char **wild_host, long *wild_ip, long *ip_mask,
                         const char *host);

#ifndef MARIADB_ONLY
#undef MYSQL_SERVICE_LOGGER_INCLUDED
#undef MYSQL_DYNAMIC_PLUGIN
#define FLOGGER_NO_PSI

/* How to access the pthread_mutex in mysql_mutex_t */
#ifdef SAFE_MUTEX
#define mysql_mutex_real_mutex(A) &(A)->m_mutex.mutex
#else
#define mysql_mutex_real_mutex(A) &(A)->m_mutex
#endif

#define flogger_mutex_init(A,B,C) do{}while(0)
#define flogger_mutex_destroy(A) do{}while(0)
#define flogger_mutex_lock(A) do{}while(0)
#define flogger_mutex_unlock(A) do{}while(0)

static char **int_mysql_data_home;
static char *default_home= (char *)".";
#define mysql_data_home (*int_mysql_data_home)

#define FLOGGER_SKIP_INCLUDES
#define my_open(A, B, C) loc_open(A, B)
#define my_close(A, B) loc_close(A)
#define my_rename(A, B, C) loc_rename(A, B)
#define my_tell(A, B) loc_tell(A)
#define my_write(A, B, C, D) loc_write(A, B, C)
#define my_malloc(A, B) malloc(A)
#define my_free(A) free(A)
#ifdef my_errno
  #undef my_errno
#endif
static int loc_file_errno;
#define my_errno loc_file_errno
#ifdef my_vsnprintf
  #undef my_vsnprintf
#endif
#define my_vsnprintf vsnprintf
#define logger_open loc_logger_open
#define logger_close loc_logger_close
#define logger_write loc_logger_write
#define logger_rotate loc_logger_rotate
#define logger_init_mutexts loc_logger_init_mutexts


enum filter_cond_func
{
  COND_ERROR= 0,
  COND_TRUE,
  COND_FALSE,
  COND_OR,
  COND_AND,
  COND_OR_NOT,
  COND_AND_NOT,
  COND_EVENT,
  COND_EVENT_QUERY,
  COND_EVENT_TABLE,
  COND_EVENT_CONNECT,
  COND_TABLES,
  COND_TABLES_IGNORE,
  COND_DATABASES,
  COND_LOGGING
};


struct table_filter_item
{
  size_t db_len;
  char db[64*3];
  size_t name_len;
  char name[64*3];
  struct table_filter_item *next;
};


struct table_filter
{
  size_t n_items;
  struct table_filter_item *t_list;
  struct table_filter_item **t_hook;
};


struct filter_cond
{
  enum filter_cond_func func;

  struct filter_cond *arg_list;
  struct table_filter tables;
  unsigned long mask;

  unsigned long query_mask;
  struct filter_cond *next;
};


struct filter_def
{
  char name[80];
  struct filter_cond *rule;
  struct filter_def *next;
};


//static int filter_version= 1;
static struct filter_def *filter_list= 0;
static struct filter_def *default_filter= 0;


struct user_def
{
  char *name;
  char *host;
  struct filter_def *filter;
  struct user_def *next;
};


static struct user_def *user_list= 0;

static size_t loc_write(File Filedes, const uchar *Buffer, size_t Count)
{
  size_t writtenbytes;
#ifdef _WIN32
  writtenbytes= (size_t)_write(Filedes, Buffer, (unsigned int)Count);
#else
  writtenbytes= write(Filedes, Buffer, Count);
#endif
  return writtenbytes;
}


static File loc_open(const char *FileName, int Flags)
				/* Path-name of file */
				/* Read | write .. */
				/* Special flags */
{
  File fd;
#ifdef _WIN32
  HANDLE h;
  /*
    We could just use _open() here. but prefer to open in unix-similar way
    just like my_open() does it on Windows.
    This gives atomic multiprocess-safe appends, and possibility to rename
    or even delete file while it is open, and CRT lacks this features.
  */
  assert(Flags == (O_APPEND | O_CREAT | O_WRONLY));
  h= CreateFile(FileName, FILE_APPEND_DATA,
    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL,
    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
  {
    fd= -1;
    my_osmaperr(GetLastError());
  }
  else
  {
    fd= _open_osfhandle((intptr)h, O_WRONLY|O_BINARY);
  }
#else
  fd= open(FileName, Flags, my_umask);
#endif
  my_errno= errno;
  return fd;
} 


static int loc_close(File fd)
{
  int err;
#ifndef _WIN32
  do
  {
    err= close(fd);
  } while (err == -1 && errno == EINTR);
#else
  err= close(fd);
#endif
  my_errno=errno;
  return err;
}


static int loc_rename(const char *from, const char *to)
{
  int error = 0;

#if defined(_WIN32)
  if (!MoveFileEx(from, to, MOVEFILE_COPY_ALLOWED |
                            MOVEFILE_REPLACE_EXISTING))
  {
    my_osmaperr(GetLastError());
#elif defined(HAVE_RENAME)
  if (rename(from,to))
  {
#else
  if (link(from, to) || unlink(from))
  {
#endif
    my_errno=errno;
    error = -1;
  }
  return error;
}


static my_off_t loc_tell(File fd)
{
  os_off_t pos= IF_WIN(_telli64(fd),lseek(fd, 0, SEEK_CUR));
  if (pos == (os_off_t) -1)
  {
    my_errno= errno;
  }
  return (my_off_t) pos;
}

#ifdef HAVE_PSI_INTERFACE
#undef HAVE_PSI_INTERFACE
#include <mysql/service_logger.h>
#include "../../mysys/file_logger.c"
#define HAVE_PSI_INTERFACE
#else
#include <mysql/service_logger.h>
#include "../../mysys/file_logger.c"
#endif
#endif /*!MARIADB_ONLY*/

#undef flogger_mutex_init
#undef flogger_mutex_destroy
#undef flogger_mutex_lock
#undef flogger_mutex_unlock

#define flogger_mutex_init(A,B,C) pthread_mutex_init(mysql_mutex_real_mutex(B), C)
#define flogger_mutex_destroy(A) pthread_mutex_destroy(mysql_mutex_real_mutex(A))
#define flogger_mutex_lock(A) pthread_mutex_lock(mysql_mutex_real_mutex(A))
#define flogger_mutex_unlock(A) pthread_mutex_unlock(mysql_mutex_real_mutex(A))

#ifndef DBUG_OFF
#define PLUGIN_DEBUG_VERSION "-debug"
#else
#define PLUGIN_DEBUG_VERSION ""
#endif /*DBUG_OFF*/
/*
 Disable __attribute__() on non-gcc compilers.
*/
#if !defined(__attribute__) && !defined(__GNUC__)
#define __attribute__(A)
#endif

#ifdef _WIN32
#define localtime_r(a, b) localtime_s(b, a)
#endif /*WIN32*/


extern char server_version[];
static const char *serv_ver= NULL;
static int started_mysql= 0;
static int mysql_57_started= 0;
static int debug_server_started= 0;
static int use_event_data_for_disconnect= 0;
static int started_mariadb= 0;
static int maria_55_started= 0;
static int maria_above_5= 0;
static char *file_path, *syslog_info;
static char path_buffer[FN_REFLEN];
static unsigned int mode, mode_readonly= 0;
static ulong output_type;
static ulong syslog_facility, syslog_priority;

static unsigned long long file_rotate_size;
static unsigned int rotations;
static my_bool rotate= TRUE;
static my_bool reload_filters= TRUE;
static my_bool load_on_error= TRUE;
static char logging;
static volatile int internal_stop_logging= 0;
static unsigned int query_log_limit= 0;

static char servhost[256];
static uint servhost_len;
static char *syslog_ident;
static char syslog_ident_buffer[128]= "mysql-server_auditing";

struct connection_info
{
  int header;
  MYSQL_THD thd;
  const char *user;
  unsigned int user_len;
  const char *host;
  unsigned int host_len;
  const char *ip;
  unsigned int ip_len;
  MYSQL_CONST_LEX_STRING db, table;
  MYSQL_CONST_LEX_STRING new_db, new_table;
  const char *query;
  unsigned int query_len;
  time_t query_time;
  unsigned long event_type;
  int event_subclass;
  int read_only;
  unsigned long long query_id;
  unsigned long thread_id;
  int error_code;
  //struct filter_def *filter;
  //int filter_version;
};

static void setc(const char *v, const char **sv, unsigned int *v_len)
{
  *sv= v ? v : "";
  *v_len= (unsigned int) strlen(*sv);
}


#define DEFAULT_FILENAME_LEN 16
static char default_file_name[DEFAULT_FILENAME_LEN+1]= "server_audit.log";

static void update_query_log_limit(MYSQL_THD thd, struct st_mysql_sys_var *var,
                                   void *var_ptr, const void *save);
static void update_file_path(MYSQL_THD thd, struct st_mysql_sys_var *var,
                             void *var_ptr, const void *save);
static void update_file_rotate_size(MYSQL_THD thd, struct st_mysql_sys_var *var,
                                    void *var_ptr, const void *save);
static void update_file_rotations(MYSQL_THD thd, struct st_mysql_sys_var *var,
                                  void *var_ptr, const void *save);
static void update_output_type(MYSQL_THD thd, struct st_mysql_sys_var *var,
                               void *var_ptr, const void *save);
static void update_syslog_facility(MYSQL_THD thd, struct st_mysql_sys_var *var,
                                   void *var_ptr, const void *save);
static void update_syslog_priority(MYSQL_THD thd, struct st_mysql_sys_var *var,
                                   void *var_ptr, const void *save);
static void update_mode(MYSQL_THD thd, struct st_mysql_sys_var *var,
                        void *var_ptr, const void *save);
static void update_logging(MYSQL_THD thd, struct st_mysql_sys_var *var,
                           void *var_ptr, const void *save);
static void update_syslog_ident(MYSQL_THD thd, struct st_mysql_sys_var *var,
                                void *var_ptr, const void *save);
static void rotate_log(MYSQL_THD thd, struct st_mysql_sys_var *var,
                       void *var_ptr, const void *save);
static void do_reload_filters(MYSQL_THD thd, struct st_mysql_sys_var *var,
                              void *var_ptr, const void *save);

/* bits in the event filter. */

#ifdef DO_SYSLOG
#define OUTPUT_SYSLOG 0
#define OUTPUT_FILE 1
#else
#define OUTPUT_SYSLOG 0xFFFF
#define OUTPUT_FILE 0
#endif /*DO_SYSLOG*/

#define OUTPUT_NO 0xFFFF
static const char *output_type_names[]= {
#ifdef DO_SYSLOG
  "syslog",
#endif
  "file", 0 };
static TYPELIB output_typelib=
{
    array_elements(output_type_names) - 1, "output_typelib",
    output_type_names, NULL
};
static MYSQL_SYSVAR_ENUM(output_type, output_type, PLUGIN_VAR_RQCMDARG,
       "Desired output type. Possible values - 'syslog', 'file'"
       " or 'null' as no output.", 0, update_output_type, OUTPUT_FILE,
       &output_typelib);
static MYSQL_SYSVAR_STR(file_path, file_path, PLUGIN_VAR_RQCMDARG,
       "Path to the log file.", NULL, update_file_path, default_file_name);
static MYSQL_SYSVAR_ULONGLONG(file_rotate_size, file_rotate_size,
       PLUGIN_VAR_RQCMDARG, "Maximum size of the log to start the rotation.",
       NULL, update_file_rotate_size,
       1000000, 100, ((long long) 0x7FFFFFFFFFFFFFFFLL), 1);
static MYSQL_SYSVAR_UINT(file_rotations, rotations,
       PLUGIN_VAR_RQCMDARG, "Number of rotations before log is removed.",
       NULL, update_file_rotations, 9, 0, 999, 1);
static MYSQL_SYSVAR_BOOL(file_rotate_now, rotate, PLUGIN_VAR_OPCMDARG,
       "Force log rotation now.", NULL, rotate_log, FALSE);
static MYSQL_SYSVAR_BOOL(reload_filters, reload_filters, PLUGIN_VAR_OPCMDARG,
       "Reload filter definitions from tables.", NULL, do_reload_filters, FALSE);
static MYSQL_SYSVAR_BOOL(logging, logging,
       PLUGIN_VAR_OPCMDARG, "Turn on/off the logging.", NULL,
       update_logging, 0);
static MYSQL_SYSVAR_UINT(mode, mode,
       PLUGIN_VAR_OPCMDARG, "Auditing mode.", NULL, update_mode, 0, 0, 1, 1);
static MYSQL_SYSVAR_STR(syslog_ident, syslog_ident, PLUGIN_VAR_RQCMDARG,
       "The SYSLOG identifier - the beginning of each SYSLOG record.",
       NULL, update_syslog_ident, syslog_ident_buffer);
static MYSQL_SYSVAR_STR(syslog_info, syslog_info,
       PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
       "The <info> string to be added to the SYSLOG record.", NULL, NULL, "");
static MYSQL_SYSVAR_UINT(query_log_limit, query_log_limit,
       PLUGIN_VAR_OPCMDARG, "Limit on the length of the query string in a record.",
       NULL, update_query_log_limit, 1024, 0, 0x7FFFFFFF, 1);
static MYSQL_SYSVAR_BOOL(load_on_error, load_on_error,
       PLUGIN_VAR_OPCMDARG,
       "Do not load the plugin if filter definitions are invalid.",
       NULL, NULL, TRUE);

char locinfo_ini_value[sizeof(struct connection_info)+4];

static MYSQL_THDVAR_STR(loc_info,
                        PLUGIN_VAR_NOSYSVAR | PLUGIN_VAR_NOCMDOPT | PLUGIN_VAR_MEMALLOC,
                        "Internal info", NULL, NULL, locinfo_ini_value);

static const char *syslog_facility_names[]=
{
  "LOG_USER", "LOG_MAIL", "LOG_DAEMON", "LOG_AUTH",
  "LOG_SYSLOG", "LOG_LPR", "LOG_NEWS", "LOG_UUCP",
  "LOG_CRON",
#ifdef LOG_AUTHPRIV
 "LOG_AUTHPRIV",
#endif
#ifdef LOG_FTP
 "LOG_FTP",
#endif
  "LOG_LOCAL0", "LOG_LOCAL1", "LOG_LOCAL2", "LOG_LOCAL3",
  "LOG_LOCAL4", "LOG_LOCAL5", "LOG_LOCAL6", "LOG_LOCAL7",
  0
};
#ifndef _WIN32
static unsigned int syslog_facility_codes[]=
{
  LOG_USER, LOG_MAIL, LOG_DAEMON, LOG_AUTH,
  LOG_SYSLOG, LOG_LPR, LOG_NEWS, LOG_UUCP,
  LOG_CRON,
#ifdef LOG_AUTHPRIV
 LOG_AUTHPRIV,
#endif
#ifdef LOG_FTP
  LOG_FTP,
#endif
  LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3,
  LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7,
};
#endif
static TYPELIB syslog_facility_typelib=
{
    array_elements(syslog_facility_names) - 1, "syslog_facility_typelib",
    syslog_facility_names, NULL
};
static MYSQL_SYSVAR_ENUM(syslog_facility, syslog_facility, PLUGIN_VAR_RQCMDARG,
       "The 'facility' parameter of the SYSLOG record."
       " The default is LOG_USER.", 0, update_syslog_facility, 0/*LOG_USER*/,
       &syslog_facility_typelib);

static const char *syslog_priority_names[]=
{
  "LOG_EMERG", "LOG_ALERT", "LOG_CRIT", "LOG_ERR",
  "LOG_WARNING", "LOG_NOTICE", "LOG_INFO", "LOG_DEBUG",
  0
};

#ifndef _WIN32
static unsigned int syslog_priority_codes[]=
{
  LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR,
  LOG_WARNING, LOG_NOTICE, LOG_INFO, LOG_DEBUG,
};
#endif

static TYPELIB syslog_priority_typelib=
{
    array_elements(syslog_priority_names) - 1, "syslog_priority_typelib",
    syslog_priority_names, NULL
};
static MYSQL_SYSVAR_ENUM(syslog_priority, syslog_priority, PLUGIN_VAR_RQCMDARG,
       "The 'priority' parameter of the SYSLOG record."
       " The default is LOG_INFO.", 0, update_syslog_priority, 6/*LOG_INFO*/,
       &syslog_priority_typelib);


static struct st_mysql_sys_var* vars[] = {
    MYSQL_SYSVAR(output_type),
    MYSQL_SYSVAR(file_path),
    MYSQL_SYSVAR(file_rotate_size),
    MYSQL_SYSVAR(file_rotations),
    MYSQL_SYSVAR(file_rotate_now),
    MYSQL_SYSVAR(reload_filters),
    MYSQL_SYSVAR(logging),
    MYSQL_SYSVAR(mode),
    MYSQL_SYSVAR(syslog_info),
    MYSQL_SYSVAR(syslog_ident),
    MYSQL_SYSVAR(syslog_facility),
    MYSQL_SYSVAR(syslog_priority),
    MYSQL_SYSVAR(query_log_limit),
    MYSQL_SYSVAR(loc_info),
    MYSQL_SYSVAR(load_on_error),
    NULL
};


/* Status variables for SHOW STATUS */
static int is_active= 0;
static long log_write_failures= 0;
static char current_log_buf[FN_REFLEN]= "";
static char last_error_buf[512]= "";

static char err_msg[512]= "";
static size_t err_len= 0;

static int show_filter(MYSQL_THD thd, struct st_mysql_show_var *var,
                       char *buff, struct system_status_var *sv,
                       enum enum_var_type scope);

static struct st_mysql_show_var audit_status[]=
{
  {"server_audit_active", (char *)&is_active, SHOW_BOOL},
  {"server_audit_current_log", current_log_buf, SHOW_CHAR},
  {"server_audit_writes_failed", (char *)&log_write_failures, SHOW_LONG},
  {"server_audit_last_error", last_error_buf, SHOW_CHAR},
  {"server_audit_session_filter", (char*) &show_filter, SHOW_SIMPLE_FUNC},
  {0,0,0}
};


#if defined(HAVE_PSI_INTERFACE) && !defined(FLOGGER_NO_PSI)
/* These belong to the service initialization */
static PSI_mutex_key key_LOCK_atomic;
static PSI_mutex_key key_LOCK_bigbuffer;
#endif

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_operations;
static PSI_mutex_info mutex_key_list[]=
{
  { &key_LOCK_operations, "SERVER_AUDIT_plugin::lock_operations",
    PSI_FLAG_GLOBAL}
#ifndef FLOGGER_NO_PSI
  ,
  { &key_LOCK_atomic, "SERVER_AUDIT_plugin::lock_atomic",
    PSI_FLAG_GLOBAL},
#endif /*FLOGGER_NO_PSI*/
};
#endif /*HAVE_PSI_INTERFACE*/

static mysql_prlock_t lock_operations;
static mysql_mutex_t lock_atomic;

#define CLIENT_ERROR my_printf_error

#define ADD_ATOMIC(x, a)              \
  do {                                \
  flogger_mutex_lock(&lock_atomic);   \
  x+= a;                              \
  flogger_mutex_unlock(&lock_atomic); \
  } while (0)


enum sa_keywords
{
  SQLCOM_NOTHING=0,
  SQLCOM_DDL,
  SQLCOM_DML,
  SQLCOM_GRANT,
  SQLCOM_CREATE_USER,
  SQLCOM_ALTER_USER,
  SQLCOM_CHANGE_MASTER,
  SQLCOM_CREATE_SERVER,
  SQLCOM_SET_OPTION,
  SQLCOM_ALTER_SERVER,
  SQLCOM_TRUNCATE,
  SQLCOM_QUERY_ADMIN,
  SQLCOM_DCL,
};

struct sa_keyword
{
  int length;
  const char *wd;
  struct sa_keyword *next;
  enum sa_keywords type;
};


struct sa_keyword xml_word=   {3, "XML", 0, SQLCOM_NOTHING};
struct sa_keyword user_word=   {4, "USER", 0, SQLCOM_NOTHING};
struct sa_keyword data_word=   {4, "DATA", 0, SQLCOM_NOTHING};
struct sa_keyword server_word= {6, "SERVER", 0, SQLCOM_NOTHING};
struct sa_keyword master_word= {6, "MASTER", 0, SQLCOM_NOTHING};
struct sa_keyword password_word= {8, "PASSWORD", 0, SQLCOM_NOTHING};
struct sa_keyword function_word= {8, "FUNCTION", 0, SQLCOM_NOTHING};
struct sa_keyword statement_word= {9, "STATEMENT", 0, SQLCOM_NOTHING};
struct sa_keyword procedure_word= {9, "PROCEDURE", 0, SQLCOM_NOTHING};


struct sa_keyword keywords_to_skip[]=
{
  {3, "SET", &statement_word, SQLCOM_QUERY_ADMIN},
  {0, NULL, 0, SQLCOM_DDL}
};


struct sa_keyword not_ddl_keywords[]=
{
  {4, "DROP", &user_word, SQLCOM_DCL},
  {6, "CREATE", &user_word, SQLCOM_DCL},
  {6, "RENAME", &user_word, SQLCOM_DCL},
  {0, NULL, 0, SQLCOM_DDL}
};


struct sa_keyword ddl_keywords[]=
{
  {4, "DROP", 0, SQLCOM_DDL},
  {5, "ALTER", 0, SQLCOM_DDL},
  {6, "CREATE", 0, SQLCOM_DDL},
  {6, "RENAME", 0, SQLCOM_DDL},
  {8, "TRUNCATE", 0, SQLCOM_DDL},
  {0, NULL, 0, SQLCOM_DDL}
};


struct sa_keyword dml_keywords[]=
{
  {2, "DO", 0, SQLCOM_DML},
  {4, "CALL", 0, SQLCOM_DML},
  {4, "LOAD", &data_word, SQLCOM_DML},
  {4, "LOAD", &xml_word, SQLCOM_DML},
  {6, "DELETE", 0, SQLCOM_DML},
  {6, "INSERT", 0, SQLCOM_DML},
  {6, "SELECT", 0, SQLCOM_DML},
  {6, "UPDATE", 0, SQLCOM_DML},
  {7, "HANDLER", 0, SQLCOM_DML},
  {7, "REPLACE", 0, SQLCOM_DML},
  {0, NULL, 0, SQLCOM_DML}
};


struct sa_keyword dml_no_select_keywords[]=
{
  {2, "DO", 0, SQLCOM_DML},
  {4, "CALL", 0, SQLCOM_DML},
  {4, "LOAD", &data_word, SQLCOM_DML},
  {4, "LOAD", &xml_word, SQLCOM_DML},
  {6, "DELETE", 0, SQLCOM_DML},
  {6, "INSERT", 0, SQLCOM_DML},
  {6, "UPDATE", 0, SQLCOM_DML},
  {7, "HANDLER", 0, SQLCOM_DML},
  {7, "REPLACE", 0, SQLCOM_DML},
  {0, NULL, 0, SQLCOM_DML}
};


struct sa_keyword dcl_keywords[]=
{
  {6, "CREATE", &user_word, SQLCOM_DCL},
  {4, "DROP", &user_word, SQLCOM_DCL},
  {6, "RENAME", &user_word, SQLCOM_DCL},
  {5, "GRANT", 0, SQLCOM_DCL},
  {6, "REVOKE", 0, SQLCOM_DCL},
  {3, "SET", &password_word, SQLCOM_DCL},
  {0, NULL, 0, SQLCOM_DDL}
};


struct sa_keyword passwd_keywords[]=
{
  {3, "SET", &password_word, SQLCOM_SET_OPTION},
  {5, "ALTER", &server_word, SQLCOM_ALTER_SERVER},
  {5, "ALTER", &user_word, SQLCOM_ALTER_USER},
  {5, "GRANT", 0, SQLCOM_GRANT},
  {6, "CREATE", &user_word, SQLCOM_CREATE_USER},
  {6, "CREATE", &server_word, SQLCOM_CREATE_SERVER},
  {6, "CHANGE", &master_word, SQLCOM_CHANGE_MASTER},
  {0, NULL, 0, SQLCOM_NOTHING}
};

#define MAX_KEYWORD 20


static void error_header()
{
  struct tm tm_time;
  time_t curtime;

  (void) time(&curtime);
  (void) localtime_r(&curtime, &tm_time);

  (void) fprintf(stderr,"%d-%02d-%02d %2d:%02d:%02d server_audit: ",
    tm_time.tm_year + 1900, tm_time.tm_mon + 1,
    tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
}


static void explain_error(const char *fmt, ...)
{
  size_t len;
  va_list args;

  if (err_len > 0 && err_len < sizeof(err_msg))
  {
    err_msg[err_len++]= ' ';
  }
  va_start(args, fmt);
  len= my_vsnprintf(err_msg + err_len, sizeof(err_msg) - err_len,
                    fmt, args);
  va_end(args);
  
  error_header();
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  err_len+= len;
}


static LOGGER_HANDLE *logfile;

static struct connection_info *get_loc_info(MYSQL_THD thd)
{
  return (struct connection_info *) THDVAR(thd, loc_info);
}


/*
  Not userd at the moment, but can be needed later.

static int ci_needs_setup(const struct connection_info *ci)
{
  return ci->header != 0;
}
*/


extern int execute_sql_command(const char *command,
                               char *hosts, char *names, char *filters);


static void free_cond(struct filter_cond *c)
{
  while (c)
  {
    struct filter_cond *tmp_c= c;

    free_cond(c->arg_list);

    while (c->tables.t_list)
    {
      struct table_filter_item *i= c->tables.t_list;
      c->tables.t_list= i->next;
      free(i);
    }

    c= c->next;
    free(tmp_c);
  }
}


static void free_filters_ex(struct filter_def **filter_list,
                            struct filter_def **default_filter,
                            struct user_def **user_list)
{
  while (*user_list)
  {
    struct user_def *tmp= *user_list;
    *user_list= tmp->next;
    free(tmp);
  }

  while (*filter_list)
  {
    struct filter_def *tmp= *filter_list;
    *filter_list= tmp->next;

    free_cond(tmp->rule);
    free(tmp);
  }
  *default_filter= 0;
}


static void free_filters()
{
  free_filters_ex(&filter_list, &default_filter, &user_list);
}


struct cond_func_dsc
{
  enum filter_cond_func func;
  const char *name;
  size_t name_len;
};

static struct cond_func_dsc cond_collection[]=
{
  {COND_OR, "or", 2},
  {COND_AND, "and", 3},
  {COND_OR_NOT, "not-or", 6},
  {COND_AND_NOT, "not-and", 7},
  {COND_EVENT, "events", 6},
  {COND_EVENT_QUERY, "query_event", 11},
  {COND_EVENT_TABLE, "table_event", 11},
  {COND_EVENT_CONNECT, "connect_event", 13},
  {COND_TABLES, "log_tables", 10},
  {COND_DATABASES, "log_databases", 13},
  {COND_TABLES_IGNORE, "ignore_tables", 13},
  {COND_LOGGING, "logging", 7},
  {COND_ERROR, NULL, 0}
};


static enum filter_cond_func find_cond_func(const char *cond_name,
                                            size_t name_len)
{
  int nf;
  for (nf= 0; cond_collection[nf].name; nf++)
  {
    if (name_len == cond_collection[nf].name_len &&
        strncasecmp(cond_collection[nf].name, cond_name, name_len) == 0)
      return cond_collection[nf].func;
  }
  return COND_ERROR;
}


struct option_value
{
  const char *name;
  size_t name_len;
  int value;
};


#define EVENT_OPT_ALL 0xFF
#define EVENT_CONNECT 1
#define EVENT_QUERY 2
#define EVENT_TABLE 4
#define EVENT_OPT_QUERY_DDL 8
#define EVENT_OPT_QUERY_DML 16
#define EVENT_OPT_QUERY_DCL 32
#define EVENT_OPT_QUERY_DML_WRITE 64
#define EVENT_OPT_QUERY_DML_READ 128
#define EVENT_QUERY_DO_PARSE (8 | 16 | 32 | 64 | 128)

static struct option_value event_options[]=
{
  {"ALL", 3, EVENT_OPT_ALL},
  {"CONNECT", 7, EVENT_CONNECT},
  {"QUERY", 5, EVENT_QUERY},
  {"TABLE", 5, EVENT_TABLE},
  {"QUERY_DDL", 9, EVENT_OPT_QUERY_DDL},
  {"QUERY_DML", 9, EVENT_OPT_QUERY_DML},
  {"QUERY_DCL", 9, EVENT_OPT_QUERY_DCL},
  {"QUERY_DML_NO_SELECT", 19, EVENT_OPT_QUERY_DML_WRITE},
  {"DML_READ", 8, EVENT_OPT_QUERY_DML_READ},
  {"DML_WRITE", 9, EVENT_OPT_QUERY_DML_WRITE},
  {0,0,0}
};



#define TABLE_OPT_ALL 0xFF
#define TABLE_OPT_READ 1
#define TABLE_OPT_WRITE 2
#define TABLE_OPT_CREATE 4
#define TABLE_OPT_DROP 8
#define TABLE_OPT_ALTER 16
#define TABLE_OPT_RENAME 32

static struct option_value table_event_options[]=
{
  {"ALL", 3, TABLE_OPT_ALL},
  {"READ", 4, TABLE_OPT_READ},
  {"WRITE", 5, TABLE_OPT_WRITE},
  {"CREATE", 6, TABLE_OPT_CREATE},
  {"DROP", 4, TABLE_OPT_DROP},
  {"ALTER", 5, TABLE_OPT_ALTER},
  {"RENAME", 5, TABLE_OPT_RENAME},
  {0,0,0}
};


#define QUERY_OPT_ALL 0xFF
#define QUERY_OPT_DDL 1
#define QUERY_OPT_DML 2
#define QUERY_OPT_DCL 4
#define QUERY_OPT_DML_WRITE 8
#define QUERY_OPT_DML_READ 16

static struct option_value query_event_options[]=
{
  {"ALL", 3, QUERY_OPT_ALL},
  {"DDL", 3, QUERY_OPT_DDL},
  {"DML", 3, QUERY_OPT_DML},
  {"DCL", 3, QUERY_OPT_DCL},
  {"DML_NO_SELECT", 14, QUERY_OPT_DML_WRITE},
  {"DML_WRITE", 9, QUERY_OPT_DML_WRITE},
  {"DML_READ", 8, QUERY_OPT_DML_READ},
  {0,0,0}
};


#define CONNECT_OPT_ALL 0xFF
#define CONNECT_OPT_CONNECT 1
#define CONNECT_OPT_FAILED_CONNECT 2
#define CONNECT_OPT_DISCONNECT 4
#define CONNECT_OPT_CHANGE_USER 8

static struct option_value connect_event_options[]=
{
  {"ALL", 3, CONNECT_OPT_ALL},
  {"CONNECT", 7, CONNECT_OPT_CONNECT},
  {"FAILED_CONNECT", 14, CONNECT_OPT_FAILED_CONNECT},
  {"DISCONNECT", 10, CONNECT_OPT_DISCONNECT},
  {"CHANGE_USER", 11, CONNECT_OPT_CHANGE_USER},
  {0,0,0}
};


static struct option_value logging_options[]=
{
  {"ON", 2, 1},
  {"OFF", 3, 0},
  {0,0,0}
};


static int parse_events(struct filter_cond *f,
                        struct option_value *options, enum json_types s_type,
                        const char *s, const char *s_end);
static int parse_tables(struct filter_cond *f, enum json_types s_type,
                        const char *s, const char *s_end);
static int parse_databases(struct filter_cond *f, enum json_types s_type,
                           const char *s, const char *s_end);

static struct filter_cond *make_cond(enum filter_cond_func cfunc,
                                     enum json_types def_type,
                                     const char *def, const char *def_end)
{
  unsigned int nkey;
  enum json_types vt;
  const char *v, *keyname, *keyname_end;
  int v_len;
  struct filter_cond *f= (struct filter_cond *) malloc(sizeof(struct filter_cond));
  struct filter_cond **arg_hook;


  bzero(f, sizeof(struct filter_cond));
  f->func= cfunc;
  switch (cfunc)
  {
  case COND_TRUE:
  case COND_FALSE:
    break;

  case COND_OR:
  case COND_AND:
  case COND_OR_NOT:
  case COND_AND_NOT:
  {
    int tables_cond= 0;

    if (def_type == JSV_TRUE)
    {
      f->func= COND_TRUE;
      goto ok;
    }
    if (def_type == JSV_FALSE)
    {
      f->func= COND_FALSE;
      goto ok;
    }
    if (def_type != JSV_OBJECT)
    {
      explain_error("Syntax error in filter definition.");
      goto error;
    }

    vt= json_get_object_nkey(def, def_end, 0,
                             &keyname, &keyname_end, &v, &v_len);

    if (vt == JSV_BAD_JSON)
      goto error;

    if (vt == JSV_NOTHING)
    {
      /* empty json object - means TRUE */
      f->func= COND_TRUE;
      goto ok;
    }

    nkey= 1;
    arg_hook= &f->arg_list;
    do
    {
      struct filter_cond *cf;
      cfunc= find_cond_func(keyname, keyname_end - keyname);

      switch (cfunc)
      {
      case COND_TABLES:
      case COND_DATABASES:
        tables_cond|= 1;
        break;
      case COND_TABLES_IGNORE:
        tables_cond|= 2;
        break;
      case COND_ERROR:
      {
        explain_error("Unknown filter function %.*s.",
                      (int) (keyname_end - keyname), keyname);
        goto error;
      }
      default:
        break;
      }
      cf= make_cond(cfunc, vt, v, v + v_len);
      if (!cf)
      {
        *arg_hook= 0;
        goto error;
      }
      *arg_hook= cf;
      arg_hook= &cf->next;

    } while ((vt= json_get_object_nkey(def, def_end, nkey++,
                    &keyname, &keyname_end, &v, &v_len)) != JSV_NOTHING);

    if (tables_cond  == 3) /* Both TABLES and IGNORE_TABLES were met. */
    {
      explain_error("Competing TABLES and IGNORE_TABLES are not allowed.");
      goto error;
    }
    *arg_hook= 0;
    break;
  }

  case COND_EVENT:
    if (parse_events(f, event_options, def_type, def, def_end))
      goto error;
    if (f->mask & EVENT_QUERY)
    {
      f->query_mask= 0;
      f->mask&= ~EVENT_QUERY_DO_PARSE;
    }
    else
      f->query_mask= f->mask & EVENT_QUERY_DO_PARSE;

    break;

  case COND_EVENT_TABLE:
    if (parse_events(f, table_event_options, def_type, def, def_end))
      goto error;
    break;

  case COND_EVENT_QUERY:
    if (parse_events(f, query_event_options, def_type, def, def_end))
      goto error;
    break;

  case COND_EVENT_CONNECT:
    if (parse_events(f, connect_event_options, def_type, def, def_end))
      goto error;
    break;
  case COND_TABLES:
  case COND_TABLES_IGNORE:
    if (parse_tables(f, def_type, def, def_end))
      goto error;
    break;

  case COND_DATABASES:
    if (parse_databases(f, def_type, def, def_end))
      goto error;
    break;
  case COND_LOGGING:
    if (parse_events(f, logging_options, def_type, def, def_end))
      goto error;
    break;
  default:
    goto error;
  };

ok:
  return f;

error:
  free(f);
  return 0;
}


static struct filter_def *find_filter(const char *name)
{
  struct filter_def *cf;
  for (cf= filter_list; cf; cf= cf->next)
  {
    if (strcmp(name, cf->name) == 0)
      return cf;
  }
  return 0;
}


static int load_filters()
{
  char hosts[1024];
  char names[1024];
  char filters[2048];
  const char *c_host, *c_name, *c_filter;
  int result;

  free_filters();
  err_len= 0;

  if ((result= execute_sql_command("select filtername, rule"
                          " from mysql.server_audit_filters",
                          names, filters, 0)))
  {
    if (result != 2)
      explain_error("Can't load data from mysql.server_audit_filters.");
    goto error;
  }

  c_name= names;
  c_filter= filters;
  while (*c_name)
  {
    struct filter_def *fd;
    struct filter_cond *fc;
    enum json_types vt;
    const char *v, *c_filter_end= c_filter + strlen(c_filter);
    int v_len;

    vt= json_type(c_filter, c_filter_end, &v, &v_len);
    if (!(fc= make_cond(COND_OR, vt, c_filter, c_filter_end)))
    {
      explain_error("Can't parse filter's '%s' definition %s.",
                    c_name, c_filter);
      result= 1;
      goto error;
    }

    fd= (struct filter_def *) malloc(sizeof(struct filter_def));
    strncpy(fd->name, c_name, sizeof(fd->name));
    fd->rule= fc;
    fd->next= filter_list;
    filter_list= fd;

    if (strcasecmp(c_name, "default") == 0)
      default_filter= fd;

    c_name+= strlen(c_name)+1;
    c_filter+= strlen(c_filter)+1;
  }


  if ((result= execute_sql_command("select host, user, filtername from"
                          " mysql.server_audit_users order by host, user",
                          hosts, names, filters)))
  {
    if (result != 2)
      explain_error("Can't load data from mysql.server_audit_users.");
    goto error;
  }

  c_host= hosts;
  c_name= names;
  c_filter= filters;

  while (*c_host)
  {
    size_t h_len= strlen(c_host);
    size_t u_len= strlen(c_name);
    size_t size;
    char *blk;
    struct user_def *u_d;

    size= ALIGN_SIZE(sizeof(struct user_def)) + h_len + u_len + 2;
    blk= (char *) malloc(size);
    u_d= (struct user_def *) blk;
    u_d->host= blk + ALIGN_SIZE(sizeof(struct user_def));
    u_d->name= u_d->host + h_len + 1;
    strcpy(u_d->host, c_host);
    strcpy(u_d->name, c_name);
    u_d->next= user_list;
    user_list= u_d;

    u_d->filter= find_filter(c_filter);
    if (!u_d->filter)
    {
      explain_error("Can't find filter '%s' for user %s@%s.",
                    c_filter, c_host, c_name);
      result= 1;
      goto error;
    }

    c_name+= u_len+1;
    c_host+= h_len+1;
    c_filter+= strlen(c_filter)+1;
  }

  return 0;

error:
  free_filters();
  return result;
}
 

static int log_config(MYSQL_THD thd, const char *var_name, const char *value);
static int log_uint_config(MYSQL_THD thd,
                           const char *var_name, unsigned long long value);
static void log_start_file()
{
  log_config(0, "file_path", file_path);
  log_uint_config(0, "rotate_size", file_rotate_size);
  log_uint_config(0, "file_rotations", rotations);
}


#if defined(_WIN32) && !defined(S_ISDIR)
#define S_ISDIR(x) ((x) & _S_IFDIR)
#endif /*__WIN__ && !S_ISDIR*/

static int start_logging(MYSQL_THD thd)
{
  last_error_buf[0]= 0;
  log_write_failures= 0;

  if (output_type == OUTPUT_FILE)
  {
    char alt_path_buffer[FN_REFLEN+1+DEFAULT_FILENAME_LEN];
    struct stat *f_stat= (struct stat *)alt_path_buffer;
    const char *alt_fname= file_path;

    while (*alt_fname == ' ')
      alt_fname++;

    if (*alt_fname == 0)
    {
      /* Empty string means the default file name. */
      alt_fname= default_file_name;
    }
    else
    {
      /* See if the directory exists with the name of file_path.    */
      /* Log file name should be [file_path]/server_audit.log then. */
      if (stat(file_path, (struct stat *)alt_path_buffer) == 0 &&
          S_ISDIR(f_stat->st_mode))
      {
        size_t p_len= strlen(file_path);
        memcpy(alt_path_buffer, file_path, p_len);
        if (alt_path_buffer[p_len-1] != FN_LIBCHAR)
        {
          alt_path_buffer[p_len]= FN_LIBCHAR;
          p_len++;
        }
        memcpy(alt_path_buffer+p_len, default_file_name, DEFAULT_FILENAME_LEN);
        alt_path_buffer[p_len+DEFAULT_FILENAME_LEN]= 0;
        alt_fname= alt_path_buffer;
      }
    }

    logfile= logger_open(alt_fname, file_rotate_size, rotations);

    if (logfile == NULL)
    {
      error_header();
      fprintf(stderr, "Could not create file '%s'.\n",
              alt_fname);
      logging= 0;
      my_snprintf(last_error_buf, sizeof(last_error_buf),
                  "Could not create file '%s'.", alt_fname);
      is_active= 0;
      ADD_ATOMIC(internal_stop_logging, 1);
      CLIENT_ERROR(1, "SERVER AUDIT plugin can't create file '%s'.",
          MYF(ME_WARNING), alt_fname);
      ADD_ATOMIC(internal_stop_logging, -1);
      return 1;
    }
    error_header();
    fprintf(stderr, "logging started to the file %s.\n", alt_fname);
    strncpy(current_log_buf, alt_fname, sizeof(current_log_buf)-1);
    current_log_buf[sizeof(current_log_buf)-1]= 0;
    log_start_file();
  }
  else if (output_type == OUTPUT_SYSLOG)
  {
    openlog(syslog_ident, LOG_NOWAIT, syslog_facility_codes[syslog_facility]);
    error_header();
    fprintf(stderr, "logging started to the syslog.\n");
    strncpy(current_log_buf, "[SYSLOG]", sizeof(current_log_buf)-1);
    compile_time_assert(sizeof current_log_buf > sizeof "[SYSLOG]");
    log_config(thd, "syslog_facility", syslog_facility_names[syslog_facility]);
    log_config(thd, "syslog_ident", syslog_ident);
    log_config(thd, "syslog_info", syslog_info);
    log_config(thd, "syslog_priority", syslog_priority_names[syslog_priority]);
  }
  is_active= 1;
  return 0;
}


static int stop_logging()
{
  last_error_buf[0]= 0;
  if (output_type == OUTPUT_FILE && logfile)
  {
    logger_close(logfile);
    logfile= NULL;
  }
  else if (output_type == OUTPUT_SYSLOG)
  {
    closelog();
  }
  error_header();
  fprintf(stderr, "logging was stopped.\n");
  is_active= 0;
  return 0;
}


static int user_fits(const char *def, const char *name)
{
  if (*def == 0) /* empty def, means fits all users */
    return 1;

  return strcmp(def, name) == 0;
}


static int host_fits(const char *def, const char *host, const char *ip)
{
  const char *whost;
  long wip, ip_mask;

  maria_update_hostname(&whost, &wip, &ip_mask, def);

  return maria_compare_hostname(whost, wip, ip_mask, host, ip);
}


static struct filter_def *find_user_filter(const struct connection_info *ci)
{
  const struct user_def *cu;

  for (cu= user_list; cu; cu= cu->next)
  {
    if (user_fits(cu->name, ci->user) &&
        host_fits(cu->host, ci->host, ci->ip))
      return cu->filter;
  }

  return default_filter;
}


static int get_next_word(const char *query, char *word)
{
  int len= 0;
  char c;
  while ((c= query[len]))
  {
    if (c >= 'a' && c <= 'z')
      word[len]= 'A' + (c-'a');
    else if (c >= 'A' && c <= 'Z')
      word[len]= c;
    else if (c == '_')
      word[len]= c;
    else
      break;

    if (len++ == MAX_KEYWORD)
      return 0;
  }
  word[len]= 0;
  return len;
}


static int parse_events(struct filter_cond *f,
                        struct option_value *options, enum json_types s_type,
                        const char *s, const char *s_end)
{
  char wd[20];
  int i;
  enum json_types vt;
  const char *v;
  int v_len;

  f->mask= 0;

  if (s_type == JSV_ARRAY)
  {
    unsigned long c_mask;
    struct filter_cond **nested_cond= &f->arg_list;
    i= 0;
    for (;;)
    {
      if ((vt= json_get_array_item(s, s_end, i, &v, &v_len)) ==
          JSV_BAD_JSON)
      {
        explain_error("Bad JSON syntax for events.");
        return 1;
      }
      if (vt == JSV_NOTHING)
        break;

      i++;
      if (vt == JSV_OBJECT)
      {
        /* nested condition. Treat as 'AND'. */
        struct filter_cond *cf= make_cond(COND_AND, vt, v, v + v_len);
        if (!cf)
          return 1;

        *nested_cond= cf;
        nested_cond= &cf->next;
        continue;
      }
      c_mask= f->mask;
      if (parse_events(f, options, vt, v, v+v_len))
        return 1;
      f->mask|= c_mask;
    }
    *nested_cond= NULL;
    return 0;
  }

  if (s_type != JSV_STRING)
  {
    explain_error("Events can only be an array or a string.");
    return 1;
  }

  while (s < s_end)
  {
    while (*s == ' ')
      s++;

    if (s >= s_end)
      break;

    s+= get_next_word(s, wd);

    for (i= 0; options[i].name; i++)
    {
      if (strcasecmp(options[i].name, wd) == 0)
      {
        f->mask|= options[i].value;
        break;
      }
    }

    if (options[i].name == 0) /* Nothing found. */
    {
      explain_error("Unknown option %s.", wd);
      return 1;
    }

    while (*s == ' ' || *s == ',')
      s++;
  }

  return 0;
}


static int get_table_id(const char **s_begin, const char *s_end,
                        char *id, size_t* id_len, size_t id_len_limit)
{
  const char *s= *s_begin;

  *id_len= 0;
  if (*s == '*')
    s++;
  else if (*s == '`')
  {
    s++;
    while (s < s_end && *s != '`')
    {
      if (s >= s_end)
      {
        explain_error("Invalid table filter - no closing '`'.");
        return 1;
      }
      if (*id_len >= id_len_limit)
      {
        explain_error("Table name is too long.");
        return 1;
      }
      id[(*id_len)++]= *s++;
    }
    s++;
  }
  else
  {
    while (s < s_end &&
        ((*s >= '0' && *s <= '9') ||
         (*s >= 'a' && *s <= 'z') ||
         (*s >= 'A' && *s <= 'Z') ||
         *s == '_' || *s == '$'))
    {
      if (*id_len >= id_len_limit)
      {
        explain_error("Table name is too long.");
        return 1;
      }
      id[(*id_len)++]= *s++;
    }
  }

  *s_begin= s;
  return 0;
}


static struct table_filter_item *add_table_filter_item(
    struct table_filter *tf)
{
  struct table_filter_item *i=
    (struct table_filter_item *) malloc(sizeof(struct table_filter_item));
  if (!i)
    return NULL; 

  *tf->t_hook= i;
  tf->t_hook= &i->next;

  return i;
}


static int parse_tables_rec(struct filter_cond *f,
                            enum json_types s_type,
                            const char *s, const char *s_end, int read_table_part)
{
  int i;
  enum json_types vt;
  const char *v;
  int v_len;
  struct table_filter_item *item;

  if (s_type == JSV_ARRAY)
  {
    struct filter_cond **nested_cond= &f->arg_list;
    i= 0;
    for (;;)
    {
      if ((vt= json_get_array_item(s, s_end, i, &v, &v_len)) ==
          JSV_BAD_JSON)
      {
        explain_error("Bad JSON syntax for events.");
        return 1;
      }
      if (vt == JSV_NOTHING)
        break;

      i++;
      if (vt == JSV_OBJECT)
      {
        /* nested condition. Treat as 'AND'. */
        struct filter_cond *cf= make_cond(COND_OR, vt, v, v + v_len);
        if (!cf)
          return 1;

        *nested_cond= cf;
        nested_cond= &cf->next;
        continue;
      }
      if (parse_tables_rec(f, vt, v, v+v_len, read_table_part))
        return 1;
    }
    *nested_cond= NULL;
    return 0;
  }

  if (s_type != JSV_STRING)
  {
    explain_error("Table filter can only be an array or a string.");
    return 1;
  }

  item= add_table_filter_item(&f->tables);
  if (!item)
    return 1;

  if (get_table_id(&s, s_end, item->db, &item->db_len, sizeof(item->db)))
    return 1;
  
  if (read_table_part)
  {
    if (s >= s_end || *s != '.')
    {
      explain_error("Table filter has the form of database_name.table_name.");
      return 1;
    }

    s++;
    if (get_table_id(&s, s_end, item->name, &item->name_len, sizeof(item->name)))
      return 1;
  }
  else
    item->name_len= 0;

  return 0;
}


static int parse_tables(struct filter_cond *f, enum json_types s_type,
                        const char *s, const char *s_end)
{
  f->tables.n_items= 0;
  f->tables.t_hook= &f->tables.t_list;
  if (parse_tables_rec(f, s_type, s, s_end, TRUE))
    return 1;

  *f->tables.t_hook= NULL;
  return 0;
}


static int parse_databases(struct filter_cond *f, enum json_types s_type,
                           const char *s, const char *s_end)
{
  f->tables.n_items= 0;
  f->tables.t_hook= &f->tables.t_list;
  if (parse_tables_rec(f, s_type, s, s_end, FALSE))
    return 1;

  *f->tables.t_hook= NULL;
  return 0;
}
static int filter_query_type(const char *query, struct sa_keyword *kwd);

static int filter_table_match(struct filter_cond *c,
    MYSQL_CONST_LEX_STRING *db, MYSQL_CONST_LEX_STRING *name)
{
  struct table_filter_item *i= c->tables.t_list;

  for (; i; i= i->next)
  {
    if (i->db_len > 0)
    {
      if (i->db_len != db->length ||
          strncmp(i->db, db->str, db->length) != 0)
        continue;
    }

    if (i->name_len == 0 ||
        (i->name_len == name->length &&
         strncmp(i->name, name->str, name->length) == 0))
      return 1;
  }
  return 0;
}


static int check_query_tables(struct connection_info *ci, struct filter_cond *c)
{
  switch (ci->event_type)
  {
  case EVENT_QUERY:
  {
#define tables_on_stack 20 /* Should be enough for most queries. */
    MYSQL_CONST_LEX_STRING names[tables_on_stack];
    size_t n_tables, c_table;

    n_tables= thd_query_table_list(ci->thd, names, tables_on_stack);
    for (c_table=0; c_table+1 < n_tables; c_table+= 2)
    {
      if (filter_table_match(c, names+c_table, names+c_table+1))
        return 1;
    }
    return 0;
  }

  case EVENT_TABLE:
    return filter_table_match(c, &ci->db, &ci->table);

  default:
    break;
  }

  return 1;
}


static int any_fits(struct connection_info *ci, struct filter_cond *f);


static int int_do_filter(struct connection_info *ci, struct filter_cond *c)
{
  int mask= 1;
  struct filter_cond *cf;
  const char *query;

  switch (c->func)
  {
  case COND_TRUE:
    return 1;
  case COND_FALSE:
    return 0;
  case COND_OR_NOT:
  case COND_OR:
    mask= c->func == COND_OR;
    if (!c->arg_list)
      return mask; /* No arguments means TRUE. */

    for (cf= c->arg_list; cf; cf= cf->next)
    {
      if (int_do_filter(ci, cf))
        return mask;
    }
    return !mask;

  case COND_AND:
  case COND_AND_NOT:
    mask= c->func == COND_AND_NOT;
    for (cf= c->arg_list; cf; cf= cf->next)
    {
      if (!int_do_filter(ci, cf))
        return mask;
    }
    return !mask;

  case COND_EVENT:
    if (!(ci->event_type & c->mask))
      return 0;

    if (!any_fits(ci, c->arg_list))
      return 0;

    query= ci->query;

    if (query && ci->event_type == EVENT_QUERY && c->query_mask)
    {
      const char *orig_query= query;

      if (filter_query_type(query, keywords_to_skip))
      {
        char fword[MAX_KEYWORD + 1];
        int len;
        do
        {
          len= get_next_word(query, fword);
          query+= len ? len : 1;
          if (len == 3 && strncmp(fword, "FOR", 3) == 0)
            break;
        } while (*query);

        if (*query == 0)
          return 0;
      }

      if (c->query_mask & EVENT_OPT_QUERY_DDL)
      {
        if (!filter_query_type(query, not_ddl_keywords) &&
            filter_query_type(query, ddl_keywords))
          goto do_log_query;
      }
      if (c->query_mask & EVENT_OPT_QUERY_DML)
      {
        if (filter_query_type(query, dml_keywords))
          goto do_log_query;
      }
      if (c->query_mask & EVENT_OPT_QUERY_DML_WRITE)
      {
        if (filter_query_type(query, dml_no_select_keywords))
          goto do_log_query;
      }
      if (c->query_mask & EVENT_OPT_QUERY_DML_READ)
      {
        if (filter_query_type(query, dml_keywords) &&
            !filter_query_type(query, dml_no_select_keywords))
          goto do_log_query;
      }
      if (c->query_mask & EVENT_OPT_QUERY_DCL)
      {
        if (filter_query_type(query, dcl_keywords))
          goto do_log_query;
      }

      return 0;
do_log_query:
      query= orig_query;
    }
    break;

  case COND_EVENT_QUERY:
    if (ci->event_type != EVENT_QUERY)
      return 0;

    if (!any_fits(ci, c->arg_list))
      return 0;

    if (c->mask == QUERY_OPT_ALL)
      return 1;

    query= ci->query;
    {
      const char *orig_query= query;

      if (filter_query_type(query, keywords_to_skip))
      {
        char fword[MAX_KEYWORD + 1];
        int len;
        do
        {
          len= get_next_word(query, fword);
          query+= len ? len : 1;
          if (len == 3 && strncmp(fword, "FOR", 3) == 0)
            break;
        } while (*query);

        if (*query == 0)
          return 0;
      }

      if (c->mask & QUERY_OPT_DDL)
      {
        if (!filter_query_type(query, not_ddl_keywords) &&
            filter_query_type(query, ddl_keywords))
          goto do_log_query_2;
      }
      if (c->mask & QUERY_OPT_DML)
      {
        if (filter_query_type(query, dml_keywords))
          goto do_log_query_2;
      }
      if (c->mask & QUERY_OPT_DML_WRITE)
      {
        if (filter_query_type(query, dml_no_select_keywords))
          goto do_log_query_2;
      }
      if (c->mask & QUERY_OPT_DML_READ)
      {
        if (filter_query_type(query, dml_keywords) &&
            !filter_query_type(query, dml_no_select_keywords))
          goto do_log_query_2;
      }
      if (c->mask & QUERY_OPT_DCL)
      {
        if (filter_query_type(query, dcl_keywords))
          goto do_log_query_2;
      }

      return 0;
do_log_query_2:
      query= orig_query;
    }
    break;

  case COND_EVENT_TABLE:
    if (ci->event_type != EVENT_TABLE)
      return 0;

    if (!any_fits(ci, c->arg_list))
      return 0;

    switch (ci->event_subclass)
    {
    case MYSQL_AUDIT_TABLE_LOCK:
      if (ci->read_only)
        return c->mask & TABLE_OPT_READ;
      return c->mask & TABLE_OPT_WRITE;

    case MYSQL_AUDIT_TABLE_CREATE:
      return c->mask & TABLE_OPT_CREATE;

    case MYSQL_AUDIT_TABLE_DROP:
      return c->mask & TABLE_OPT_DROP;

    case MYSQL_AUDIT_TABLE_RENAME:
      return c->mask & TABLE_OPT_RENAME;

    case MYSQL_AUDIT_TABLE_ALTER:
      return c->mask & TABLE_OPT_ALTER;
    default:
      return 1;
    }
    break;

  case COND_EVENT_CONNECT:
    if (ci->event_type != EVENT_CONNECT)
      return 0;

    switch (ci->event_subclass)
    {
    case MYSQL_AUDIT_CONNECTION_CONNECT:
      return c->mask & (ci->error_code ? CONNECT_OPT_FAILED_CONNECT :
                                         CONNECT_OPT_CONNECT);
    case MYSQL_AUDIT_CONNECTION_DISCONNECT:
      return c->mask & CONNECT_OPT_DISCONNECT;
    case MYSQL_AUDIT_CONNECTION_CHANGE_USER:
      return c->mask & CONNECT_OPT_CHANGE_USER;
    default:;
    };
    break;

  case COND_LOGGING:
    return c->mask;

  case COND_TABLES:
  case COND_DATABASES:
    if (!any_fits(ci, c->arg_list))
      return 0;
    return check_query_tables(ci, c);

  case COND_TABLES_IGNORE:
    if (!any_fits(ci, c->arg_list))
      return 0;
    if (ci->event_type == EVENT_QUERY || ci->event_type == EVENT_TABLE)
      return !check_query_tables(ci, c);
    return 1;
  default:
    break;
  };

  return 1;
}


static int any_fits(struct connection_info *ci, struct filter_cond *f)
{
  if (!f)
    return 1;

  for (; f; f= f->next)
  {
    if (int_do_filter(ci, f))
      return 1;
  }
  return 0;
}


static int do_filter(struct connection_info *ci)
{
  struct filter_def *f= find_user_filter(ci);
  if (!f)
    return 1;

  return int_do_filter(ci, f->rule);
}


#define SAFE_STRLEN(s) (s ? strlen(s) : 0)
static char empty_str[1]= { 0 };


static int is_space(char c)
{
  return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}


#define SKIP_SPACES(str) \
do { \
  while (is_space(*str)) \
    ++str; \
} while(0)


#define ESC_MAP_SIZE 0x60
static const char esc_map[ESC_MAP_SIZE]=
{
  0, 0, 0, 0, 0, 0, 0, 0, 'b', 't', 'n', 0, 'f', 'r', 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, '\'', 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\\', 0, 0, 0
};

static char escaped_char(char c)
{
  return ((unsigned char ) c) >= ESC_MAP_SIZE ? 0 : esc_map[(unsigned char) c];
}


/*
  Write to the log

  @param wrlock_taken  If set, take a read lock (or write lock on rotate).
                       If not set, the caller has a already taken a write lock
*/

static int write_log(const char *message, size_t len, int wrlock_taken)
{
  int result= 0;

  if (output_type == OUTPUT_FILE)
  {
    if (logfile)
    {
      my_bool allow_rotate= wrlock_taken; /* Allow rotate if caller write lock */
      if (!wrlock_taken && logger_time_to_rotate(logfile))
      {
        /* We have to rotate the log, change above read lock to write lock */
        mysql_prlock_unlock(&lock_operations);
        mysql_prlock_wrlock(&lock_operations);
        allow_rotate= 1;
      }
      if (!(is_active= (logger_write_r(logfile, allow_rotate, message, len) ==
                        (int) len)))
      {
        ++log_write_failures;
        result= 1;
      }
    }
  }
  else if (output_type == OUTPUT_SYSLOG)
  {
    syslog(syslog_facility_codes[syslog_facility] |
           syslog_priority_codes[syslog_priority],
           "%s %.*s", syslog_info, (int)len, message);
  }
  return result;
}


static size_t log_header(char *message, size_t message_len,
                      time_t *ts,
                      const char *serverhost, unsigned int serverhost_len,
                      const char *username, unsigned int username_len,
                      const char *host, unsigned int host_len,
                      const char *userip, unsigned int userip_len,
                      unsigned int connection_id, long long query_id,
                      const char *operation)
{
  struct tm tm_time;

  if (host_len == 0 && userip_len != 0)
  {
    host_len= userip_len;
    host= userip;
  }

  if (output_type == OUTPUT_SYSLOG)
    return my_snprintf(message, message_len,
        "%.*s,%.*s,%.*s,%d,%lld,%s",
        serverhost_len, serverhost,
        username_len, username,
        host_len, host,
        connection_id, query_id, operation);

  (void) localtime_r(ts, &tm_time);
  return my_snprintf(message, message_len,
      "%04d%02d%02d %02d:%02d:%02d,%.*s,%.*s,%.*s,%d,%lld,%s",
      tm_time.tm_year+1900, tm_time.tm_mon+1, tm_time.tm_mday,
      tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
      serverhost_len, serverhost,
      username_len, username,
      host_len, host,
      connection_id, query_id, operation);
}


static int log_config(MYSQL_THD thd, const char *var_name, const char *value)
{
  time_t ctime;
  size_t csize;
  char message[1024];
  const char *user= 0;
  const char *host= 0;
  const char *ip= 0;
  const char *db= 0;
  uint db_len= 0;
  int status= 0;
  unsigned long id= 0;

  if (!logging)
    return 0;

  (void) time(&ctime);

  if (thd)
  {
    LEX_CSTRING *dbs= thd_current_db(thd);
    if (dbs)
    {
      db= dbs->str;
      db_len= (uint) dbs->length;
    }
    user= thd_user_name(thd);
    host= thd_client_host(thd);
    ip= thd_client_ip(thd);
    status= thd_current_status(thd);
    id= thd_get_thread_id(thd);
  }

  if (!user) user= "";
  if (!host) host= "";
  if (!ip) ip= "";
  if (!db) db= "";

  csize= log_header(message, sizeof(message)-1, &ctime,
                    servhost, servhost_len, user, (uint) strlen(user),
                    host, (uint) strlen(host), ip, (uint) strlen(ip), id, 0,
                    "AUDIT_CONFIG");

  csize+= my_snprintf(message+csize, sizeof(message) - 1 - csize,
    ",%.*s,%s=%s,%d\n", (int) db_len, db, var_name, value, status);

  return write_log(message, csize, 1);
}


static int log_uint_config(MYSQL_THD thd,
                           const char *var_name, unsigned long long value)
{
  char ui_buf[30];
  my_snprintf(ui_buf, sizeof(ui_buf), "%llu", value);
  return log_config(thd, var_name, ui_buf);
}


static int log_proxy(MYSQL_THD thd,
                     const struct connection_info *cn,
                     const struct mysql_event_connection *event)
                   
{
  time_t ctime;
  size_t csize;
  char message[1024];

  size_t proxy_user_len;
  const char *proxy_user= thd_priv_user(thd, &proxy_user_len);
  size_t proxy_host_len;
  const char *proxy_host= thd_priv_host(thd, &proxy_host_len);

  (void) time(&ctime);
  csize= log_header(message, sizeof(message)-1, &ctime,
                    servhost, servhost_len,
                    cn->user, cn->user_len,
                    cn->host, cn->host_len,
                    cn->ip, cn->ip_len,
                    event->thread_id, 0, "PROXY_CONNECT");
  csize+= my_snprintf(message+csize, sizeof(message) - 1 - csize,
    ",%.*s,`%.*s`@`%.*s`,%d", cn->db.length, cn->db.str,
                     proxy_user_len, proxy_user,
                     proxy_host_len, proxy_host,
                     event->status);
  message[csize]= '\n';
  return write_log(message, csize + 1, 0);
}


static int log_connection(const struct connection_info *cn,
                          const struct mysql_event_connection *event,
                          const char *type,
                          const char *qwe_string)
{
  time_t ctime;
  size_t csize;
  char message[1024];

  (void) time(&ctime);
  csize= log_header(message, sizeof(message)-1, &ctime,
                    servhost, servhost_len,
                    cn->user, cn->user_len,
                    cn->host, cn->host_len,
                    cn->ip, cn->ip_len,
                    event->thread_id, 0, type);
  csize+= my_snprintf(message+csize, sizeof(message) - 1 - csize,
    ",%.*s,%s,%d", cn->db.length, cn->db.str, qwe_string, event->status);
  message[csize]= '\n';
  return write_log(message, csize + 1, 0);
}


static int log_connection_event(const struct mysql_event_connection *event,
                                const char *type)
{
  time_t ctime;
  size_t csize;
  char message[1024];

  (void) time(&ctime);
  csize= log_header(message, sizeof(message)-1, &ctime,
                    servhost, servhost_len,
                    event->user, event->user_length,
                    event->host, event->host_length,
                    event->ip, event->ip_length,
                    event->thread_id, 0, type);
  csize+= my_snprintf(message+csize, sizeof(message) - 1 - csize,
    ",%.*s,,%d", event->database.length, event->database.str, event->status);
  message[csize]= '\n';
  return write_log(message, csize + 1, 0);
}


static size_t escape_string(const char *str, unsigned int len,
                          char *result, size_t result_len)
{
  const char *res_start= result;
  const char *res_end= result + result_len - 2;
  while (len)
  {
    char esc_c;

    if (result >= res_end)
      break;
    if ((esc_c= escaped_char(*str)))
    {
      if (result+1 >= res_end)
        break;
      *(result++)= '\\';
      *(result++)= esc_c;
    }
    else if (is_space(*str))
      *(result++)= ' ';
    else
      *(result++)= *str;
    str++;
    len--;
  }
  *result= 0;
  return result - res_start;
}


static size_t escape_string_hide_passwords(const char *str, unsigned int len,
    char *result, size_t result_len,
    const char *word1, size_t word1_len,
    const char *word2, size_t word2_len,
    int next_text_string)
{
  const char *res_start= result;
  const char *res_end= result + result_len - 2;
  size_t d_len;
  char b_char;

  while (len)
  {
    if (len > word1_len + 1 && strncasecmp(str, word1, word1_len) == 0)
    {
      const char *next_s= str + word1_len;
      size_t c;

      if (next_text_string)
      {
        while (*next_s && *next_s != '\'' && *next_s != '"')
          ++next_s;
      }
      else
      {
        if (word2)
        {
          SKIP_SPACES(next_s);
          if (len < (next_s - str) + word2_len + 1 ||
              strncasecmp(next_s, word2, word2_len) != 0)
            goto no_password;
          next_s+= word2_len;
        }

        while (*next_s && *next_s != '\'' && *next_s != '"')
          ++next_s;
      }

      d_len= next_s - str;
      if (result + d_len + 5 > res_end)
        break;

      for (c=0; c<d_len; c++)
        result[c]= is_space(str[c]) ? ' ' : str[c];

      if (*next_s)
      {
        memmove(result + d_len, "*****", 5);
        result+= d_len + 5;
        b_char= *(next_s++);
      }
      else
        result+= d_len;

      while (*next_s)
      {
        if (*next_s == b_char)
        {
          ++next_s;
          break;
        }
        if (*next_s == '\\')
        {
          if (next_s[1])
            next_s++;
        }
        next_s++;
      }

      c= next_s - str;
      len= (c > len) ? 0:(uint) (len - c);

      str= next_s;
      continue;
    }
no_password:
    if (result >= res_end)
      break;
    if ((b_char= escaped_char(*str)))
    {
      if (result+1 >= res_end)
        break;
      *(result++)= '\\';
      *(result++)= b_char;
    }
    else if (is_space(*str))
      *(result++)= ' ';
    else
      *(result++)= *str;
    str++;
    len--;
  }
  *result= 0;
  return result - res_start;
}


static int filter_query_type(const char *query, struct sa_keyword *kwd)
{
  int qwe_in_list;
  char fword[MAX_KEYWORD + 1], nword[MAX_KEYWORD + 1];
  int len, nlen= 0;
  const struct sa_keyword *l_keywords;

  while (*query && (is_space(*query) || *query == '(' || *query == '/'))
  {
    /* comment handling */
    if (*query == '/' && query[1] == '*')
    {
      if (query[2] == '!')
      {
        query+= 3;
        while (*query >= '0' && *query <= '9')
          query++;
        continue;
      }
      query+= 2;
      while (*query)
      {
        if (*query=='*' && query[1] == '/')
        {
          query+= 2;
          break;
        }
        query++;
      }
      continue;
    }
    query++;
  }

  qwe_in_list= 0;
  if (!(len= get_next_word(query, fword)))
    goto not_in_list;
  query+= len+1;

  l_keywords= kwd;
  while (l_keywords->length)
  {
    if (l_keywords->length == len && strncmp(l_keywords->wd, fword, len) == 0)
    {
      if (l_keywords->next)
      {
        if (nlen == 0)
        {
          while (*query && is_space(*query))
            query++;
          nlen= get_next_word(query, nword);
        }
        if (l_keywords->next->length != nlen ||
            strncmp(l_keywords->next->wd, nword, nlen) != 0)
          goto do_loop;
      }

      qwe_in_list= l_keywords->type;
      break;
    };
do_loop:
    l_keywords++;
  }

not_in_list:
  return qwe_in_list;
}


static int log_statement(struct connection_info *cn, const char *type)
{
  size_t csize;
  char message_loc[2048];
  char *message= message_loc;
  size_t message_size= sizeof(message_loc);
  char *uh_buffer;
  size_t uh_buffer_size;
  int result;
  unsigned int query_len;
  char *big_buffer= NULL;

  csize= log_header(message, message_size-1, &cn->query_time,
                    servhost, servhost_len,
                    cn->user, cn->user_len,
                    cn->host, cn->host_len,
                    cn->ip, cn->ip_len,
                    cn->thread_id, cn->query_id, type);

  csize+= my_snprintf(message+csize, message_size - 1 - csize,
      ",%.*s,\'", (int) cn->db.length, cn->db.str);

  query_len= cn->query_len;
  if (query_log_limit > 0 && query_len > query_log_limit)
    query_len= query_log_limit;

  if (query_len > (message_size - csize)/2)
  {
    size_t big_buffer_alloced= (query_len * 2 + csize + 4095) & ~4095L;
    if (!(big_buffer= malloc(big_buffer_alloced)))
      return 0;

    memcpy(big_buffer, message, csize);
    message= big_buffer;
    message_size= big_buffer_alloced;
  }

  uh_buffer= message + csize;
  uh_buffer_size= message_size - csize;
  if (query_log_limit > 0 && uh_buffer_size > query_log_limit+2)
    uh_buffer_size= query_log_limit+2;

  switch (filter_query_type(cn->query, passwd_keywords))
  {
    case SQLCOM_GRANT:
    case SQLCOM_CREATE_USER:
    case SQLCOM_ALTER_USER:
      csize+= escape_string_hide_passwords(cn->query, query_len,
                                           uh_buffer, uh_buffer_size,
                                           "IDENTIFIED", 10, "BY", 2, 0);
      break;
    case SQLCOM_CHANGE_MASTER:
      csize+= escape_string_hide_passwords(cn->query, query_len,
                                           uh_buffer, uh_buffer_size,
                                           "MASTER_PASSWORD", 15, "=", 1, 0);
      break;
    case SQLCOM_CREATE_SERVER:
    case SQLCOM_ALTER_SERVER:
      csize+= escape_string_hide_passwords(cn->query, query_len,
                                           uh_buffer, uh_buffer_size,
                                           "PASSWORD", 8, NULL, 0, 0);
      break;
    case SQLCOM_SET_OPTION:
      csize+= escape_string_hide_passwords(cn->query, query_len,
                                           uh_buffer, uh_buffer_size,
                                           "=", 1, NULL, 0, 1);
      break;
    default:
      csize+= escape_string(cn->query, query_len,
                               uh_buffer, uh_buffer_size); 
      break;
  }
  csize+= my_snprintf(message+csize, message_size - 1 - csize,
                      "\',%d", cn->error_code);
  message[csize]= '\n';
  result= write_log(message, csize + 1, 0);
  if (big_buffer)
    free(big_buffer);

  return result;
}


static int log_table(struct connection_info *cn, const char *type)
{
  size_t csize;
  char message[1024];
  time_t ctime;

  (void) time(&ctime);
  csize= log_header(message, sizeof(message)-1, &ctime,
                    servhost, servhost_len,
                    cn->user, cn->user_len,
                    cn->host, cn->host_len,
                    cn->ip, cn->ip_len,
                    cn->thread_id, cn->query_id, type);
  csize+= my_snprintf(message+csize, sizeof(message) - 1 - csize,
            ",%.*s,%.*s,\n", cn->db.length,  cn->db.str,
                             cn->table.length, cn->table.str);
  return write_log(message, csize, 0);
}


static int log_rename(struct connection_info *cn,
                      const struct mysql_event_table *event)
{
  size_t csize;
  char message[1024];
  time_t ctime;

  (void) time(&ctime);
  csize= log_header(message, sizeof(message)-1, &ctime,
                    servhost, servhost_len,
                    cn->user, cn->user_len, cn->host, cn->host_len, cn->ip, cn->ip_len,
                    event->thread_id, cn->query_id, "RENAME");
  csize+= my_snprintf(message+csize, sizeof(message) - 1 - csize,
            ",%.*s,%.*s|%.*s.%.*s,",event->database.length, event->database.str,
                         event->table.length, event->table.str,
                         event->new_database.length, event->new_database.str,
                         event->new_table.length, event->new_table.str);
  message[csize]= '\n';
  return write_log(message, csize + 1, 0);
}

static int event_query_command(MYSQL_THD thd, int error_code)
{
  enum enum_server_command cmd= thd_current_command(thd);

  return cmd == COM_QUERY ||
         cmd == COM_STMT_EXECUTE ||
         (error_code != 0 && cmd == COM_STMT_PREPARE);
}


void auditing(MYSQL_THD thd, unsigned int event_class, const void *ev)
{
  struct connection_info *cn;

  /* That one is important as this function can be called with      */
  /* &lock_operations locked when the server logs an error reported */
  /* by this plugin.                                                */
  if (!thd || internal_stop_logging)
    return;

  if (!logging)
    return;

  if (!(cn= get_loc_info(thd)))
    return;

  mysql_prlock_rdlock(&lock_operations);

  if (event_class == MYSQL_AUDIT_GENERAL_CLASS)
  {
    const struct mysql_event_general *event = (const struct mysql_event_general *) ev;

    /*
      Only one subclass is logged.
    */
    if (event->event_subclass == MYSQL_AUDIT_GENERAL_STATUS &&
        event_query_command(thd, event->general_error_code))
    {
      cn->thd= thd;
      cn->db= event->database;
      setc(thd_user_name(thd), &cn->user, &cn->user_len);
      setc(thd_client_host(thd), &cn->host, &cn->host_len);
      setc(thd_client_ip(thd), &cn->ip, &cn->ip_len);
      cn->query= event->general_query;
      cn->query_len= event->general_query_length;
      if (cn->query == 0)
      {
#ifdef DBUG_OFF
        /* cn->query can only be 0 in extreme errors like OUT-OF-MEMORY */
        /* so don't fix it for the DEBUG version. */
        cn->query= "";
#endif /*DBUG_OFF*/
        cn->query_len= 0;
      }
      cn->query_time= event->general_time;
      cn->event_type= EVENT_QUERY;
      cn->event_subclass= -1;
      cn->error_code= event->general_error_code;
      cn->thread_id= event->general_thread_id;
      cn->query_id= thd_query_id(thd);

      if (do_filter(cn))
      {
        log_statement(cn, "QUERY");
      }
    }
  }
  else if (event_class == MYSQL_AUDIT_TABLE_CLASS)
  {
    const struct mysql_event_table *event =
      (const struct mysql_event_table *) ev;

    cn->thd= thd;
    cn->db= event->database;
    cn->table= event->table;
    cn->new_db= event->new_database;
    cn->new_table= event->new_table;
    setc(event->user, &cn->user, &cn->user_len);
    setc(event->host, &cn->host, &cn->host_len);
    setc(event->ip, &cn->ip, &cn->ip_len);
    cn->query= 0;
    cn->query_time= 0;
    cn->event_type= EVENT_TABLE;
    cn->event_subclass= (int) event->event_subclass;
    cn->error_code= 0;
    cn->read_only= event->read_only;
    cn->query_id= event->query_id;
    cn->thread_id= event->thread_id;

    if (do_filter(cn))
    {
      switch (event->event_subclass)
      {
        case MYSQL_AUDIT_TABLE_LOCK:
          log_table(cn, event->read_only ? "READ" : "WRITE");
          break;
        case MYSQL_AUDIT_TABLE_CREATE:
          log_table(cn, "CREATE");
          break;
        case MYSQL_AUDIT_TABLE_DROP:
          log_table(cn, "DROP");
          break;
        case MYSQL_AUDIT_TABLE_RENAME:
          log_rename(cn, event);
          break;
        case MYSQL_AUDIT_TABLE_ALTER:
          log_table(cn, "ALTER");
          break;
        default:
          break;
      }
    }
  }
  else if (event_class == MYSQL_AUDIT_CONNECTION_CLASS && cn)
  {
    const struct mysql_event_connection *event =
      (const struct mysql_event_connection *) ev;

    cn->thd= thd;
    cn->thread_id= event->thread_id;
    cn->db= event->database;
    cn->user= event->user;
    cn->user_len= event->user_length;
    if (!cn->user)
    {
      cn->user= "";
      cn->user_len= 0;
    }
    cn->host= event->host;
    cn->host_len= event->host_length;
    if (!cn->host)
    {
      cn->host= "";
      cn->host_len= 0;
    }
    cn->ip= event->ip;
    cn->ip_len= event->ip_length;
    if (!cn->ip)
    {
      cn->ip= "";
      cn->ip_len= 0;
    }
    cn->event_type= EVENT_CONNECT;
    cn->event_subclass= (int) event->event_subclass;
    cn->error_code= event->status;

    cn->read_only= 0;
    cn->query_id= 0;
    cn->query= 0;
    cn->query_time= 0;

    if (do_filter(cn))
    {
      switch (event->event_subclass)
      {
      case MYSQL_AUDIT_CONNECTION_CONNECT:
        log_connection(cn, event, event->status ? "FAILED_CONNECT": "CONNECT", "");
        if (event->status == 0 && event->proxy_user && event->proxy_user[0])
          log_proxy(thd, cn, event);
        break;
      case MYSQL_AUDIT_CONNECTION_DISCONNECT:
          log_connection_event(event, "DISCONNECT");
        break;
      case MYSQL_AUDIT_CONNECTION_CHANGE_USER:
      {
        const char *user= thd_user_name(thd); /* that returns the new user */
        char userbuf[100];

        if (!user)
          user= "";

        my_snprintf(userbuf, sizeof(userbuf), "`%s`", user);

        log_connection(cn, event, "CHANGE_USER", userbuf);
        if (event->proxy_user && event->proxy_user[0])
          log_proxy(thd, cn, event);
        break;
      }
      default:;
      }
    }

    if (event->event_subclass == MYSQL_AUDIT_CONNECTION_CHANGE_USER &&
        event->status == 0)
    {
      cn->db= *thd_current_db(thd);
      setc(thd_user_name(thd), &cn->user, &cn->user_len);
      setc(thd_client_host(thd), &cn->host, &cn->host_len);
      setc(thd_client_ip(thd), &cn->ip, &cn->ip_len);

      if (do_filter(cn))
        log_connection(cn, event, "CHANGE_USER_DONE", "");

    }
  }

  mysql_prlock_unlock(&lock_operations);
}


/*
   As it's just too difficult to #include "sql_class.h",
   let's just copy the necessary part of the system_variables
   structure here.
*/
typedef struct loc_system_variables
{
  ulong dynamic_variables_version;
  char* dynamic_variables_ptr;
  uint dynamic_variables_head;    /* largest valid variable offset */
  uint dynamic_variables_size;    /* how many bytes are in use */
  
  ulonglong max_heap_table_size;
  ulonglong tmp_table_size;
  ulonglong long_query_time;
  ulonglong optimizer_switch;
  ulonglong sql_mode; ///< which non-standard SQL behaviour should be enabled
  ulonglong option_bits; ///< OPTION_xxx constants, e.g. OPTION_PROFILING
  ulonglong join_buff_space_limit;
  ulonglong log_slow_filter; 
  ulonglong log_slow_verbosity; 
  ulonglong bulk_insert_buff_size;
  ulonglong join_buff_size;
  ulonglong sortbuff_size;
  ulonglong group_concat_max_len;
  ha_rows select_limit;
  ha_rows max_join_size;
  ha_rows expensive_subquery_limit;
  ulong auto_increment_increment, auto_increment_offset;
  ulong lock_wait_timeout;
  ulong join_cache_level;
  ulong max_allowed_packet;
  ulong max_error_count;
  ulong max_length_for_sort_data;
  ulong max_sort_length;
  ulong max_tmp_tables;
  ulong max_insert_delayed_threads;
  ulong min_examined_row_limit;
  ulong multi_range_count;
  ulong net_buffer_length;
  ulong net_interactive_timeout;
  ulong net_read_timeout;
  ulong net_retry_count;
  ulong net_wait_timeout;
  ulong net_write_timeout;
  ulong optimizer_prune_level;
  ulong optimizer_search_depth;
  ulong preload_buff_size;
  ulong profiling_history_size;
  ulong read_buff_size;
  ulong read_rnd_buff_size;
  ulong mrr_buff_size;
  ulong div_precincrement;
  /* Total size of all buffers used by the subselect_rowid_merge_engine. */
  ulong rowid_merge_buff_size;
  ulong max_sp_recursion_depth;
  ulong default_week_format;
  ulong max_seeks_for_key;
  ulong range_alloc_block_size;
  ulong query_alloc_block_size;
  ulong query_prealloc_size;
  ulong trans_alloc_block_size;
  ulong trans_prealloc_size;
  ulong log_warnings;
  /* Flags for slow log filtering */
  ulong log_slow_rate_limit; 
  ulong binlog_format; ///< binlog format for this thd (see enum_binlog_format)
  ulong progress_report_time;
  my_bool binlog_annotate_row_events;
  my_bool binlog_direct_non_trans_update;
  my_bool sql_log_bin;
  ulong completion_type;
  ulong query_cache_type;
} LOC_SV;


static int init_done= 0;


static void *find_sym(const char *sym)
{
#ifdef _WIN32
  return GetProcAddress(GetModuleHandle("server.dll"), sym);
#else
  return dlsym(RTLD_DEFAULT, sym);
#endif
}

static int server_audit_init(void *p __attribute__((unused)))
{
  if (!serv_ver)
  {
#ifdef _WIN32
    serv_ver= (const char *) find_sym("server_version");
#else
    serv_ver= server_version;
#endif /*_WIN32*/
  }
  if (!mysql_57_started)
  {
    const void *my_hash_init_ptr= find_sym("_my_hash_init");
    if (!my_hash_init_ptr)
    {
      maria_above_5= 1;
      my_hash_init_ptr= find_sym("my_hash_init2");
    }
    if (!my_hash_init_ptr)
      return 1;
  }

  if(!(int_mysql_data_home= find_sym("mysql_data_home")))
  {
    if(!(int_mysql_data_home= find_sym("?mysql_data_home@@3PADA")))
      int_mysql_data_home= &default_home;
  }

  if (!serv_ver)
    return 1;

  if (!started_mysql)
  {
    if (!maria_above_5 && serv_ver[4]=='3' && serv_ver[5]<'3')
    {
      mode= 1;
      mode_readonly= 1;
    }
  }

  if (gethostname(servhost, sizeof(servhost)))
    strcpy(servhost, "unknown");

  servhost_len= (uint)strlen(servhost);

  logger_init_mutexes();
#ifdef HAVE_PSI_INTERFACE
  if (PSI_server)
    PSI_server->register_mutex("server_audit", mutex_key_list, 1);
#endif
  mysql_prlock_init(key_LOCK_operations, &lock_operations);
  flogger_mutex_init(key_LOCK_atomic, &lock_atomic, MY_MUTEX_INIT_FAST);


  error_header();
  fprintf(stderr, "MariaDB Audit Plugin version %s%s STARTED.\n",
          PLUGIN_STR_VERSION, PLUGIN_DEBUG_VERSION);

  /* The Query Cache shadows TABLE events if the result is taken from it */
  /* so we warn users if both Query Cashe and TABLE events enabled.      */
  if (!started_mysql)
  {
    ulonglong *qc_size= (ulonglong *) find_sym("query_cache_size");
    if (qc_size == NULL || *qc_size != 0)
    {
      struct loc_system_variables *g_sys_var=
        (struct loc_system_variables *) dlsym(RTLD_DEFAULT,
                                          "global_system_variables");
      if (g_sys_var && g_sys_var->query_cache_type != 0)
      {
        error_header();
        fprintf(stderr, "Query cache is enabled with the TABLE events."
                        " Some table reads can be veiled.\n");
      }
    }
  }

  if (logging)
  {
    ADD_ATOMIC(internal_stop_logging, 1);
    if (load_filters())
    {
      if (load_on_error)
      {
        explain_error("Filters aren't loaded, logging everything.");
        memcpy(last_error_buf, err_msg, err_len+1);
      }
      else
      {
        explain_error("Filters aren't loaded, plugin is disabled.");
        memcpy(last_error_buf, err_msg, err_len+1);
        ADD_ATOMIC(internal_stop_logging, -1);
        return 1;
      }
    }
    start_logging(0);
    ADD_ATOMIC(internal_stop_logging, -1);
  }

  init_done= 1;
  return 0;
}


static int server_audit_init_mysql(void *p)
{
  started_mysql= 1;
  mode= 1;
  mode_readonly= 1;
  return server_audit_init(p);
}


static int server_audit_deinit(void *p __attribute__((unused)))
{
  if (!init_done)
    return 0;

  free_filters();
  init_done= 0;

  if (output_type == OUTPUT_FILE && logfile)
    logger_close(logfile);
  else if (output_type == OUTPUT_SYSLOG)
    closelog();

  mysql_prlock_destroy(&lock_operations);
  flogger_mutex_destroy(&lock_atomic);

  error_header();
  fprintf(stderr, "STOPPED\n");
  return 0;
}


static void rotate_log(MYSQL_THD thd  __attribute__((unused)),
                       struct st_mysql_sys_var *var  __attribute__((unused)),
                       void *var_ptr  __attribute__((unused)),
                       const void *save  __attribute__((unused)))
{
  mysql_prlock_wrlock(&lock_operations);
  log_config(thd, "file_rotate_now", "ON");
  if (output_type == OUTPUT_FILE && logfile && *(my_bool*) save)
  {
    (void) logger_rotate(logfile);
    log_start_file();
  }
  mysql_prlock_unlock(&lock_operations);
}


static void do_reload_filters(MYSQL_THD thd  __attribute__((unused)),
                              struct st_mysql_sys_var *var  __attribute__((unused)),
                              void *var_ptr  __attribute__((unused)),
                              const void *save  __attribute__((unused)))
{
  struct filter_def *old_filter_list;
  struct filter_def *old_default_filter;
  struct user_def *old_user_list;

  ADD_ATOMIC(internal_stop_logging, 1);
  mysql_prlock_wrlock(&lock_operations);

  old_filter_list= filter_list;
  old_default_filter= default_filter;
  old_user_list= user_list;

  filter_list= 0;
  default_filter= 0;
  user_list= 0;

  if (load_filters())
  {
    log_config(thd, "failed reload_filters", "ON");
    CLIENT_ERROR(1, "%.*s SERVER AUDIT can't load filters - old filters are saved.",
                 MYF(ME_FATAL), (int) err_len, err_msg);
    explain_error("can't load filters - old filters are saved.");
    memcpy(last_error_buf, err_msg, err_len+1);
    filter_list= old_filter_list;
    default_filter= old_default_filter;
    user_list= old_user_list;
  }
  else
  {
    log_config(thd, "reload_filters", "ON");
    free_filters_ex(&old_filter_list, &old_default_filter, &old_user_list);
  }

  mysql_prlock_unlock(&lock_operations);
  ADD_ATOMIC(internal_stop_logging, -1);
}


static struct st_mysql_audit mysql_descriptor =
{
  MYSQL_AUDIT_INTERFACE_VERSION,
  NULL,
  auditing,
  { MYSQL_AUDIT_GENERAL_CLASSMASK | MYSQL_AUDIT_CONNECTION_CLASSMASK }
};


mysql_declare_plugin(server_audit)
{
  MYSQL_AUDIT_PLUGIN,
  &mysql_descriptor,
  "SERVER_AUDIT",
  " Alexey Botchkov (MariaDB Corporation)",
  "MariaDB Enterprise Audit",
  PLUGIN_LICENSE_GPL,
  server_audit_init_mysql,
  server_audit_deinit,
  PLUGIN_VERSION,
  audit_status,
  vars,
  NULL,
  0
}
mysql_declare_plugin_end;


static struct st_mysql_audit maria_descriptor =
{
  MYSQL_AUDIT_INTERFACE_VERSION,
  NULL,
  auditing,
  { MYSQL_AUDIT_GENERAL_CLASSMASK |
    MYSQL_AUDIT_TABLE_CLASSMASK |
    MYSQL_AUDIT_CONNECTION_CLASSMASK }
};
maria_declare_plugin(server_audit)
{
  MYSQL_AUDIT_PLUGIN,
  &maria_descriptor,
  "SERVER_AUDIT",
  "Alexey Botchkov (MariaDB Corporation)",
  "MariaDB Enterprise Audit",
  PLUGIN_LICENSE_GPL,
  server_audit_init,
  server_audit_deinit,
  PLUGIN_VERSION,
  audit_status,
  vars,
  PLUGIN_STR_VERSION,
  MariaDB_PLUGIN_MATURITY_STABLE
}
maria_declare_plugin_end;


static void update_file_path(MYSQL_THD thd,
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  char *new_name= (*(char **) save) ? *(char **) save : empty_str;

  ADD_ATOMIC(internal_stop_logging, 1);
  mysql_prlock_wrlock(&lock_operations);
  error_header();
  fprintf(stderr, "Log file name was changed to '%s'.\n", new_name);

  log_config(thd, "file_path", new_name);

  if (logging && output_type == OUTPUT_FILE)
  {
    char *sav_path= file_path;

    file_path= new_name;
    stop_logging();
    if (start_logging(thd))
    {
      file_path= sav_path;
      error_header();
      fprintf(stderr, "Reverting log filename back to '%s'.\n", file_path);
      logging= (start_logging(thd) == 0);
      if (!logging)
      {
        error_header();
        fprintf(stderr, "Logging was disabled..\n");
        CLIENT_ERROR(1, "Logging was disabled.", MYF(ME_WARNING));
      }
      goto exit_func;
    }
  }

  strncpy(path_buffer, new_name, sizeof(path_buffer)-1);
  path_buffer[sizeof(path_buffer)-1]= 0;
  file_path= path_buffer;
exit_func:
  mysql_prlock_unlock(&lock_operations);
  ADD_ATOMIC(internal_stop_logging, -1);
}


static void update_file_rotations(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  rotations= *(unsigned int *) save;
  error_header();
  fprintf(stderr, "Log file rotations was changed to '%d'.\n", rotations);

  mysql_prlock_wrlock(&lock_operations);

  log_uint_config(thd, "file_rotattions", rotations);

  if (logging && output_type == OUTPUT_FILE)
    logfile->rotations= rotations;

  mysql_prlock_unlock(&lock_operations);
}


static void update_query_log_limit(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  query_log_limit= *(unsigned int *) save;
  error_header();
  fprintf(stderr, "Query log limit was changed to '%d'.\n", query_log_limit);

  mysql_prlock_wrlock(&lock_operations);
  log_uint_config(thd, "query_log_Limit", query_log_limit);
  mysql_prlock_unlock(&lock_operations);

}


static void update_file_rotate_size(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  file_rotate_size= *(unsigned long long *) save;
  error_header();
  fprintf(stderr, "Log file rotate size was changed to '%lld'.\n",
          file_rotate_size);
  mysql_prlock_wrlock(&lock_operations);
  log_uint_config(thd, "rotate_size", file_rotate_size);

  if (logging && output_type == OUTPUT_FILE)
    logfile->size_limit= file_rotate_size;

  mysql_prlock_unlock(&lock_operations);
}


static void update_output_type(MYSQL_THD thd,
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  ulong new_output_type= *((ulong *) save);
  if (output_type == new_output_type)
    return;

  ADD_ATOMIC(internal_stop_logging, 1);
  mysql_prlock_wrlock(&lock_operations);
  if (logging)
  {
    log_config(thd, "output_type", output_type_names[new_output_type]);
    stop_logging();
  }

  output_type= new_output_type;
  error_header();
  fprintf(stderr, "Output was redirected to '%s'\n",
          output_type_names[output_type]);

  if (logging)
    start_logging(thd);
  mysql_prlock_unlock(&lock_operations);
  ADD_ATOMIC(internal_stop_logging, -1);
}


static void update_syslog_facility(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  ulong new_facility= *((ulong *) save);
  if (syslog_facility == new_facility)
    return;

  mysql_prlock_wrlock(&lock_operations);
  log_config(thd, "syslog_facility", syslog_facility_names[new_facility]);
  error_header();
  fprintf(stderr, "SysLog facility was changed from '%s' to '%s'.\n",
          syslog_facility_names[syslog_facility],
          syslog_facility_names[new_facility]);
  syslog_facility= new_facility;
  mysql_prlock_unlock(&lock_operations);
}


static void update_syslog_priority(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  ulong new_priority= *((ulong *) save);
  if (syslog_priority == new_priority)
    return;

  mysql_prlock_wrlock(&lock_operations);
  log_config(thd, "syslog_priority", syslog_priority_names[new_priority]);
  error_header();
  fprintf(stderr, "SysLog priority was changed from '%s' to '%s'.\n",
          syslog_priority_names[syslog_priority],
          syslog_priority_names[new_priority]);
  syslog_priority= new_priority;
  mysql_prlock_unlock(&lock_operations);
}


static void update_logging(MYSQL_THD thd,
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  char new_logging= *(char *) save;
  if (new_logging == logging)
    return;

  ADD_ATOMIC(internal_stop_logging, 1);
  mysql_prlock_wrlock(&lock_operations);
  log_config(thd, "logging", "OFF");
  if ((logging= new_logging))
  {
    if (load_filters())
    {
      error_header();
      fprintf(stderr, "Filters aren't loaded, logging can't be enabled!.\n");

      logging= 0;
    }
    else
      start_logging(thd);

    if (!logging)
    {
      CLIENT_ERROR(1, "%.*s SERVER_AUDIT cannot enable Logging.",
                   MYF(ME_FATAL), err_len, err_msg);
    }
    log_config(thd, "logging", "ON");
  }
  else
  {
    stop_logging();
  }

  mysql_prlock_unlock(&lock_operations);
  ADD_ATOMIC(internal_stop_logging, -1);
}


static void update_mode(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  unsigned int new_mode= *(unsigned int *) save;

  if (mode_readonly || new_mode == mode)
    return;

  if (!maria_55_started || !debug_server_started)
    mysql_prlock_wrlock(&lock_operations);

  log_config(thd, "mode", new_mode ? "1" : "0");

  error_header();
  fprintf(stderr, "Logging mode was changed from %d to %d.\n", mode, new_mode);
  mode= new_mode;
  if (!maria_55_started || !debug_server_started)
    mysql_prlock_unlock(&lock_operations);
}


static void update_syslog_ident(MYSQL_THD thd  __attribute__((unused)),
              struct st_mysql_sys_var *var  __attribute__((unused)),
              void *var_ptr  __attribute__((unused)), const void *save)
{
  char *new_ident= (*(char **) save) ? *(char **) save : empty_str;
  strncpy(syslog_ident_buffer, new_ident, sizeof(syslog_ident_buffer)-1);
  syslog_ident_buffer[sizeof(syslog_ident_buffer)-1]= 0;
  syslog_ident= syslog_ident_buffer;
  ADD_ATOMIC(internal_stop_logging, 1);
  mysql_prlock_wrlock(&lock_operations);
  log_config(thd, "syslog_ident", syslog_ident);
  error_header();
  fprintf(stderr, "SYSYLOG ident was changed to '%s'\n", syslog_ident);
  if (logging && output_type == OUTPUT_SYSLOG)
  {
    stop_logging();
    start_logging(thd);
  }
  mysql_prlock_unlock(&lock_operations);
  ADD_ATOMIC(internal_stop_logging, -1);
}


static int show_filter(MYSQL_THD thd, struct st_mysql_show_var *var,
                       char *buff,
                       struct system_status_var *sv  __attribute__((unused)),
                       enum enum_var_type scope)
{
  struct connection_info *cn= get_loc_info(thd);
  struct filter_def *f;

  var->type= SHOW_CHAR;

  if (scope == SHOW_OPT_GLOBAL)
  {
    strcpy(buff, "N/A");
    var->value= buff;
    return 1;
  }

  if (!cn)
    goto err;

  cn->thd= thd;
  setc(thd_user_name(thd), &cn->user, &cn->user_len);
  setc(thd_client_host(thd), &cn->host, &cn->host_len);
  setc(thd_client_ip(thd), &cn->ip, &cn->ip_len);

  f= find_user_filter(cn);
  if (!f)
    goto err;
  var->value= f->name;
  return 0;

err:
  strcpy(buff, "no filter");
  var->value= buff;
  return 0;
}


struct st_my_thread_var *loc_thread_var(void)
{
  return 0;
}



#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
  if (fdwReason != DLL_PROCESS_ATTACH)
    return 1;

  serv_ver= (const char *) find_sym("server_version");
#else
void __attribute__ ((constructor)) audit_plugin_so_init(void)
{
  serv_ver= server_version;
#endif /*_WIN32*/

  if (!serv_ver)
    goto exit;

  started_mariadb= strstr(serv_ver, "MariaDB") != 0;
  debug_server_started= strstr(serv_ver, "debug") != 0;

  if (started_mariadb)
  {
    if (serv_ver[0] == '1')
      use_event_data_for_disconnect= 1;
    else
      maria_55_started= 1;
  }
  else
  {
    /* Started MySQL. */
    goto exit;
  }

  memset(locinfo_ini_value, 'O', sizeof(locinfo_ini_value)-1);
  locinfo_ini_value[sizeof(locinfo_ini_value)-1]= 0;

exit:
#ifdef _WIN32
  return 1;
#else
  return;
#endif
}

