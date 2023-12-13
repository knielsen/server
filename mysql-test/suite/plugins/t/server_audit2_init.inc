# Preparations needed for all Audit Plugin2 tests

--disable_warnings
# create system tables
CREATE TABLE IF NOT EXISTS mysql.server_audit_filters (
    filtername char(80) COLLATE utf8_bin NOT NULL DEFAULT '',
    rule longtext CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT 'true' CHECK (json_valid(rule)),
    CONSTRAINT c_filtername UNIQUE (filtername)
) ENGINE=Aria;

CREATE TABLE IF NOT EXISTS mysql.server_audit_users (host char(60) COLLATE utf8_bin NOT NULL DEFAULT '',
    user char(80) COLLATE utf8_bin NOT NULL DEFAULT '',
    filtername char(80) NOT NULL DEFAULT '',
    CONSTRAINT c_host_user UNIQUE (host, user)
) ENGINE=Aria;
--enable_warnings

# load plugin
install plugin server_audit soname 'server_audit2';

# show audit plugin variables
show variables like 'server_audit%';

# show status
