namespace Exported {
    #include <IO, std>
}
int main() {
}
namespace ImImportant {
    int x = 0;
    int add(int z, int y) {
        return z + y;
    }
    class C {
        C() {
            IO::qout("%i", ImUnimportant::add(123, 321));
        }
    }
}
namespace ImUnimportant {
    int c = 0;
    int add(int x, int y) {
        return x + y;
    }
    namespace IsImportant {
        int uh = 12;
    }
    class C {
        C() {
            
        }
    }
}
