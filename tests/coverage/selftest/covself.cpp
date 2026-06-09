#include "covself.h"

int covselfCovered(int n)
{
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += i;
    }
    return total;
}

int covselfUncovered(int n)
{
    int total = 1;
    for (int i = 1; i <= n; ++i) {
        total *= i;
    }
    return total;
}
