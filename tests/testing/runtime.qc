#nomain
extern:
///////////////////
// METADATA //////
/////////////////
string __qc_version() {
    return "x0.18.3";
}
int __qc_llvm_ver() {
    return 21;
}
void __qc_release_notes() {
    `inline(R"(
    mov rax, 1
    mov rdi, 1
    mov rsi, $0r
    mov rdx, 63
    )", "Added generic functions & methods, deprecated `auto` functions\n", "~{rax}~{rdi}~{rsi}~{rdx}");
}
///////////////////
// VARIADICS /////
/////////////////
struct qc_variadic {
    void** items;
    int count;
    int current_index;
};
bool qc_variadic_is_empty(qc_variadic* variadic) {
    return variadic->current_index >= variadic->count;
}
void* qc_variadic_next(qc_variadic* variadic) {
    if (qc_variadic_is_empty(variadic)) { return nullptr; }
    void* data = variadic->items[variadic->current_index];
    variadic->current_index++;
    return data;
}
///////////////////
// Q LOGICAL /////
/////////////////

int qc_qand(int a, int b) {
    if (a == 0 && b == 0) return 0;
    if (a == 0 || a == 1 || b == 0 || b == 1) return 1;
    if (a == 2 && b == 2) return 2;
    if (a == 2 && b == 3) return 3;
    if (a == 3 && b == 2) return 3;
    return 3;
}

int qc_qor(int a, int b) {
    if (a == 0 && b == 0) return 0;
    if (a == 0 && b == 1) return 1;
    if (a == 0 && b == 2) return 2;
    if (a == 1 && b == 0) return 1;
    if (a == 1 && b == 1) return 1;
    if (a == 2 && b == 0) return 2;
    if (a == 2 && b == 2) return 2;
    return 3;
}

int qc_qxor(int a, int b) {
    if (a <= 1 && b <= 1) return 0;
    if (a <= 1 && b >= 2) return 3;
    if (a >= 2 && b <= 1) return 3;
    if (a == 2 && b == 2) return 1;
    return 3;
}

int qc_qnot(int a) {
    return 3 - a;
}

bool qc_qand_collapse(int a, int b) {
    return (a & 0b10) && (b & 0b10);
}

bool qc_qor_collapse(int a, int b) {
    return (a & 0b10) || (b & 0b10);
}
///////////////////
// REAL MALLOC ///
/////////////////
const int ALIGNMENT = 16;
void* __qc_memcpy(void *dst, void *src, addr_t size) {
    char *d = dst;
    char *s = src;
    while (size && (`to_address(d) & 7)) {
        *d = *s;
        d = d + 1;
        s = s + 1;
        size--;
    }
    addr_t *dw = d;
    addr_t *sw = s;
    while (size >= 8) {
        *dw = *sw;
        dw = dw + 1;
        sw = sw + 1;
        size -= 8;
    }
    d = dw;
    s = sw;
    while (size) {
        *d = *s;
        d = d + 1;
        s = s + 1;
        size--;
    }
    return dst;
}
const addr_t MAX_ADDR = ~0a;
// Memset. Because calloc is literally memset(malloc())
void *qc_memset(void *ptr, int value, addr_t size) {
    addr_t address = `to_address(ptr);
    value = value & 0xFF;
    while (size > 0 && address % sizeof "addr_t" != 0) {
        char *p = `mapped_ptr(address);
        *p = value;
        address++;
        size--;
    }
    addr_t word = value;
    word = word | word << 8;
    word = word | word << 16;
    if (sizeof "addr_t" >= 8) word = word | word << 32;
    while (size >= sizeof "addr_t") {
        addr_t *p = `mapped_ptr(address);
        *p = word;
        address += sizeof "addr_t";
        size -= sizeof "addr_t";
    }
    while (size > 0) {
        char *p = `mapped_ptr(address);
        *p = value;
        address++;
        size--;
    }
    return ptr;
}
// Alignment
addr_t __qc_align(addr_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}
// Chunk of heap memory
struct __qc_malloc_chunk {
    addr_t size;
    bool free;
    __qc_malloc_chunk *prev_phys;
    __qc_malloc_chunk *next_phys;
    __qc_malloc_chunk *prev_free;
    __qc_malloc_chunk *next_free;
    char* user_data;
};
// Current state type
struct __qc_malloc_state {
    __qc_malloc_chunk *first_chunk;
    __qc_malloc_chunk *free_list;
    __qc_malloc_chunk *top_chunk;
    addr_t system_mem;
};
// Current state
__qc_malloc_state __qc_heap = {};
// Find first free chunk
__qc_malloc_chunk* __qc_find_free_chunk(addr_t size) {
    __qc_malloc_chunk *current = __qc_heap.free_list;
    while (current != nullptr) {
        if (current->free && current->size >= size) return current;
        current = current->next_free;
    }
    return nullptr;
}
// Add chunk to heaps freelist
void __qc_add_to_free_list(__qc_malloc_chunk *chunk) {
    chunk->free = true;
    chunk->prev_free = nullptr;
    chunk->next_free = __qc_heap.free_list;
    if (__qc_heap.free_list) __qc_heap.free_list->prev_free = chunk;
    __qc_heap.free_list = chunk;
}
char* __qc_chunk_user_data(__qc_malloc_chunk *chunk) {
    return `mapped_ptr(`to_address(chunk) + sizeof "__qc_malloc_chunk");
}
// Split chunk to get wanted size.
void __qc_split_chunk(__qc_malloc_chunk *chunk, addr_t wanted) {
    if (chunk->size < wanted + sizeof "__qc_malloc_chunk" + 16) return;
    addr_t remaining = chunk->size - wanted - sizeof "__qc_malloc_chunk";
    __qc_malloc_chunk *new_chunk = `mapped_ptr(`to_address(chunk->user_data) + wanted);
    new_chunk->user_data = __qc_chunk_user_data(new_chunk);
    new_chunk->size = remaining;
    new_chunk->free = true;
    new_chunk->prev_phys = chunk;
    new_chunk->next_phys = chunk->next_phys;
    if (chunk->next_phys) chunk->next_phys->prev_phys = new_chunk;
    chunk->next_phys = new_chunk;
    chunk->size = wanted;
    chunk->free = false;
    __qc_add_to_free_list(new_chunk);
} // Fun Fact: malloc.c has a 1 : 1 : 1 code to whitespace to comment ratio. (around 2000 : 2000 : 2000)
// Self explanitory
__qc_malloc_chunk* __qc_get_chunk_header(void *ptr) {
    return `mapped_ptr(`to_address(ptr) - sizeof "__qc_malloc_chunk");
}
// Self explanitory
__qc_malloc_chunk* __qc_get_next_physical_chunk(__qc_malloc_chunk *chunk) {
    return chunk->next_phys;
}
// Self explanitory
// Wow. C⁴ Code that follows the style conventions.
__qc_malloc_chunk* __qc_get_previous_physical_chunk(__qc_malloc_chunk *chunk) {
    return chunk->prev_phys;
}
// Self explanitory
void __qc_merge_chunks(__qc_malloc_chunk *a, __qc_malloc_chunk *b) {
    __qc_malloc_chunk *successor = b->next_phys;
    a->size += sizeof "__qc_malloc_chunk";
    a->size += b->size;
    a->free = true;
    a->next_phys = successor;
    if (successor) successor->prev_phys = a;
    b->prev_phys = nullptr;
    b->next_phys = nullptr;
}
// Alignment helper
addr_t __qc_page_align(addr_t size) {
    addr_t page = 4096;
    return (size + page - 1) & ~(page - 1);
}
// Allocate memory
addr_t __qc_system_alloc(addr_t size) {
    size = __qc_page_align(size);
    addr_t result;
    // How does one format inline asm calls.
    `inline(R"(
            mov rax, 9
            mov rdi, 0
            mov rsi, $1r
            mov rdx, 3
            mov r10, 0x22
            mov r8, -1
            mov r9, 0
            syscall
            mov $0=r, rax
        )", result, size, "~{rax,rdi,rsi,rdx,r8,r9,r10,r11,rcx,memory}");
    if (result > (~0a - 4096a)) return 0a;
    __qc_heap.system_mem += size;
    return result;
}
void* qc_malloc(addr_t request_size) {
    if (request_size == 0a) return nullptr;
    addr_t size = __qc_align(request_size);
    // Search free first
    __qc_malloc_chunk *chunk = __qc_heap.free_list;
    while (chunk) {
        if (chunk->free && chunk->size >= size) {
            if (chunk->prev_free) chunk->prev_free->next_free = chunk->next_free;
            else __qc_heap.free_list = chunk->next_free;
            if (chunk->next_free) chunk->next_free->prev_free = chunk->prev_free;
            chunk->next_free = nullptr;
            chunk->prev_free = nullptr;
            // If the size of chunk is much bigger than size. (entierly arbatrary)
            if (chunk->size >= size + 128) {
                __qc_split_chunk(chunk, size);
            }
            chunk->free = false;
            return chunk->user_data;
        }
        chunk = chunk->next_free;
    }
    if (__qc_heap.top_chunk && __qc_heap.top_chunk->size >= size + sizeof "__qc_malloc_chunk") {
        chunk = __qc_heap.top_chunk;
        addr_t remaining = chunk->size - size - sizeof "__qc_malloc_chunk";
        chunk->size = size;
        chunk->free = false;
        if (remaining >= 16) {
            __qc_malloc_chunk *new_top = `mapped_ptr(`to_address(chunk->user_data) + size);
            new_top->user_data = __qc_chunk_user_data(new_top);
            new_top->size = remaining;
            new_top->free = true;
            new_top->prev_phys = chunk;
            new_top->next_phys = nullptr;
            new_top->prev_free = nullptr;
            new_top->next_free = nullptr;
            chunk->next_phys = new_top;
            __qc_heap.top_chunk = new_top;
        } else {
            __qc_heap.top_chunk = nullptr;
        }
        return chunk->user_data;
    }
    // Need more memory
    addr_t grow_size = 4096;
    if (grow_size < sizeof "__qc_malloc_chunk" + size) grow_size = sizeof "__qc_malloc_chunk" + size;
    addr_t memory = __qc_system_alloc(grow_size);
    if (memory == 0a) return nullptr;
    __qc_malloc_chunk *new_chunk = `mapped_ptr(memory);
    new_chunk->user_data = __qc_chunk_user_data(new_chunk);
    new_chunk->size = size;
    new_chunk->free = false;
    new_chunk->prev_phys = nullptr;
    new_chunk->next_phys = nullptr;
    new_chunk->prev_free = nullptr;
    new_chunk->next_free = nullptr;
    __qc_malloc_chunk *last = nullptr;
    if (__qc_heap.first_chunk == nullptr) {
        __qc_heap.first_chunk = new_chunk;
    } else {
        if (__qc_heap.top_chunk) {
            last = __qc_heap.top_chunk->prev_phys;
            __qc_heap.top_chunk = nullptr;
        } else {
            last = __qc_heap.first_chunk;
            while (last->next_phys)
                last = last->next_phys;
        }
        last->next_phys = new_chunk;
        new_chunk->prev_phys = last;
    }
    addr_t remaining = grow_size - sizeof "__qc_malloc_chunk" - size;
    if (remaining >= 16) {
        __qc_malloc_chunk *top = `mapped_ptr(`to_address(new_chunk->user_data) + size);
        top->user_data = __qc_chunk_user_data(top);
        top->size = remaining;
        top->free = true;
        top->prev_phys = new_chunk;
        top->next_phys = nullptr;
        top->prev_free = nullptr;
        top->next_free = nullptr;
        __qc_heap.top_chunk = top;
        new_chunk->next_phys = top;
    }
    return new_chunk->user_data;
}
void qc_free(void *ptr) {
    if (ptr == nullptr) return;
    __qc_malloc_chunk *chunk = __qc_get_chunk_header(ptr);
    if (chunk->free) {
        `inline(R"(
            mov rax, 1
            mov rdi, 2
            mov rsi, $0r
            mov rdx, 25
            syscall
        )", "Attempted to double free\n", "~{rax}~{rdi}~{rsi}~{rdx}");
        return;
    }
    chunk->free = true;
    __qc_malloc_chunk *previous = __qc_get_previous_physical_chunk(chunk);
    if (previous && previous->free) {
        if (previous->prev_free) previous->prev_free->next_free = previous->next_free;
        else __qc_heap.free_list = previous->next_free;
        if (previous->next_free) previous->next_free->prev_free = previous->prev_free;
        previous->next_free = nullptr;
        previous->prev_free = nullptr;
        __qc_merge_chunks(previous, chunk);
        chunk = previous;
    }
    __qc_malloc_chunk *next = __qc_get_next_physical_chunk(chunk);
    if (next && next->free && next != __qc_heap.top_chunk) {
        if (next->prev_free) next->prev_free->next_free = next->next_free;
        else __qc_heap.free_list = next->next_free;
        if (next->next_free) next->next_free->prev_free = next->prev_free;
        next->next_free = nullptr;
        next->prev_free = nullptr;
        __qc_merge_chunks(chunk, next);
    }
    next = __qc_get_next_physical_chunk(chunk);
    if (next != nullptr && next == __qc_heap.top_chunk && next->free) {
        chunk->size += sizeof "__qc_malloc_chunk" + next->size;
        chunk->next_phys = next->next_phys;
        if (next->next_phys) next->next_phys->prev_phys = chunk;
        __qc_heap.top_chunk = chunk;
        chunk->prev_free = nullptr;
        chunk->next_free = nullptr;
        return;
    }
    __qc_add_to_free_list(chunk);
}
void *qc_calloc(addr_t count, addr_t size) {
    if (count != 0 && size > MAX_ADDR / count) return nullptr;
    addr_t total = count * size;
    void *ptr = `malloc(total);
    if (!ptr) return nullptr;
    ptr = qc_memset(ptr, 0, total);
    return ptr;
}
void *qc_realloc(void *ptr, addr_t new_size) {
    if (!ptr) {
        return `malloc(new_size);
    }
    if (new_size == 0a) {
        `free(ptr);
        return nullptr;
    }
    __qc_malloc_chunk *old_chunk = __qc_get_chunk_header(ptr);
    addr_t old_size = old_chunk->size;
    new_size = __qc_align(new_size);
    if (new_size <= old_size) {
        __qc_split_chunk(old_chunk, new_size);
        return ptr;
    }
    __qc_malloc_chunk *next = __qc_get_next_physical_chunk(old_chunk);
    if (next && next->free && old_size + sizeof "__qc_malloc_chunk" + next->size >= new_size) {
        if (next->prev_free) next->prev_free->next_free = next->next_free;
        else __qc_heap.free_list = next->next_free;
        if (next->next_free) next->next_free->prev_free = next->prev_free;
        next->next_free = nullptr;
        next->prev_free = nullptr;
        __qc_merge_chunks(old_chunk, next);
        if (old_chunk->size >= new_size + 128) __qc_split_chunk(old_chunk, new_size);
        return ptr;
    }
    void *new_ptr = `malloc(new_size);
    if (!new_ptr) {
        return nullptr;
    }
    __qc_memcpy(new_ptr, ptr, old_size);
    `free(ptr);
    return new_ptr;
}
///////////////////
// STRING ////////
/////////////////

addr_t qc_len(char* str) {
    addr_t i = 0;
    while (str[i] != '\0') {
        ++i;
    }
    return i;
}
bool qc_string_eq(string a, string b) {
    if (a == nullptr || b == nullptr)
        return a == b;
    addr_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i])
            return false;
        i++;
    }
    return a[i] == b[i];
}
string qc_string_concat(string a, string b) {
    if (!a) a = "";
    if (!b) b = "";
    addr_t len_a = `len(a);
    addr_t len_b = `len(b);

    string result = `malloc(len_a + len_b + 1);
    if (!result) return nullptr;
    __qc_memcpy(result, a, len_a);
    __qc_memcpy(result + len_a, b, len_b);
    result[len_a + len_b] = '\0';
    return result;
} 
bool qc_contains(string str, string sub) {
    if (!str || !sub) return false;
    if (*sub == '\0') {
        return true;
    }
    for (int i = 0; str[i] != '\0'; i++) {
        int j = 0;
        while (str[i + j] == sub[j]) {
            j++;
            if (sub[j] == '\0') {
                return true;
            }
            if (str[i + j] == '\0') {
                break;
            }
        }
    }
    return false;
}
string qc_to_lower(string str) {
    if (!str) return nullptr;
    addr_t len = `len(str);
    string result = `malloc(len + 1);
    if (!result) return nullptr;
    for (addr_t i = 0; i < len; i++) {
        char c = str[i];
        if (c >= 'A' && c <= 'Z') {
            result[i] = `to_char((c + 32));
        } else {
            result[i] = c;
        }
    }
    result[len] = '\0';
    return result;
}

