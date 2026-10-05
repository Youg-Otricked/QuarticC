modifier async {
    on_call {
        return proceed();
    }
    on_return {
        `qout("%s", `typeof(returns));
    }
}
async void doStuff() {
    `qout("Proceed called this");
}
class TestModifiersOnMethods {
    async TestModifiersOnMethods() {
    }
    async void doStuff() {
    }
}
struct Promise<T> {
    T value;
}
async Promise<int> returnAPromise() {
    return Promise<int>{
        123
    };
}
modifier await {
    on_use {
        if (!`startswith(`typeof(value), "Promise")) {
            throw "Can only await promises";
        }
        return value.value;
    }
}
modifier log_call {
    on_call {
        `qout("entering generic function\n");
        return proceed();
    }
    on_return {
        `qout("exiting generic function\n");
    }
}

log_call T identity<T>(T val) {
    `qout("inside identity body\n");
    return val;
}

log_call T add<T>(T a, T b) {
    return a + b;
}
int main() {
    doStuff();
    int x = await returnAPromise();
    `qout("%i\n", x);
    int x = identity<int>(42);
    `qout("result x = %i\n", x);
    int sum = add<int>(10, 20);
    `qout("result sum = %i\n", sum);
    return 0;
}
