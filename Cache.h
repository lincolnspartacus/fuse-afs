namespace Cache {
    void init(const std::string user_cache_dir);
    void update_entry(const char *path, unsigned long long timestamp);
    void delete_entry(const char *path);
    void mkdir(const char *path);
    int create_file(const char *path,uint64_t flags,mode_t mode);
    unsigned long long lookup(const char *path);
    //void clear();
}
