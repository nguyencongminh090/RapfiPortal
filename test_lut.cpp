#include "portal_src/game/pattern.h"
#include <iostream>
using namespace std;

int main() {
    uint64_t key = 0x2af63;
    uint64_t fused = PatternConfig::fuseKey<FREESTYLE>(key);
    cout << "Fused: " << hex << fused << dec << endl;
    Pattern2x p = PatternConfig::PATTERN2x[fused];
    cout << "patBlack: " << p.patBlack << endl;
    cout << "B4 is: " << B4 << ", F3 is: " << F3 << endl;
    return 0;
}
