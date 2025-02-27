#include "filesys/filesys.h"

#include <debug.h>
#include <stdio.h>
#include <string.h>

#include "devices/disk.h"
#include "filesys/directory.h"
#include "filesys/fat.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "threads/thread.h"

/** #Project 4: File System */
struct disk *filesys_disk;

static void do_format(void);

/* Thêm hàm báo cáo bộ nhớ */
void report_memory_usage(void) {
    // Giả sử bạn có các biến toàn cục hoặc cấu trúc để theo dõi bộ nhớ
    size_t total_memory = 1024 * 1024 * 100; // Ví dụ: tổng bộ nhớ là 100MB
    size_t used_memory = 0; // Thay đổi logic này để tính bộ nhớ đã sử dụng

    // Tính toán bộ nhớ đã sử dụng (có thể cần thêm logic của riêng bạn)
    // Ví dụ:
    //used_memory = /* Tính toán bộ nhớ đã sử dụng */;

    printf("Total Memory: %zu bytes\n", total_memory);
    printf("Used Memory: %zu bytes\n", used_memory);
    printf("Free Memory: %zu bytes\n", total_memory - used_memory);
}

/* Initializes the file system module. */
void filesys_init(bool format) {
    filesys_disk = disk_get(0, 1);
    if (filesys_disk == NULL)
        PANIC("hd0:1 (hdb) not present, file system initialization failed");

    inode_init();

#ifdef EFILESYS
    fat_init();

    if (format)
        do_format();

    fat_open();

    thread_current()->cwd = dir_open_root(); /** #Project 4: File System - hiện tại thread의 cwd를 root로 설정 */
#else
    /* Original FS */
    free_map_init();

    if (format)
        do_format();

    free_map_open();
#endif
}

/* Shuts down the file system module, writing any unwritten data to disk. */
void filesys_done(void) {
#ifdef EFILESYS
    fat_close();
#else
    free_map_close();
#endif

    // Gọi hàm báo cáo bộ nhớ
    report_memory_usage();
}

/* Creates a file named NAME with the given INITIAL_SIZE. */
bool filesys_create(const char *name, off_t initial_size) {
#ifndef EFILESYS
    disk_sector_t inode_sector = 0;
    struct dir *dir = dir_open_root();
    bool success = (dir != NULL && free_map_allocate(1, &inode_sector) && inode_create(inode_sector, initial_size, FILE_TYPE) && dir_add(dir, name, inode_sector));
    if (!success && inode_sector != 0)
        free_map_release(inode_sector, 1);
    dir_close(dir);
    return success;
#else
    cluster_t inode_cluster = fat_create_chain(0);
    disk_sector_t inode_sector = cluster_to_sector(inode_cluster);
    bool success;

    char target[128];
    target[0] = '\0';

    struct dir *dir_path = parse_path(name, target);

    if (strcmp(target, "") == 0)
        return false;

    if (dir_path == NULL || inode_is_removed(dir_get_inode(dir_path)))
        return false;

    struct dir *dir = dir_reopen(dir_path);

    success = (dir != NULL && inode_create(inode_sector, initial_size, FILE_TYPE) && dir_add(dir, target, inode_sector));

    if (!success && inode_sector != 0)
        fat_remove_chain(inode_cluster, 1);

    dir_close(dir);

    return success;
#endif
}

/* Opens the file with the given NAME. */
struct file *filesys_open(const char *name) {
#ifndef EFILESYS
    struct dir *dir = dir_open_root();
    struct inode *inode = NULL;

    if (dir != NULL)
        dir_lookup(dir, name, &inode);
    dir_close(dir);

    return file_open(inode);
#else
    if (strlen(name) == 0)
        return NULL;

    if (strlen(name) == 1 && name[0] == '/')
        return file_open(dir_get_inode(dir_open_root()));

    char target[128];
    target[0] = '\0';
    struct inode *inode = NULL;
    struct dir *dir_path = parse_path(name, target);

    if (dir_path == NULL || inode_is_removed(dir_get_inode(dir_path)))
        return NULL;

    struct dir *dir = dir_reopen(dir_path);

    if (!dir_lookup(dir, target, &inode))
        return NULL;

    if (inode_is_removed(inode))
        return NULL;

    while (inode_get_type(inode) == 2) {  // link xử lý
        char target[128];
        target[0] = '\0';

        struct dir *target_dir = parse_path(inode_get_linkpath(inode), target);

        if (!dir_lookup(target_dir, target, &inode))
            return NULL;

        if (inode_is_removed(inode))
            return NULL;
    }

    return file_open(inode);
#endif
}

/* Deletes the file named NAME. */
bool filesys_remove(const char *name) {
#ifndef EFILESYS
    struct dir *dir = dir_open_root();
    bool success = dir != NULL && dir_remove(dir, name);
    dir_close(dir);

    return success;
#else
    if (strlen(name) == 1 && name[0] == '/') /** Project 4: File System - root 디렉토리 remove 금지 */
        return false;

    char target[128];
    target[0] = '\0';
    bool success = false;

    struct dir *dir_path = parse_path(name, target);

    if (dir_path == NULL)
        goto done;

    struct inode *inode = NULL;

    dir_lookup(dir_path, target, &inode);

    while (inode_get_type(inode) == 2) {  // link xử lý
        char target[128];
        target[0] = '\0';

        struct dir *target_dir = parse_path(inode_get_linkpath(inode), target);

        if (!dir_lookup(target_dir, target, &inode))
            return NULL;

        if (inode_is_removed(inode))
            return NULL;
    }

    if (inode_get_type(inode) == 1) {  // nếu là thư mục
        struct dir *dir = dir_open(inode);

        if (!dir_is_empty(dir) || inode_is_removed(inode))
            return false;

        dir_finddir(dir, dir_path, target);
        dir_close(dir);

        return dir_remove(dir_path, target);
    }

    struct dir *file = dir_reopen(dir_path);  // nếu là tệp

    success = file != NULL && dir_remove(file, target);

    if (dir_lookup(dir_path, target, &inode))
        return false;

    file_close(file);
done:
    dir_close(dir_path);
    return success;
#endif
}

