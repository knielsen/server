#include <my_global.h>
#include <mysql_com.h>
#include <sql_class.h>
#include "snc_ull_hash.h"


#define ULL_ROOT_BLOCK_SIZE 32768
#define DEFAULT_LOCK_EXPIRATION 120
#define ULL_HASH_SIZE 512
#define ULL_SESSION_HASH_SIZE 512

// For now. Need to modify  HASH while iterating
// efficiently. For that HASH_LINK needs to be exposed.
// TODO:fix HASH to have an API call for it 
typedef struct st_hash_info {
  uint32 next;                                  /* index to next key */
  my_hash_value_type hash_nr;
  uchar *data;                                  /* data for current entry */
} HASH_LINK;

Snc_ull_hash snc_ull_hash;

static PSI_cond_key key_COND_snc_ull;
static PSI_mutex_key key_MUTEX_snc_ull;
static PSI_mutex_key key_MUTEX_snc_ull_hash;
static PSI_mutex_key key_MUTEX_snc_ull_root_swap;
static PSI_cond_info all_snc_ull_conds[]= {
   { &key_COND_snc_ull, "Snc_ull_hash::cond", 0},
};

static PSI_mutex_info all_snc_ull_mutexes[] = {
    { &key_MUTEX_snc_ull, "Snc_ull::mutex", 0},
    { &key_MUTEX_snc_ull_root_swap, "Snc_ull::mutex_root_swap", 0},
    { &key_MUTEX_snc_ull_hash, "Snc_ull::mutex_hash", 0},
};

static int update_session_head(Snc_ull* old_head, Snc_ull* new_head)
{
  // my_hash_update unforutnately does not work - TODO: figure out if there is a way to make it work
  if (my_hash_delete(old_head->session_h, (uchar*)old_head))
  {
    sql_print_error("Snc_ull: Failed to delete the session head for lock %.*s session %.*s",
        old_head->lock_name_len, old_head->lock_name, old_head->session_name_len, old_head->session_name
    );
    return 1;
  }
  return my_hash_insert(old_head->session_h, (uchar*)new_head);
}

static int update_in_session(Snc_ull* ull_e, const char* session_name, uint session_name_len)
{
  ull_e->session_prev = 0;
  Snc_ull* session_head = (Snc_ull*)my_hash_search(ull_e->session_h,
      (const uchar*)session_name, session_name_len);

  if (session_head)
  {
    ull_e->session_next = session_head;
    session_head->session_prev = ull_e;
    if (update_session_head(session_head, ull_e))
    {
      sql_print_error("Snc_ull: Failed to update the session head for lock %.*s session %.*s",
        ull_e->lock_name_len, ull_e->lock_name, session_name_len, session_name
      );
      update_session_head(session_head, ull_e);
      return 1;
    }
  }
  else
  {
    ull_e->session_next = 0;

    if (my_hash_insert(ull_e->session_h, (const uchar*)ull_e))
    {
        sql_print_error("Snc_ull: Failed to insert the session head for lock %.*s session %.*s",
          ull_e->lock_name_len, ull_e->lock_name, session_name_len, session_name
        );
        return 1;
    }
  }

  return 0;
}

static void remove_from_session(Snc_ull* e)
{
  // removing the last entry for the session
  if (!e->session_next && !e->session_prev)
  {
    my_hash_delete(e->session_h, (uchar*)e);
    return;
  }

  if (e->session_next)
  {
    e->session_next->session_prev = e->session_prev;
  }

  if (e->session_prev)
  {
    e->session_prev->session_next = e->session_next;
  }
  else // we are removing the head
  {
    update_session_head(e, e->session_next);
  }

  e->session_prev = e->session_next = 0;
}

static void free_ull_entry(Snc_ull* e)
{
  if (!e || !e->alloced_from)
    return;

  mysql_mutex_lock(&e->lock);
  remove_from_session(e);
  mysql_mutex_unlock(&e->lock);
  mysql_mutex_destroy(&e->lock);
  mysql_cond_destroy(&e->cond);
  e->alloced_from = 0;
}

static uchar *get_key(const uchar *e, size_t *len,
                                  my_bool)
{
    if (!e)
    {
      *len = 0;
      return (uchar*) "";
    }
    
    Snc_ull* ull_e = (Snc_ull*)e;
    *len = ull_e->lock_name_len;
    return (uchar*)ull_e->lock_name;
}

static uchar *get_session_key(const uchar *e, size_t *len,
                                  my_bool)
{
    if (!e)
    {
      *len = 0;
      return (uchar*) "";
    }

    Snc_ull* ull_e = (Snc_ull*)e;
    *len = ull_e->session_name_len;
    return (uchar*)ull_e->session_name;
}


