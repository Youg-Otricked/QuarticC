namespace Exported {
    #include <Core, generics.hqc> 
}
int main() {
    Core::Mine<int> x;
    Core::i_do_generic_nonsense<int>(x);
    Core::non_generic();
}
