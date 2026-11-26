// Semantic Error 5: Invalid assignment between incompatible types
int count;
float ratio;
bool ready;

count = true;   // Error: bool assigned to int
count = 3.14;   // Error: float assigned to int (narrowing)
ratio = false;  // Error: bool assigned to float
ready = 1;      // Error: int assigned to bool
ready = 2.5;    // Error: float assigned to bool
