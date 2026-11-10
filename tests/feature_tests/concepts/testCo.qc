namespace Exported {
    #include <IO, std>
}
concept Printable {
    all_of {
        char* to_string(Self self);
    }
}
struct Paper {
    char* data;
}
Paper proves Printable with_proof {
    char* to_string(Paper *self) {
        return self->data;
    }
};
int main() {
    Paper p = {"Im Paper i think"};
    IO::qout("%s", p.to_string());
    return 0;
}
