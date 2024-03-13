connection con1;

# Valid characters for session/lock name
select snc_get_lock("abcd1234_-+/={}.:", "abcd1234_-+/={}.:", 10) as valid;
do snc_release_lock("abcd1234_-+/={}.:", "abcd1234_-+/={}.:");

#
# Invalid arguments
#
--error ER_WRONG_ARGUMENTS
select snc_get_lock("session'1", "lock1", 10) as error; # invalud session name (')
--error ER_WRONG_ARGUMENTS
select snc_get_lock("session1", "lock'1", 10) as error; # invalid lock name (')
--error ER_WRONG_PARAMCOUNT_TO_NATIVE_FCT
select snc_get_lock("lock1", 10) as error; # incorrect parameter count - session name is missing
select snc_get_lock("", "lock1", 10) as error; # empty session name
select snc_get_lock(NULL, "lock1", 10) as error; # NULL session name
select snc_get_lock("session1", "", 10) as error; # empty lock name
select snc_get_lock("session1", NULL, 10) as error; # NULL lock name
--error ER_WRONG_VALUE_FOR_TYPE
select snc_get_lock("session1", "lock1", -1) as error; # negative acquire_time
--error ER_WRONG_VALUE_FOR_TYPE
select snc_get_lock("session1", "lock1", 10, -1) as error; # negative expiration_time

# Max length of session/lock name is 192 characters
select snc_get_lock("123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012", "l", 10, 0) as len192;
select snc_get_lock("session1", "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012", 10, 0) as len192;
--error 1059
select snc_get_lock("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123", "l", 10, 0) as len193;
--error 1059
select snc_get_lock("session1", "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123", 10, 0) as len193;

--error ER_WRONG_ARGUMENTS
select snc_is_free_lock("lock'1") as error; # invalid lock name (')
select snc_is_free_lock("") as error; # empty lock name
select snc_is_free_lock(NULL) as error; # NULL lock name

--error ER_WRONG_ARGUMENTS
select snc_is_used_lock("lock'1") as error; # invalid lock name (')
select snc_is_used_lock("") as error; # empty lock name
select snc_is_used_lock(NULL) as error; # NULL lock name

--error ER_WRONG_ARGUMENTS
select snc_release_lock("session'1", "lock1") as error; # invalid session name
--error ER_WRONG_ARGUMENTS
select snc_release_lock("session1", "lock'1") as error; # invalid lock name
select snc_release_lock("", "lock1") as error; # empty session name
select snc_release_lock(NULL, "lock1") as error; # NULL session name
select snc_release_lock("session1", "") as error; # empty lock name
select snc_release_lock("session1", NULL) as error; # NULL lock name
--error ER_WRONG_VALUE_FOR_TYPE
select snc_release_lock("session1", "lock1", NULL) as error; # NULL final release

--error ER_WRONG_ARGUMENTS
select snc_release_all_locks("session'1") as error; # invalid session name
select snc_release_all_locks("") as error; # empty session name
select snc_release_all_locks(NULL) as error; # NULL session name

#
# Invalid argument type, string instead of int
#
--error ER_WRONG_VALUE_FOR_TYPE
select snc_get_lock("session1", "lock1", "foo");
--error ER_WRONG_VALUE_FOR_TYPE
select snc_get_lock("session1", "lock1", 10, "foo");
--error ER_WRONG_VALUE_FOR_TYPE
select snc_release_lock("session1", "lock1", "all");

#
# Regular operation
#
select snc_is_free_lock("lock1") as yes;
select snc_get_lock("session1", "lock1", 10) as counter; # no one holds it
select snc_is_used_lock("lock1");
select snc_is_free_lock("lock1") as no;
select snc_get_lock("session1", "lock1", 10) as counter; # reentrant
select snc_is_used_lock("lock1");
select snc_is_free_lock("lock1") as no;
select snc_release_lock("session1", "lock1") as counter; # session1 holds it twice
select snc_release_lock("session1", "lock1") as counter; # session1 holds it once
select snc_is_used_lock("lock1");
select snc_is_free_lock("lock1") as yes;
select snc_release_all_locks("foo") as error; # session not found
select snc_get_lock("foo", "bar", 1) as yes;
select snc_get_lock("foo", "baz", 1) as yes;
select snc_release_all_locks("foo") as two; # 2 locks released
select snc_get_lock("session1", "lock1", 0, 0) as yes; # 0 acquire_time and 0 expiration time are valid values
select snc_is_free_lock("lock1") as yes; # lock with 0 expiration time expires immediately

