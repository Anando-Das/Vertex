// Semantic Error 6: Invalid expressions (operators applied to wrong operand types)
int x;
float y;
bool flag;
x = 10;
y = 2.5;
flag = true;

bool r1;
r1 = x && flag;   // Error: logical && on int
r1 = x || y;      // Error: logical || on numeric operands
r1 = !x;          // Error: logical ! on int

int r2;
r2 = flag + 1;    // Error: arithmetic + on bool
r2 = x * flag;    // Error: arithmetic * on bool
r2 = x % y;       // Error: modulo % on float
r2 = -flag;       // Error: unary - on bool

bool r3;
r3 = flag < x;    // Error: relational < on bool
