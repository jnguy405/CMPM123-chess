#pragma once

#include "Game.h"
#include "Grid.h"
#include "Bitboard.h"
#include "MagicBitboards.h"

constexpr int pieceSize = 80;

class Chess : public Game
{
public:
    Chess();
    ~Chess();
    void setUpBoard() override;
    void stopGame() override;

    // Player & turn management
    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;
    void makeMove(const BitMove& move);
    void moveCompleted(Bit* bit, BitHolder* src, BitHolder* dst);
    void undoMove(const BitMove& move, Bit* capturedPiece);
    Player* checkForWinner() override;
    bool checkForDraw() override;
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);

    // State string methods
    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override { return _grid; }
    std::vector<BitMove> generateAllMoves();

private:
    Grid* _grid;

    // Piece bitboards
    uint64_t _whitePawns, _whiteKnights, _whiteBishops;
    uint64_t _whiteRooks, _whiteQueens,  _whiteKing;
    uint64_t _blackPawns, _blackKnights, _blackBishops;
    uint64_t _blackRooks, _blackQueens,  _blackKing;

    // Bitboard helpers
    uint64_t& getBitboard(ChessPiece pieceType, int playerNumber);
    uint64_t getWhitePieces() const;
    uint64_t getBlackPieces() const;
    uint64_t getAllPieces() const;
    void updateBitboardsFromGrid();
    void updateGridFromBitboards();
    void applyMoveToBitboards(const BitMove& move, int playerNumber);

    // Move generation
    void addPawnBitboardMovesToList(std::vector<BitMove>& moves, uint64_t bitboard, int shift);
    void generatePawnMoves(std::vector<BitMove>& moves, char color);
    void generatePieceMoves(std::vector<BitMove>& moves, char color, ChessPiece pieceType, uint64_t(*attackFn)(int, uint64_t));

    // Utility
    char pieceNotation(int x, int y) const;
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
};