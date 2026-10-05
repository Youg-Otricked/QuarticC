class Counter {
    static int count;
    static int MAX = 100;
    static void increment() {
        Counter::count = Counter::count + 1;
    }
    static void add(int amount) {
        Counter::count = Counter::count + amount;
    }
    static int get_count() {
        return Counter::count;
    }
}
Counter::count = 0;
class MathUtils {
    static int square(int x) {
        return x * x;
    }
    static int clamp(int val, int min_val, int max_val) {
        if (val < min_val) return min_val;
        if (val > max_val) return max_val;
        return val;
    }
}
class TestRunner {
    static int run_all_tests() {
        Counter::count = 10;
        Counter::increment();
        if (Counter::get_count() != 11) return 1;
        Counter::add(5);
        if (Counter::count != 16) return 2;
        if (Counter::MAX != 100) return 3;
        if (MathUtils::square(4) != 16) return 4;
        if (MathUtils::clamp(150, 0, Counter::MAX) != 100) return 5;
        if (MathUtils::clamp(-10, 0, Counter::MAX) != 0) return 6;
        return 0;
    }
}
int main() {
    int result = TestRunner::run_all_tests();
    if (result == 0) {
        `qout("pass");
    } else {
        `qout("Static test %i failed", result);
    }
    return result;
}
