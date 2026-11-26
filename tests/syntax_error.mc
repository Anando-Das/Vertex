// Syntax error test: multiple grammar violations to demonstrate error recovery
int x;
x = 5 + ;
int y;
y = 10;
if (y > 0 {
    print y;
}
int z;
z = y * 2;
print z;
