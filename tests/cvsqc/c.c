#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
typedef struct {
    void** items;
    int count;
    int current_index;
} variadic;
bool variadic_is_empty(variadic* v) {
    return v->current_index >= v->count;
}
void* variadic_next(variadic* v) {
    if (variadic_is_empty(v)) {
        return NULL;
    }
    void* data = v->items[v->current_index];
    v->current_index++;
    return data;
}
int main() {
    variadic* v = malloc(sizeof(variadic));
    v->items = NULL;
    v->count = 1; 
    v->current_index = 0;
    int x = 123;
    int* xp = &x;
    v->items = (void**)&xp;
    bool b = variadic_is_empty(v);
    x = *((int*)variadic_next(v));
    free(v);
    printf("%i%p%i", x, xp, b);
}