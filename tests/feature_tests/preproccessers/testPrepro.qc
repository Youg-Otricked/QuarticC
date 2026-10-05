#ifdef __linux
int main() {
    `qout("Compiled on linux");
    `qout("The current file: %s", __FILE__);
    return 0;
}
#else
int main() {
    `qout("This was compiled on a linux machine. what");
    return 1;
}
#endif

