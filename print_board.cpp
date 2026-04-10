#include <iostream>
#include <vector>
#include <string>

using namespace std;

int main() {
    int sz = 17;
    char board[17][17];
    for(int y=0; y<sz; y++) for(int x=0; x<sz; x++) board[y][x] = '.';

    auto put = [&](int srcX, int srcY, char c) {
        int dstX = srcY;
        int dstY = sz - 1 - srcX;
        board[dstY][dstX] = c;
    };

    put(4,5, 'W');
    put(5,11, 'W');
    put(9,6, 'W');

    put(4,9, 'A'); put(6,4, 'a');
    put(11,6, 'B'); put(9,12, 'b');

    // 1=Black(X), 2=White(O) per my deduction
    put(4,6, 'X'); put(4,7, 'O'); put(6,7, 'X'); put(5,8, 'O'); put(5,7, 'X');
    put(6,9, 'O'); put(6,8, 'X'); put(7,9, 'O'); put(6,6, 'X'); put(9,10, 'O');
    put(9,4, 'X'); put(10,10, 'O');

    cout << "   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6" << endl;
    for(int y=0; y<sz; ++y) {
        cout << (y<10?" ":"") << y << " ";
        for(int x=0; x<sz; ++x) {
            cout << board[y][x] << " ";
        }
        cout << endl;
    }
    return 0;
}
