#define MYSQL_SERVER 1
#include <my_global.h>
#include "mysql_version.h"
#include "mysql/plugin.h"
#include "sql_class.h"
#include "sql_show.h"

static ST_FIELD_INFO i_s_metadata_snc_lock_info_fields_info[] =
{
  {"SESSION_NAME", 192, MYSQL_TYPE_STRING, 0,
    MY_I_S_MAYBE_NULL, "session name", SKIP_OPEN_TABLE},
  {"LOCK_NAME", 192, MYSQL_TYPE_STRING, 0,
    MY_I_S_MAYBE_NULL, "lock name", SKIP_OPEN_TABLE},
  {"COUNTER", 20, MYSQL_TYPE_LONGLONG, 0,
    MY_I_S_UNSIGNED, "counter", SKIP_OPEN_TABLE},
  {"SECONDS_ELAPSED", 20, MYSQL_TYPE_LONGLONG, 0,
    MY_I_S_UNSIGNED, "seconds elapsed", SKIP_OPEN_TABLE},
  {"SECONDS_TO_EXPIRE", 20, MYSQL_TYPE_LONGLONG, 0,
    MY_I_S_UNSIGNED, "seconds to expire", SKIP_OPEN_TABLE},
  {NULL, 0, MYSQL_TYPE_STRING, 0, 0, NULL, 0}
};

struct st_i_s_metadata_param
{
  THD   *thd;
  TABLE *table;
};

int i_s_metadata_snc_lock_info_fill_row(
  Snc_ull *snc_ull,
  void *arg)
{
  st_i_s_metadata_param *param = (st_i_s_metadata_param *) arg;
  THD *thd = param->thd;
  TABLE *table = param->table;
  DBUG_ENTER("i_s_metadata_snc_lock_info_fill_row");

  time_t now = time(NULL);
  table->field[0]->store(snc_ull->session_name, snc_ull->session_name_len, system_charset_info);
  table->field[0]->set_notnull();
  table->field[1]->store(snc_ull->lock_name, snc_ull->lock_name_len, system_charset_info);
  table->field[1]->set_notnull();
  table->field[2]->store((longlong) snc_ull->locked_count, TRUE);
  table->field[3]->store((longlong) (now - snc_ull->acquire_ts), TRUE);
  table->field[4]->store((longlong) (snc_ull->expire_ts - now), TRUE);

  if (schema_table_store_record(thd, table))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}

int i_s_metadata_snc_lock_info_fill_table(
  THD *thd,
  TABLE_LIST *tables,
  COND *cond
) 
{
  st_i_s_metadata_param param;
  DBUG_ENTER("i_s_metadata_snc_lock_info_fill_table");
  param.table = tables->table;
  param.thd = thd;
  DBUG_RETURN(snc_ull_iterate(i_s_metadata_snc_lock_info_fill_row, &param));
}

static int i_s_metadata_snc_lock_info_init(
  void *p
) 
{
  ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *) p;
  DBUG_ENTER("i_s_metadata_snc_lock_info_init");
  schema->fields_info = i_s_metadata_snc_lock_info_fields_info;
  schema->fill_table = i_s_metadata_snc_lock_info_fill_table;
  schema->idx_field1 = 0;
  DBUG_RETURN(0);
}

static int i_s_metadata_snc_lock_info_deinit(
  void *p
) 
{
  DBUG_ENTER("i_s_metadata_snc_lock_info_deinit");
  DBUG_RETURN(0);
}

static struct st_mysql_information_schema i_s_metadata_snc_lock_info_plugin =
{ MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION };

maria_declare_plugin(metadata_snc_lock_info)
{
  MYSQL_INFORMATION_SCHEMA_PLUGIN,
  &i_s_metadata_snc_lock_info_plugin,
  "METADATA_SNC_LOCK_INFO",
  "ServiceNow",
  "SNC user level locks view",
  PLUGIN_LICENSE_PROPRIETARY,
  i_s_metadata_snc_lock_info_init,
  i_s_metadata_snc_lock_info_deinit,
  0x0001,
  NULL,
  NULL,
  "1.0",
  MariaDB_PLUGIN_MATURITY_STABLE
}
maria_declare_plugin_end;