/* Formats the file system. */
static void do_format(void) {
    printf("Formatting file system...");

#ifdef EFILESYS
    /* Create FAT and save it to the disk. */
    fat_create();

    /* Root Directory 생성 */
    disk_sector_t root = cluster_to_sector(ROOT_DIR_CLUSTER);
    if (!dir_create(root, 16))
        PANIC("root directory creation failed");

    /* Root Directory에 ., .. 추가 */
    struct dir *root_dir = dir_open_root();
    dir_add(root_dir, ".", root);
    dir_add(root_dir, "..", root);
    dir_close(root_dir);

    fat_close();
#else
    free_map_create();
    if (!dir_create(ROOT_DIR_SECTOR, 16))
        PANIC("root directory creation failed");
    free_map_close();
#endif

    printf("done.\n");
}

struct dir *parse_path(char *path_name, char *target) {
    struct dir *dir = dir_open_root();
    char *token, *next_token, *ptr;
    char *path = malloc(strlen(path_name) + 1);
    strlcpy(path, path_name, strlen(path_name) + 1);

    if (path[0] != '/' && thread_current()->cwd != NULL) {
        dir_close(dir);
        dir = dir_reopen(thread_current()->cwd);
    }

    token = strtok_r(path, "/", &ptr);
    next_token = strtok_r(NULL, "/", &ptr);

    if (token == NULL)  // path_name = "/" 만 입력되었을 때
        return dir_open_root();

    while (next_token != NULL) {
        struct inode *inode = NULL;
        if (!dir_lookup(dir, token, &inode))
            goto err;

        while (inode_get_type(inode) == 2) {  // link xử lý
            char target[128];
            target[0] = '\0';

            struct dir *target_dir = parse_path(inode_get_linkpath(inode), target);

            if (!dir_lookup(target_dir, target, &inode))
                goto err;
        }

        dir_close(dir);
        dir = dir_open(inode);

        token = next_token;
        next_token = strtok_r(NULL, "/", &ptr);
    }

    if (token == NULL || strlen(token) >= 128)
        goto err;

    strlcpy(target, token, strlen(token) + 1);
    free(path);
    return dir;
err:
    free(path);
    dir_close(dir);
    return NULL;
}

bool filesys_chdir(const char *dir_name) {
    struct inode *inode = NULL;
    char target[128];
    target[0] = '\0';
    struct dir *dir = parse_path(dir_name, target);

    if (!dir_lookup(dir, target, &inode))
        return false;

    if (inode_get_type(inode) == 0 || inode_is_removed(inode))
        return false;

    dir = dir_open(inode);

    thread_current()->cwd = dir;

    return true;
}

bool filesys_mkdir(const char *dir_name) {
    cluster_t inode_cluster = fat_create_chain(0);
    disk_sector_t inode_sector = cluster_to_sector(inode_cluster);
    char target[128];

    if (strlen(dir_name) == 0)
        return false;

    struct dir *dir_path = parse_path(dir_name, target);
    if (dir_path == NULL)
        return false;

    struct dir *dir = dir_reopen(dir_path);

    bool success = (dir != NULL && inode_create(inode_sector, 0, DIR_TYPE) && dir_add(dir, target, inode_sector));

    if (!success && inode_cluster != 0)
        fat_remove_chain(inode_cluster, 0);

    if (success) {  // thêm . và .. vào thư mục
        struct inode *inode = NULL;
        dir_lookup(dir, target, &inode);
        struct dir *new_dir = dir_open(inode);

        if (!dir_add(new_dir, ".", inode_sector))
            success = false;
        if (!dir_add(new_dir, "..", inode_get_inumber(dir_get_inode(dir))))
            success = false;

        dir_close(new_dir);
    }

    dir_close(dir);

    return success;
}

bool filesys_symlink(const char *target, const char *linkpath) {
    cluster_t inode_cluster = fat_create_chain(0);
    disk_sector_t inode_sector = cluster_to_sector(inode_cluster);

    struct inode *target_inode = NULL;
    struct inode *inode = NULL;
    bool success;

    char link_name[128];
    link_name[0] = '\0';

    struct dir *link_dir = parse_path(linkpath, link_name);

    if (strcmp(link_name, "") == 0)
        return false;

    if (link_dir == NULL || inode_is_removed(dir_get_inode(link_dir)))
        return false;

    success = (link_dir != NULL && inode_create(inode_sector, 0, LINK_TYPE) && dir_add(link_dir, link_name, inode_sector));

    if (!success && inode_sector != 0) {
        fat_remove_chain(inode_cluster, 1);
        return success;
    }

    dir_lookup(link_dir, link_name, &inode);

    inode_set_linkpath(inode, target);

    return success;
}
