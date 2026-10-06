(int, int) makeCoordinate(int x, int y) {
    return (x, y);
}
int, int unpackCoordinates((int, int) coord) {
    return coord.0, coord.1;
}
int main() {
    (int, int) coord = makeCoordinate(-1, 21);
    int x, int y = unpackCoordinates(coord);
    (int, int) testAssignment = (23, 32);
    testAssignment = (0, 0);
    ((int, int), int) testNested = ((321, 123), -1);
    `qout("%u\n", sizeof ((int, int), void*));
    return x + y - 20;
}

