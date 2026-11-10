namespace Exported {
    #include <IO, std>
}
int main() {
    defer IO::qout("thing\n");
    int i = 0;
    {
nested_label:
        IO::qout("Nested\n");
        i++;
    } 
    {
        defer IO::qout("thing1\n");
        if (i < 2) goto nested_label;
        else goto exit;
    }
exit:
    return 0;
}
