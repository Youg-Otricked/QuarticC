#nomain
extern:
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
:extern