Snc_ull_hash::Snc_ull_hash():inited(0), default_lock_expiration(DEFAULT_LOCK_EXPIRATION), max_session_name_len(NAME_LEN)
{
  // we call init separately to avoid the memory tracking confusion in debug mode
}

void Snc_ull_hash::init()
{
  if (inited)
    return;

  set_default_lock_expiration(snc_default_lock_expiration);
  set_memory_target(snc_lock_memory_target);

  init_root(&root1);
  init_root(&root2);

  mysql_mutex_init(key_MUTEX_snc_ull_hash, &h_lock, 0);
  my_hash_init(&h, &my_charset_bin, ULL_HASH_SIZE,
               0, 0, (my_hash_get_key)get_key, (my_hash_free_key)free_ull_entry, HASH_UNIQUE);
  cur_root = &root1;
  old_root = &root2;

  mysql_mutex_init(key_MUTEX_snc_ull_root_swap, &root_swap_lock, 0);
  my_hash_init(&session_h, &my_charset_bin, ULL_SESSION_HASH_SIZE,
               0, 0, (my_hash_get_key)get_session_key, 0, HASH_UNIQUE);
  inited = 1;

  if (!PSI_server)
    return;

  size_t count = array_elements(all_snc_ull_mutexes);
  mysql_mutex_register("snc_ull", all_snc_ull_mutexes, count);
  count = array_elements(all_snc_ull_conds);
  mysql_cond_register("snc_ull", all_snc_ull_conds, count);
}

void Snc_ull_hash::try_reset_root(MEM_ROOT* root)
{
  static time_t last_call_time = 0;
  time_t now = time(NULL);

  // Do not proceed if this function has already been called
  // less than a second ago
  if (now == last_call_time)
  {
    return;
  }

  last_call_time = now;

  mysql_mutex_assert_owner(&h_lock);
  bool can_free_root = true;
  uint i;

  for (i = 0; i < h.records; i++)
  {
    HASH_LINK* hl = dynamic_element(&h.array, i, HASH_LINK *);
    Snc_ull* ull_e = (Snc_ull*)hl->data;
    
    if (!ull_e || ull_e->alloced_from != root)
      continue;

    mysql_mutex_lock(&ull_e->lock);
    
    if (ull_e->locked_count == 0 || ull_e->expire_ts <= now)
    {
      hl->data = 0;
      mysql_mutex_unlock(&ull_e->lock);
      free_ull_entry(ull_e);
      continue;
    }
    
    mysql_mutex_unlock(&ull_e->lock);
    can_free_root = false;
  }
  
  if (can_free_root)
  {
    free_root(root, MYF(0));
    init_root(root);
  }
}

void Snc_ull_hash::init_root(MEM_ROOT* root)
{
    init_alloc_root(root, "Snc_ull_hash::root", ULL_ROOT_BLOCK_SIZE, 0, MYF(0));
}

void Snc_ull_hash::end()
{
  if (!inited)
    return;

  my_hash_free(&h);
  my_hash_free(&session_h);
  free_root(&root1, MYF(0));
  free_root(&root2, MYF(0));
  mysql_mutex_destroy(&root_swap_lock);
  mysql_mutex_destroy(&h_lock);
  inited = 0;
}

Snc_ull_hash::~Snc_ull_hash()
{
  end();
}

Snc_ull* Snc_ull_hash::make_ull_entry(const char* session_name, uint session_name_len, const char* lock_name,
                uint lock_name_len)
{
  size_t mem_size = sizeof(Snc_ull) + lock_name_len + max_session_name_len;

  mysql_mutex_lock(&root_swap_lock);

  if (cur_root->total_alloc > memory_target)
  {
    try_reset_root(old_root);
    if (old_root->total_alloc < memory_target)
    {
      MEM_ROOT* tmp = cur_root;
      cur_root = old_root;
      old_root = tmp;
    }
  }

  Snc_ull* ull_e = (Snc_ull*)alloc_root(cur_root, mem_size);

  mysql_mutex_unlock(&root_swap_lock);

  if (!ull_e)
    return 0;

  if (session_name_len > max_session_name_len)
    session_name_len = max_session_name_len;

  ull_e->session_name = (char*)(ull_e+1);
  memcpy(ull_e->session_name, session_name, session_name_len);
  ull_e->session_name_len = session_name_len;
  ull_e->lock_name = ull_e->session_name + max_session_name_len;
  ull_e->lock_name_len = lock_name_len;
  ull_e->locked_count = 1; // make it locked right away
  ull_e->alloced_from = cur_root;
  ull_e->session_next = 0;
  ull_e->session_prev = 0;
  ull_e->session_h = &session_h;
  memcpy(ull_e->lock_name, lock_name, lock_name_len);
  mysql_cond_init(key_COND_snc_ull, &ull_e->cond, 0);
  mysql_mutex_init(key_MUTEX_snc_ull, &ull_e->lock, 0);
  return ull_e;
}


