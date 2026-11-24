//!===----------------------------------------------------------------------===!//
//!
//! Part of the QuarticC programming language standard library, MIT license.
//!
//!===----------------------------------------------------------------------===!//
//!

namespace Exported {
}
namespace CoreConcepts {
    concept Eq {
        1_of {
            bool operator==(Self other);
            bool operator==(Self self, Self other);
        }
    }
    concept Allocated {
        at_least 1_of {
            void _destroy();
            void _destroy(Self self);
        }
    }
}
namespace Errors {
    enum Codes { OK; Unknown; NotFound; InvalidArgument; OutOfBounds; Unsupported; }
    type Error<T> = T|Codes;
}
namespace Math {
    double e() {
        return 2.71828182845904523536;
    }
    
    double pi() {
        return 3.14159265358979323846;
    }
    type Floating = float | double;
    int ceil(Floating a) {
        double d;
        if (typeof a == double) d = (a as double);
        else d = `to_double(a as float);
        int i = `to_int(d);
        if (d == `to_double(i)) {
            return i;
        } else if (d > 0) {
            return i + 1;
        } else {
            return i;
        }
    }
    int floor(Floating a) {
        double d;
        if (typeof a == double) d = (a as double);
        else d = `to_double(a as float);
        int i = `to_int(d);
        if (d >= 0) {
            return i;
        } else {
            if (d == `to_double(i)) {
                return i;
            } else {
                return i - 1;
            }
        }
    }
    double sin(double x) {
        double pi = 3.14159265358979323846;
        while (x > pi) { x -= 2 * pi; }
        while (x < -pi) { x += 2 * pi; }
        
        double result = 0.0;
        double term = x;
        int n = 1;
        for (int i = 0; i < 10; i++) {
            result += term;
            term *= -1.0 * x * x / ((2 * n) * (2 * n + 1));
            n++;
        }
        
        return result;
    }
    double cos(double x) {
        
        double pi = 3.14159265358979323846;
        while (x > pi) { x -= 2 * pi; }
        while (x < -pi) { x += 2 * pi; }
        
        double result = 0.0;
        double term = 1.0;
        int n = 1;
        
        for (int i = 0; i < 10; i++) {
            result += term;
            term *= -1.0 * x * x / ((2 * n - 1) * (2 * n));
            n++;
        }
        
        return result;
    }
    double tan(double x) {
        return `ternary(sin(x) == 0.0 || cos(x) == 0.0, 0.0, sin(x) / cos(x));
    }
    Errors::Error<double> log(double x) {
        if (x <= 0) {
            return Errors::Codes.InvalidArgument;
        }
        double log2 = 0.693147180559945309417;
        int n = 0;
        
        while (x > 2.0) {
            x /= 2.0;
            n++;
        }
        while (x < 0.5) {
            x *= 2.0;
            n--;
        }
        double y = x - 1.0;
        double result = 0.0;
        double term = y;
        
        for (int i = 1; i <= 20; i++) {
            result += term / `to_double(i);
            term *= -y;
        }
        
        return result + n * log2;
    }
}
namespace String {
    struct View {
        char *data;
        addr_t length;
    }
    class Str {
        static addr_t NOT_FOUND = ~(0a);
        private char *data;    
        private addr_t length;
        private addr_t capacity;
        Str(char *data) {
            addr_t length = 0;
            while (data[length] != '\0') {
                length++;
            }
            this->length = length;
            char* heap_str = `malloc(length + 1);
            `memcpy(heap_str, data, length + 1);
            this->data = heap_str;
            this->capacity = length + 1;
        }
        Str(addr_t copies, char letter) {
            this->length = copies;
            char* heap_str = `malloc(copies + 1);
            for (addr_t i = 0; i < copies; i++) {
                heap_str[i] = letter;
            }
            heap_str[copies] = '\0';
            this->data = heap_str;
            this->capacity = copies + 1;
            this->capacity = copies + 1;
        }
        Str(char *data, addr_t length) {
            this->data = data;
            this->capacity = length + 1;
            this->length = length;
        }
        Str operator=(char* data) {
            `free(this->data);
            this->data = nullptr;
            addr_t length = 0;
            while (data[length] != '\0') {
                length++;
            }
            this->length = length;
            char* heap_str = `malloc(length + 1);
            `memcpy(heap_str, data, length + 1);
            this->data = heap_str;
            this->capacity = length + 1;
            return *this;
        }
        addr_t len() {
            return this->length;
        }
        Str substr(addr_t start, addr_t length) {
            if (length < 0) {
                panic "attempted to extract a negative length subtring";
            }
            if (start > this->length || length > this->length - start) {
                panic "attempted to extract a substring larger than the length of a string";
            }
            char *alloc = `malloc((sizeof char) * (length + 1));
            for (addr_t i = 0a; i < length; i++) {
                alloc[i] = this->data[i + start];
            }
            alloc[length] = '\0';
            Str result = Str(alloc, length);
            return result;
        }
        View subView(addr_t start, addr_t length) {
            if (start > this->length || length > this->length - start) {
                panic "substring view out of bounds";
            }
            return View {
                data: this->data + start,
                length: length
            };
        }
        addr_t find(Str other, addr_t offset = 0) {
            if (offset > this->length) {
                return Str::NOT_FOUND;
            }
            if (other.length == 0) {
                return offset;
            }
            if (other.length > this->length - offset) {
                return Str::NOT_FOUND;
            }
            for (addr_t i = offset; i <= this->length - other.length; i++) {
                bool has_match = true;
                for (addr_t j = 0; j < other.length; j++) {
                    if (this->data[i + j] != other.data[j]) {
                        has_match = false;
                        break;
                    }
                }
                if (has_match) {
                    return i;
                }
            }
            return Str::NOT_FOUND;
        }
        bool contains(Str other) {
            return this->find(other) != Str::NOT_FOUND;
        }
        bool startsWith(Str other) {
            return this->find(other) == 0a;
        }
        bool endsWith(Str other) {
            if (other.length > this->length) return false;
            return this->find(other, this->length - other.length) == this->length - other.length;
        }
        bool isEmpty() {
            return this->length == 0a;
        }
        private bool eq(Str other) {
            if (this->length != other.length) return false;
            for (addr_t i = 0; i < this->length; i++) {
                if (this->data[i] != other.data[i]) return false;
            }
            return true;
        }
        bool operator==(Str other) {
            return this->eq(other);
        }
        bool operator!=(Str other) {
            return !this->eq(other);
        }
        bool _eval() {
            return !this->isEmpty();
        }
        Str operator+(char *other) {
            addr_t length = 0;
            while (other[length] != '\0') {
                length++;
            }
            char *data_2 = `malloc((sizeof char) * (this->length + length + 1));
            for (addr_t i = 0; i < this->length; i++) {
                data_2[i] = this->data[i];
            }
            for (addr_t i = 0; i < length; i++) {
                data_2[i + this->length] = other[i];
            }
            data_2[this->length + length] = '\0';
            return Str(data_2, this->length + length);
        }
        Str operator+=(char *other) {
            addr_t length = 0;
            while (other[length] != '\0') {
                length++;
            }
            addr_t new_length = this->length + length;
            this->data = `realloc(
                this->data,
                (sizeof char) * (new_length + 1)
            );
            for (addr_t i = 0; i < length; i++) {
                this->data[this->length + i] = other[i];
            }
            this->length = new_length;
            this->capacity = new_length + 1;
            this->data[this->length] = '\0';
            return *this;
        }
        Str operator+(Str other) {
            char *data_2 = `malloc((sizeof char) * (this->length + other.length + 1));
            for (addr_t i = 0; i < this->length; i++) {
                data_2[i] = this->data[i];
            }
            for (addr_t i = 0; i < other.length; i++) {
                data_2[i + this->length] = other.data[i];
            }
            data_2[this->length + other.length] = '\0';
            return Str(data_2, this->length + other.length);
        }
        Str operator+=(Str other) {
            addr_t new_length = this->length + other.length;
            this->data = `realloc(
                this->data,
                (sizeof char) * (new_length + 1)
            );
            for (addr_t i = 0; i < other.length; i++) {
                this->data[this->length + i] = other.data[i];
            }
            this->length = new_length;
            this->capacity = new_length + 1;
            this->data[this->length] = '\0';
            return *this;
        }
        char& operator[](addr_t index) {
            if (index >= this->length) {
                panic "string index out of bounds";
            }
            return this->data[index];
        }
        void _destroy() {
            `free(this->data);
            this->data = nullptr;
        }
        void insert(addr_t index, Str str) {
            if (index > this->length) {
                panic "string insertion index out of bounds";
            }
            addr_t new_length = this->length + str.length;
            if (new_length + 1 > this->capacity) {
                this->capacity = new_length + 1;
                this->data = `realloc(this->data, (sizeof char) * this->capacity);
            }
            for (addr_t i = this->length; i > index; i--) {
                this->data[i + str.length - 1] = this->data[i - 1];
            }
            for (addr_t i = 0; i < str.length; i++) {
                this->data[index + i] = str.data[i];
            }
            this->length = new_length;
            this->data[this->length] = '\0';
        }
        void erase(addr_t start, addr_t length) {
            if (start > this->length || length > this->length - start) {
                panic "string erase range out of bounds";
            }
            if (length == 0) {
                return;
            }
            for (addr_t i = start; i < this->length - length; i++) {
                this->data[i] = this->data[i + length];
            }
            this->length -= length;
            this->data[this->length] = '\0';
        } 
        private bool streq(char *data) {
            addr_t i = 0;
            while (this->data[i] != '\0' && data[i] != '\0') {
                if (this->data[i] != data[i]) {
                    return false;
                }
                i++;
            }
            return this->data[i] == data[i];
        }
        bool operator==(char *data) {
            return this->streq(data);
        }
        bool operator!=(char *data) {
            return !this->streq(data);
        }
        bool toBool() {
            return this->streq("true") ? true : false;
        }
        qbool toQBool() {
            return this->streq("qtrue") ? qtrue : this->streq("both") ? both : this->streq("qfalse") ? qfalse : none;
        }
        char *c_str() {
            return this->data;   
        }
    }
    Str proves CoreConcepts::Allocated;
}
namespace IO {
    int output = 1;
    enum IntegerFmt {
        DECIMAL;
        OCTAL;
        HEX;
        BINARY;
    }
    void printInteger(long int value, IntegerFmt fmt, nibble width, bool zero_pad) {
        char[68] buffer = [char, 68];
        byte offset = 0y;
        if (fmt != IntegerFmt.DECIMAL) {
            offset = 2y;
            buffer[0] = '0';
            match (fmt) {
                IntegerFmt.DECIMAL => unreachable;
                IntegerFmt.OCTAL => buffer[1] = 'o';
                IntegerFmt.HEX => buffer[1] = 'x';
                IntegerFmt.BINARY => buffer[1] = 'b';
            }
        }
        if (value == 0l) {
            if (width > 0n) {
                for (nibble i = 0n; i < width - 1; i++) {
                    buffer[i + offset] = fmt == IntegerFmt.DECIMAL ? zero_pad ? '0' : ' ' : '0';
                }
                buffer[width - 1 + offset] = '0';
                buffer[offset + width] = '\0';
                `write(output, &(buffer[0]));
                return;
            } else {
                buffer[offset] = '0';
                buffer[offset + 1] = '\0';
                `write(output, &(buffer[0]));
                return;
            }
        } else {
            bool negative = value < 0l && fmt == IntegerFmt.DECIMAL;
            char[64] digits = [char, 64];
            int count = 0;
            match (fmt) {
                IntegerFmt.DECIMAL => {
                    long int v = value;
                    while (v != 0l) {
                        long int d = v % 10l;
                        if (d < 0l) { d = -d; }
                        digits[count] = ('0' + d) as char;
                        count++;
                        v = v / 10l;
                    }
                }
                IntegerFmt.OCTAL => {
                    for (int shift = 0; shift < 64; shift += 3) {
                        long int d = (value |> shift) & 7l;
                        digits[count] = ('0' + d) as char;
                        count++;
                    }
                }
                IntegerFmt.HEX => {
                    for (int shift = 0; shift < 64; shift += 4) {
                        long int d = (value |> shift) & 15l;
                        digits[count] = (d < 10l ? '0' + d : 'a' + (d - 10l)) as char;
                        count++;
                    }
                }
                IntegerFmt.BINARY => {
                    for (int shift = 0; shift < 64; shift++) {
                        long int d = (value |> shift) & 1l;
                        digits[count] = ('0' + d) as char;
                        count++;
                    }
                }
            }
            while (count > 1 && digits[count - 1] == '0') {
                count--;
            }
            int pos = fmt == IntegerFmt.DECIMAL ? 0 : 2;
            int field_len = count + (negative ? 1 : 0);
            int pad = width > field_len ? width - field_len : 0;
            if (fmt == IntegerFmt.DECIMAL && !zero_pad) {
                for (int i = 0; i < pad; i++) { buffer[pos] = ' '; pos++; }
                if (negative) { buffer[pos] = '-'; pos++; }
            } else {
                if (negative) { buffer[pos] = '-'; pos++; }
                for (int i = 0; i < pad; i++) { buffer[pos] = '0'; pos++; }
            }
            for (int i = count - 1; i >= 0; i--) {
                buffer[pos] = digits[i];
                pos++;
            }
            buffer[pos] = '\0';
            `write(output, &(buffer[0]));
        }
    }
    void printUnsignedInteger(addr_t value, IntegerFmt fmt, nibble width, bool zero_pad) {
        char[68] buffer = [char, 68];
        byte offset = 0y;
        if (fmt != IntegerFmt.DECIMAL) {
            offset = 2y;
            buffer[0] = '0';
            match (fmt) {
                IntegerFmt.DECIMAL => unreachable;
                IntegerFmt.OCTAL => buffer[1] = 'o';
                IntegerFmt.HEX => buffer[1] = 'x';
                IntegerFmt.BINARY => buffer[1] = 'b';
            }
        }
        if (value == 0a) {
            if (width > 0n) {
                for (nibble i = 0n; i < width - 1; i++) {
                    buffer[i + offset] = fmt == IntegerFmt.DECIMAL ? zero_pad ? '0' : ' ' : '0';
                }
                buffer[width - 1 + offset] = '0';
                buffer[offset + width] = '\0';
                `write(output, &(buffer[0]));
                return;
            } else {
                buffer[offset] = '0';
                buffer[offset + 1] = '\0';
                `write(output, &(buffer[0]));
                return;
            }
        } else {
            char[64] digits = [char, 64];
            int count = 0;
            match (fmt) {
                IntegerFmt.DECIMAL => {
                    addr_t v = value;
                    while (v != 0a) {
                        addr_t d = v % 10l;
                        digits[count] = ('0' + d) as char;
                        count++;
                        v = v / 10l;
                    }
                }
                IntegerFmt.OCTAL => {
                    for (int shift = 0; shift < 64; shift += 3) {
                        addr_t d = (value |> shift) & 7l;
                        digits[count] = ('0' + d) as char;
                        count++;
                    }
                }
                IntegerFmt.HEX => {
                    for (int shift = 0; shift < 64; shift += 4) {
                        addr_t d = (value |> shift) & 15l;
                        digits[count] = (d < 10l ? '0' + d : 'a' + (d - 10l)) as char;
                        count++;
                    }
                }
                IntegerFmt.BINARY => {
                    for (int shift = 0; shift < 64; shift++) {
                        addr_t d = (value |> shift) & 1l;
                        digits[count] = ('0' + d) as char;
                        count++;
                    }
                }
            }
            while (count > 1 && digits[count - 1] == '0') {
                count--;
            }
            int pos = fmt == IntegerFmt.DECIMAL ? 0 : 2;
            int pad = width > count ? width - count : 0;
            if (fmt == IntegerFmt.DECIMAL && !zero_pad) {
                for (int i = 0; i < pad; i++) { buffer[pos] = ' '; pos++; }
            } else {
                for (int i = 0; i < pad; i++) { buffer[pos] = '0'; pos++; }
            }
            for (int i = count - 1; i >= 0; i--) {
                buffer[pos] = digits[i];
                pos++;
            }
            buffer[pos] = '\0';
            `write(output, &(buffer[0]));
        }
    }
    void printDecimal(double value, nibble width, nibble precision, bool zero_pad) {
        char[64] buffer = [char, 64];
        char[32] digits = [char, 32];
        bool negative = value < 0.0;
        if (negative) { value = -value; }
        long int int_part = `to_long_int(value);
        double frac = value - `to_double(int_part);
        long int scale = 1l;
        for (int i = 0; i < precision; i++) { scale *= 10l; }
        long int frac_scaled = (frac * scale + 0.5) as long int;
        if (frac_scaled >= scale) {
            int_part = int_part + 1l;
            frac_scaled = frac_scaled - scale;
        }
        if (int_part == 0l && frac_scaled == 0l) { negative = false; }
        int int_count = 0;
        if (int_part == 0l) {
            digits[0] = '0';
            int_count = 1;
        } else {
            long int v = int_part;
            while (v != 0l) {
                digits[int_count] = ('0' + (v % 10l)) as char;
                int_count++;
                v /= 10l;
            }
        }
        char[16] frac_digits = [char, 16];
        long int f = frac_scaled;
        for (int i = precision - 1; i >= 0; i--) {
            frac_digits[i] = ('0' + (f % 10l)) as char;
            f /= 10l;
        }
        int field_len = int_count + (negative ? 1 : 0) + (precision > 0 ? 1 + precision : 0);
        int pad = width > field_len ? width - field_len : 0;
        int pos = 0;
        if (!zero_pad) {
            for (int i = 0; i < pad; i++) { buffer[pos] = ' '; pos++; }
            if (negative) { buffer[pos] = '-'; pos++; }
        } else {
            if (negative) { buffer[pos] = '-'; pos++; }
            for (int i = 0; i < pad; i++) { buffer[pos] = '0'; pos++; }
        }
        for (int i = int_count - 1; i >= 0; i--) {
            buffer[pos] = digits[i];
            pos++;
        }
        if (precision > 0) {
            buffer[pos] = '.';
            pos++;
            for (int i = 0; i < precision; i++) {
                buffer[pos] = frac_digits[i];
                pos++;
            }
        }
        buffer[pos] = '\0';
        `write(output, &(buffer[0]));
    }
    void printChar(char value) {
        char[2] v = [value, '\0'];
        `write(output, &(v[0])); 
    }
    void printBool(bool value) {
        `write(output, value ? "true" : "false"); 
    }
    void printQbool(qbool value) {
        `write(output, value == qtrue ? "qtrue" : value == qfalse ? "qfalse" : value == both ? "both" : "none"); 
    }
    void printString(char *value) {
        `write(output, value); 
    }
    void qout(char *fmt_string, ...args) {
        addr_t length = 0a;
        while (fmt_string[length] != '\0') {
            length++;
        }
        addr_t i = 0a;
        while (i < length) {
            char c = fmt_string[i];
            if (c != '%') {
                printChar(c);
                i++;
            } else {
                i++;
                if (i >= length) {
                    printChar('%');
                } else {
                    bool zero_pad = false;
                    if (fmt_string[i] == '0') {
                        zero_pad = true;
                        i++;
                    }
                    int width_i = 0;
                    while (i < length && fmt_string[i] >= '0' && fmt_string[i] <= '9') {
                        width_i = width_i * 10 + (fmt_string[i] - '0');
                        i++;
                    }
                    if (width_i > 15) { width_i = 15; }
                    nibble width = width_i as nibble;
                    int prec_i = 6;
                    if (i < length && fmt_string[i] == '.') {
                        i++;
                        prec_i = 0;
                        while (i < length && fmt_string[i] >= '0' && fmt_string[i] <= '9') {
                            prec_i = prec_i * 10 + (fmt_string[i] - '0');
                            i++;
                        }
                        if (prec_i > 15) { prec_i = 15; }
                    }
                    nibble precision = prec_i as nibble;
                    if (i >= length) {
                        printChar('%');
                    } else {
                        char spec = fmt_string[i];
                        i++;
                        if (spec == '%') {
                            printChar('%');
                        } else if (`is_empty(args)) {
                            printChar('%');
                            printChar(spec);
                        } else if (spec == 'd' || spec == 'i') {
                            printInteger(`next(args, long int), IntegerFmt.DECIMAL, width, zero_pad);
                        } else if (spec == 'p') {
                            printUnsignedInteger(`to_address(`next(args, void*)), IntegerFmt.HEX, width, zero_pad);
                        } else if (spec == 'u') {
                            printUnsignedInteger(`next(args, addr_t), IntegerFmt.DECIMAL, width, zero_pad);
                        } else if (spec == 'x') {
                            printInteger(`next(args, long int), IntegerFmt.HEX, width, zero_pad);
                        } else if (spec == 'o') {
                            printInteger(`next(args, long int), IntegerFmt.OCTAL, width, zero_pad);
                        } else if (spec == 'b') {
                            printInteger(`next(args, long int), IntegerFmt.BINARY, width, zero_pad);
                        } else if (spec == 'f') {
                            printDecimal(`next(args, double), width, precision, zero_pad);
                        } else if (spec == 'c') {
                            printChar(`next(args, char));
                        } else if (spec == 's') {
                            printString(`next(args, char *));
                        } else if (spec == 't') {
                            printBool(`next(args, bool));
                        } else if (spec == 'q') {
                            printQbool(`next(args, qbool));
                        } else {
                            printChar('%');
                            printChar(spec);
                        }
                    }
                }
            }
        }
    }
    char *readUntil(char delim, bool stopAtNewline) {
        addr_t cap = 16;
        addr_t len = 0;
        char *res = `malloc((sizeof char) * (cap + 1));
        while (true) {
            char c;
            addr_t n = `read(0, &c, 1);
            if (n != 1) break;
            if (c == delim) break;
            if (stopAtNewline && c == '\n') break;
            if (len == cap) {
                cap = cap * 2;
                res = `realloc(res, (sizeof char) * (cap + 1));
            }
            res[len] = c;
            len = len + 1;
        }
        res[len] = 0;
        res = `realloc(res, (sizeof char) * (len + 1));
        return res;
    }
    char *getWord() {
        return readUntil(' ', true);
    }
    char *getLine() {
        return readUntil('\n', false);
    }
}
