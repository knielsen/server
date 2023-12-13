
#ifndef XTRABACKUP_BACKUP_COPY_H
#define XTRABACKUP_BACKUP_COPY_H

#include <functional>
#include <my_global.h>
#include <mysql.h>
#include "datasink.h"

/* special files */
#define XTRABACKUP_SLAVE_INFO "xtrabackup_slave_info"
#define XTRABACKUP_GALERA_INFO "xtrabackup_galera_info"
#define XTRABACKUP_BINLOG_INFO "xtrabackup_binlog_info"
#define XTRABACKUP_INFO "xtrabackup_info"

extern bool binlog_locked;

bool
backup_file_printf(const char *filename, const char *fmt, ...)
		ATTRIBUTE_FORMAT(printf, 2, 0);

/************************************************************************
Return true if first and second arguments are the same path. */
bool
equal_paths(const char *first, const char *second);

/************************************************************************
Copy file for backup/restore.
@return true in case of success. */
bool
copy_file(ds_ctxt_t *datasink,
	  const char *src_file_path,
	  const char *dst_file_path,
	  uint thread_n,
		bool rewrite = false);

/** Start --backup */
bool backup_files(const char *from);
/** Release resources after backup_start() */
void backup_release();
/** Finish after backup_start() and backup_release() */
bool backup_finish();
bool
apply_log_finish();
bool
copy_back();
bool
decrypt_decompress();
bool
is_path_separator(char);
bool
directory_exists(const char *dir, bool create);

bool has_rocksdb_plugin();
void rocksdb_create_checkpoint();
void foreach_file_in_db_dirs(
	const char *dir_path, std::function<bool(const char *)> func);
void foreach_file_in_datadir(
	const char *dir_path, std::function<bool(const char *)> func);
bool ends_with(const char *str, const char *suffix);
bool starts_with(const char *str, const char *prefix);
void parse_db_table_from_file_path(
	const char *filepath, char *dbname, char *tablename);
const char *trim_dotslash(const char *path);
bool backup_files_from_datadir(const char *dir_path);
bool is_system_table(const char *dbname, const char *tablename);
std::unique_ptr<std::vector<std::string>>
	find_files(const char *dir_path, const char *prefix, const char *suffix);
bool file_exists(const char *filename);
bool
filename_matches(const char *filename, const char **ext_list);
bool backup_file_print_buf(const char *filename, const char *buf, int buf_len);
#endif
