#include "filesys/filesys.h"
#include <stdio.h>

int main() {
    // Khởi tạo hệ thống tệp (format lần đầu để đảm bảo không có dữ liệu cũ)
    filesys_init(true);

    // Kiểm tra bộ nhớ đã sử dụng ban đầu
    size_t memory_used = filesys_memory_usage();
    printf("Initial memory used: %zu bytes\n", memory_used);

    // Tạo một file để kiểm tra
    const char *test_file = "testfile.txt";
    if (filesys_create(test_file, 1024)) {
        printf("Created file '%s' with size 1024 bytes.\n", test_file);
    } else {
        printf("Failed to create file '%s'.\n", test_file);
    }

    // Kiểm tra bộ nhớ sau khi tạo file
    memory_used = filesys_memory_usage();
    printf("Memory used after creating file: %zu bytes\n", memory_used);

    // Xóa file và kiểm tra lại bộ nhớ
    if (filesys_remove(test_file)) {
        printf("Removed file '%s'.\n", test_file);
    } else {
        printf("Failed to remove file '%s'.\n", test_file);
    }

    memory_used = filesys_memory_usage();
    printf("Memory used after removing file: %zu bytes\n", memory_used);

    // Dọn dẹp hệ thống tệp
    filesys_done();
    return 0;
}

