# queries to be executed by server audit plugin
# connects
# successful connect
connect (con1,localhost,root,,mysql);
connection default;
disconnect con1;
# failed  connect pwd
--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_MYPORT MASTER_PORT
--error ER_ACCESS_DENIED_ERROR
connect (con1,localhost,root,wrong_pwd,mysql);
connection default;
# trigger failed connect unknown user (will use default filter)
--replace_result $MASTER_MYSOCK MASTER_SOCKET $MASTER_MYPORT MASTER_PORT
--error ER_ACCESS_DENIED_ERROR
connect (con1,localhost,no_such_user,,mysql);
connection default;

# Trigger a change user event
change_user root;

#combined ddl and dml tests
CREATE DATABASE d1;
CREATE TABLE d1.t1 (id int);
INSERT INTO d1.t1 VALUES (1);
SELECT * from d1.t1;
UPDATE d1.t1 SET id=2;
DELETE FROM d1.t1;
ALTER TABLE d1.t1 ADD COLUMN (id2 int);
RENAME TABLE d1.t1 to d1.t2;
DROP TABLE d1.t2;
DROP DATABASE d1;


# dcl tests
CREATE USER u1 IDENTIFIED BY 'pwd-123';
GRANT ALL ON *.* TO u1 IDENTIFIED BY "pwd-321";
SET PASSWORD FOR u1 = PASSWORD('pwd 098');
#cleanup
drop user u1;

# errors
--error ER_NO_SUCH_TABLE
select * from test.t_doesnt_exist;
--error 1064
syntax_error_query;


