namespace Filesystem {
    class Path;
    Path getCwd();
    char* realpath(char* input_path) {
        char[4096] result = [char, 4096]; 
        long int ret = -1;
        #ifdef __APPLE__
        `inline(R"(
            mov x0, $1r 
            mov x1, 0 
            mov x16, 5
            svc #0
            cmp x0, #0
            blt 1f 
            mov x9, x0
            mov x0, x9
            mov x1, 50
            mov x2, $0m
            mov x16, 92 
            svc #0
            mov x10, x0 
            mov x0, x9
            mov x16, 6
            svc #0
            mov $0=r, x10
            b 2f
        1:
            mov $0=r, #-1
        2:
        )", ret, input_path, "~{x0,x1,x2,x9,x10,x16,memory}");
        #endif
        #ifdef __linux
            long int fd = -1;
            `inline(R"(
                mov rax, 2
                mov rdi, $1r
                mov rsi, 0
                syscall
                mov $0=r, rax
            )", fd, input_path, "~{rax,rdi,rsi,memory}");
            if (fd >= 0) {
                char* proc_ptr = "/proc/self/fd/";
                int offset = 14; 
                int temp_fd = fd;
                char[16] rev_digits = [char, 16];
                int digit_idx = 0;
                if (temp_fd == 0) {
                    proc_ptr[offset++] = '0';
                } else {
                    while (temp_fd > 0) {
                        rev_digits[digit_idx++] = '0' + (temp_fd % 10);
                        temp_fd /= 10;
                    }
                    while (digit_idx > 0) {
                        proc_ptr[offset++] = rev_digits[--digit_idx];
                    }
                }
                proc_ptr[offset] = '\0';
                `inline(R"(
                    mov rax, 89
                    mov rdi, $1r
                    mov rsi, $2m
                    mov rdx, 4096
                    syscall
                    mov $0=r, rax
                )", ret, proc_ptr, result, "~{rax,rdi,rsi,rdx,memory}");
                long int close_ret;
                `inline(R"(
                    mov rax, 3
                    mov rdi, $1r
                    syscall
                    mov $0=r, rax
                )", close_ret, fd, "~{rax,rdi,memory}");
                if (ret >= 0 && ret < 4096) {
                    result[ret] = '\0';
                    ret = 0;
                } else {
                    ret = -1;
                }
            }
        #endif
        if (ret < 0) {
            return "";
        }
        return result;
    }
    type DateTime = addr_t;
    enum FileType {
        Unknown = 0;
        File = 1;
        Directory = 2;
        Symlink = 3;
        Other = 4;
    }
    struct FileStatus {
        FileType filetype;
        addr_t size;
        addr_t created;
        addr_t accessed;
        addr_t modified;
        short int permissions;
        bool exists;
    }
    class Path {
        string path;
        Path(string path) {
            this->path = path;
        }
        Path operator/(string other) {
            if (`len(this->path) == 0) {
                return Path(other);
            }
            if (`len(other) == 0) {
                return Path(this->path);
            }
            if (this->path[`len(this->path) - 1] == '/') {
                if (other[0] == '/') {
                    return Path(this->path + `substring(other, 1, `len(other) - 1));
                }
                return Path(this->path + other);
            }
            if (other[0] == '/') {
                return Path(this->path + other);
            }
            return Path(this->path + "/" + other);
        }
        Path operator/(Path other) {
            return (*this) / other.path;
        }
        Path& operator=(Path other) {
            this->path = other.path;
            return *this;
        }
        Path& operator=(string other) {
            this->path = other;
            return *this;
        }
        string _repr() {
            return this->path;
        }
        string toString() {
            return this->path;
        }
        bool empty() {
            return `len(this->path) == 0;
        }
        bool isAbsolute() {
            if (`len(this->path) == 0) {
                return false;
            }
            return this->path[0] == '/';
        }
        bool isRelative() {
            return !this->isAbsolute();
        }
        Path parent() {
            if (`len(this->path) == 0) {
                return Path(".");
            }
            int end = `len(this->path) - 1;
            while (end > 0 && this->path[end] == '/') {
                end--;
            }
            while (end >= 0 && this->path[end] != '/') {
                end--;
            }
            if (end < 0) {
                return Path(".");
            }
            if (end == 0) {
                return Path("/");
            }
            return Path(`substring(this->path, 0, end));
        }
        string filename() {
            if (`len(this->path) == 0) {
                return "";
            }
            int end = `len(this->path) - 1;
            while (end >= 0 && this->path[end] == '/') {
                end--;
            }
            if (end < 0) {
                return "";
            }
            int start = end;
            while (start >= 0 && this->path[start] != '/') {
                start--;
            }
            return `substring(this->path, start + 1, end - start);
        }
        string extension() {
            string name = this->filename();
            if (`len(name) == 0) {
                return "";
            }
            int dot = `len(name) - 1;
            while (dot >= 0 && name[dot] != '.') {
                dot--;
            }
            if (dot <= 0) {
                return "";
            }
            return `substring(name, dot, `len(name) - dot);
        }
        string stem() {
            string name = this->filename();
            if (`len(name) == 0) {
                return "";
            }
            int dot = `len(name) - 1;
            while (dot >= 0 && name[dot] != '.') {
                dot--;
            }
            if (dot <= 0) {
                return name;
            }
            return `substring(name, 0, dot);
        }
        bool hasFilename() {
            return `len(this->filename()) != 0;
        }
        bool hasExtension() {
            return `len(this->extension()) != 0;
        }
        bool hasParent() {
            return this->parent().toString() != this->path;
        }
        Path normalize() {
            if (`len(this->path) == 0) {
                return Path(".");
            }
            bool absolute = this->isAbsolute();
            string result = "";
            int i = 0;
            int length = `len(this->path);
            while (i < length) {
                while (i < length && this->path[i] == '/') {
                    i++;
                }
                if (i >= length) {
                    break;
                }
                int start = i;
                while (i < length && this->path[i] != '/') {
                    i++;
                }
                string component = `substring(this->path, start, i - start);
                if (component == ".") {
                    continue;
                }
                if (component == "..") {
                    if (`len(result) > 0) {
                        int end = `len(result) - 1;
                        while (end >= 0 && result[end] == '/') {
                            end--;
                        }
                        int begin = end;
                        while (begin >= 0 && result[begin] != '/') {
                            begin--;
                        }
                        string previous = `substring(
                            result,
                            begin + 1,
                            end - begin
                        );
                        if (previous == "..") {
                            if (!absolute) {
                                if (`len(result) == 0) {
                                    result = "..";
                                } else {
                                    result = result + "/..";
                                }
                            }
                        } else {
                            if (begin < 0) {
                                result = "";
                            } else {
                                result = `substring(result, 0, begin);
                            }
                        }
                    } else {
                        if (!absolute) {
                            if (`len(result) == 0) {
                                result = "..";
                            } else {
                                result = result + "/..";
                            }
                        }
                    }
                    continue;
                }
                if (`len(result) == 0) {
                    if (absolute) {
                        result = "/" + component;
                    } else {
                        result = component;
                    }
                }
                else {
                    if (result == "/") {
                        result = result + component;
                    } else {
                        result = result + "/" + component;
                    }
                }
            }
            if (`len(result) == 0) {
                if (absolute) {
                    return Path("/");
                } return Path(".");
            }
            return Path(result);
        }
        Path absolute() {
            if (this->isAbsolute()) {
                return this->normalize();
            }
            string cwd = getCwd().filename();
            if (`len(this->path) == 0) {
                return Path(cwd).normalize();
            }
            return (Path(cwd) / this->path).normalize();
        }

        Path canonical() {
            Path absolutePath = this->absolute();
            string resolved = realpath(absolutePath.toString());
            if (`len(resolved) == 0) {
                return Path("");
            }
            return Path(resolved);
        }
        bool exists() {
            FileStatus status = this->status();
            return status.exists;
        }
        addr_t fileSize() {
            FileStatus status = this->status();
            return status.size;
        }
        DateTime lastWriteTime() {
            FileStatus status = this->status();
            return status.modified;
        }
        DateTime lastAccessTime() {
            FileStatus status = this->status();
            return status.accessed;
        }
        DateTime creationTime() {
            FileStatus status = this->status();
            return status.created;
        }
#ifdef __linux
        FileStatus status() {
            FileStatus output = FileStatus{};
            byte[144] buf = [byte, 144];
            addr_t res = 0;
            `inline(R"(
                mov rax, 4
                mov rdi, $1r
                mov rsi, $2r
                syscall
                mov $0=r, rax
            )", res, this->filename(), &buf, "~{rax, rdi, rsi, memory}"); 
            if (res != 0) {
                output.exists = false;
                output.filetype = FileType.Unknown;
                return output;
            }
            output.exists = true;
            byte* base = &(buf[0]);
            output.size     = *(`cast(base + 48, addr_t*));
            output.accessed = *(`cast(base + 72, addr_t*));
            output.modified = *(`cast(base + 88, addr_t*));
            output.created  = *(`cast(base + 104, addr_t*));
            int mode = *(`cast(base + 24, int*));
            output.permissions = (mode & 0x01FF);
            int type_bits = mode & 0xF000;
            if (type_bits == 0x8000) { output.filetype = FileType.File; }
            else if (type_bits == 0x4000) { output.filetype = FileType.Directory; }
            else if (type_bits == 0xA000) { output.filetype = FileType.Symlink; }
            else { output.filetype = FileType.Other; }
            return output;
        }
        FileStatus linkStatus() {
            FileStatus output = FileStatus{};
            byte[144] buf = [byte, 144];
            addr_t res = 0;
            `inline(R"(
                mov rax, 6
                mov rdi, $1r
                mov rsi, $2r
                syscall
                mov $0=r, rax
            )", res, this->filename(), &buf, "~{rax, rdi, rsi, memory}"); 
            if (res != 0) {
                output.exists = false;
                output.filetype = FileType.Unknown;
                return output;
            }
            output.exists = true;
            byte* base = &(buf);
            output.size     = *(`cast(base + 48, addr_t*));
            output.accessed = *(`cast(base + 72, addr_t*));
            output.modified = *(`cast(base + 88, addr_t*));
            output.created  = *(`cast(base + 104, addr_t*));
            int mode = *(`cast(base + 24, int*));
            output.permissions = (mode & 0x01FF);
            int type_bits = mode & 0xF000;
            if (type_bits == 0x8000) { output.filetype = FileType.File; }
            else if (type_bits == 0x4000) { output.filetype = FileType.Directory; }
            else if (type_bits == 0xA000) { output.filetype = FileType.Symlink; }
            else { output.filetype = FileType.Other; }
            return output;
        }
#endif
#ifdef __APPLE__
        FileStatus status() {
            FileStatus output = FileStatus{};
            byte buf = [byte, 144];
            addr_t res = 0; 
            `inline(R"(
                mov rax, 0x20000bc
                mov rdi, $1r
                lea rsi, $2r
                syscall
                mov $0=r, rax
            )", res, this->filename(), &buf); 
            if (res != 0) {
                output.exists = false;
                output.filetype = FileType.Unknown;
                return output;
            }
            output.exists = true;
            byte* base = `cast(&(buf), byte*);
            short mode = *(`cast(base + 4, short*));
            output.permissions = (mode & 0x01FF);
            int type_bits = mode & 0xF000;
            if (type_bits == 0x8000) { output.filetype = FileType.File; }
            else if (type_bits == 0x4000) { output.filetype = FileType.Directory; }
            else if (type_bits == 0xA000) { output.filetype = FileType.Symlink; }
            else { output.filetype = FileType.Other; }
            output.size     = *(`cast(base + 96, addr_t*));
            output.accessed = *(`cast(base + 32, addr_t*));
            output.modified = *(`cast(base + 48, addr_t*));
            output.created  = *(`cast(base + 80, addr_t*));   
            return output;
        }
        FileStatus linkStatus() {
            FileStatus output = FileStatus{};
            byte buf = [byte, 144];
            addr_t res = 0;
            `inline(R"(
                mov rax, 0x20000be
                mov rdi, $1r
                lea rsi, $2r
                syscall
                mov $0=r, rax
            )", res, this->filename(), &buf); 
            if (res != 0) {
                output.exists = false;
                output.filetype = FileType.Unknown;
                return output;
            }
            output.exists = true;
            byte* base = `cast(&(buf), byte*);
            short mode = *(`cast(base + 4, short*));
            output.permissions = (mode & 0x01FF);
            int type_bits = mode & 0xF000;
            if (type_bits == 0x8000) { output.filetype = FileType.File; }
            else if (type_bits == 0x4000) { output.filetype = FileType.Directory; }
            else if (type_bits == 0xA000) { output.filetype = FileType.Symlink; }
            else { output.filetype = FileType.Other; }
            output.size     = *(`cast(base + 96, addr_t*));
            output.accessed = *(`cast(base + 32, addr_t*));
            output.modified = *(`cast(base + 48, addr_t*));
            output.created  = *(`cast(base + 80, addr_t*));
            return output;
        }
#endif
        addr_t size() {
            return this->status().size;
        }
        bool isHidden() {
            string name = this->filename();
            if (`len(name) == 0) {
                return false;
            }

            return name[0] == '.';
        }
        bool isFile() {
            return this->status().filetype == FileType.File;
        }
        bool isDirectory() {
            return this->status().filetype == FileType.Directory;
        }
        bool isSymlink() {
            return this->status().filetype == FileType.Symlink;
        }
    }
    Path getCwd() {
#ifdef __linux
        char[4096] result = [char, 4096];
        `inline(R"(
            mov rax, 79
            mov rdi, $0=m
            mov rsi, 4096
            syscall
        )", result, "~{rax,rdi,rsi}");
        return Path(&(result[0]));
#endif
#ifdef __APPLE__
        char[1024] = [char, 4096];
        char* dot = ".";
        `inline(R"(
            mov x0, $1r
            mov x1, 0
            mov x16, 5
            svc #0
            mov x9, x0
            mov x0, x9 
            mov x1, 50
            mov x2, $0m 
            mov x16, 92 
            svc #0
            mov x0, x9
            mov x16, 6
            svc #0
        )", result, dot, "~{x0,x1,x2,x9,x16}");
        return Path(result);
#endif
    }
    class InputStream {
        private int fd;
        private addr_t most_recent;
        InputStream() {
            this->fd = -1;
            this->most_recent = 0;
        }
        InputStream(Path path) {
            this->most_recent = 0;
            this->open(path);
        }
        void open(Path path) {
            if (path.empty()) {
                panic "Cannot read a empty file";
            }
            if (this->isOpen()) {
                this->close();
            }
            this->fd = `open(path.absolute().toString(), "r");
        }
        // InputStream now owns fd.
        void setFd(int fd) {
            this->fd = fd;
        }
        char get() {
            char c;
            if (!this->isOpen()) panic "Cannot get a uninitialized InputStream";
            int bytesRead = `read(this->fd, &c, 1);
            this->most_recent = bytesRead;
            if (bytesRead != 1) return '\0';
            return c;
        }
        void getline(string dest, char till = '\n') {
            if (!this->isOpen()) {
                panic "Cannot get a uninitializd InputStream";
            }
            addr_t byteReadCount = 0;
            while (true) {
                char c = '\0';
                int read = `read(this->fd, &c, 1);
                if (read != 1 || c == till) {
                    this->most_recent = byteReadCount;
                    return;
                }
                byteReadCount++;
                dest += c;
            }
        }
        void read(string dest, addr_t count) {
            if (!this->isOpen()) {
                panic "Cannot get a uninitializd InputStream";
            }
            int bytesRead = `read(this->fd, dest, count);
            this->most_recent = bytesRead;
        }
        addr_t gcount() {
            return this->most_recent;
        }
        bool isOpen() {
            return this->fd != -1;
        }
        void close() {
            if (this->isOpen()) {
                `close(this->fd);
                this->fd = -1;
            }
        }
        void _destroy() {
            this->close();
        }
    }
    class OutputStream {
        private int fd;
        OutputStream() {
            this->fd = -1;
        }
        OutputStream(Path path) {
            this->fd = -1;
            this->open(path);
        }
        void open(Path path) {
            if (path.empty()) {
                panic "Cannot write to an empty file";
            }
            if (this->isOpen()) {
                this->close();
            }
            this->fd = `open(path.absolute().toString(), "r+");
            if (this->fd == -1) {
                panic "Cannot open file for writing";
            }
        }
        // OutputStream now owns fd
        void setFd(int fd) {
            if (this->isOpen()) {
                this->close();
            }
            this->fd = fd;
        }
        void put(char c) {
            if (!this->isOpen()) {
                panic "Cannot put to an uninitialized OutputStream";
            }
            `write(this->fd, &c);
        }
        void write(string data) {
            if (!this->isOpen()) {
                panic "Cannot write to an uninitialized OutputStream";
            }
            `write(this->fd, data);
        }
        bool isOpen() {
            return this->fd != -1;
        }
        void close() {
            if (!this->isOpen()) {
                return;
            }
            `close(this->fd);
            this->fd = -1;
        }
        void _destroy() {
            this->close();
        }
    }
    class RWStream {
        private int fd;
        private addr_t most_recent_read;
        RWStream() {
            this->fd = -1;
            this->most_recent_read = 0;
        }
        RWStream(Path path) {
            this->fd = -1;
            this->most_recent_read = 0;
            this->open(path);
        }
        void open(Path path) {
            if (path.empty()) {
                panic "Cannot read/write to an empty file";
            }
            if (this->isOpen()) {
                this->close();
            }
            this->fd = `open(path.absolute().toString(), "r+");
            if (this->fd == -1) {
                panic "Cannot open file for reading/writing";
            }
        }
        // RWStream now owns
        void setFd(int fd) {
            if (this->isOpen()) {
                this->close();
            }
            this->fd = fd;
        }
        void put(char c) {
            if (!this->isOpen()) {
                panic "Cannot put to an uninitialized RWStream";
            }
            `write(this->fd, &c);
        }
        void write(string data) {
            if (!this->isOpen()) {
                panic "Cannot write to an uninitialized RWStream";
            }
            `write(this->fd, data);
        }
        char get() {
            char c;
            if (!this->isOpen()) panic "Cannot get a uninitialized RWStream";
            int bytesRead = `read(this->fd, &c, 1);
            this->most_recent_read = bytesRead;
            if (bytesRead != 1) return '\0';
            return c;
        }
        void getline(string dest, char till = '\n') {
            if (!this->isOpen()) {
                panic "Cannot get a uninitializd RWStream";
            }
            addr_t byteReadCount = 0;
            while (true) {
                char c = '\0';
                int read = `read(this->fd, &c, 1);
                if (read != 1 || c == till) {
                    this->most_recent_read = byteReadCount;
                    return;
                }
                byteReadCount++;
                dest += c;
            }
        }
        void read(string dest, addr_t count) {
            if (!this->isOpen()) {
                panic "Cannot get a uninitializd RWStream";
            }
            int bytesRead = `read(this->fd, dest, count);
            this->most_recent_read = bytesRead;
        }
        addr_t gcount() {
            return this->most_recent_read;
        }
        bool isOpen() {
            return this->fd != -1;
        }
        void close() {
            if (!this->isOpen()) {
                return;
            }
            `close(this->fd);
            this->fd = -1;
            this->most_recent_read = 0;
        }
        void _destroy() {
            this->close();
        }
    }
    class DirectoryIterator {
        private void* dir;
        private Path directory;
        private string next_name;
        private bool cached;
        private bool finished;
        DirectoryIterator() {
            this->dir = nullptr;
            this->cached = false;
            this->finished = true;
        }
        DirectoryIterator(Path path) {
            this->dir = nullptr;
            this->directory = path;
            this->next_name = "";
            this->cached = false;
            this->finished = false;
            if (path.empty()) {
                panic "Cannot iterate an empty directory";
            }
            if (!path.isDirectory()) {
                panic "Cannot iterate a non-directory";
            }
            this->dir = `opendir(path.absolute().toString());
            if (!this->dir) {
                panic "Cannot open directory";
            }
        }
        private bool read_Next() {
            if (this->cached) {
                return true;
            }
            if (this->finished) {
                return false;
            }
            while (true) {
                char buffer[4096] = [char, 4096];
                if (!`readdir(this->dir, buffer, sizeof buffer)) {
                    this->finished = true;
                    return false;
                }
                string name = buffer;
                if (name == "." || name == "..") {
                    continue;
                }
                this->next_name = name;
                this->cached = true;
                return true;
            }
        }
        bool hasNext() {
            return this->read_Next();
        }
        Path next() {
            if (!this->read_Next()) {
                panic "DirectoryIterator has no next entry";
            }
            Path result = this->directory / this->next_name;
            this->cached = false;
            this->next_name = "";
            return result;
        }
        void close() {
            if (this->dir == nullptr) {
                return;
            }
            `closedir(this->dir);
            this->dir = nullptr;
            this->cached = false;
            this->finished = true;
            this->next_name = "";
        }
        bool isOpen() {
            return this->dir != nullptr;
        }
        void _destroy() {
            this->close();
        }
    }
    bool createDirectory(Path path, int mode = 0o755) {
        if (path.empty()) {
            panic "Cannot create an empty directory";
        }
        if (path.exists()) {
            return false;
        }
        long int result;
        #ifdef __linux
            `inline(R"(
                mov rax, 83
                mov rdi, $1r
                mov esi, $2r
                syscall
                mov $0=r, rax
            )", result, path.absolute().toString(), mode, "~{rax,rdi,rsi,memory}");
        #endif
        #ifdef __APPLE__
            `inline(R"(
                mov rax, 0x2000068
                mov rdi, $1r
                mov esi, $2r
                syscall
                mov $0=r, rax
            )", result, path.absolute().toString(), mode, "~{rax,rdi,rsi,memory}"
            );
        #endif
        if (result < 0) {
            panic "Failed to create directory";
        }
        return true;
    }
    bool createDirectories(Path path) {
        if (path.empty()) {
            panic "Cannot create an empty directory";
        }
        if (path.exists()) {
            if (!path.isDirectory()) {
                panic "Path exists and is not a directory";
            }
            return false;
        }
        Path parent = path.parent();
        if (!parent.empty() && !parent.exists()) {
            createDirectories(parent);
        }
        return createDirectory(path);
    }
    bool remove(Path path) {
        if (path.empty()) {
            panic "Cannot remove an empty path";
        }
        if (!path.exists()) {
            return false;
        }
        long int result;
        if (path.isDirectory() && !path.isSymlink()) {
            #ifdef __linux
                `inline(R"(
                    mov rax, 84
                    mov rdi, $1r
                    syscall
                    mov $0=r, rax
                )", result, path.absolute().toString(), "~{rax,rdi,memory}");
            #endif
            #ifdef __APPLE__
                `inline(R"(
                    mov rax, 0x2000051
                    mov rdi, $1r
                    syscall
                    mov $0=r, rax
                )", result, path.absolute().toString(), "~{rax,rdi,memory}");
            #endif
        } else {
            #ifdef __linux
                `inline(R"(
                    mov rax, 87
                    mov rdi, $1r
                    syscall
                    mov $0=r, rax
                )", result, path.absolute().toString(), "~{rax,rdi,memory}");
            #endif
            #ifdef __APPLE__
                `inline(R"(
                    mov rax, 0x200000A
                    mov rdi, $1r
                    syscall
                    mov $0=r, rax
                )", result, path.absolute().toString(), "~{rax,rdi,memory}");
            #endif
        }
        if (result < 0) {
            panic "Failed to remove path";
        }
        return true;
    }
    bool removeAll(Path path) {
        if (path.empty()) {
            panic "Cannot remove an empty path";
        }
        if (!path.exists()) {
            return false;
        }
        if (path.isDirectory() && !path.isSymlink()) {
            DirectoryIterator iterator = DirectoryIterator(path);
            while (iterator.hasNext()) {
                removeAll(iterator.next());
            }
            iterator.close();
        }
        remove(path);
        return true;
    }
    bool rename(Path from, Path to) {
        if (from.empty() || to.empty()) {
            panic "Cannot rename using an empty path";
        }
        long int result;
        #ifdef __linux
            `inline(R"(
                mov rax, 82
                mov rdi, $1r
                mov rsi, $2r
                syscall
                mov $0=r, rax
            )", result, from.absolute().toString(), to.absolute().toString(), "~{rax,rdi,rsi,memory}");
        #endif
        #ifdef __APPLE__
            `inline(R"(
                mov rax, 0x2000052
                mov rdi, $1r
                mov rsi, $2r
                syscall
                mov $0r, rax
            )", result, from.absolute().toString(), to.absolute().toString(), "~{rax,rdi,rsi,memory}");
        #endif
        if (result < 0) {
            panic "Failed to rename path";
        }
        return true;
    }
    void copy(Path source, Path destination) {
        if (source.empty() || destination.empty()) {
            panic "Cannot copy using an empty path";
        }
        if (!source.exists()) {
            panic "Source does not exist";
        }
        if (source.isDirectory() && !source.isSymlink()) {
            if (destination.exists()) {
                if (!destination.isDirectory()) {
                    panic "Destination exists and is not a directory";
                }
            } else {
                createDirectory(destination);
            }
            DirectoryIterator iterator = DirectoryIterator(source);
            while (iterator.hasNext()) {
                Path child = iterator.next();
                Path target = destination / child.filename();
                copy(child, target);
            }
            iterator.close();
            return;
        }
        InputStream input = InputStream(source);
        OutputStream output = OutputStream(destination);
        string buffer = "";
        while (true) {
            input.read(buffer, 8192a);
            addr_t count = input.gcount();
            if (count == 0) {
                break;
            }
            output.write(buffer);
        }
        input.close();
        output.close();
    }
}

