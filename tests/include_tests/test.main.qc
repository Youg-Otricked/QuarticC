namespace Exported {
    #include<ImImportant, ImUnimportant::IsImportant, ./test.a.qc>
    #include<IO, std>
}
int main() {
    IO::qout("%i, %i", ImImportant::x, ImImportant::add(123, 321));
    ImImportant::C im_a_thing = ImImportant::C();
    IO::qout("%i", ImUnimportant::IsImportant::uh);
}
