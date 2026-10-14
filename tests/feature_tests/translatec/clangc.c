static inline int inline_function(int value) {
    return value + 1;
}
enum Color { COLOR_RED, COLOR_GREEN = 4, COLOR_BLUE, COLOR_CUSTOM = 100 };
enum SignedValues { SIGNED_NEGATIVE = -1, SIGNED_ZERO = 0, SIGNED_POSITIVE = 1 };
struct Point {
    int x;
    int y;
};
struct NamedNode {
    int value;
    struct NamedNode* next;
};
union U {
    int integer;
    float real;
};

struct AnonymousMembers {
    int tag;
    union U data;
};
union Number {
    int integer;
    float real;
    unsigned char bytes[sizeof(double)];
};
typedef struct Point Point;
typedef struct NamedNode Node;
typedef enum Color Color;
typedef union Number Number;
static Point global_point = {.x = 10, .y = 20};
static Color global_color = COLOR_GREEN;
static Number global_number = {.integer = 42};
typedef unsigned long Identifier;
typedef int IntegerArray[8];
typedef int (*BinaryOperation)(int, int);
typedef void (*Callback)(void*);
typedef int FunctionType(int);
typedef int (*FunctionPointerArray[3])(int);
static int one_dimensional[5] = {1, 2, 3, 4, 5};
static int matrix[2][3] = {{1, 2, 3}, {4, 5, 6}};
static const char ordinary_string[] = "ordinary string";
int add_integers(int left, int right);
void no_arguments(void);
int variadic_sum(int count, ...);
int compare_integers(const void* left, const void* right);
void callback_function(void* context);
int apply_operation(BinaryOperation operation, int a, int b);
int add_integers(int left, int right) {
    return left + right;
}
void no_arguments(void) {
    return;
}
static int subtract_integers(int left, int right) {
    return left - right;
}
int apply_operation(BinaryOperation operation, int a, int b) {
    return operation(a, b);
}
static int statement_tests(int value) {
    int result = 0;
    if (value < 0) {
        result = -value;
    } else if (value == 0) {
        result = 100;
    } else {
        result = value;
    }
    switch (value) {
    case 0: result += 1; break;
    case 1:
    case 2: result += 2; break;
    case 3: result += 3;
    default: result += 4; break;
    }
    for (int i = 0; i < 5; ++i) {
        if (i == 1) { continue; }
        if (i == 4) { break; }
        result += i;
    }
    {
        int i = 0;
        while (i < 2) { result += i++; }
    }
    {
        int i = 0;
        do {
            ++i;
            result += i;
        } while (i < 2);
    }
    goto finished;
finished:
    return result;
}
static Point make_point(int x, int y) {
    int p = global_point.x;
    global_point.x = x;
    return (Point){.x = x, .y = y};
}