#
# Final release
#
select snc_get_lock("session1", "lock1", 10) as counter; # count=1
select snc_get_lock("session1", "lock1", 10) as counter; # count=2
select snc_get_lock("session1", "lock1", 10) as counter; # count=3
select snc_release_lock("session1", "lock1", 1) as counter; # final release
select snc_is_used_lock("lock1");
select snc_is_free_lock("lock1") as yes;

#
# Lock expiration
#
select snc_get_lock("session1", "lock1", 10, 2) as counter; # will expire in 2 seconds
do sleep(3);
select snc_is_used_lock("lock1");
select snc_is_free_lock("lock1") as yes;

select snc_get_lock("session1", "lock1", 10, 60) as counter;
do sleep(5);
select snc_is_used_lock("lock1");
select snc_get_lock("session1", "lock1", 10, 60) as counter;
select snc_is_used_lock("lock1");
select snc_release_lock("session1", "lock1") as counter;
select snc_release_lock("session1", "lock1") as counter;

do snc_get_lock("session1", "long1", 10);
do snc_get_lock("session1", "short", 10, 1);
do snc_get_lock("session1", "long2", 10);
do sleep(1);
select snc_release_all_locks("session1") as two;

do snc_get_lock("session1", "short", 10, 1);
do sleep(1);
select snc_release_lock("session1", "short") as none;

#
# Expiration times for reentrant locks do not add up
#
select snc_get_lock("session1", "lock1", 10, 60) as counter;
select snc_is_used_lock("lock1");
select snc_get_lock("session1", "lock1", 10, 30) as counter;
select snc_is_used_lock("lock1");
select snc_get_lock("session1", "lock1", 10, 1) as counter;
select snc_is_used_lock("lock1");
do sleep(2);
select snc_is_used_lock("lock1");

#
# Multiple sessions
#
connection con2;
select snc_get_lock("session2", "lock1", 10) as counter; # it was previously acquired and released by session1
connection con1;
select snc_release_lock("session1", "lock1") as failure; # session1 cannot release the lock held by session2
select snc_get_lock("session1", "lock1", 2) as counter; # session2 still holds it
select snc_get_lock("session1", "lock2", 2) as counter; # no one holds
select snc_release_lock("session1", "lock2") as counter; # session1 holds it, can release
connection con2;
select snc_release_lock("session2", "lock1") as counter; # session2 holds it at this point, can release
connection con1;
select snc_get_lock("session1", "lock1", 2) as counter; # re-entrant lock acquired
select snc_release_lock("session1", "lock1") as counter; # session1 holds it, can release
let save_duration = `select @@snc_default_lock_expiration`;
set global snc_default_lock_expiration = 2;
let save_memory_target = `select @@snc_lock_memory_target`;
set global snc_lock_memory_target = (1<<20);
select snc_is_used_lock("lock3");
select snc_get_lock("session1", "lock3", 2) as counter; # no one holds it
do sleep(@@snc_default_lock_expiration);
connection con2;
# should be able to get it as it is expired
select snc_get_lock("session2", "lock3", 2) as counter;
select snc_release_lock("session2", "lock3") as counter; # session2 holds it, can release
select snc_get_lock("session1", "lock_to_expire", 2, 5) as counter; # new lock, succeeds
select snc_get_lock("session2", "lock_to_expire", 3) as counter; # not yet
select snc_get_lock("session2", "lock_to_expire", 3) as counter; # now 6 seconds are passed, should succeed
select snc_release_lock("session2", "lock_to_expire") as counter;
connection con1;
select 1;
--disable_query_log
--disable_warnings
let n=10000;
create table t1(val int not null primary key) engine=memory;
while ($n)
{
  --eval insert ignore into t1 select snc_get_lock("session1", "lock$n", 2) as counter;
  --eval insert ignore into t1 select snc_release_lock("session1", "lock$n") as counter;
  dec $n;
}
drop table t1;
--enable_query_log
--enable_warnings
let $N = 10;
let $i = $N;
while ($i)
{
    --eval select snc_get_lock("session1", "lock$i", 10) as counter;
    --eval select snc_get_lock("session1", "lock$i", 10) as counter;
    dec $i;
}

let $i = $N;
while ($i)
{
    --eval select snc_is_used_lock("lock$i");
    dec $i;
}

select snc_release_all_locks("session1");

let $i = $N;
while ($i)
{
    --eval select snc_is_used_lock("lock$i");
    dec $i;
}

let $i = $N;
while ($i)
{
    --eval select snc_is_used_lock("lock$i");
    dec $i;
}

# In 11.8 the MEM_ROOT no longer keeps track of total allocation, and the
# patch instead maintains own counters of actually allocated bytes.
# This allocation is non-deterministic as it depends on exactly when all
# entries in the old root have been released.
# So this part of the test gives non-deterministic output now in 11.8-based
# branch and is therefore disabled.
#show global status like "snc_%";
