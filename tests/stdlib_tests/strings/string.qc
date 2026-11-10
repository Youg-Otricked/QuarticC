// TODO: SSO
namespace Exported {
    #include <IO, std>
}

struct StringView {
    char *data;
    addr_t length;
}
class String {
    static addr_t NOT_FOUND = ~(0a);
    private char *data;    
    private addr_t length;
    private addr_t capacity;
    String(char *data) {
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
    String(addr_t copies, char letter) {
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
    String(char *data, addr_t length) {
        this->data = data;
        this->capacity = length + 1;
        this->length = length;
    }
    String operator=(char* data) {
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
    String substr(addr_t start, addr_t length) {
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
        String result = String(alloc, length);
        return result;
    }
    StringView subView(addr_t start, addr_t length) {
        if (start > this->length || length > this->length - start) {
            panic "substring view out of bounds";
        }
        return StringView {
            data: this->data + start,
            length: length
        };
    }
    addr_t find(String other, addr_t offset = 0) {
        if (offset > this->length) {
            return String::NOT_FOUND;
        }
        if (other.length == 0) {
            return offset;
        }
        if (other.length > this->length - offset) {
            return String::NOT_FOUND;
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
        return String::NOT_FOUND;
    }
    bool contains(String other) {
        return this->find(other) != String::NOT_FOUND;
    }
    bool startsWith(String other) {
        return this->find(other) == 0a;
    }
    bool endsWith(String other) {
        if (other.length > this->length) return false;
        return this->find(other, this->length - other.length) == this->length - other.length;
    }
    bool isEmpty() {
        return this->length == 0a;
    }
    private bool eq(String other) {
        if (this->length != other.length) return false;
        for (addr_t i = 0; i < this->length; i++) {
            if (this->data[i] != other.data[i]) return false;
        }
        return true;
    }
    bool operator==(String other) {
        return this->eq(other);
    }
    bool operator!=(String other) {
        return !this->eq(other);
    }
    bool _eval() {
        return !this->isEmpty();
    }
    String operator+(String other) {
        char *data_2 = `malloc((sizeof char) * (this->length + other.length + 1));
        for (addr_t i = 0; i < this->length; i++) {
            data_2[i] = this->data[i];
        }
        for (addr_t i = 0; i < other.length; i++) {
            data_2[i + this->length] = other.data[i];
        }
        data_2[this->length + other.length] = '\0';
        return String(data_2, this->length + other.length);
    }
    void operator+=(String other) {
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
    void insert(addr_t index, String str) {
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
    T toInteger<T(numeric:)>() {
        if (this->length == 0) {
            panic "cannot parse an empty string as a whole number";
        }
        addr_t i = 0;
        bool negative = false;
        if (this->data[0] == '-') {
            negative = true;
            i++;
        } else if (this->data[0] == '+') {
            i++;
        }
        if (i == this->length) {
            panic "invalid whole number";
        }
        T value = 0;
        for (; i < this->length; i++) {
            char c = this->data[i];
            if (c < '0' || c > '9') {
                panic "invalid character in whole number";
            }
            T digit = c - '0';
            value = value * 10 + digit;
        }
        if (negative) {
            value = -value;
        }
        return value;
    }
    T toFloating<T(numeric:)>() {
        if (this->length == 0) {
            panic "cannot parse an empty string as a floating-point number";
        }
        addr_t i = 0;
        bool negative = false;
        if (this->data[0] == '-') {
            negative = true;
            i++;
        } else if (this->data[0] == '+') {
            i++;
        }
        if (i == this->length) {
            panic "invalid floating-point number";
        }
        T value = 0;
        T divisor = 1;
        bool decimal = false;
        bool has_digit = false;
        for (; i < this->length; i++) {
            char c = this->data[i];
            if (c == '.') {
                if (decimal) {
                    panic "multiple decimal points in floating-point number";
                }
                decimal = true;
                continue;
            }
            if (c < '0' || c > '9') {
                panic "invalid character in floating-point number";
            }
            has_digit = true;
            T digit = c - '0';
            if (decimal) {
                divisor *= 10;
                value += digit / divisor;
            } else {
                value = value * 10 + digit;
            }
        }
        if (!has_digit) {
            panic "invalid floating-point number";
        }
        if (negative) {
            value = -value;
        }
        return value;
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
    char *_repr() {
        return this->data;
    }
    char *c_str() {
        return this->data;   
    }
}
String toString<T(numeric:)>(T value) {
    if (value == 0) {
        return String("0");
    }
    bool negative = false;
    if (is_signed<T>() && value < 0) {
        negative = true;
        value = -value;
    }
    char buffer[64];
    addr_t i = 0;
    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    if (negative) {
        buffer[i++] = '-';
    }
    for (addr_t j = 0; j < i / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
    buffer[i] = '\0';
    return String(buffer);
}
int main() {
    String s = "hello, world!";
    defer s._destroy();
    IO::qout("s", s.c_str());
    return 0;
}
