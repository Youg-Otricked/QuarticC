namespace Exported {
    #include <IO, std>
}
class NoVtable {
    NoVtable() {}
    void no_virtual_methods() {
    }
}
class B {
    B() {
    }
    virtual int getNum() {
        return 1;
    }
    virtual double getDoub() {
        return 1.2;
    }
}
class C : B {
    C() : B() {
    }
    double getDoub() override {
        return 2.1;
    }
    virtual void other() {
    }
    int getNum() override {
        return 3;
    }
}
class G<T> : B {
    G() : B() {
    }
    double getDoub() override {
        return 123.0;
    }
}
int main() {
    B b = B();
    B *p = &b;
    IO::qout("%i, %d\n", p->getNum(), p->getDoub());
    *p = C();
    IO::qout("%i, %d\n", p->getNum(), p->getDoub());
    *p = G<int>();
    IO::qout("%d\n", p->getDoub());
    return 0;
}
