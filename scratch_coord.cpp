#include <iostream>
#include <vector>
using namespace std;
int main() {
    int boardsize = 17;
    int x = 4, y = 8;
    // Let's print out what inputCoordConvert does in standard Gomocup!
    // But since Config::IOCoordMode might be different, let's just print all permutations.
    cout << "Direct 0-based: x=" << x << ", y=" << y << " -> " << char('A' + x) << y + 1 << endl;
    cout << "1-based to 0-based: x=" << x-1 << ", y=" << y-1 << " -> " << char('A' + x - 1) << y << endl;
    cout << "FLIPY_X: " << char('A' + y) << 17 - 1 - x + 1 << endl;
    return 0;
}
