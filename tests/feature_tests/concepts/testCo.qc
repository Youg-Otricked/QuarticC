concept Printable {
    all_of {
        string to_string(Self self);
    }
}
struct Paper {
    string data;
}
Paper proves Printable with_proof {
    string to_string(Paper *self) {
        return self->data;
    }
};
int main() {
    Paper p = {"Im Paper i think"};
    `qout("%s", p.to_string());
    return 0;
}
