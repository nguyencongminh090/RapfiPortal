#include <iostream>
#include <vector>
#include <string>

using namespace std;

int main() {
    int sz = 17;
    char board[17][17];
    for(int y=0; y<sz; y++) for(int x=0; x<sz; x++) board[y][x] = '.';

    auto putYX = [&](int y, int x, char c) {
        board[y][x] = c;
    };

    putYX(4,5, '#');
    putYX(5,11, '#');
    putYX(9,6, '#');

    putYX(4,9, 'A'); putYX(6,4, 'a');
    putYX(11,6, 'B'); putYX(9,12, 'b');

    putYX(4,6, 'X'); putYX(4,7, 'O'); putYX(6,7, 'X'); putYX(5,8, 'O'); putYX(5,7, 'X');
    putYX(6,9, 'O'); putYX(6,8, 'X'); putYX(7,9, 'O'); putYX(6,6, 'X'); putYX(9,10, 'O');
    putYX(9,4, 'X'); putYX(10,10, 'O');

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
