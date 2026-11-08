#include <cctype>
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
DIR* qc_opendir(const char* path) {
    return opendir(path);
}
bool qc_readdir(DIR* dir, char* buffer, size_t size) {
    struct dirent* entry = readdir(dir);
    if (entry == NULL) { return false; }
    size_t length = strlen(entry->d_name);
    if (length + 1 > size) { return false; }
    memcpy(buffer, entry->d_name, length + 1);
    return true;
}
int qc_closedir(DIR* dir) {
    return closedir(dir);
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
static inline int qc_w(int width) {
    return width > 0 ? width : 0;
}
void qc_fmt_int(intptr_t v, int width, int precision, bool zero_pad) {
    zero_pad ? printf("%0*.*jd", qc_w(width), precision, (intmax_t)v) : printf("%*.*jd", qc_w(width), precision, (intmax_t)v);
}
void qc_fmt_unsigned_int(size_t v, int width, bool zero_pad) {
    zero_pad ? printf("%0*ju", qc_w(width), (uintmax_t)v) : printf("%*ju", qc_w(width), (uintmax_t)v);
}
void qc_fmt_float(double v, int width, int precision, bool zero_pad) {
    zero_pad ? printf("%0*.*f", qc_w(width), precision, v) : printf("%*.*f", qc_w(width), precision, v);
}
void qc_fmt_double(double v, int width, int precision, bool zero_pad) {
    qc_fmt_float(v, width, precision, zero_pad);
}
void qc_fmt_scientific(double v, int width, int precision, bool zero_pad) {
    zero_pad ? printf("%0*.*e", qc_w(width), precision, v) : printf("%*.*e", qc_w(width), precision, v);
}
void qc_fmt_hex(intptr_t v, int width, bool zero_pad) {
    zero_pad ? printf("%0*jx", qc_w(width), (uintmax_t)(uintptr_t)v) : printf("%*jx", qc_w(width), (uintmax_t)(uintptr_t)v);
}
void qc_fmt_octal(intptr_t v, int width, bool zero_pad) {
    zero_pad ? printf("%0*jo", qc_w(width), (uintmax_t)(uintptr_t)v) : printf("%*jo", qc_w(width), (uintmax_t)(uintptr_t)v);
}
static int qc_pad(int n, bool zero_pad) {
    for (int i = 0; i < n; i++)
        if (putchar(zero_pad ? '0' : ' ') == EOF) return -1;
    return n;
}
int qc_fmt_string(const char* s, int width, bool zero_pad) {
    if (!s) s = "";
    int len = (int)strlen(s);
    int pad = width > len ? width - len : 0;
    if (qc_pad(pad, zero_pad) < 0) return -1;
    if (fputs(s, stdout) == EOF) return -1;
    return pad + len;
}
int qc_fmt_char(char c, int width, bool zero_pad) {
    int pad = width > 1 ? width - 1 : 0;
    if (qc_pad(pad, zero_pad) < 0) return -1;
    if (putchar((unsigned char)c) == EOF) return -1;
    return pad + 1;
}
int qc_fmt_bool(bool v, int width, bool zero_pad) {
    return qc_fmt_string(v ? "true" : "false", width, zero_pad);
}
int qc_fmt_qbool(uint8_t q, int width, bool zero_pad) {
    static const char* names[4] = {"none", "qfalse", "qtrue", "both"};
    return qc_fmt_string(names[q & 0x3], width, zero_pad);
}
int qc_fmt_ptr(void* p, int width, bool zero_pad) {
    if (zero_pad && width > 0) return printf("0x%0*jx", width, (uintmax_t)(uintptr_t)p);
    return printf("%*p", qc_w(width), p);
}
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
void qc_print_string(const char* s) {
    if (!s) s = "";
    fputs(s, stdout);
} // self hosted
char* qc_qin() {
    fflush(stdout);
    fflush(stderr);
    char buffer[1024];
    int result = scanf("%s", buffer);
    fflush(stderr);
    if (result == 1) { return strdup(buffer); }
    return strdup("");
} // self hosted
char* qc_trim(const char* str) {
    if (!str) return nullptr;
    while (*str && isspace((unsigned char)*str)) str++;
    if (*str == '\0') {
        char* empty = (char*)malloc(1);
        empty[0] = '\0';
        return empty;
    }
    const char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    size_t len = end - str + 1;
    char* result = (char*)malloc(len + 1);
    if (!result) return nullptr;
    memcpy(result, str, len);
    result[len] = '\0';
    return result;
} // self hosted
char* qc_replace(const char* str, const char* find, const char* replace) {
    if (!str || !find || !replace) return nullptr;
    size_t find_len = strlen(find);
    size_t replace_len = strlen(replace);
    if (find_len == 0) {
        char* dup = (char*)malloc(strlen(str) + 1);
        strcpy(dup, str);
        return dup;
    }
    int count = 0;
    const char* p = str;
    while ((p = strstr(p, find)) != nullptr) {
        count++;
        p += find_len;
    }
    if (count == 0) {
        char* dup = (char*)malloc(strlen(str) + 1);
        strcpy(dup, str);
        return dup;
    }
    size_t str_len = strlen(str);
    size_t result_len = str_len + count * (replace_len - find_len);
    char* result = (char*)malloc(result_len + 1);
    if (!result) return nullptr;
    char* dst = result;
    p = str;
    while (*p) {
        const char* match = strstr(p, find);
        if (match == p) {
            memcpy(dst, replace, replace_len);
            dst += replace_len;
            p += find_len;
        } else {
            *dst++ = *p++;
        }
    }
    *dst = '\0';
    return result;
} // self hosted
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
