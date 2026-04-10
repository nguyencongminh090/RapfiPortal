#include <iostream>
#include <array>
#include <tuple>
#include <cstdint>

using namespace std;

enum Color { BLACK = 0, WHITE = 1 };
enum ColorFlag { SELF, OPPO, EMPT };
typedef std::array<ColorFlag, 9> LineT;

enum Pattern { DEAD, OL, B1, F1, B2, F2, F2A, F2B, B3, F3, F3S, B4, F4, F5 };

std::tuple<int, int, int, int> countLine(const LineT &line) {
    int realLen = 1, fullLen = 1;
    int realLenInc = 1;
    int start = 4, end = 4;
    for (int i = 3; i >= 0; i--) {
        if (line[i] == SELF) realLen += realLenInc;
        else if (line[i] == OPPO) break;
        else realLenInc = 0;
        fullLen++; start = i;
    }
    realLenInc = 1;
    for (int i = 5; i < 9; i++) {
        if (line[i] == SELF) realLen += realLenInc;
        else if (line[i] == OPPO) break;
        else realLenInc = 0;
        fullLen++; end = i;
    }
    return {realLen, fullLen, start, end};
}

LineT shiftLine(const LineT &line, int i) {
    LineT sl;
    for (int j = 0; j < 9; j++) {
        int idx = j + i - 4;
        sl[j] = (idx >= 0 && idx < 9) ? line[idx] : OPPO;
    }
    return sl;
}

Pattern getPattern(const LineT &line, int depth=0) {
    auto [realLen, fullLen, start, end] = countLine(line);
    if (realLen >= 5) return F5;
    if (fullLen < 5) return DEAD;
    int patCnt[20] = {0};
    for (int i = start; i <= end; i++) {
        if (line[i] == EMPT) {
            LineT sl = shiftLine(line, i);
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

LineT initLine(uint64_t key, Color self) {
    LineT line;
    for (int i = 0; i < 9; i++) {
        bool c[2];
        if (i < 4) {
            c[BLACK] = (key >> (2 * i)) & (0x1 + BLACK);
            c[WHITE] = (key >> (2 * i)) & (0x1 + WHITE);
        } else if (i > 4) {
            c[BLACK] = (key >> (2 * (i - 1))) & (0x1 + BLACK);
            c[WHITE] = (key >> (2 * (i - 1))) & (0x1 + WHITE);
        }
        if (i == 4) line[i] = SELF;
        else line[i] = c[BLACK] && c[WHITE] ? EMPT : c[self == BLACK ? WHITE : BLACK] ? SELF : OPPO;
    }
    return line;
}

int main() {
    uint64_t key = 0x2af63;
    uint64_t fused = ((key >> 2) & 0xff00) | (key & 0x00ff);
    cout << "Fused: " << hex << fused << dec << endl;
    
    LineT l = initLine(fused, BLACK);
    cout << "Line: ";
    for(int i=0; i<9; i++) {
        if(l[i] == EMPT) cout << "E ";
        else if(l[i] == SELF) cout << "S ";
        else cout << "O ";
    }
    cout << endl;
    
    Pattern p = getPattern(l);
    cout << "Pattern: " << p << " (B4 is 11, F3 is 9, DEAD is 0)" << endl;
    return 0;
}
