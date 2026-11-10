// This is the QuarticC Standerd Library
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
                if (this->size == 0) {
                    panic "attempted to iterate a empty array";
                }
                if (!this->_atEnd()) {
                    return this->data[this->current_index++];
                }
                return this->data[this->size - 1]; 
            } 
            bool _atStart() {
                return this->current_index <= 0;
            }
            T _prev() {
                if (this->size == 0) {
                    panic "attempted to iterate a empty array";
                }
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
                if (this->size == 0) {
                    panic "attempted to iterate a empty vector";
                }
                if (!this->_atEnd()) {
                    return this->data[this->current_index++];
                }
                return this->data[this->size - 1];
            } 
            bool _atStart() {
                return this->current_index <= 0;
            }
            T _prev() {
                if (this->size == 0) {
                    panic "attempted to iterate a empty vector";
                }
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
                if (!this->current) {
                    panic "attempted to iterate a empty list";
                }
                T value = this->current->data;
                this->current = this->current->next;
                return value;
            }

            bool _atStart() {
                return this->current->prev == nullptr;
            }

            T _prev() {
                if (!this->current) {
                    panic "attempted to iterate a empty list";
                }
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
    T max<T(numeric:)>(T a, T b) {
        if (a > b) {
            return a;
        }
        return b;
    }
    T min<T(numeric:)>(T a, T b) {
        if (a > b) {
            return b;
        }
        return a;
    }
    T sqrt<T(numeric:)>(T a, T b) {
        return `to_double(val) #^ 0.5;
    }
    T root<T(numeric:)>(T a, T b) {
        double base = `to_double(val);
        double exp  = 1.0 / `to_double(power);
        return base #^ exp;
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
namespace OSInterop {
    void system(char *command) {
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
            if (!this->has_value) panic "no value in option";
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
                if (this->size == 0) {
                    panic "cannot iterate an empty";
                }
                if (!this->_atEnd()) {
                    return this->data[this->current_index++];
                }
                return this->data[this->size - 1];
            } 
            bool _atStart() {
                return this->current_index <= 0;
            }
            T _prev() {
                if (this->size == 0) {
                    panic "cannot iterate an empty";
                }
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
            if (index >= this->length) panic "Cannot index past end of a Span";
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
                if (!this->current) {
                    panic "cannot iterate an empty";
                }
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
                if (!this->current) {
                    panic "cannot iterate an empty";
                }
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
            panic "Key not found";
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
        char *c_str() {
            return this->data;   
        }
    }
    Str toStr<T(numeric:)>(T value) {
        if (value == 0) {
            return Str("0");
        }
        bool negative = false;
        if (value < 0) {
            negative = true;
            value = -value;
        }
        char[64] buffer = [char, 64];
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
        return Str(buffer);
    }
    Str proves CoreConcepts::Allocated;
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
    IO::qout("QuarticC Standard Library. Don't run this file. Include from it\n");
    return 0;
}
