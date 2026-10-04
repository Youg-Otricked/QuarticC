concept A {
    1_of {
        void a();
    }
}
concept B {
    1_of {
        void b();
    }
}
concept C {
    1_of {
        void c();
    }
}
struct AB {
    int x;
}
AB proves A with_proof {
    void a() {}
};
AB proves B with_proof {
    void b() {}
};
struct AC {
    int x;
}
AC proves A with_proof {
    void a() {}
};
AC proves C with_proof {
    void c() {}
};
struct OnlyA {
    int x;
}
OnlyA proves A with_proof {
    void a() {}
};
struct None {
    int x;
}
void test_and<T(proves A && B:)>(T value) {
    `qout("AND");
}
void test_or<T(proves A || B:)>(T value) {
    `qout("OR");
}
void test_not<T(proves A && !C:)>(T value) {
    `qout("NOT");
}
void test_precedence<T(proves A || B && !C:)>(T value) {
    `qout("PRECEDENCE");
}
int main() {
    AB ab = AB{1};
    AC ac = AC{2};
    OnlyA onlyA = OnlyA{3};
    test_and<AB>(ab);
    test_or<AC>(ac);
    test_not<OnlyA>(onlyA);
    test_precedence<AB>(ab);
    return 0;
}
