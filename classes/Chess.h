#pragma once

#include "Game.h"
#include "Grid.h"
#include "Bitboard.h"
#include "MagicBitboards.h"

constexpr int pieceSize = 80;

class Chess : public Game
{
public:
    // Construction & Initialization
    Chess();
    ~Chess();
    void setUpBoard() override;
    void stopGame() override;
    void FENtoBoard(const std::string& fen);

    // Player & Turn Management
    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;
    void makeMove(const BitMove& move);
    void moveCompleted(Bit* bit, BitHolder* src, BitHolder* dst);
    void undoMove(const BitMove& move, Bit* capturedPiece);
    Player *checkForWinner() override;
    bool checkForDraw() override;
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;

    // State String Methods
    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;
    char pieceNotation(int x, int y) const;
    bool isValidMove(int fromSquare, int toSquare, ChessPiece pieceType, char color);

    // Bitboard Management
    Grid* getGrid() override { return _grid; }
    std::vector<BitMove> generateMovesForCurrentPlayer();
    uint64_t getWhitePieces() const;
    uint64_t getBlackPieces() const;
    uint64_t getAllPieces() const;
    void updateBitboardsFromGrid();
    void updateGridFromBitboards();

private:
    Grid* _grid;

    // Piece bitboards
    uint64_t _whitePawns;
    uint64_t _whiteKnights;
    uint64_t _whiteKing;
    uint64_t _blackPawns;
    uint64_t _blackKnights;
    uint64_t _blackKing;
    uint64_t _whiteBishops;
    uint64_t _whiteRooks;
    uint64_t _whiteQueens;
    uint64_t _blackBishops;
    uint64_t _blackRooks;
    uint64_t _blackQueens;
    
    // Move generation
    void addPawnBitboardMovesToList(std::vector<BitMove>& moves, uint64_t bitboard, int shift);
    void generatePawnMoves(std::vector<BitMove>& moves, char color);
    void generateKnightMoves(std::vector<BitMove>& moves, char color);
    void generateKingMoves(std::vector<BitMove>& moves, char color);
};