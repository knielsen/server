if (`select count(*) = 1 from information_schema.session_variables where variable_name = 'debug_sync' and variable_value like 'on %'`)
{
    connection con1;
    set global snc_default_lock_expiration=120;
    set debug_sync='snc_ull_lock signal lock wait_for past_lock';
    --send select snc_get_lock("session1", "lock1", 2) as success_con1_session1
    # con1 should now be stuck after acquring the mutex on the ull entry for lock1
    connection con2;
    set debug_sync='now wait_for lock';
    --send select snc_get_lock("session2", "lock1", 2) as failure_con2_session2
    connection con3;
    # unblock con2
    set debug_sync='now signal past_lock';
    connection con1;
    --reap
    set debug_sync='reset';
    connection con2;
    --reap
    set debug_sync='reset';
    connection con1;
    select snc_get_lock("session2", "lock1", 2) as failure_con1_session2; # held by session1
    connection con2;
    let $thread_id = `select connection_id()`;
    # con2 should now block
    --send select snc_get_lock("session2", "lock1", 60) as failure_con2_session1
    connection con3;
    --disable_query_log
    --eval kill $thread_id;
    --enable_query_log
    connection con2;
    --error 2006,2013
    --reap
    connection con1;
}
