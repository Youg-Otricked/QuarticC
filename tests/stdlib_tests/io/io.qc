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
int main() {
    char *str = "Hello, World!";
    IO::qout("%s, %c, It worked? %i, %x\nType a word:\n> ", str, 'c', 12380, 0x87642194);
    char *input = IO::getWord();
    IO::qout("%s", input);
    `free(input);
    return 0;
}
