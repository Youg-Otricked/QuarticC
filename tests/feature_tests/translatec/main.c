int add(int a, int b) {
    return a + b;
}
int main() {
    int block = 123;
    if (block) {
    } else {
    }
    switch (block) {
        case 123:
        case 12:
            block = 2;
            break;
        case 4: {
            break; 
        }
        default:
            block = 12;
    }
    while(1) {
        int i = 0;
    }
    do {
        int i = 0;
    } while (1);
    for (int i = 0; i < 10; i++) {
        int x = 0;
    }
    int arr[3] = {1, 2, 3};
    arr[2];
thing:
    int x = add(123, 321);

    goto thing;
    return x;
}
