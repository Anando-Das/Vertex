// Semantic Error 3: Scope violation (using variables outside their declaring block)
int a;
a = 5;

{
    int block_var;
    block_var = 100;
    print block_var;
}
print block_var; // Error: block_var is out of scope

if (a > 0) {
    int if_var;
    if_var = 10;
    print if_var;
}
print if_var; // Error: if_var is out of scope

while (a > 0) {
    int while_var;
    while_var = a;
    a = a - 1;
}
while_var = 42; // Error: while_var is out of scope
