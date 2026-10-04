concept Serializable {
    1_of {
        string serialize();
        string serialize(Self self);
    }
    default {
class:
        string serialize() {
            return "No special implementation";
        }
else:
        string serialize(Self self) {
            return "No special implementation";
        }
    }
}
concept GenericConcept<T> {
    1_of {
        T doStuff();
    }
}
struct X<T>  {
    T x;
}
X<int> proves GenericConcept<int> with_proof {
    int doStuff() {}
};
X<int> x;
class Y {
    string serialize() {
    }
} 
Y proves Serializable;
struct Z {
    int x;
}
type T = int | string;
T proves Serializable;
Z proves Serializable with_proof {
    string serialize(Self self) {
        return "Hello";
    }
};
int main() {
    Z thing = Z{123};
    `qout("%s", thing.serialize(thing));
    return 0;
}
