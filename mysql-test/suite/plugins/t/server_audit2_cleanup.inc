# default cleanups after server_audit2 tests

# cleanup
uninstall plugin server_audit;

#System table cleanup
truncate mysql.server_audit_filters;
truncate mysql.server_audit_users;

let $MYSQLD_DATADIR= `SELECT @@datadir`;
# replace the timestamp and the hostname with constant values
--replace_regex /[0-9]* [0-9][0-9]:[0-9][0-9]:[0-9][0-9]\,[^,]*\,/TIME,HOSTNAME,/ /\,[1-9][0-9]*\,/,1,/ /\,[1-9][0-9]*/,ID/
cat_file $MYSQLD_DATADIR/server_audit.log;
remove_file $MYSQLD_DATADIR/server_audit.log;

