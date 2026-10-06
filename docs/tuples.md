# Tuples

Tuples store indexed collections of types.

```qc
(int, int) makeCoord(int x, int y) {
    return (x, y);
}
int main() {
    (int, int) coord = makeCoord(123, 2);
    `qout("%i, %i", coord.0, coord.1);
}
```

Essentially, they are nameless structs.
