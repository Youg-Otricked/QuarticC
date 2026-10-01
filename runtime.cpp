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
#include <unwind.h>
extern "C" {
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
int qc_time() {
    return (int)time(NULL);
} // self hosted
void qc_seed(int seed) {
    srand(seed);
} // self hosted
int qc_random_int(int max) {
    if (max <= 0) return 0;
    return rand() % max;
} // self hosted
float qc_random_float() {
    return (float)rand() / (float)RAND_MAX;
} // self hosted
int qc_random_range(int min, int max) {
    if (max <= min) return min;
    return min + (rand() % (max - min));
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
int qc_sizeof_type(int elem_type) {
    switch (elem_type) {
    case 0: return sizeof(int);
    case 1: return sizeof(float);
    case 2: return sizeof(double);
    case 3: return sizeof(char);
    case 4: return sizeof(bool);
    case 5: return sizeof(uint8_t);
    case 6: return sizeof(char*);
    default: return 4;
    }
}
struct qc_jagged_array {
    void** data;
    int* sizes;
    int count;
    int elem_type;
    int depth;
}; // self hosted
qc_jagged_array* qc_create_jagged_array(int count, int elem_type, int depth) {
    qc_jagged_array* arr = (qc_jagged_array*)malloc(sizeof(qc_jagged_array));
    arr->data = (void**)malloc(count * sizeof(void*));
    arr->sizes = (int*)malloc(count * sizeof(int));
    arr->count = count;
    arr->elem_type = elem_type;
    arr->depth = depth;
    return arr;
} // self hosted
void qc_set_jagged_element(qc_jagged_array* arr, int idx, void* elem, int size) {
    arr->data[idx] = elem;
    arr->sizes[idx] = size;
} // self hosted
void qc_free_jagged_array(qc_jagged_array* arr) {
    if (arr->depth > 0) {
        for (int i = 0; i < arr->count; i++) { qc_free_jagged_array((qc_jagged_array*)arr->data[i]); }
    } else {
        for (int i = 0; i < arr->count; i++) { free(arr->data[i]); }
    }
    free(arr->data);
    free(arr->sizes);
    free(arr);
} // self hosted
void* qc_jagged_array_get(qc_jagged_array* arr, int* indices, int num_indices) {
    if (num_indices == 0 || !arr) { return nullptr; }
    int idx = indices[0];
    if (idx < 0 || idx >= arr->count) { return nullptr; }
    if (arr->depth == 0) {
        if (num_indices == 1) {
            return arr->data[idx];
        } else {
            int elem_idx = indices[1];
            if (elem_idx < 0 || elem_idx >= arr->sizes[idx]) { return nullptr; }
            void* row = arr->data[idx];
            return (char*)row + (elem_idx * qc_sizeof_type(arr->elem_type));
        }
    } else if (arr->depth == 1) {
        void* row = arr->data[idx];
        if (num_indices == 1) {
            return row;
        } else {
            int elem_idx = indices[1];
            if (elem_idx < 0 || elem_idx >= arr->sizes[idx]) { return nullptr; }
            return (char*)row + (elem_idx * qc_sizeof_type(arr->elem_type));
        }
    } else {
        qc_jagged_array* sub = (qc_jagged_array*)arr->data[idx];
        if (!sub) return nullptr;
        return qc_jagged_array_get(sub, indices + 1, num_indices - 1);
    }
} // self hosted
void* qc_create_leaf_row(int size, int elem_type) {
    int elem_size = qc_sizeof_type(elem_type);
    int total = size * elem_size;
    void* row = malloc(total);
    return row;
} // self hosted
void qc_set_leaf_element(void* row, int idx, void* value, int elem_type) {
    void* dest = (char*)row + (idx * qc_sizeof_type(elem_type));
    memcpy(dest, value, qc_sizeof_type(elem_type));
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
///////////////////
// EXCEPTIONS ////
/////////////////
#define QC_EXCEPTION_CLASS                                                                                                                           \
    0x5143455843455054ULL // My lawyer(coderabbit, its usefull for first-time making crap) advised against "Rust=bad", "IHATEYOU", "WHYYYYYY",
                          // "DAMCATCH" and "AGHHHHHH". Not because these are "inappropriate", but because all but the first are too common among
                          // compiler devs.
struct __qc_exception {
    _Unwind_Exception unwind;
    char* type;
    void* value;
};
void* __qc_exception_get_value(__qc_exception* e) {
    return e->value;
}
static void __qc_cleanup(_Unwind_Reason_Code reason, _Unwind_Exception* exception) {
    (void)reason;
    __qc_exception* e = (__qc_exception*)exception;
    free(e->type);
    free(e);
}
__qc_exception* __qc_create_exception(char* type, void* value) {
    __qc_exception* e = (__qc_exception*)malloc(sizeof(__qc_exception));
    e->unwind.exception_class = QC_EXCEPTION_CLASS;
    e->unwind.exception_cleanup = __qc_cleanup;
    e->type = strdup(type);
    e->value = value;
    return e;
}
void __qc_throw(__qc_exception* e) {
    _Unwind_Reason_Code result = _Unwind_RaiseException(&e->unwind);
    fprintf(stderr, "Uncaught exception: %s\n", e->type);
    abort();
}
#define EXCEPTION_REG 0
#define SELECTOR_REG 1
#define DW_EH_PE_absptr 0x00
#define DW_EH_PE_omit 0xff
#define DW_EH_PE_uleb128 0x01
#define DW_EH_PE_udata2 0x02
#define DW_EH_PE_udata4 0x03
#define DW_EH_PE_udata8 0x04
#define DW_EH_PE_sleb128 0x09
#define DW_EH_PE_sdata2 0x0A
#define DW_EH_PE_sdata4 0x0B
#define DW_EH_PE_sdata8 0x0C
#define DW_EH_PE_pcrel 0x10
#define DW_EH_PE_textrel 0x20
#define DW_EH_PE_datarel 0x30
#define DW_EH_PE_funcrel 0x40
#define DW_EH_PE_indirect 0x80
static uintptr_t __qc_read_uleb128(uint8_t** p) {
    uintptr_t result = 0;
    int shift = 0;
    while (1) {
        uint8_t b = **p;
        (*p)++;
        result |= ((uintptr_t)(b & 0x7f)) << shift;
        if (!(b & 0x80)) return result;
        shift += 7;
    }
}
static intptr_t __qc_read_sleb128(uint8_t** p) {
    intptr_t result = 0;
    int shift = 0;
    uint8_t b;
    do {
        b = **p;
        (*p)++;
        result |= ((intptr_t)(b & 0x7f)) << shift;
        shift += 7;
    } while (b & 0x80);
    if ((shift < sizeof(result) * 8) && (b & 0x40)) { result |= -((intptr_t)1 << shift); }
    return result;
}
#define DW_EH_PE_aligned 0x50

static uintptr_t __qc_read_encoded(uint8_t** p, uint8_t encoding, uintptr_t textBase, uintptr_t dataBase, uintptr_t funcBase) {
    if (encoding == DW_EH_PE_omit) return 0;
    uint8_t application = encoding & 0x70;
    if (application == DW_EH_PE_aligned) {
        uintptr_t address = (uintptr_t)*p;
        uintptr_t alignment = sizeof(uintptr_t);
        address = (address + alignment - 1) & ~(alignment - 1);
        *p = (uint8_t*)address;
        encoding = (encoding & 0x8f) | DW_EH_PE_absptr;
        application = DW_EH_PE_absptr;
    }
    uint8_t* encodedStart = *p;
    intptr_t result = 0;
    switch (encoding & 0x0f) {
    case DW_EH_PE_absptr:
        memcpy(&result, *p, sizeof(uintptr_t));
        *p += sizeof(uintptr_t);
        break;
    case DW_EH_PE_uleb128: result = (intptr_t)__qc_read_uleb128(p); break;
    case DW_EH_PE_udata2: {
        uint16_t value;
        memcpy(&value, *p, sizeof(value));
        *p += sizeof(value);
        result = value;
        break;
    }
    case DW_EH_PE_udata4: {
        uint32_t value;
        memcpy(&value, *p, sizeof(value));
        *p += sizeof(value);
        result = value;
        break;
    }
    case DW_EH_PE_udata8: {
        uint64_t value;
        memcpy(&value, *p, sizeof(value));
        *p += sizeof(value);
        result = (intptr_t)value;
        break;
    }
    case DW_EH_PE_sleb128: result = __qc_read_sleb128(p); break;
    case DW_EH_PE_sdata2: {
        int16_t value;
        memcpy(&value, *p, sizeof(value));
        *p += sizeof(value);
        result = value;
        break;
    }
    case DW_EH_PE_sdata4: {
        int32_t value;
        memcpy(&value, *p, sizeof(value));
        *p += sizeof(value);
        result = value;
        break;
    }
    case DW_EH_PE_sdata8: {
        int64_t value;
        memcpy(&value, *p, sizeof(value));
        *p += sizeof(value);
        result = (intptr_t)value;
        break;
    }
    default: abort();
    }
    switch (application) {
    case DW_EH_PE_absptr: break;
    case DW_EH_PE_pcrel: result += (uintptr_t)encodedStart; break;
    case DW_EH_PE_textrel: result += textBase; break;
    case DW_EH_PE_datarel: result += dataBase; break;
    case DW_EH_PE_funcrel: result += funcBase; break;
    default: abort();
    }
    if (encoding & DW_EH_PE_indirect) {
        uintptr_t indirectValue;
        memcpy(&indirectValue, (void*)(uintptr_t)result, sizeof(indirectValue));
        result = (intptr_t)indirectValue;
    }
    return (uintptr_t)result;
}
_Unwind_Reason_Code __qc_personality(int version, _Unwind_Action actions, uint64_t exceptionClass, _Unwind_Exception* exceptionObject,
                                     _Unwind_Context* context) {
    if (version != 1) return _URC_FATAL_PHASE1_ERROR;
    if (exceptionClass != QC_EXCEPTION_CLASS) return _URC_CONTINUE_UNWIND;

    auto* exception = reinterpret_cast<__qc_exception*>(exceptionObject);
    auto* lsda = reinterpret_cast<uint8_t*>(_Unwind_GetLanguageSpecificData(context));
    if (!lsda) return _URC_CONTINUE_UNWIND;

    uintptr_t funcStart = _Unwind_GetRegionStart(context);
    uintptr_t ip = _Unwind_GetIP(context);
    if (ip != 0) --ip;

    uint8_t* p = lsda;

    uint8_t lpStartEncoding = *p++;
    uintptr_t lpStart = funcStart;
    if (lpStartEncoding != DW_EH_PE_omit) { lpStart = __qc_read_encoded(&p, lpStartEncoding, 0, 0, funcStart); }

    uint8_t typeEncoding = *p++;
    uint8_t* typeTable = nullptr;
    if (typeEncoding != DW_EH_PE_omit) {
        uintptr_t offset = __qc_read_uleb128(&p);
        typeTable = p + offset;
    }

    uint8_t callSiteEncoding = *p++;
    uintptr_t callSiteSize = __qc_read_uleb128(&p);
    uint8_t* callSiteTable = p;
    uint8_t* actionTable = callSiteTable + callSiteSize;

    uintptr_t landingPad = 0;
    uintptr_t actionIndex = 0;
    uintptr_t pc = ip - funcStart;

    for (uint8_t* cs = callSiteTable; cs < actionTable;) {
        uintptr_t start = __qc_read_encoded(&cs, callSiteEncoding, 0, 0, funcStart);
        uintptr_t length = __qc_read_encoded(&cs, callSiteEncoding, 0, 0, funcStart);
        uintptr_t pad = __qc_read_encoded(&cs, callSiteEncoding, 0, 0, funcStart);
        uintptr_t action = __qc_read_uleb128(&cs);

        if (pc >= start && pc < start + length) {
            landingPad = lpStart + pad;
            actionIndex = action;
            break;
        }
    }

    if (landingPad == 0 || actionIndex == 0) { return _URC_CONTINUE_UNWIND; }

    uint8_t* action = actionTable + actionIndex - 1;
    bool matched = false;
    bool hasCleanup = false;
    int selector = 0;
    int selectorIndex = 0;
    bool hasCatchAll = false;
    while (true) {
        int64_t typeIndex = __qc_read_sleb128(&action);
        uint8_t* nextField = action;
        int64_t next = __qc_read_sleb128(&action);
        if (typeIndex == 0) {
            hasCleanup = true;
        } else if (typeIndex > 0 && typeTable != nullptr) {
            size_t typeSize;
            switch (typeEncoding & 0x0f) {
            case DW_EH_PE_absptr: typeSize = sizeof(uintptr_t); break;
            case DW_EH_PE_udata2:
            case DW_EH_PE_sdata2: typeSize = 2; break;
            case DW_EH_PE_udata4:
            case DW_EH_PE_sdata4: typeSize = 4; break;
            case DW_EH_PE_udata8:
            case DW_EH_PE_sdata8: typeSize = 8; break;
            default: abort();
            }
            uint8_t* entry = typeTable - typeIndex * typeSize;
            uintptr_t raw = 0;
            memcpy(&raw, entry, typeSize);
            if (raw == 0) {
                hasCatchAll = true;
            } else {
                ++selectorIndex;
                uintptr_t typeAddr = __qc_read_encoded(&entry, typeEncoding, 0, 0, reinterpret_cast<uintptr_t>(entry));
                const char* catchType = reinterpret_cast<const char*>(typeAddr);
                if (catchType && strcmp(exception->type, catchType) == 0) {
                    matched = true;
                    selector = selectorIndex;
                    break;
                }
            }
        }
        if (next == 0) break;
        action = nextField + next;
    }
    if (!matched && hasCatchAll) {
        matched = true;
        selector = 0;
    }
    if (actions & _UA_SEARCH_PHASE) { return matched ? _URC_HANDLER_FOUND : _URC_CONTINUE_UNWIND; }

    if (actions & _UA_CLEANUP_PHASE) {
        if (matched && (actions & _UA_HANDLER_FRAME)) {
            _Unwind_SetGR(context, 0, reinterpret_cast<uintptr_t>(exceptionObject));
            _Unwind_SetGR(context, 1, static_cast<uintptr_t>(selector));
            _Unwind_SetIP(context, landingPad);
            return _URC_INSTALL_CONTEXT;
        }

        if (hasCleanup) {
            _Unwind_SetGR(context, 0, reinterpret_cast<uintptr_t>(exceptionObject));
            _Unwind_SetGR(context, 1, 0);
            _Unwind_SetIP(context, landingPad);
            return _URC_INSTALL_CONTEXT;
        }
    }

    return _URC_CONTINUE_UNWIND;
}
}
