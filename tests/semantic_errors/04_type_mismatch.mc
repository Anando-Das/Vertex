// Semantic Error 4: Type mismatch (initializer, control conditions, equality)
bool b = 5 + 3.2;

int n;
n = 10;

if (n + 1) {
    print n;
}

while (3.14) {
    print n;
}

bool cmp;
cmp = (true == n);
