#include <iostream>
#include <array>
using namespace std;

enum ColorFlag { SELF, OPPO, EMPT };
typedef std::array<ColorFlag, 9> Line;

enum Pattern { DEAD, OL, B1, F1, B2, F2, F2A, F2B, B3, F3, F3S, B4, F4, F5 };

std::tuple<int, int, int, int> countLine(const Line &line) {
    int realLen = 1, fullLen = 1;
    int realLenInc = 1;
    int start = 4, end = 4;

    for (int i = 3; i >= 0; i--) {
        if (line[i] == SELF) realLen += realLenInc;
        else if (line[i] == OPPO) break;
        else realLenInc = 0;
        fullLen++;
        start = i;
    }
    realLenInc = 1;
    for (int i = 5; i < 9; i++) {
        if (line[i] == SELF) realLen += realLenInc;
        else if (line[i] == OPPO) break;
        else realLenInc = 0;
        fullLen++;
        end = i;
    }
    return {realLen, fullLen, start, end};
}

Line shiftLine(const Line &line, int i) {
    Line sl;
    for (int j = 0; j < 9; j++) {
        int idx = j + i - 4;
        sl[j] = (idx >= 0 && idx < 9) ? line[idx] : OPPO;
    }
    return sl;
}

Pattern getPattern(const Line &line, int depth=0) {
    auto [realLen, fullLen, start, end] = countLine(line);
    if (realLen >= 5) return F5;
    if (fullLen < 5) return DEAD;
    int patCnt[20] = {0};
    for (int i = start; i <= end; i++) {
        if (line[i] == EMPT) {
            Line sl = shiftLine(line, i);
            sl[4] = SELF;
            Pattern slp = getPattern(sl, depth+1);
            patCnt[slp]++;
        }
    }
    if (patCnt[F5] >= 2) return F4;
    else if (patCnt[F5]) return B4;
    else if (patCnt[F4] >= 2) return F3S;
    else if (patCnt[F4]) return F3;
    else if (patCnt[B4]) return B3;
    else if (patCnt[F3S] + patCnt[F3] >= 4) return F2B;
    else if (patCnt[F3S] + patCnt[F3] >= 3) return F2A;
    else if (patCnt[F3S] + patCnt[F3] >= 1) return F2;
    else if (patCnt[B3]) return B2;
    else if (patCnt[F2] + patCnt[F2A] + patCnt[F2B]) return F1;
    else if (patCnt[B2]) return B1;
    return DEAD;
}

int main() {
    // E (4,12)   = EMPT
    // Wall(5,12) = OPPO (Wall acts as opponent stone in block logic)
    // B (6,12)   = SELF
    // W (7,12)   = OPPO
    // * (8,12)   = SELF (Center)
    // E (5,10)   = EMPT
    // B (6,10)   = SELF
    // B (7,10)   = SELF
    // B (8,10)   = SELF

    Line l = {EMPT, OPPO, SELF, OPPO, SELF, EMPT, SELF, SELF, SELF};
    cout << "Pattern: " << getPattern(l) << endl;
    cout << "F5=" << F5 << " B4=" << B4 << " F3=" << F3 << " L_FLEX2... wait." << endl;
}
