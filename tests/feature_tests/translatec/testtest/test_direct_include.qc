#include<math_utils.c>
namespace Exported {
    #include <IO, std/io.hqc>
}
int main() {
    IO::qout("%i\n", factorial(6));
    return 0;
}
