#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <unistd.h>
extern "C" {
[[noreturn]] void __qc_panic(const char* msg, const char* file, int line) {
    fprintf(stderr, "panic: %s (%s:%d)\n", msg, file, line);
    abort();
}
void* qc_malloc(size_t size) {
    return malloc(size);
} // self hosted
void qc_free(void* ptr) {
    free(ptr);
} // self hosted
void* qc_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
} // self hosted
void* qc_calloc(size_t num, size_t size) {
    return calloc(num, size);
} // self hosted
void qc_flush() {
    fflush(NULL);
} // self hosted
int qc_powi_i32(int base, int exp) {
    if (exp < 0) return 0;
    int result = 1;
    while (exp > 0) {
        if (exp & 1) result *= base;
        base *= base;
        exp >>= 1;
    }
    return result;
} // self hosted
uint8_t qc_qand(uint8_t a, uint8_t b) { // self hosted
    if (a == 0 && b == 0) return 0;
    if (a == 0 || a == 1 || b == 0 || b == 1) return 1;
    if (a == 2 && b == 2) return 2;
    if (a == 2 && b == 3) return 3;
    if (a == 3 && b == 2) return 3;
    return 3;
}
uint8_t qc_qor(uint8_t a, uint8_t b) { // self hosted
    if (a == 0 && b == 0) return 0;
    if (a == 0 && b == 1) return 1;
    if (a == 0 && b == 2) return 2;
    if (a == 1 && b == 0) return 1;
    if (a == 1 && b == 1) return 1;
    if (a == 2 && b == 0) return 2;
    if (a == 2 && b == 2) return 2;
    return 3;
}
uint8_t qc_qxor(uint8_t a, uint8_t b) { // self hosted
    if (a <= 1 && b <= 1) return 0;
    if (a <= 1 && b >= 2) return 3;
    if (a >= 2 && b <= 1) return 3;
    if (a == 2 && b == 2) return 1;
    return 3;
}
uint8_t qc_qnot(uint8_t a) { // self hosted
    return 3 - a;
}
bool qc_qand_collapse(uint8_t a, uint8_t b) { // self hosted
    return (a & 0b10) && (b & 0b10);
}
bool qc_qor_collapse(uint8_t a, uint8_t b) { // self hosted
    return (a & 0b10) || (b & 0b10);
}
int qc_open(const char* path, const char* mode) {
    FILE* f = fopen(path, mode);
    if (f == nullptr) return -1;
    return fileno(f);
} // self hosted
void qc_close(int fd) {
    close(fd);
} // self hosted
ssize_t qc_read(int fd, char* buffer, size_t size) {
    return read(fd, buffer, size);
}
long long qc_lseek(int fd, long long offset, int whence) {
    return lseek(fd, offset, whence);
}
void qc_write(int fd, const char* data) {
    if (!data) return;
    size_t len = strlen(data);
    while (len > 0) {
        ssize_t n = write(fd, data, len);
        if (n <= 0) return;
        data += n;
        len -= n;
    }
} // self hosted
ssize_t qc_write_sized(int fd, const char* data, ssize_t bc) {
    if (!data) return -1;
    return write(fd, data, bc);
} // self hosted
//////////////////
/// VARIADICS ///
////////////////
typedef struct {
    void** items;
    int count;
    int current_index;
} qc_variadic; // self hosted
bool qc_variadic_is_empty(qc_variadic* variadic) {
    return variadic->current_index >= variadic->count; // self hosted
}
void* qc_variadic_next(qc_variadic* variadic) { // self hosted
    if (qc_variadic_is_empty(variadic)) { return nullptr; }
    void* data = variadic->items[variadic->current_index];
    variadic->current_index++;
    return data;
}
}
