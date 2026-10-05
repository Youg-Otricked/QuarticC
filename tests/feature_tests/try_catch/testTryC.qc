void throwA() {
    throw 123;
}

void throwB() {
    throw 123.0;
}
void test_single() {
    try {
        throwA();
    } catch(int e) {
        `qout("caught ErrorA: %i\n", e);
    }
}


void test_multiple() {
    try {
        throwB();
    } catch(int e) {
        `qout("wrong\n");
    } catch(double e) {
        `qout("caught ErrorB\n");
    }
}
void test_catch_all() {
    try {
        throwA();
    } catch(...) {
        `qout("caught anything\n");
    }
}
int main() {
    test_single();
    test_multiple();
    test_catch_all();
    return 0;
}
