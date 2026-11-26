/*
 * Non-trivial valid program demonstrating:
 * - Data types: int, float, bool
 * - Variable declarations, assignments, and comments
 * - Arithmetic, relational, logical, and unary operators
 * - while loop, if-else control flow, nested blocks with scoping, and print
 */

int n;
int i;
int sum;
float threshold;
float total;
bool passed;
bool done;

n = 5;
i = 1;
sum = 0;
threshold = 12.5;
done = false;

while (i <= n && !done) {
    sum = sum + i * 2;
    i = i + 1;
}

total = sum + 0.5;
passed = (total >= threshold) && (sum % 2 == 0);

if (passed == true) {
    int bonus;
    bonus = -10;
    {
        float adjusted;
        adjusted = total - bonus;
        print adjusted;
    }
    print sum;
} else {
    print threshold;
}

done = true;
print passed;
