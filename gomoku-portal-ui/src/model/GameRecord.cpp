/*
 *  Portal Gomoku UI — Game Record Implementation
 */

#include "GameRecord.hpp"
#include "Board.hpp"

namespace model {

GameRecord GameRecord::fromBoard(const Board& board) {
    GameRecord rec;
    rec.boardSize = board.size();
    rec.topology  = board.topology();
    rec.moves     = board.history();
    return rec;
}

}  // namespace model