int Snc_ull_hash::get_lock(const char* session_name, uint session_name_len, const char* lock_name,
                uint lock_name_len, uint acquire_timeout, uint lock_expiration)
{
    if (session_name_len > max_session_name_len)
        session_name_len = max_session_name_len;

    Snc_ull* ull_e = 0;

    Hash_guard g(&h_lock);
    ull_e = (Snc_ull*)my_hash_search(&h, (const uchar*)lock_name, lock_name_len);

    bool created = false;
    int res = 0;
    struct timespec abstime;
    int err = 0;
    THD* thd = current_thd;
    time_t now = time(NULL);

retry:
    if (!ull_e)
    {
      g.ensure_locked();
      ull_e = make_ull_entry(session_name, session_name_len, lock_name, lock_name_len);
      if (update_in_session(ull_e, session_name, session_name_len))
        goto end;

      if (my_hash_insert(&h, (const uchar*)ull_e))
      {
        remove_from_session(ull_e);
        goto end;
      }
      g.ensure_unlocked();

      created = true;
      res = 1;
    }

    mysql_mutex_lock(&ull_e->lock);
    thd->ENTER_COND(&ull_e->cond, &ull_e->lock, NULL, NULL);
    DEBUG_SYNC(current_thd, "snc_ull_lock");

    if (created || ull_e->expire_ts <= now || (ull_e->session_name_len == session_name_len &&
            memcmp(ull_e->session_name, session_name, session_name_len) == 0) || ull_e->locked_count == 0)
    {
        if (!created && ull_e->alloced_from == old_root && (ull_e->locked_count == 0 || ull_e->expire_ts <= now))
        {
          // do not reuse entries from the old pool
          g.ensure_locked();
          remove_from_session(ull_e);
          mysql_mutex_unlock(&ull_e->lock);
          my_hash_delete(&h, (uchar*)ull_e);
          g.ensure_unlocked();
          ull_e = 0;
          goto retry;
        }

        if (!created)
        {
          g.ensure_locked();
          remove_from_session(ull_e);
          memcpy(ull_e->session_name, session_name, session_name_len);
          ull_e->session_name_len = session_name_len;
          update_in_session(ull_e, session_name, session_name_len);
          g.ensure_unlocked();
          ull_e->locked_count = ull_e->expire_ts <= now ? 1 : ull_e->locked_count + 1;
        }

        ull_e->acquire_ts = now;
        ull_e->expire_ts = now + lock_expiration;
        res = 1;
        goto end_mutex;
    }

    g.ensure_unlocked();

    // did not get the lock, need to wait
    uint wait_time;
    while (ull_e->expire_ts > (now = time(NULL)) && ull_e->locked_count > 0)
    {
        if (thd->check_killed())
        {
            res = 0;
            goto end_mutex;
        }

        wait_time = ull_e->expire_ts - now;

        if (wait_time > acquire_timeout)
            wait_time = acquire_timeout;

        set_timespec(abstime, wait_time);
        err = mysql_cond_timedwait(&ull_e->cond, &ull_e->lock, &abstime);

        if (err == ETIMEDOUT || err == ETIME)
        {
            if (ull_e->locked_count == 0 || ull_e->expire_ts <= time(NULL))
              goto take_lock;
            res = 0;
            goto end_mutex;
        }
    }

take_lock:
    now = time(NULL);
    ull_e->acquire_ts = now;
    ull_e->expire_ts = now + lock_expiration;
    mysql_mutex_unlock(&ull_e->lock);
    g.ensure_locked();
    mysql_mutex_lock(&ull_e->lock);
    remove_from_session(ull_e);
    memcpy(ull_e->session_name, session_name, session_name_len);
    ull_e->session_name_len = session_name_len;
    ull_e->locked_count = 1;
    if (update_in_session(ull_e, session_name, session_name_len))
      goto end_mutex;
    res = 1;
end_mutex:
    g.ensure_unlocked();
    thd->EXIT_COND(NULL);
    mysql_cond_broadcast(&ull_e->cond);
end:
    return res ? ull_e->locked_count : 0;
}

