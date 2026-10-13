#ifndef TEST_INPUT_H
#define TEST_INPUT_H
#define MAX_BUFFER_SIZE 1024
#define PI 3.14159
typedef unsigned long long u64;
typedef enum { STATUS_OK, STATUS_ERROR, STATUS_PENDING } Status_t;
struct Point {
    int x;
    int y;
};
typedef struct {
    struct Point position;
    u64 id;
    Status_t current_status;
} Entity_t;
extern int global_counter;
extern const char* VERSION_STRING;
int calculate_distance(struct Point p1, struct Point p2);
Entity_t* create_entity(u64 id, int initial_x, int initial_y);
#endif
