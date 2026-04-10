#include <iostream>
#include <iomanip>
using namespace std;
int main() {
    uint64_t key = 0;
    // Let's build W B E B B B
    // window slots 0..8
    // 0..2 out of bounds/wall?
    // -4: Wall = 0
    // -3: Black = 1
    // -2: Wall = 0 (oh wait! (5,12) is Wall)
    // -1: White = 2
    //  0: Black = 1 (Center)
    // +1: Empty = 3
    // +2: Black = 1
    // +3: Black = 1
    // +4: Black = 1
    int arr[] = {0, 1, 0, 2, 1, 3, 1, 1, 1};
    for(int i=0; i<9; i++) {
        key |= (uint64_t)arr[i] << (2*i);
    }
    cout << "Key: " << hex << key << endl;
}