string qc_to_upper(string str) {
    if (!str) return nullptr;
    addr_t len = `len(str);
    string result = `malloc(len + 1);
    if (!result) return nullptr;
    for (addr_t i = 0; i < len; i++) {
        char c = str[i];
        if (c >= 'a' && c <= 'z') {
            result[i] = `to_char((c - 32));
        } else {
            result[i] = c;
        }
    }
    result[len] = '\0';
    return result;
}
string qc_substring(string str, int start, int length) {
    if (!str) return nullptr;
    int len = `len(str);
    if (start < 0 || start >= len || length <= 0) {
        string empty = `malloc(1);
        empty[0] = '\0';
        return empty;
    }
    if (start + length > len) { length = len - start; }
    string result = `malloc(length + 1);
    if (!result) return nullptr;
    __qc_memcpy(result, str + start, length);
    result[length] = '\0';
    return result;
}
bool qc_startswith(string str, string prefix) {
    if (!str || !prefix) return false;
    addr_t i = 0;
    while (prefix[i] != '\0') {
        if (str[i] == '\0' || str[i] != prefix[i]) {
            return false;
        }
        i++;
    }
    return true;
}
bool qc_endswith(string str, string suffix) {
    if (!str || !suffix) return false;
    addr_t str_len = `len(str);
    addr_t suf_len = `len(suffix);
    if (suf_len > str_len) return false;
    addr_t start_idx = str_len - suf_len;
    for (addr_t i = 0; i < suf_len; i++) {
        if (str[start_idx + i] != suffix[i]) {
            return false;
        }
    }
    return true;
}

bool __qc_is_space(char c) {
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

string qc_trim(string str) {
    if (!str) return nullptr;
    addr_t start = 0;
    while (str[start] != '\0' && __qc_is_space(str[start])) {
        start++;
    }
    if (str[start] == '\0') {
        string empty = `malloc(1);
        if (!empty) return nullptr;
        empty[0] = '\0';
        return empty;
    }
    addr_t end = `len(str) - 1;
    while (end > start && __qc_is_space(str[end])) {
        end--;
    }
    addr_t len = end - start + 1;
    string result = `malloc(len + 1);
    if (!result) return nullptr;
    for (addr_t i = 0; i < len; i++) {
        result[i] = str[start + i];
    }
    result[len] = '\0';
    return result;
}

string qc_replace(string str, string find, string replace) {
    if (!str || !find || !replace) return nullptr;
    addr_t str_len = `len(str);
    addr_t find_len = `len(find);
    addr_t replace_len = `len(replace);
    if (find_len == 0) {
        string dup = `malloc(str_len + 1);
        if (!dup) return nullptr;
        for (addr_t i = 0a; i <= str_len; i++) dup[i] = str[i];
        return dup;
    }
    int count = 0;
    if (find_len > str_len) {
        string dup = `malloc(str_len + 1);
        if (!dup) return nullptr;
        for (addr_t i = 0a; i <= str_len; i++) dup[i] = str[i];
        return dup;
    }
    addr_t last_start = str_len - find_len;
    for (addr_t i = 0a; i <= last_start; ) {
        int match = 1;
        for (addr_t j = 0a; j < find_len; j++) {
            if (str[i + j] != find[j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            count++;
            i += find_len;
        } else {
            i++;
        }
    }
    if (count == 0) {
        string dup = `malloc(str_len + 1);
        if (!dup) return nullptr;
        for (addr_t i = 0a; i <= str_len; i++) dup[i] = str[i];
        return dup;
    }
    addr_t result_len = str_len + (count * replace_len) - (count * find_len);
    string result = `malloc(result_len + 1);
    if (!result) return nullptr;
    addr_t src_idx = 0;
    addr_t dst_idx = 0;
    while (src_idx < str_len) {
        int match = 0;
        if (src_idx <= last_start) {    
            match = 1;
            for (addr_t j = 0a; j < find_len; j++) {
                if (str[src_idx + j] != find[j]) {
                    match = 0;
                    break;
                }
            }
        }
        if (match) {
            for (addr_t j = 0a; j < replace_len; j++) {
                result[dst_idx] = replace[j];
                dst_idx++;
            }
            src_idx += find_len;
        } else {
            result[dst_idx] = str[src_idx];
            dst_idx++;
            src_idx++;
        }
    }
    result[dst_idx] = '\0';
    return result;
}

///////////////////
// INTEGER ///////
/////////////////

int qc_powi_i32(int base, int exp) {
    if (exp < 0) return 0;
    int result = 1;
    while (exp > 0) {
        if (exp & 1) result *= base;
        base *= base;
        exp = exp |> 1;
    }
    return result;
}
//////////////////
// MEM STUFF ////
////////////////
string __qc_strdup(string src) {
    if (src == nullptr) {
        return nullptr;
    }
    addr_t len = `len(src) + 1; 
    char *dst = `malloc(len); 
    if (dst == nullptr) {
        return nullptr; 
    }
    return __qc_memcpy(dst, src, len); 
}
//////////////////
// FILES ////////
////////////////
const int O_WRONLY = 1;
const int O_CREAT  = 64;
const int O_TRUNC  = 512;
const int O_APPEND = 2048;
const int O_RDWR   = 2;
const int O_RDONLY = 0;

int qc_open(string path, string permission) {
    int fgs = 0;
    if (permission == "r") {
        fgs = O_RDONLY;
    } else if (permission == "w") {
        fgs = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (permission == "a") {
        fgs = O_WRONLY | O_CREAT | O_APPEND;
    } else if (permission == "rw") {
        fgs = O_RDWR;
    } else if (permission == "r+") {
        fgs = O_RDWR;
    } else if (permission == "w+") {
        fgs = O_RDWR | O_CREAT | O_TRUNC;
    } else if (permission == "a+") {
        fgs = O_RDWR | O_CREAT | O_APPEND;
    } else {
        return -1;
    }
    int res = 0;
    `inline(R"(
    mov rax, 257
    mov rdi, -100
    mov rsi, $1r
    mov edx, $2r
    mov r10d, $3r
    syscall
    mov $0=r, eax
    )", res, path, fgs, `ternary(fgs & O_CREAT, 0o0, 0o0644), "~{rax}~{rdi}~{rsi}~{rdx}~{r10}");
    if (res < 0) {
        return -1;
    }
    return res;
}
void qc_close(int fd) {
    `inline(R"(
    mov rax, 3
    mov edi, $0r
    syscall
    )", fd, "~{rax}~{rdi}");
}

long int qc_read(int fd, char *buffer, long int size) {
    long int n = 0l;
    `inline(R"(
        mov rax, 0
        mov edi, $1r
        mov rsi, $2r
        mov rdx, $3r
        syscall
        mov $0=r, rax
    )", n, fd, buffer, size, "~{rax}~{rdi}~{rsi}~{rdx}");
    return n;
}
long int qc_write(int fd, string data) {
    long int n = 0l;
    `inline(R"(
        mov rax, 1
        mov edi, $1r 
        mov rsi, $2r
        mov rdx, $3r
        syscall
        mov $0=r, rax
    )", n, fd, data, `len(data), "~{rax}~{rdi}~{rsi}~{rdx}~{rcx}~{r11}");
    return n;
}
//////////////////
// DIRECT I/O ///
////////////////
const addr_t __QC_BUF_CAP = 4096a;
char[4096] __qc_out_buffer = ['\0'];
addr_t __qc_out_len = 0a;
int __qc_write_to = 1;
void qc_flush() {
    if (__qc_out_len == 0a) return;
    __qc_out_buffer[__qc_out_len] = '\0';
    qc_write(__qc_write_to, __qc_out_buffer);
    __qc_out_len = 0a;
}
void qc_print_string(string value) {
    if (!value) return;
    if (__qc_write_to != 1) { `write(__qc_write_to, value); return; }
    addr_t n = `len(value);
    if (n >= __QC_BUF_CAP - 1a) { `flush(); `write(__qc_write_to, value); return; }
    if (__qc_out_len + n >= __QC_BUF_CAP - 1a) `flush();
    __qc_memcpy(&__qc_out_buffer[__qc_out_len], value, n);
    __qc_out_len += n;
    if (`contains(value, "\n")) `flush();
}
string qc_qin() {
    string result = "";
    while (true) {
        char c;
        long int n = qc_read(0, &c, 1);
        if (n <= 0) break;
        if (c == '\n') break;
        result += c;
    }
    return result;
}
//////////////////
// X_TO_STRING //
////////////////
void __qc_reverse(char* str, int length) {
    int left = 0;
    int right = length - 1;
    while (left < right) {
        char temp = str[left];
        str[left] = str[right];
        str[right] = temp;

        left++;
        right--;
    }
}
string __qc_to_string_long_int_helper(long int x) {
    char[64] buf = ['\0'];
    int index = 0;
    bool negative = x < 0;
    addr_t value;
    if (negative) {
        value = 0a - `to_addr_t(x);    
    } else {
        value = `to_addr_t(x);
    }
    if (value == 0) {
        buf[index++] = '0';
    } else {
        while (value != 0) {
            int digit = value % 10;
            buf[index++] = '0' + digit;
            value /= 10;
        }
    }
    if (negative) {
        buf[index++] = '-';
    }
    __qc_reverse(buf, index);
    char* output = `malloc(index + 1);
    if (!output) return nullptr;
    __qc_memcpy(output, buf, index);
    output[index] = '\0';
    return output;
}
string __qc_to_string_addr_t(addr_t x) {
    char[64] buf = ['\0'];
    int index = 0;
    if (x == 0) {
        buf[index++] = '0';
    } else {
        while (x != 0) {
            int digit = x % 10;
            buf[index++] = '0' + digit;
            x /= 10;
        }
    }
    __qc_reverse(buf, index);
    char* output = `malloc(index + 1);
    if (!output) return nullptr;
    __qc_memcpy(output, buf, index);
    output[index] = '\0';
    return output;
}
string qc_to_string_nibble(nibble x) {
    return __qc_to_string_addr_t(x);
}
string qc_to_string_byte(byte x) {
    return __qc_to_string_addr_t(x);
}
string qc_to_string_long_int(long int x) {
    return __qc_to_string_long_int_helper(x);
}
string qc_to_string_int(int x) {
    return __qc_to_string_long_int_helper(x);
}
string qc_to_string_short_int(short int x) {
    return __qc_to_string_long_int_helper(x);
}
string qc_to_string_float(float x) {
    char[64] buf = ['\0'];
    int index = 0;
    bool negative = x < 0;
    if (negative) {
        x = -x;
        buf[index++] = '-';
    }
    int whole = `to_int(x);
    float fraction = x - whole;
    string whole_string = qc_to_string_int(whole);
    int whole_len = `len(whole_string);
    __qc_memcpy(buf + index, whole_string, whole_len);
    index += whole_len;
    if (fraction != 0) {
        buf[index++] = '.';
        for (int i = 0; i < 6; i++) {
            fraction *= 10;
            int digit = `to_int(fraction);
            buf[index++] = '0' + digit;
            fraction -= digit;
        }
        while (index > 0 && buf[index - 1] == '0') {
            index--;
        }
        if (index > 0 && buf[index - 1] == '.') {
            index--;
        }
    }
    char* output = `malloc(index + 1);
    if (!output) return nullptr;
    __qc_memcpy(&output[0], buf, index);
    output[index] = '\0';
    return output;
}
string qc_to_string_double(double x) {
    char[96] buf = ['\0'];
    int index = 0;
    bool negative = x < 0;
    if (negative) {
        x = -x;
        buf[index++] = '-';
    }
    long int whole = `to_long_int(x);
    double fraction = x - whole;
    string whole_string = qc_to_string_long_int(whole);
    int whole_len = `len(whole_string);
    __qc_memcpy(buf + index, whole_string, whole_len);
    index += whole_len;
    if (fraction != 0) {
        buf[index++] = '.';
        for (int i = 0; i < 14; i++) {
            fraction *= 10;
            int digit = `to_int(fraction);
            buf[index++] = '0' + digit;
            fraction -= digit;
        }
        while (index > 0 && buf[index - 1] == '0') {
            index--;
        }
        if (index > 0 && buf[index - 1] == '.') {
            index--;
        }
    }
    char* output = `malloc(index + 1);
    if (!output) return nullptr;
    __qc_memcpy(output, buf, index);
    output[index] = '\0';
    return output;
}
string qc_to_string_bool(bool b) {
    string s = `ternary(b, "true", "false");
    addr_t len = `len(s);
    string output = `malloc(len + 1);
    if (!output) return nullptr;
    __qc_memcpy(output, s, len + 1);
    return output;
}
string qc_to_string_qbool(qbool q) {
    string s;
    switch (`to_int(q) & 0x3i) {
    case 0: s = "none"; break;
    case 1: s = "qfalse"; break;
    case 2: s = "qtrue"; break;
    case 3: s = "both"; break;
    default: s = "none"; break;
    }
    addr_t len = `len(s);
    string output = `malloc(len + 1);
    if (!output) return nullptr;
    __qc_memcpy(output, s, len + 1);
    return output;
}
string qc_to_string_char(char c) {
    string output = `malloc(2);
    if (!output) return nullptr;
    output[0] = c;
    output[1] = '\0';
    return output;
}
//////////////////
// FMT //////////
////////////////
string qc_fmt_int(long int v, int width, int precision, bool zero_pad) {
    bool is_negative = false;
    if (v < 0) {
        is_negative = true;
        v = -v;
    }
    string stringified = `to_string(v);
    while (`len(stringified) < precision) {
        stringified = "0" + stringified;
    }
    int target_width = width;
    if (is_negative) {
        target_width = width - 1;
    }
    while (`len(stringified) < target_width) {
        if (zero_pad) {
            stringified = "0" + stringified;
        } else {
            stringified = " " + stringified;
        }
    }
    if (is_negative) {
        stringified = "-" + stringified;
    }
    return stringified;
}
string qc_fmt_unsigned_int(addr_t v, int width, int precision, bool zero_pad) {
    string stringified = __qc_to_string_addr_t(v);
    while (`len(stringified) < precision) {
        stringified = "0" + stringified;
    }
    while (`len(stringified) < width) {
        if (zero_pad) {
            stringified = "0" + stringified;
        } else {
            stringified = " " + stringified;
        }
    }
    return stringified;
}
string qc_fmt_scientific(double v, int width, int precision, bool zero_pad) {
    bool is_negative = false;
    if (v < 0.0) {
        is_negative = true;
        v = -v;
    }
    int exponent = 0;
    if (v > 0.0) {
        while (v >= 10.0) {
            v = v / 10.0;
            exponent = exponent + 1;
        }
        while (v < 10.0 && v < 1.0) {
            v = v * 10.0;
            exponent = exponent - 1;
        }
    }
    long int int_part = `to_long_int(v);
    double frac_raw = v - int_part;
    string frac_part = "";
    int p = 0;
    while (p < precision) {
        frac_raw = frac_raw * 10.0;
        long int digit = `to_long_int(frac_raw);
        frac_part = frac_part + `to_string(digit);
        frac_raw = frac_raw - digit;
        p = p + 1;
    }
    string stringified = `to_string(int_part);
    if (precision > 0) {
        stringified = stringified + "." + frac_part;
    }
    string exp_sign = "+";
    int abs_exponent = exponent;
    if (exponent < 0) {
        exp_sign = "-";
        abs_exponent = -exponent;
    }
    string exp_str = `to_string(abs_exponent);
    if (`len(exp_str) < 2) {
        exp_str = "0" + exp_str;
    }
    stringified = stringified + "e" + exp_sign + exp_str;
    if (is_negative && zero_pad) {
        stringified = "-" + stringified;
        is_negative = false;
    }
    while (`len(stringified) < width) {
        if (zero_pad) { stringified = "0" + stringified; }
        else { stringified = " " + stringified; }
    }
    if (is_negative) {
        stringified = "-" + stringified;
    }
    return stringified;
}


string qc_fmt_float(double v, int width, int precision, bool zero_pad) {
    bool is_negative = false;
    if (v < 0.0) {
        is_negative = true;
        v = -v;
    }
    string stringified = `to_string(v);
    int dot_pos = -1;
    int i = 0;
    while (i < `len(stringified)) {
        if (`substring(stringified, i, 1) == ".") {
            dot_pos = i;
        }
        i = i + 1;
    }
    if (dot_pos == -1) {
        dot_pos = `len(stringified);
        stringified = stringified + ".";
    }
    string int_part = `substring(stringified, 0, dot_pos);
    string frac_part = `substring(stringified, dot_pos + 1, `len(stringified) - dot_pos - 1);
    if (precision == 0) {
        stringified = int_part;
    } else {
        if (`len(frac_part) > precision) {
            frac_part = `substring(frac_part, 0, precision);
        }
        while (`len(frac_part) < precision) {
            frac_part = frac_part + "0";
        }
        stringified = int_part + "." + frac_part;
    }
    if (is_negative && zero_pad) {
        stringified = "-" + stringified;
        is_negative = false; 
    }
    while (`len(stringified) < width) {
        if (zero_pad) {
            stringified = "0" + stringified;
        } else {
            stringified = " " + stringified;
        }
    }
    if (is_negative) {
        stringified = "-" + stringified;
    }
    return stringified;
}



string qc_fmt_double(double v, int width, int precision, bool zero_pad) {
    return qc_fmt_float(v, width, precision, zero_pad);
}
string qc_fmt_char(char v, int width, bool zero_pad) {
    string stringified = `to_string(v); 
    while (`len(stringified) < width) {
        if (zero_pad) {
            stringified = "0" + stringified;
        } else {
            stringified = " " + stringified;
        }
    }
    return stringified;
}
string qc_fmt_string(string v, int width, bool zero_pad) {
    while (`len(v) < width) {
        if (zero_pad) {
            v = "0" + v;
        } else {
            v = " " + v;
        }
    }
    return v;
}
string qc_fmt_hex(addr_t v, int width, bool zero_pad) {
    if (v == 0) {
        string stringified = "0x0";
        while (`len(stringified) < width) {
            if (zero_pad) { stringified = "0" + stringified; }
            else { stringified = " " + stringified; }
        }
        return stringified;
    }
    string stringified = "";
    string hex_digits = "0123456789abcdef";
    while (v > 0) {
        addr_t remainder = v % 16;
        stringified = `substring(hex_digits, remainder, 1) + stringified;
        v = v / 16;
    }
    if (zero_pad) {
        stringified = "0x" + stringified;
    }
    int target_width = width;
    if (!zero_pad) {
        target_width = width - 2;
    }
    while (`len(stringified) < target_width) {
        if (zero_pad) { stringified = "0" + stringified; }
        else { stringified = " " + stringified; }
    }
    if (!zero_pad) {
        stringified = "0x" + stringified;
    }
    return stringified;
}
string qc_fmt_octal(addr_t v, int width, bool zero_pad) {
    if (v == 0) {
        string stringified = "0o0";
        while (`len(stringified) < width) {
            if (zero_pad) { stringified = "0" + stringified; }
            else { stringified = " " + stringified; }
        }
        return stringified;
    }
    string stringified = "";
    string oct_digits = "01234567";
    while (v > 0) {
        addr_t remainder = v % 8;
        stringified = `substring(oct_digits, remainder, 1) + stringified;
        v = v / 8;
    }
    if (zero_pad) {
        stringified = "0o" + stringified;
    }
    int target_width = width;
    if (!zero_pad) {
        target_width = width - 2;
    }
    while (`len(stringified) < target_width) {
        if (zero_pad) { stringified = "0" + stringified; }
        else { stringified = " " + stringified; }
    }
    if (!zero_pad) {
        stringified = "0o" + stringified;
    }
    return stringified;
}
string qc_fmt_bool(bool v, int width, bool zero_pad) {
    return `to_string(v);
}
string qc_fmt_qbool(qbool q, int width, bool zero_pad) {
    return `to_string(q);
}
string qc_fmt_ptr(void *v, int width, bool zero_pad) {
    string stringified = "";
    if (v == 0) {
        stringified = "0";
    } else {
        string hex_digits = "0123456789abcdef";
        addr_t temp = `to_address(v);
        while (temp > 0) {
            addr_t remainder = temp % 16;
            stringified = `substring(hex_digits, remainder, 1) + stringified;
            temp = temp / 16;
        }
    }
    if (zero_pad) {
        stringified = "0x" + stringified;
    }
    while (`len(stringified) < width) {
        if (zero_pad) { stringified = "0" + stringified; }
        else { stringified = " " + stringified; }
    }
    if (!zero_pad) {
        stringified = "0x" + stringified;
    }
    return stringified;
}
//////////////////
// RANDOM ///////
////////////////
addr_t __qc_current_seed = 0a;
addr_t __qc_s0 = 0a;
addr_t __qc_s1 = 0a;
addr_t __qc_s2 = 0a;
addr_t __qc_s3 = 0a;
struct __qc_timespec {
    addr_t tv_sec;
    addr_t tv_nsec;
};
addr_t __qc_randnext() {
    addr_t result = (__qc_s1 * 5 <<< 7) * 9;
    addr_t t = __qc_s1 << 17;
    __qc_s2 = __qc_s2 $ __qc_s0;
    __qc_s3 = __qc_s3 $ __qc_s1;
    __qc_s1 = __qc_s1 $ __qc_s2;
    __qc_s0 = __qc_s0 $ __qc_s3;
    __qc_s2 = __qc_s2 $ t;
    __qc_s3 = __qc_s3 <<< 45;
    return result;
}

addr_t qc_time() {
    __qc_timespec timespec = {0a, 0a};  
    `inline(R"(
    mov rax, 228
    mov rdi, 0
    lea rsi, $0=m
    syscall   
    )", timespec, "~{rax,rdi,rsi,rcx,r11,memory}");
    return timespec.tv_sec;
}
addr_t __qc_splitmix(addr_t *x) {
    *x += 0x9E3779B97F4A7C15;
    addr_t z = *x;
    z = (z $ (z |> 30)) * 0xBF58476D1CE4E5B9;
    z = (z $ (z |> 27)) * 0x94D049BB133111EB;
    return z $ (z |> 31);
}
void qc_seed(addr_t seed) {
    __qc_current_seed = seed;

    __qc_s0 = __qc_splitmix(&seed);
    __qc_s1 = __qc_splitmix(&seed);
    __qc_s2 = __qc_splitmix(&seed);
    __qc_s3 = __qc_splitmix(&seed);
}
int qc_random_int(int max) {
    return `to_int(__qc_randnext() % max);
}
float qc_random_float() {
    return `to_float(__qc_randnext() |> 40) / `to_float(1 << 24);
}
int qc_random_range(int min, int max) {
    return min + `to_int(__qc_randnext() % `to_addr_t(max - min));
}
//////////////////
// TO_FROM_STR //
////////////////
bool __qc_is_digit(char c) {
    return c >= '0' && c <= '9';
}
bool __qc_is_binary(char c) {
    return c == '0' || c == '1';
}
bool __qc_is_hex(char c) {
    return __qc_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
bool __qc_is_octal(char c) {
    return c >= '0' && c <= '7';
}
long int qc_to_long_int_from_string(string str) {
    long int result = 0l;
    bool is_negative = false;
    int base = 10;
    int start = 0;
    if (`len(str) == 0) return 0l;
    if (str[0] == '-') {
        is_negative = true;
        start = 1;
    }
    if (`len(str) >= start + 2 && str[start] == '0') {
        if (str[start + 1] == 'x' || str[start + 1] == 'X') {
            base = 16;
            start += 2;
        } else if (str[start + 1] == 'b' || str[start + 1] == 'B') {
            base = 2;
            start += 2;
        } else if (str[start + 1] == 'o' || str[start + 1] == 'O') {
            base = 8;
            start += 2;
        }
    }
    for (int i = start; i < `len(str); i++) {
        if (str[i] == '\'') continue;
        result *= base;
        if (base == 2) {
            if (!__qc_is_binary(str[i])) return 0l;
            result += str[i] - '0';
        } else if (base == 8) {
            if (!__qc_is_octal(str[i])) return 0l;
            result += str[i] - '0';
        } else if (base == 10) {
            if (!__qc_is_digit(str[i])) return 0l;
            result += str[i] - '0';
        } else {
            if (!__qc_is_hex(str[i])) return 0l;
            if (__qc_is_digit(str[i])) result += str[i] - '0';
            else if (str[i] >= 'a') result += str[i] - 'a' + 10;
            else result += str[i] - 'A' + 10;
        }
    }
    if (is_negative) result = -result;
    return result;
}
int qc_to_int_from_string(string str) {
    return `to_int(qc_to_long_int_from_string(str));
}
short int qc_to_short_int_from_string(string str) {
    return `to_short_int(qc_to_long_int_from_string(str));
}
addr_t qc_to_addr_t_from_string(string str) {
    return qc_to_long_int_from_string(str);
}
byte qc_to_byte_from_string(string str) {
    return `to_byte(qc_to_long_int_from_string(str));
}
nibble qc_to_nibble_from_string(string str) {
    return `to_nibble(qc_to_long_int_from_string(str));
}
double qc_to_double_from_string(string str) {
    if (!str) return 0.0;
    double result = 0.0;
    double fraction = 0.0;
    double divisor = 10.0;
    bool negative = false;
    bool exponent_negative = false;
    int i = 0;
    if (str[i] == '-') {
        negative = true;
        i++;
    } else if (str[i] == '+') {
        i++;
    }
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10.0 + (str[i] - '0');
        i++;
    }
    if (str[i] == '.') {
        i++;
        while (str[i] >= '0' && str[i] <= '9') {
            fraction += (str[i] - '0') / divisor;
            divisor *= 10.0;
            i++;
        }
        result += fraction;
    }
    if (str[i] == 'e' || str[i] == 'E') {
        i++;
        if (str[i] == '-') {
            exponent_negative = true;
            i++;
        } else if (str[i] == '+') {
            i++;
        }
        int exponent = 0;
        while (str[i] >= '0' && str[i] <= '9') {
            exponent = exponent * 10 + (str[i] - '0');
            i++;
        }
        double multiplier = 1.0;
        while (exponent > 0) {
            multiplier *= 10.0;
            exponent--;
        }
        if (exponent_negative) result /= multiplier;
        else result *= multiplier;
    }
    if (negative) return -result;
    return result;
}
float qc_to_float_from_string(string str) {
    return `to_float(qc_to_double_from_string(str));
}
char qc_to_char_from_string(string str) {
    if (str) {
        return str[0];
    }
    return '\0';
}
//////////////////
// JAGGED ARRAY /
////////////////
addr_t qc_sizeof_type(int elem_type) {
    switch (elem_type) {
    case 0: return sizeof "int";
    case 1: return sizeof "float";
    case 2: return sizeof "double";
    case 3: return sizeof "char";
    case 4: return sizeof "bool";
    case 5: return sizeof "qbool";
    case 6: return sizeof "char*";
    default: return 4a;
    }
}
struct qc_jagged_array {
    void** data;
    int* sizes;
    int count;
    int elem_type;
    int depth;
};
qc_jagged_array* qc_create_jagged_array(int count, int elem_type, int depth) {
    qc_jagged_array* arr = `malloc(sizeof "qc_jagged_array");
    arr->data = `malloc(count * sizeof "void*");
    arr->sizes = `malloc(count * sizeof "int");
    arr->count = count;
    arr->elem_type = elem_type;
    arr->depth = depth;
    return arr;
}

void qc_set_jagged_element(qc_jagged_array* arr, int idx, void* elem, int size) {
    arr->data[idx] = elem;
    arr->sizes[idx] = size;
}
void qc_free_jagged_array(qc_jagged_array* arr) {
    if (arr->depth > 0) {
        for (int i = 0; i < arr->count; i++) { qc_free_jagged_array(arr->data[i]); }
    } else {
        for (int i = 0; i < arr->count; i++) { `free(arr->data[i]); }
    }
    `free(arr->data);
    `free(arr->sizes);
    `free(arr);
}
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
            char* row = arr->data[idx];
            return row + (elem_idx * qc_sizeof_type(arr->elem_type));
        }
    } else if (arr->depth == 1) {
        char* row = arr->data[idx];
        if (num_indices == 1) {
            return row;
        } else {
            int elem_idx = indices[1];
            if (elem_idx < 0 || elem_idx >= arr->sizes[idx]) { return nullptr; }
            return row + (elem_idx * qc_sizeof_type(arr->elem_type));
        }
    } else {
        qc_jagged_array* sub = arr->data[idx];
        if (!sub) return nullptr;
        return qc_jagged_array_get(sub, indices + 1, num_indices - 1);
    }
}
void* qc_create_leaf_row(int size, int elem_type) {
    int elem_size = qc_sizeof_type(elem_type);
    int total = size * elem_size;
    void* row = `malloc(total);
    return row;
}
void qc_set_leaf_element(void* row, int idx, void* value, int elem_type) {
    char* r = row;
    void* dest = r + (idx * qc_sizeof_type(elem_type));
    __qc_memcpy(dest, value, qc_sizeof_type(elem_type));
}
:extern