int Snc_ull_hash::release_all_locks(const char* session_name, uint session_name_len)
{
  Hash_guard g(&h_lock);
  Snc_ull* ull_e = (Snc_ull*)my_hash_search(&session_h, (uchar*)session_name,
                                            session_name_len);

  int released = 0;

  if (ull_e)
  {
    time_t now = time(NULL);

    while (ull_e)
    {
      mysql_mutex_lock(&ull_e->lock);
      Snc_ull* next = ull_e->session_next;

      if (ull_e->locked_count > 0)
      {
        ull_e->locked_count = 0;

        // Release all locks (for garbage collection)
        // but count only those that were not expired
        if (ull_e->expire_ts > now)
        {
          released++;
        }
      }

      mysql_cond_broadcast(&ull_e->cond);
      mysql_mutex_unlock(&ull_e->lock);
      ull_e = next;
    }
  }

  // 0 released locks means that all either session was not found 
  // or that session locks have already expired
  return released > 0 ? released : -1;
}

String* Snc_ull_hash::is_used_lock(String* res, const char* lock_name, uint lock_name_len)
{
  Hash_guard g(&h_lock);
  Snc_ull* ull_e = (Snc_ull*)my_hash_search(&h, (uchar*)lock_name, lock_name_len);
  if (!ull_e || ull_e->locked_count == 0)
  {
    return 0;
  }

  time_t now = time(NULL);

  long seconds_to_expire = (long)(ull_e->expire_ts - now);
  if (seconds_to_expire <= 0)
  {
    return 0;
  }

  char buf[NAME_LEN + 128];
  size_t len = snprintf(buf, sizeof(buf),
                 "{\"session\":\"%.*s\", \"count\":\"%u\", \"seconds_elapsed\":\"%ld\", \"seconds_to_expire\":\"%ld\"}",
                 ull_e->session_name_len,
                 ull_e->session_name,
                 ull_e->locked_count,
                 now - ull_e->acquire_ts,
                 seconds_to_expire);
  res->set(buf, len, &my_charset_bin);
  return res;
}

longlong Snc_ull_hash::is_free_lock(const char* lock_name, uint lock_name_len)
{
  Hash_guard g(&h_lock);
  Snc_ull* ull_e = (Snc_ull*)my_hash_search(&h, (uchar*)lock_name, lock_name_len);
  return !ull_e || ull_e->locked_count == 0 || ull_e->expire_ts <= time(NULL);
}

void Snc_ull_hash::show_pool(SHOW_VAR* var, char* buf, bool old)
{
    var->type = SHOW_LONGLONG;
    var->value = buf;
    MEM_ROOT* root = old ? old_root : cur_root;
    *(longlong*)buf = (longlong)(root->total_alloc);
}

int Snc_ull_hash::release_lock(const char* session_name, uint session_name_len, const char* lock_name,
                    uint lock_name_len, bool final_release)
{
    if (session_name_len > max_session_name_len)
    {
      session_name_len = max_session_name_len;
    }

    Snc_ull* ull_e;

    {
      Hash_guard g(&h_lock);
      ull_e = (Snc_ull*)my_hash_search(&h, (const uchar*)lock_name, lock_name_len);
    }

    int res = -1;

    if (ull_e)
    {
      mysql_mutex_lock(&ull_e->lock);
      DEBUG_SYNC(current_thd, "snc_ull_release");

      if (ull_e->locked_count > 0)
      {
        // Lock found. Is it expired?
        if (ull_e->expire_ts <= time(NULL))
        {
          ull_e->locked_count = 0;
          mysql_cond_broadcast(&ull_e->cond);
        }
        else // The lock is active. Do we own it?
        if (ull_e->session_name_len == session_name_len &&
            memcmp(ull_e->session_name, session_name, session_name_len) == 0)
        {
          res = ull_e->locked_count = final_release ? 0 : ull_e->locked_count - 1;
          mysql_cond_broadcast(&ull_e->cond);
        }
      }

      mysql_mutex_unlock(&ull_e->lock);
    }

    return res;
}

int snc_ull_iterate(snc_ull_iterator_callback callback, void *arg)
{
  Snc_ull_hash::Hash_guard g(&snc_ull_hash.h_lock);
  DBUG_ENTER("snc_ull_iterate");

  time_t now = time(NULL);

  for (uint i = 0; i < snc_ull_hash.h.records; i++)
  {
    HASH_LINK* hl = dynamic_element(&snc_ull_hash.h.array, i, HASH_LINK *);
    if (hl)
    {
      Snc_ull* ull_e = (Snc_ull*)hl->data;

      if (ull_e && ull_e->locked_count > 0 && ull_e->expire_ts > now)
      {
        callback(ull_e, arg);
      }
    }
  }

  DBUG_RETURN(0);
}
