pattern P {
    N;
    T(int, int);
    S {
        int x;
        int y;
    };
}
struct MS {
    int x;
}
class C {
    C() {}
    int y;
}
MS fits P(self) {
    N if (self->x == 0);
    T(self->x, -self->x) if (self->x < 10);
    S {
        x: self->x,
        y: 0
    } if (self->x < 20);
    default N;
}
C fits P(self) {
    default N;
}
int main() {
    C c = C();
    c.y = 0;
    MS ms = {19};
    match (c) {
        P.N => `qout("Worked");
        default => `qout("Failed");
    }
    match (ms) {
        P.S {x: x, y: y} => `qout("Worked");
        default => `qout("Failed");
    }
    return 0;
}
