namespace Exported {
    #include <IO, std>
}
#ifdef __linux
int main() {
    IO::qout("Compiled on linux");
    IO::qout("The current file: %s", __FILE__);
    return 0;
}
#else
int main() {
    IO::qout("This was compiled on a linux machine. what");
    return 1;
}
#endif

