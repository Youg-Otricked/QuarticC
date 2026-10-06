int main() {
    (int, int) tooMany = (1, 2, 3);
    (int, int) typeMismatch = ("hello");
    (int, int) tooFew = (1);
}
