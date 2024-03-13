#ifndef SNC_ULL_HASH_H
#define SNC_ULL_HASH_H

struct Snc_ull
{
    char* lock_name;
    uint lock_name_len;
    char* session_name;
    uint session_name_len;
    time_t acquire_ts;
    time_t expire_ts;
    mysql_mutex_t lock;
    mysql_cond_t cond;
    uint locked_count;
    MEM_ROOT* alloced_from;
    Snc_ull* session_next;
    Snc_ull* session_prev;
    HASH* session_h;
};

typedef int (*snc_ull_iterator_callback)(Snc_ull *ull, void *arg);
extern MYSQL_PLUGIN_IMPORT
int snc_ull_iterate(snc_ull_iterator_callback callback, void *arg);

class Snc_ull_hash
{
protected:
    bool inited;
    HASH h;
    mysql_mutex_t h_lock;
    MEM_ROOT root1;
    MEM_ROOT root2;
    MEM_ROOT *cur_root, *old_root;
    size_t cur_total_alloc, old_total_alloc;

    uint default_lock_expiration;
    uint max_session_name_len;
    ulonglong memory_target;
    mysql_mutex_t root_swap_lock;
    HASH session_h;

    Snc_ull* make_ull_entry(const char* session_name, uint session_name_len, const char* lock_name,
                uint lock_name_len);

    void try_reset_root(MEM_ROOT* root, size_t *total_alloc);
    void init_root(MEM_ROOT* root);
  
    class Hash_guard
    {
    protected:
        mysql_mutex_t* lock;
        bool locked;
    public:
        Hash_guard(mysql_mutex_t* lock):lock(lock),locked(true)
        {
            mysql_mutex_lock(lock);
        }
        ~Hash_guard()
        {
          ensure_unlocked();
        }

        void ensure_unlocked()
        {
          if (locked)
          {
            mysql_mutex_unlock(lock);
            locked = false;
          }
        }

        void ensure_locked()
        {
          if (!locked)
          {
            mysql_mutex_lock(lock);
            locked = true;
          }
        }
    };

public:
    Snc_ull_hash();
    ~Snc_ull_hash();
    int get_lock(const char* session_name, uint session_name_len, const char* lock_name,
                    uint lock_name_len, uint acquire_timeout, uint lock_expiration);
    int release_lock(const char* session_name, uint session_name_len, const char* lock_name,
                    uint lock_name_len, bool final_release);

    int release_all_locks(const char* session_name, uint session_name_len);
    String* is_used_lock(String* res, const char* lock_name, uint lock_name_len);
    longlong is_free_lock(const char* lock_name, uint lock_name_len);

    void set_default_lock_expiration(uint arg)
    {
        my_atomic_store32(&default_lock_expiration, arg);
    }

    uint get_default_lock_expiration()
    {
        return my_atomic_load32(&default_lock_expiration);
    }

    void set_memory_target(ulonglong arg)
    {
        memory_target = arg;
    }

    void show_pool(SHOW_VAR* var, char* buf, bool old);
    void init();
    void end();

    friend int snc_ull_iterate(snc_ull_iterator_callback callback, void *arg);
};

extern Snc_ull_hash snc_ull_hash;

#endif
