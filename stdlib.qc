// This is the QuarticC Standerd Library

int main() {
    `qout("QuarticC Standerd Lib. Don't run this file. include from it\n");
    return 0;
}
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
namespace Array {
    namespace Iterator {
        class It<T> {
            T* data;
            int size;
            int current_index;
            It(T* data, int size, bool is_end) {
                this->data = data;
                this->size = size;
                this->current_index = `ternary(is_end, size, 0);
            }
            bool _atEnd() {
                return this->size <= this->current_index;
            }
            T _next() {
                if (!this->_atEnd()) {
                    return this->data[this->current_index++];
                }
                return this->data[this->size - 1]; 
            } 
            bool _atStart() {
                return this->current_index <= 0;
            }
            T _prev() {
                if (!this->_atStart()) {
                    return this->data[--this->current_index];
                }
                return this->data[this->current_index];
            }
            void _moveTo(int index) {
                if (index >= this->size) {
                    index = this->size - 1;
                } else if (index < 0) {
                    index = 0;
                }
                this->current_index = index;
            }
        }
    }
    class Arr<T, int S = 0> {
        T* data;
        int size;
        Arr() {
            this->data = nullptr;
            this->size = 0;
        }
        void operator[]=(T* data, int length) {
            if (this->size != 0) `free(this->data);
            if (S == 0) {
                this->size = length;
            } else {
                this->size = `ternary((length > S), S, length);
            }
            this->data = `malloc((sizeof T) * this->size);
            for (int i = 0; i < this->size; i++) {
                this->data[i] = data[i];
            }
        }
        T& operator[](addr_t index) {
            return this->data[index];
        }
        Iterator::It<T> _begin() {
            return Iterator::It<T>(this->data, this->size, false);
        }
        Iterator::It<T> _end() {
            return Iterator::It<T>(this->data, this->size, true);
        }
        void _destroy() {
            `free(this->data);
            this->data = nullptr;
            this->size = -1;
        }
        Arr<T, S>& operator=(Arr<T, S> other) {
            if (other.size != this->size) {
                return Arr();
            }
            for (int i = 0; i < other.size; i++) {
                this->data[i] = other.data[i];
            }
            return this;
        }
    }
    Arr proves CoreConcepts::Allocated;
}
namespace Vector {
    namespace Iterator {
        class It<T> {
            T* data;
            int size;
            int current_index;
            It(T* data, int size, bool is_end) {
                this->data = data;
                this->size = size;
                this->current_index = `ternary(is_end, size, 0);
            }
            bool _atEnd() {
                return this->size <= this->current_index;
            }
            T _next() {
                if (!this->_atEnd()) {
                    return this->data[this->current_index++];
                }
                return this->data[this->size - 1];
            } 
            bool _atStart() {
                return this->current_index <= 0;
            }
            T _prev() {
                if (!this->_atStart()) {
                    return this->data[--this->current_index];
                }
                return this->data[this->current_index];
            }
            void _moveTo(int index) {
                if (index >= this->size) {
                    index = this->size - 1;
                } else if (index < 0) {
                    index = 0;
                }
                this->current_index = index;
            }
        }
    }
    class Vec<T> {
        T* data;
        int size;
        int capacity;
        Vec() {
            this->size = 0;
            this->capacity = 0;
            this->data = nullptr;
        }
        void reserve(int cap) {
            if (cap == 0) {
                `free(this->data);                
                this->data = nullptr;
                this->capacity = 0;
                this->size = 0;
                return;
            }

