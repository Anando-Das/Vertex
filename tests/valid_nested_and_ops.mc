/* Valid test: nested blocks, dangling-else, relational, logical, and unary ops */
int x;
int y;
bool active;

x = -5;
y = 10;
active = !false && (x < y || y >= 10) && (x != y);

if (active)
    if (x <= 0) {
        int inner;
        inner = y * 2;
        print inner;
    } else {
        print y;
    }

{
    int scoped;
    scoped = 1;
    while (scoped < 4 && active) {
        print scoped;
        scoped = scoped + 1;
    }
}
