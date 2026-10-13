int main() {
    defer `qout("thing\n");
    int i = 0;
    {
nested_label:
        `qout("Nested\n");
        i++;
    } 
    {
        defer `qout("thing1\n");
        if (i < 2) goto nested_label;
        else goto exit;
    }
exit:
    return 0;
}
