// Semantic Error 2: Redeclaration of a variable in the same scope
int x;
x = 1;
{
    // Shadowing outer 'x' in a nested scope is valid
    float x;
    x = 2.5;
    float x; // Error: redeclaration in the same inner scope
}
int x; // Error: redeclaration in the global scope
