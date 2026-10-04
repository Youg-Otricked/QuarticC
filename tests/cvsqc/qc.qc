
struct variadic {
    void** items;
    int count;
    int current_index;
}
bool variadic_is_empty(variadic* v) {
    return v->current_index >= v->count;
}
void* variadic_next(variadic* v) {
    if (variadic_is_empty(v)) {
        return nullptr;
    }
    void* data = v->items[v->current_index];
    v->current_index++;
    return data;
}
int main() {
    variadic* v = `malloc(sizeof "variadic");
    v->items = nullptr;
    v->count = 1; 
    v->current_index = 0;
    int x = 123;
    int* xp = &x;
    v->items = &xp;
    bool b = variadic_is_empty(v);
    xp = variadic_next(v);
    x = *xp;
    `free(v);
    `qout("%i, %p, %b", x, xp, b);
}