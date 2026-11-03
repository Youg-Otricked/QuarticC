Can define patterns.
```qc
pattern P {
    T;
    X(int, int);
    S{int x; int y;};
}
```
Then fulfill them.
```qc
struct BigBigInt {
    int n1;
    int n2;
    int n3;
    int n4;
}
BigBigInt thing fits P {
    T if (BigBigInt == ...);
    X(...) if ...
    ...
    default T;
};
```
And use.
```qc
match (my_bigint) {
    P.T => ...
    P.X(x, y) => ...
    P.S{z: x, d: y} => ...
}
```
