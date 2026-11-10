namespace Exported {
    #include <IO, std>
}
concept None {
}
class N {
}
N proves None;
int main() {
    IO::qout("%b", None proved_by N);
    return 0;
}
