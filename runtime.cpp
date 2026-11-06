#include <cctype>
#include <cinttypes>
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
char* qc_fmt_int(intptr_t v, int width, int precision, bool zero_pad) {
    char fmt[32];
    if (precision >= 0) {
        if (width > 0)
            snprintf(fmt, sizeof(fmt), "%%%s%d.%dd", zero_pad ? "0" : "", width, precision);
        else
            snprintf(fmt, sizeof(fmt), "%%.%dd", precision);
    } else {
        if (width > 0)
            snprintf(fmt, sizeof(fmt), "%%%s%dd", zero_pad ? "0" : "", width);
        else
            snprintf(fmt, sizeof(fmt), "%%d");
    }
    int len = snprintf(nullptr, 0, fmt, v);
    char* out = (char*)malloc(len + 1);
    if (!out) return nullptr;
    snprintf(out, len + 1, fmt, v);
    return out;
} // self hosted
char* qc_fmt_unsigned_int(size_t v, bool zero_pad) {
    char fmt[32];
    if (zero_pad) {
        snprintf(fmt, sizeof(fmt), "%%010" PRIuPTR);
    } else {
        snprintf(fmt, sizeof(fmt), "%%" PRIuPTR);
    }
    int len = snprintf(nullptr, 0, fmt, (uintptr_t)v);
    if (len < 0) return nullptr;
    char* out = (char*)malloc(len + 1);
    if (!out) return nullptr;
    snprintf(out, len + 1, fmt, (uintptr_t)v);
    return out;
} // self hosted
char* qc_fmt_float(double v, int width, int precision, bool zero_pad) {
    char fmt[32];
    if (precision >= 0) {
        if (width > 0) {
            snprintf(fmt, sizeof(fmt), "%%%s%d.%df", zero_pad ? "0" : "", width, precision);
        } else {
            snprintf(fmt, sizeof(fmt), "%%.%df", precision);
        }
    } else {
        if (width > 0) {
            snprintf(fmt, sizeof(fmt), "%%%s%df", zero_pad ? "0" : "", width);
        } else {
            snprintf(fmt, sizeof(fmt), "%%f");
        }
    }
    int len = snprintf(NULL, 0, fmt, v);
    if (len < 0) return NULL;
    char* out = (char*)malloc((size_t)len + 1);
    if (!out) return NULL;
    snprintf(out, (size_t)len + 1, fmt, v);
    return out;
} // self hosted
char* qc_fmt_double(double v, int width, int precision, bool zero_pad) {
    return qc_fmt_float(v, width, precision, zero_pad);
} // self hosted
char* qc_fmt_scientific(double v, int width, int precision, bool zero_pad) {
    char fmt[32];
    if (precision >= 0) {
        if (width > 0) {
            snprintf(fmt, sizeof(fmt), "%%%s%d.%de", zero_pad ? "0" : "", width, precision);
        } else {
            snprintf(fmt, sizeof(fmt), "%%.%de", precision);
        }
    } else {
        if (width > 0) {
            snprintf(fmt, sizeof(fmt), "%%%s%de", zero_pad ? "0" : "", width);
        } else {
            snprintf(fmt, sizeof(fmt), "%%e");
        }
    }
    int len = snprintf(nullptr, 0, fmt, v);
    if (len < 0) return nullptr;
    char* out = (char*)malloc(len + 1);
    if (!out) return nullptr;
    snprintf(out, len + 1, fmt, v);
    return out;
} // self hosted
char* qc_fmt_char(char c, int width, bool zero_pad) {
    char fmt[16];
    if (width > 0) {
        snprintf(fmt, sizeof(fmt), "%%%s%dc", zero_pad ? "0" : "", width);
    } else {
        snprintf(fmt, sizeof(fmt), "%%c");
    }
    int len = snprintf(nullptr, 0, fmt, c);
    if (len < 0) return nullptr;
    char* out = (char*)malloc(len + 1);
    if (!out) return nullptr;
    snprintf(out, len + 1, fmt, c);
    return out;
} // self hosted
char* qc_fmt_string(const char* s, int width, bool zero_pad) {
    if (!s) s = "";
    char fmt[16];
    if (width > 0) {
        snprintf(fmt, sizeof(fmt), "%%%s%ds", zero_pad ? "0" : "", width);
    } else {
        snprintf(fmt, sizeof(fmt), "%%s");
    }
    int len = snprintf(nullptr, 0, fmt, s);
    if (len < 0) return nullptr;
    char* out = (char*)malloc(len + 1);
    if (!out) return nullptr;
    snprintf(out, len + 1, fmt, s);
    return out;
} // self hosted
char* qc_fmt_hex(intptr_t v, int width, bool zero_pad) {
    char fmt[32];
    if (width > 0)
        snprintf(fmt, sizeof(fmt), "%%%s%dx", zero_pad ? "0" : "", width);
    else
        snprintf(fmt, sizeof(fmt), "%%x");
    int len = snprintf(nullptr, 0, fmt, v);
    char* out = (char*)malloc(len + 1);
    if (!out) return nullptr;
    snprintf(out, len + 1, fmt, v);
    return out;
} // self hosted
char* qc_fmt_octal(intptr_t v, int width, bool zero_pad) {
    char fmt[32];
    if (width > 0)
        snprintf(fmt, sizeof(fmt), "%%%s%do", zero_pad ? "0" : "", width);
    else
        snprintf(fmt, sizeof(fmt), "%%o");
    int len = snprintf(nullptr, 0, fmt, v);
    char* out = (char*)malloc(len + 1);
    if (!out) return nullptr;
    snprintf(out, len + 1, fmt, v);
    return out;
} // self hosted
char* qc_fmt_bool(bool v, int width, bool zero_pad) {
    const char* s = v ? "true" : "false";
    return qc_fmt_string(s, width, zero_pad);
} // self hosted
char* qc_fmt_qbool(uint8_t q, int width, bool zero_pad) {
    const char* s;
    switch (q & 0x3) {
    case 0: s = "none"; break;
    case 1: s = "qfalse"; break;
    case 2: s = "qtrue"; break;
    default: s = "both"; break;
    }
    return qc_fmt_string(s, width, zero_pad);
} // self hosted
char* qc_fmt_ptr(void* p, int width, bool zero_pad) {
    char buf[64];
    if (zero_pad && width > 0)
        snprintf(buf, sizeof(buf), "0x%0*jx", width, (uintmax_t)(size_t)p);
    else if (width > 0)
        snprintf(buf, sizeof(buf), "%*p", width, p);
    else
        snprintf(buf, sizeof(buf), "%p", p);
    return strdup(buf);
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
char* qc_string_concat(const char* a, const char* b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    char* result = (char*)malloc(len_a + len_b + 1);
    if (!result) return nullptr;
    memcpy(result, a, len_a);
    memcpy(result + len_a, b, len_b);
    result[len_a + len_b] = '\0';
    return result;
} // self hosted
bool qc_string_eq(const char* a, const char* b) {
    if (!a || !b) return 0;
    return strcmp(a, b) == 0 ? 1 : 0;
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
char* qc_to_string_int(int x) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%d", x);
    if (n < 0) return nullptr;
    char* out = (char*)malloc(n + 1);
    if (!out) return nullptr;
    memcpy(out, buf, n + 1);
    return out;
} // self hosted
char* qc_to_string_float(float x) {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%g", (double)x);
    if (n < 0) return nullptr;
    char* out = (char*)malloc(n + 1);
    if (!out) return nullptr;
    memcpy(out, buf, n + 1);
    return out;
} // self hosted
char* qc_to_string_long_int(intptr_t x) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%" PRIdPTR, x);
    if (n < 0) return nullptr;
    char* out = (char*)malloc(n + 1);
    if (!out) return nullptr;
    memcpy(out, buf, n + 1);
    return out;
} // self hosted
char* qc_to_string_short_int(short x) {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%hd", x);
    if (n < 0) return nullptr;
    char* out = (char*)malloc(n + 1);
    if (!out) return nullptr;
    memcpy(out, buf, n + 1);
    return out;
} // self hosted
char* qc_to_string_double(double x) {
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "%g", x);
    if (n < 0) return nullptr;
    char* out = (char*)malloc(n + 1);
    if (!out) return nullptr;
    memcpy(out, buf, n + 1);
    return out;
} // self hosted
char* qc_to_string_addr_t(uintptr_t x) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%" PRIuPTR, x);
    if (n < 0) return nullptr;
    char* out = (char*)malloc(n + 1);
    if (!out) return nullptr;
    memcpy(out, buf, n + 1);
    return out;
} // self hosted
char* qc_to_string_byte(uint8_t x) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%u", x);
    if (n < 0) return nullptr;
    char* out = (char*)malloc(n + 1);
    if (!out) return nullptr;
    memcpy(out, buf, n + 1);
    return out;
} // self hosted
char* qc_to_string_nibble(uint8_t x) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%u", (unsigned)(x & 0x0F));
    if (n < 0) return nullptr;
    char* out = (char*)malloc(n + 1);
    if (!out) return nullptr;
    memcpy(out, buf, n + 1);
    return out;
} // self hosted
char* qc_to_string_bool(bool b) {
    const char* s = b ? "true" : "false";
    size_t len = strlen(s);
    char* out = (char*)malloc(len + 1);
    if (!out) return nullptr;
    memcpy(out, s, len + 1);
    return out;
} // self hosted
char* qc_to_string_qbool(uint8_t q) {
    const char* s;
    switch (q & 0x3) {
    case 0: s = "none"; break;
    case 1: s = "qfalse"; break;
    case 2: s = "qtrue"; break;
    case 3: s = "both"; break;
    default: s = "none"; break;
    }
    size_t len = strlen(s);
    char* out = (char*)malloc(len + 1);
    if (!out) return nullptr;
    memcpy(out, s, len + 1);
    return out;
} // self hosted
char* qc_to_string_char(char c) {
    char* out = (char*)malloc(2);
    if (!out) return nullptr;
    out[0] = c;
    out[1] = '\0';
    return out;
} // self hosted
void qc_print_string(const char* s) {
    if (!s) s = "";
    fputs(s, stdout);
} // self hosted
int qc_len(const char* str) { // self hosted
    return strlen(str);
}
char* qc_to_lower(const char* str) {
    if (!str) return nullptr;
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 1);
    if (!result) return nullptr;
    for (size_t i = 0; i < len; i++) { result[i] = tolower((unsigned char)str[i]); }
    result[len] = '\0';
    return result;
} // self hosted
char* qc_to_upper(const char* str) {
    if (!str) return nullptr;
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 1);
    if (!result) return nullptr;
    for (size_t i = 0; i < len; i++) { result[i] = toupper((unsigned char)str[i]); }
    result[len] = '\0';
    return result;
} // self hosted
char* qc_substring(const char* str, int start, int length) {
    if (!str) return nullptr;
    int len = strlen(str);
    if (start < 0 || start >= len || length <= 0) {
        char* empty = (char*)malloc(1);
        empty[0] = '\0';
        return empty;
    }
    if (start + length > len) { length = len - start; }
    char* result = (char*)malloc(length + 1);
    if (!result) return nullptr;
    memcpy(result, str + start, length);
    result[length] = '\0';
    return result;
} // self hosted
int qc_contains(const char* str, const char* substr) {
    if (!str || !substr) return 0;
    return strstr(str, substr) != nullptr ? 1 : 0;
} // self hosted
int qc_startswith(const char* str, const char* prefix) {
    if (!str || !prefix) return 0;
    return strncmp(str, prefix, strlen(prefix)) == 0 ? 1 : 0;
} // self hosted
int qc_endswith(const char* str, const char* suffix) {
    if (!str || !suffix) return 0;
    size_t str_len = strlen(str);
    size_t suf_len = strlen(suffix);
    if (suf_len > str_len) return 0;
    return strcmp(str + str_len - suf_len, suffix) == 0 ? 1 : 0;
} // self hosted
bool qc_to_bool_from_string(const char* str) {
    if (!str) return false;
    return strcmp(str, "true") == 0 || strcmp(str, "1") == 0;
}
uint8_t qc_to_qbool_from_string(const char* str) {
    if (!str) return 0;
    if (strcmp(str, "qtrue") == 0) return 2;
    if (strcmp(str, "qfalse") == 0) return 1;
    if (strcmp(str, "both") == 0) return 3;
    return 0; // none
}
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
int qc_to_int_from_string(const char* str) {
    return str ? atoi(str) : 0;
} // self hosted
short qc_to_short_int_from_string(const char* str) {
    long val = str ? strtol(str, nullptr, 10) : 0;
    return static_cast<short>(val);
} // self hosted
intptr_t qc_to_long_int_from_string(const char* str) {
    return str ? strtoll(str, nullptr, 10) : 0;
} // self hosted
size_t qc_to_addr_t_from_string(const char* str) {
    return str ? static_cast<size_t>(strtoull(str, nullptr, 10)) : 0;
} // self hosted
uint8_t qc_to_nibble_from_string(const char* str) {
    return str ? static_cast<uint8_t>(strtoull(str, nullptr, 10) & 0x0FULL) : 0;
}
uint8_t qc_to_byte_from_string(const char* str) {
    return str ? static_cast<uint8_t>(strtoull(str, nullptr, 10) & 0xFFULL) : 0;
}
float qc_to_float_from_string(const char* str) {
    return str ? (float)atof(str) : 0.0f;
} // self hosted
double qc_to_double_from_string(const char* str) {
    return str ? atof(str) : 0.0;
} // self hosted
char qc_to_char_from_string(const char* str) {
    return (str && str[0]) ? str[0] : '\0';
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
