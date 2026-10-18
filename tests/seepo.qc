concept Addable {
    1_of {
        int did_i_seriously_never_test_this();
    }
}
class C {
    static int static_works(int a, int b) {
        return a + b;
    }
    int add(int a, int b) {
        return a + b;
    }
    C(){}
}
C proves Addable with_proof {
        int did_i_seriously_never_test_this() { return 123; }
};
class G<T> {
    G(){}
}
G proves Addable with_proof {
        int did_i_seriously_never_test_this() { return 9; }
};
int add(int a, int b) {
    return a + b;
}
int main() {
    C c = C();
    c.add(123, 321);
    `qout("%i\n", C::static_works(12, 23));
    `qout("%i\n", c.did_i_seriously_never_test_this());
    return add(0, 0);
}
