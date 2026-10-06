class Coord {
    private int y;
    private int x;
    Coord(int x, int y) {
        this->x = x;
        this->y = y;
    }
    (int, int) operator{}() {
        return (this->x, this->y);
    }
}
(int, int) makeCoordinate(int x, int y) {
    return (x, y);
}
int, int unpackCoordinates((int, int) coord) {
    return coord.0, coord.1;
}
type Color = (byte, byte, byte);
int main() {
    (int, int) coord = makeCoordinate(-1, 21);
    int x, int y = unpackCoordinates(coord);
    (int, int) testAssignment = (23, 32);
    testAssignment = (0, 0);
    ((int, int), int) testNested = ((321, 123), -1);
    `qout("%u\n", sizeof ((int, int), void*));
    (int, int){ x, y } = coord;
    Coord custom = Coord(12, 21);
    (int, int){ x1, y1 } = custom;
    `qout("Custom destructure: (%i:%i)\n", x1, y1);
    return x + y - 20;
}