            T* new_data = `realloc(this->data, (sizeof T) * cap);
            if (new_data == nullptr) {
                return;
            }
            this->capacity = cap;
            if (this->size > this->capacity) {
                this->size = this->capacity;
            }
            this->data = new_data;
        }
        void push(T value) {
            if (this->capacity <= this->size) {
                this->reserve(`ternary(this->capacity == 0, 1, this->capacity * 2));
                if (this->data == nullptr) return;
            }
            this->data[this->size] = value;
            this->size++;
        }
        int length() {
            return this->size;
        }
        void pop() {
            if (this->size > 0) {
                this->size--;
            }
        }
        void shrinkToFit() {
            this->reserve(this->size);
        }
        void operator[]=(T* data, addr_t length) {
            if (this->size != 0) `free(this->data);
            this->capacity = length;
            this->size = length;
            this->data = `malloc((sizeof T) * this->capacity);
            for (int i = 0; i < this->size; i++) {
                this->data[i] = data[i];
            }
        }
        T& operator[](addr_t index) {
            return this->data[index];
        }
        Iterator::It<T> _begin() {
            return Iterator::It<T>(this->data, this->size, false);
        }
        Iterator::It<T> _end() {
            return Iterator::It<T>(this->data, this->size, true);
        }
        void _destroy() {
            `free(this->data);
            this->data = nullptr;
            this->size = -1;
            this->capacity = -1;
        }
        Vec<T>& operator=(Vec<T> other) {
            if (other.capacity != this->capacity) {
                this->reserve(other.capacity);
            }
            this->size = other.size;
            for (int i = 0; i < other.size; i++) {
                this->data[i] = other.data[i];
            }
            return this;
        }
        bool isEmpty() {
            return this->size == 0;
        }
    }
    Vec proves CoreConcepts::Allocated;
}
namespace List {
    class Node<T> {
        T data;
        Node<T>* next;
        Node<T>* prev;

        Node(T data) {
            this->data = data;
            this->next = nullptr;
            this->prev = nullptr;
        }
    }

    namespace Iterator {
        class It<T> {
            Node<T>* current;

            It(Node<T>* node) {
                this->current = node;
            }

            bool _atEnd() {
                return this->current == nullptr;
            }

            T _next() {
                T value = this->current->data;
                this->current = this->current->next;
                return value;
            }

            bool _atStart() {
                return this->current->prev == nullptr;
            }

            T _prev() {
                T value = this->current->data;
                this->current = this->current->prev;
                return value;
            }
        }
    }

    class List<T> {
        Node<T>* head;
        Node<T>* tail;
        int size;
        List() {
            this->head = nullptr;
            this->tail = nullptr;
            this->size = 0;
        }
        private Node<T>* _getNode(int index) {
            if (index < 0 || index >= this->size) {
                return nullptr;
            }
            Node<T>* current;
            if (index < this->size / 2) {
                current = this->head;
                for (int i = 0; i < index; i++) {
                    current = current->next;
                }
            } else {
                current = this->tail;
                for (int i = this->size - 1; i > index; i--) {
                    current = current->prev;
                }
            }
            return current;
        }
        bool isEmpty() {
            return this->size == 0;
        }
        T front() {
            return this->head->data;
        }
        T back() {
            return this->tail->data;
        }
        void pushFront(T value) {
            Node<T>* node = `malloc(sizeof Node<T>);
            *node = Node<T>(value);
            node.next = this->head;
            if (this->head != nullptr) {
                this->head->prev = node;
            } else {
                this->tail = node;
            }
            this->head = node;
            this->size++;
        }
        void pushBack(T value) {
            Node<T>* node = `malloc(sizeof Node<T>);
            *node = Node<T>(value);
            node.prev = this->tail;
            if (this->tail != nullptr) {
                this->tail->next = node;
            } else {
                this->head = node;
            }
            this->tail = node;
            this->size++;
        }
        void popFront() {
            if (this->head == nullptr) {
                return;
            }
            Node<T>* old = this->head;
            this->head = this->head->next;
            if (this->head != nullptr) {
                this->head->prev = nullptr;
            } else {
                this->tail = nullptr;
            }
            `free(old);
            this->size--;
        }
        void popBack() {
            if (this->tail == nullptr) {
                return;
            }
            Node<T>* old = this->tail;
            this->tail = this->tail->prev;
            if (this->tail != nullptr) {
                this->tail->next = nullptr;
            } else {
                this->head = nullptr;
            }
            `free(old);
            this->size--;
        }
        void clear() {
            Node<T>* current = this->head;
            while (current != nullptr) {
                Node<T>* next = current->next;
                `free(current);
                current = next;
            }
            this->head = nullptr;
            this->tail = nullptr;
            this->size = 0;
        }
        bool contains(T value) {
            Node<T>* current = this->head;
            while (current != nullptr) {
                if (current->data == value) {
                    return true;
                }
                current = current->next;
            }
            return false;
        }
        Iterator::It<T> _begin() {
            return Iterator::It<T>(this->head);
        }
        Iterator::It<T> _end() {
            return Iterator::It<T>(nullptr);
        }
        void _destroy() {
            this->clear();

            this->head = nullptr;
            this->tail = nullptr;
            this->size = -1;
        }
        T& operator[](addr_t index) {
            Node<T>* node = this->_getNode(index);
            if (node == nullptr) {
                return nullptr;
            }
            return node.data;
        }
        void operator[]=(T* data, addr_t length) {
            while (this->size != 0) this->popBack();
            this->head = nullptr;
            this->tail = nullptr;
            for (int i = 0; i < length; i++) {
                this->pushBack(data[i]);
            }
        }
        List<T>& operator=(List<T> other) {
            this->clear();

            Node<T>* current = other.head;

            while (current != nullptr) {
                this->pushBack(current->data);
                current = current->next;
            }

            return this;
        }
    }
    List proves CoreConcepts::Allocated;
}
namespace Utils {
    Vector::Vec<int> range(int start, int stop, int step = 1) {
        Vector::Vec<int> fin = Vector::Vec<int>();
        if (step > 0 && start <= stop) {
            for (; start < stop; start += step) {
                fin.push(start);
            }
        }
        else if (step < 0 && start >= stop) {
            for (; start > stop; start += step) {
                fin.push(start);
            }
        }
        else {
            return Vector::Vec<int>();
        }
        return fin;
    }
    struct _timespec {
        long int rem_sec;
        long int rem_nsec;
    }
    volatile void sleep(long int milliseconds) {
        volatile long int req[2] = [milliseconds / 1000, (milliseconds % 1000) * 1000000];
        volatile _timespec rem = {0, 0};
        while (true) {
            volatile long int result = 0;
            `inline(R"(
                mov rax, 230
                mov rdi, 1
                mov rsi, 0
                mov rdx, $1r
                mov r10, $2r
                syscall
                mov $0=r, rax
            )", result, &(req[0]), &(rem.rem_sec), "~{rax}~{rdi}~{rsi}~{rdx}~{r10}~{rcx}~{r11}");
            if (result == 0) {
                break;
            }
            if (result == -4) {
                req[0] = rem.rem_sec;
                req[1] = rem.rem_nsec;
                continue;
            }
            break;
        }
    }
}
namespace Math {
    double e() {
        return 2.71828182845904523536;
    }
    
    double pi() {
        return 3.14159265358979323846;
    }
    type Number = int | float | double | long int | short int | long double | addr_t | byte | nibble;
    Number max(Number a, Number b) {
        if (a > b) {
            return a;
        }
        return b;
    }
    Number min(Number a, Number b) {
        if (a > b) {
            return b;
        }
        return a;
    }
    Number sqrt(Number val) {
        return `to_double(val) #^ 0.5;
    }
    Number root(Number val, Number power) {
        double base = `to_double(val);
        double exp  = 1.0 / `to_double(power);
        return base #^ exp;
    }
    type Floating = float | double;
    int ceil(Floating a) {
        double d = `to_double(a);
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
        double d = `to_double(a);
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
    Number abs(Number a) {
        if (a < 0) {
            return a * -1;
        }
        return a;
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
    double log(double x) {
        if (x <= 0) {
            throw "log: x must be positive";
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
namespace OSInterop {
    void system(string command) {
        `flush();
        `inline(R"(
        mov rdi, $0r
        call system
        )", command, "~{rdi}");
    }
}
namespace Pair {
    struct Pair<A, B> {
        A first;
        B second;
    }
    Pair<A, B> makePair<A, B>(A first, B second) {
        return Pair<A, B>{first, second};
    }
}
namespace Optional {
    class Option<T> {
        private T data;
        bool has_value;
        Option() {
            this->has_value = false;
        }
        Option(T value) {
            this->data = value;
            this->has_value = true;
        }
        T& value() {
            if (!this->has_value) throw "No value";
            return this->data;
        }
        bool safeGet(T& output) {
            if (!this->has_value) return false;
            output = this->data;
            return true;
        }
        void setValue(T& val) {
            this->data = val;
            this->has_value = true;
        }
        void removeValue() {
            this->has_value = false;
        }
    }
}
namespace Span {
    namespace Iterator {
        class It<T> {
            T* data;
            int size;
            int current_index;
            It(T* data, int size, bool is_end) {
                this->data = data;
                this->size = size;
                this->current_index = `ternary(is_end, size, 0);
            }
            bool _atEnd() {
                return this->size <= this->current_index;
            }
            T _next() {
                if (!this->_atEnd()) {
                    return this->data[this->current_index++];
                }
                return this->data[this->size - 1];
            } 
            bool _atStart() {
                return this->current_index <= 0;
            }
            T _prev() {
                if (!this->_atStart()) {
                    return this->data[--this->current_index];
                }
                return this->data[this->current_index];
            }
            void _moveTo(int index) {
                if (index >= this->size) {
                    index = this->size - 1;
                } else if (index < 0) {
                    index = 0;
                }
                this->current_index = index;
            }
        }
    }
    class Span<T> {
        T* data;
        int length;
        T& operator[](addr_t index) {
            if (index >= this->length) throw "Cannot index past end of a Span";
            return this->data[index];
        }
        Span() {
            this->length = 0;
            this->data = nullptr;
        }
        void operator[]=(T *data, addr_t length) {
            this->length = length;
            this->data = data;
        }
        Iterator::It<T> _begin() {
            return Iterator::It<T>(this->data, this->length, false);
        }
        Iterator::It<T> _end() {
            return Iterator::It<T>(this->data, this->length, true);
        }
    }
}
namespace Encodings {
    namespace Hex {
        const string chars = "0123456789abcdef";
        string encode(string data) {
            string result = "";
            for (int i = 0; i < `len(data); i++) {
                char c = data[i];
                nibble lower = c & 0x0fy;
                nibble upper = (c :> 4y) & 0x0fy;
                result += chars[upper];
                result += chars[lower];
            }
            return result;
        }   
        string decode(string hex) {
            string ascii = "";
            int i = 0;
            int j = 0;
            while (hex[i] != '\0' && hex[i + 1] != '\0') {
                int high = hex[i];
                int low = hex[i + 1];
                high = (high >= 'a') ? (high - 'a' + 10) :  (high >= 'A') ? (high - 'A' + 10) : (high - '0');
                low = (low >= 'a') ? (low - 'a' + 10) : (low >= 'A') ? (low - 'A' + 10) : (low - '0');
                ascii += `to_char((high << 4) | low);
                i += 2;
            }
            return ascii;
        }
    }
    namespace Base64 {
        const string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        string encode(string data) {
            string result = "";
            int i = 0;
            int len = `len(data);
            while (i < len) {
                int b0 = data[i];
                int b1 = (i + 1 < len) ? `to_int(data[i + 1]) : 0;
                int b2 = (i + 2 < len) ? `to_int(data[i + 2]) : 0;
                int idx0 = (b0 :> 2) & 0x3f;
                int idx1 = ((b0 << 4) | (b1 :> 4)) & 0x3f;
                int idx2 = ((b1 << 2) | (b2 :> 6)) & 0x3f;
                int idx3 = b2 & 0x3f;
                result += chars[idx0];
                result += chars[idx1];
                result += (i + 1 < len) ? chars[idx2] : '=';
                result += (i + 2 < len) ? chars[idx3] : '=';
                i += 3;
            }
            return result;
        }
        int charValue(char c) {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return 0;
        }
        string decode(string data) {
            string result = "";
            int i = 0;
            int len = `len(data);
            while (i < len) {
                int v0 = charValue(data[i]);
                int v1 = charValue(data[i + 1]);
                int v2 = (data[i + 2] != '=') ? charValue(data[i + 2]) : 0; // THE HELL IS THIS
                int v3 = (data[i + 3] != '=') ? charValue(data[i + 3]) : 0;
                int b0 = ((v0 << 2) | (v1 :> 4)) & 0xff;
                int b1 = ((v1 << 4) | (v2 :> 2)) & 0xff;
                int b2 = ((v2 << 6) | v3) & 0xff;
                result += `to_char(b0);
                if (data[i + 2] != '=') result += `to_char(b1);
                if (data[i + 3] != '=') result += `to_char(b2);
                i += 4;
            }
            return result;
        }
    }
    namespace URI {
        bool isUnreserved(char c) {
            if (c >= 'A' && c <= 'Z') return true;
            if (c >= 'a' && c <= 'z') return true;
            if (c >= '0' && c <= '9') return true;
            if (c == '-' || c == '_' || c == '.' || c == '~') return true;
            return false;
        }
        string encode(string data) {
            string result = "";
            for (int i = 0; i < `len(data); i++) {
                char c = data[i];
                if (isUnreserved(c)) {
                    result += c;
                } else {
                    result += '%';
                    result += Hex::chars[(c :> 4) & 0x0fb];
                    result += Hex::chars[c & 0x0fb];
                }
            }
            return result;
        }
        string decode(string data) {
            string result = "";
            int i = 0;
            int len = `len(data);
            while (i < len) {
                if (data[i] == '%' && i + 2 < len) {
                    string byteStr = "";
                    byteStr += data[i + 1];
                    byteStr += data[i + 2];
                    string decoded = Hex::decode(byteStr);
                    result += decoded[0];
                    i += 3;
                } else {
                    result += data[i];
                    i += 1;
                }
            }
            return result;
        }
    }
}
namespace Unordered {
    concept Hash {
        1_of {
            addr_t hash();
            addr_t hash(Self self);
        }
    }
    class SetNode<T> {
        T value;
        SetNode<T>* next;
    }
    class MapNode<K, V> {
        Pair::Pair<K, V> data;
        MapNode<K, V>* next;
    }
    namespace Iterators {
        class MapIt<K, V> {
            MapNode<K, V>** buckets;
            int num_buckets;
            int bucket_index;
            MapNode<K, V>* current;

            MapIt(MapNode<K, V>** buckets, int num_buckets, bool is_end) {
                this->buckets = buckets;
                this->num_buckets = num_buckets;
                if (is_end) {
                    this->bucket_index = num_buckets;
                    this->current = nullptr;
                } else {
                    this->bucket_index = 0;
                    this->current = nullptr;
                    this->_advanceToNext();
                }
            }
            private void _advanceToNext() {
                while (this->current == nullptr && this->bucket_index < this->num_buckets) {
                    this->current = this->buckets[this->bucket_index];
                    if (this->current == nullptr) this->bucket_index++;
                }
            }
            bool _atEnd() {
                return this->bucket_index >= this->num_buckets;
            }
            Pair::Pair<K, V>& _next() {
                Pair::Pair<K, V>& val = this->current->data;
                this->current = this->current->next;
                if (this->current == nullptr) {
                    this->bucket_index++;
                    this->_advanceToNext();
                }
                return val;
            }
        }
        class SetIt<T> {
            SetNode<T>** buckets;
            int num_buckets;
            int bucket_index;
            SetNode<T>* current;

            SetIt(SetNode<T>** buckets, int num_buckets, bool is_end) {
                this->buckets = buckets;
                this->num_buckets = num_buckets;
                if (is_end) {
                    this->bucket_index = num_buckets;
                    this->current = nullptr;
                } else {
                    this->bucket_index = 0;
                    this->current = nullptr;
                    this->_advanceToNext();
                }
            }
            private void _advanceToNext() {
                while (this->current == nullptr && this->bucket_index < this->num_buckets) {
                    this->current = this->buckets[this->bucket_index];
                    if (this->current == nullptr) this->bucket_index++;
                }
            }
            bool _atEnd() {
                return this->bucket_index >= this->num_buckets;
            }
            T _next() {
                T val = this->current->value;
                this->current = this->current->next;
                if (this->current == nullptr) {
                    this->bucket_index++;
                    this->_advanceToNext();
                }
                return val;
            }
        }
    }
    class Set<T(proves Hash:)> {
        private int num_buckets;
        private int count;
        private SetNode<T>** buckets;
        private int get_Bucket_Index(T item) {
            return item.hash() % this->num_buckets;
        }
        Set() {
            this->num_buckets = 8;
            this->count = 0;
            this->buckets = `malloc((sizeof SetNode<T>*) * this->num_buckets);
            for (int i = 0; i < this->num_buckets; i++) {
                this->buckets[i] = nullptr;
            }
        }
        void operator[]=(T *data, addr_t length) {
            if (num_buckets == 0) this->Set();
            for (int i = 0; i < length; i++) {
                this->add(data[i]);
            }
        }
        bool contains(T item) {
            comptime if (!(CoreConcepts::Eq proved_by T)) {
                `compile_error("T must prove CoreConcepts::Eq to use contains");
                return false;
            }
            int index = this->get_Bucket_Index(item);
            SetNode<T>* node = this->buckets[index];
            while (node != nullptr) {
                if (node->value == item) return true;
                node = node->next;
            }
            return false;
        }
        private void resize() {
            int old_num_buckets = this->num_buckets;
            SetNode<T>** old_buckets = this->buckets;
            this->num_buckets = old_num_buckets * 2;
            this->buckets = `malloc((sizeof SetNode<T>*) * this->num_buckets);
            for (int i = 0; i < this->num_buckets; i++) {
                this->buckets[i] = nullptr;
            }
            for (int i = 0; i < old_num_buckets; i++) {
                SetNode<T>* node = old_buckets[i];
                while (node != nullptr) {
                    SetNode<T>* next = node->next;
                    int newIndex = node->value.hash() % this->num_buckets;
                    node->next = this->buckets[newIndex];
                    this->buckets[newIndex] = node;
                    node = next;
                }
            }
            `free(old_buckets);
        }
        void add(T item) {
            if (this->contains(item)) return;
            if (this->count + 1 > this->num_buckets * 0.75) {
                this->resize();
            }
            int index = this->get_Bucket_Index(item);
            SetNode<T>* node = `malloc(sizeof SetNode<T>);
            node->value = item;
            node->next = this->buckets[index];
            this->buckets[index] = node;
            this->count++;
        }
        void _destroy() {
            for (int i = 0; i < this->num_buckets; i++) {
                SetNode<T>* node = this->buckets[i];
                comptime if (CoreConcepts::Allocated proved_by T) {
                    node->value._destroy();
                }
                while (node != nullptr) {
                    SetNode<T>* next = node->next;
                    `free(node);
                    node = next;
                }
            }
            `free(this->buckets);
        }
        Iterators::SetIt<T> _begin() {
            return Iterators::SetIt<T>(this->buckets, this->num_buckets, false);
        }
        Iterators::SetIt<T> _end() {
            return Iterators::SetIt<T>(this->buckets, this->num_buckets, true);
        }
    }
    Set proves CoreConcepts::Allocated; 
    class Map<K(proves Hash:), V> {
        private int num_buckets;
        private int count;
        private MapNode<K, V>** buckets;
        private int get_Bucket_Index(K key) {
            return key.hash() % this->num_buckets;
        }
        Map() {
            this->num_buckets = 8;
            this->count = 0;
            this->buckets = `malloc((sizeof MapNode<K, V>*) * this->num_buckets);
            for (int i = 0; i < this->num_buckets; i++) {
                this->buckets[i] = nullptr;
            }
        }
        bool containsKey(K key) {
            int index = this->get_Bucket_Index(key);
            MapNode<K, V>* node = this->buckets[index];
            while (node != nullptr) {
                if (node->data.first.equals(key)) return true;
                node = node->next;
            }
            return false;
        }
        bool safeGet(K key, V& output) {
            int index = this->get_Bucket_Index(key);
            MapNode<K, V>* node = this->buckets[index];
            while (node != nullptr) {
                if (node->data.first.equals(key)) {
                    output = node->data.second;
                    return true;
                }
                node = node->next;
            }
            return false;
        }
        void set(K key, V value) {
            int index = this->get_Bucket_Index(key);
            MapNode<K, V>* node = this->buckets[index];
            while (node != nullptr) {
                if (node->data.first.equals(key)) {
                    node->data.second = value;
                    return;
                }
                node = node->next;
            }
            if ((this->count + 1) * 4 > this->num_buckets * 3) {
                this->resize();
                index = this->get_Bucket_Index(key);
            }
            MapNode<K, V>* newNode = `malloc(sizeof MapNode<K, V>);
            newNode->data.first = key;
            newNode->data.second = value;
            newNode->next = this->buckets[index];
            this->buckets[index] = newNode;
            this->count++;
        }
        private void resize() {
            int old_num_buckets = this->num_buckets;
            MapNode<K, V>** old_buckets = this->buckets;
            this->num_buckets = old_num_buckets * 2;
            this->buckets = `malloc((sizeof MapNode<K, V>*) * this->num_buckets);
            for (int i = 0; i < this->num_buckets; i++) {
                this->buckets[i] = nullptr;
            }
            for (int i = 0; i < old_num_buckets; i++) {
                MapNode<K, V>* node = old_buckets[i];
                while (node != nullptr) {
                    MapNode<K, V>* next = node->next;
                    int newIndex = node->data.first.hash() % this->num_buckets;
                    node->next = this->buckets[newIndex];
                    this->buckets[newIndex] = node;
                    node = next;
                }
            }
            `free(old_buckets);
        }
        void _destroy() {
            for (int i = 0; i < this->num_buckets; i++) {
                MapNode<K, V>* node = this->buckets[i];
                while (node != nullptr) {
                    comptime if (CoreConcepts::Allocated proved_by V) {
                        node->data.second._destroy();
                    }
                    comptime if (CoreConcepts::Allocated proved_by K) {
                        node->data.first._destroy();
                    }
                    MapNode<K, V>* next = node->next;
                    `free(node);
                    node = next;
                }
            }
            `free(this->buckets);
        }
        void operator[]=(Pair::Pair<K, V> *data, int length) {
            if (this->num_buckets == 0) this->Map(); 
            for (int i = 0; i < length; i++) {
                this->set(data[i].first, data[i].second);
            }
        }
        V& operator[](K key) {
            int index = this->getBucketIndex(key);
            MapNode<K, V>* node = this->buckets[index];
            while (node != nullptr) {
                if (node->data.first.equals(key)) return node->data.second;
                node = node->next;
            }
            throw "Key not found";
        }
        Iterators::MapIt<K, V> _begin() {
            return Iterators::MapIt<K, V>(this->buckets, this->num_buckets, false);
        }
        Iterators::MapIt<K, V> _end() {
            return Iterators::MapIt<K, V>(this->buckets, this->num_buckets, true);
        }

    }
    Map proves CoreConcepts::Allocated;
}
/*
namespace Ordered {
    concept Ord {
        at_least 1_of {
            bool operator<(Self other);
            bool operator<(Self self, Self other);
        }
    }
}
*/
namespace Filesystem {
    class Path;
    Path getCwd();
    string realpath(string input_path) {
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
        DateTime lastAccess_Time() {
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
                throw "Cannot read a empty file";
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
            if (!this->isOpen()) throw "Cannot get a uninitialized InputStream";
            int bytesRead = `read(this->fd, &c, 1);
            this->most_recent = bytesRead;
            if (bytesRead != 1) return '\0';
            return c;
        }
        void getline(string dest, char till = '\n') {
            if (!this->isOpen()) {
                throw "Cannot get a uninitializd InputStream";
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
                throw "Cannot get a uninitializd InputStream";
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
                throw "Cannot write to an empty file";
            }
            if (this->isOpen()) {
                this->close();
            }
            this->fd = `open(path.absolute().toString(), "r+");
            if (this->fd == -1) {
                throw "Cannot open file for writing";
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
                throw "Cannot put to an uninitialized OutputStream";
            }
            `write(this->fd, &c);
        }
        void write(string data) {
            if (!this->isOpen()) {
                throw "Cannot write to an uninitialized OutputStream";
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
                throw "Cannot read/write to an empty file";
            }
            if (this->isOpen()) {
                this->close();
            }
            this->fd = `open(path.absolute().toString(), "r+");
            if (this->fd == -1) {
                throw "Cannot open file for reading/writing";
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
                throw "Cannot put to an uninitialized RWStream";
            }
            `write(this->fd, &c);
        }
        void write(string data) {
            if (!this->isOpen()) {
                throw "Cannot write to an uninitialized RWStream";
            }
            `write(this->fd, data);
        }
        char get() {
            char c;
            if (!this->isOpen()) throw "Cannot get a uninitialized RWStream";
            int bytesRead = `read(this->fd, &c, 1);
            this->most_recent_read = bytesRead;
            if (bytesRead != 1) return '\0';
            return c;
        }
        void getline(string dest, char till = '\n') {
            if (!this->isOpen()) {
                throw "Cannot get a uninitializd RWStream";
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
                throw "Cannot get a uninitializd RWStream";
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
                throw "Cannot iterate an empty directory";
            }
            if (!path.isDirectory()) {
                throw "Cannot iterate a non-directory";
            }
            this->dir = `opendir(path.absolute().toString());
            if (!this->dir) {
                throw "Cannot open directory";
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
                throw "DirectoryIterator has no next entry";
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
    bool createDirectory(Path path, int mode = 755) {
        if (path.empty()) {
            throw "Cannot create an empty directory";
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
            throw "Failed to create directory";
        }
        return true;
    }
    bool createDirectories(Path path) {
        if (path.empty()) {
            throw "Cannot create an empty directory";
        }
        if (path.exists()) {
            if (!path.isDirectory()) {
                throw "Path exists and is not a directory";
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
            throw "Cannot remove an empty path";
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
            throw "Failed to remove path";
        }
        return true;
    }
    bool removeAll(Path path) {
        if (path.empty()) {
            throw "Cannot remove an empty path";
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
            throw "Cannot rename using an empty path";
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
            throw "Failed to rename path";
        }
        return true;
    }
    void copy(Path source, Path destination) {
        if (source.empty() || destination.empty()) {
            throw "Cannot copy using an empty path";
        }
        if (!source.exists()) {
            throw "Source does not exist";
        }
        if (source.isDirectory() && !source.isSymlink()) {
            if (destination.exists()) {
                if (!destination.isDirectory()) {
                    throw "Destination exists and is not a directory";
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
namespace Owned {
    class Owned<T> {
        private T *data;
        Owned(T *data) {
            this->data = data;
        }
        T *own() {
            return this->data;
        }
        void ignore() {
            `free(this->data);
            this->data = nullptr;
        }
        void _destroy() {
            `free(this->data);
        }
    }
    Owned proves CoreConcepts::Allocated;
}
