#include "Chess.h"
#include "MagicBitboards.h"
#include "Bitboard.h"
#include <limits>
#include <cmath>
#include <cctype>
#include <sstream>
#include <algorithm>

static struct MagicInit {
    MagicInit() { initMagicBitboards(); }
    ~MagicInit() { cleanupMagicBitboards(); }
} magicInit;

// ============================================================================
// CONSTRUCTION & INITIALIZATION
// ============================================================================

Chess::Chess()
    : _whitePawns(0), _whiteKnights(0), _whiteBishops(0), _whiteRooks(0), _whiteQueens(0), _whiteKing(0),
      _blackPawns(0), _blackKnights(0), _blackBishops(0), _blackRooks(0), _blackQueens(0), _blackKing(0)
{
    _grid = new Grid(8, 8);
}

Chess::~Chess()
{
    delete _grid;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;
    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");
    startGame();
}

void Chess::stopGame() {
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
    
    _whitePawns = _whiteKnights = _whiteBishops = _whiteRooks = _whiteQueens = _whiteKing = 0;
    _blackPawns = _blackKnights = _blackBishops = _blackRooks = _blackQueens = _blackKing = 0;
}

void Chess::FENtoBoard(const std::string& fen) {
    std::istringstream iss(fen);
    std::string boardPosition;
    iss >> boardPosition;
    
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            _grid->getSquare(x, y)->setBit(nullptr);
    
    _whitePawns = _whiteKnights = _whiteBishops = _whiteRooks = _whiteQueens = _whiteKing = 0;
    _blackPawns = _blackKnights = _blackBishops = _blackRooks = _blackQueens = _blackKing = 0;
    
    int row = 7;
    int col = 0;
    
    for (char c : boardPosition) {
        if (c == '/') {
            row--;
            col = 0;
        } else if (std::isdigit(c)) {
            col += (c - '0');
        } else {
            int playerNumber = (std::isupper(c) ? 0 : 1);
            ChessPiece pieceType;
            int square = row * 8 + col;
            uint64_t squareMask = 1ULL << square;
            
            char upperC = std::toupper(c);
            switch (upperC) {
                case 'P': pieceType = Pawn; 
                    if (playerNumber == 0) _whitePawns |= squareMask;
                    else _blackPawns |= squareMask;
                    break;
                case 'N': pieceType = Knight;
                    if (playerNumber == 0) _whiteKnights |= squareMask;
                    else _blackKnights |= squareMask;
                    break;
                case 'B': pieceType = Bishop;
                    if (playerNumber == 0) _whiteBishops |= squareMask;
                    else _blackBishops |= squareMask;
                    break;
                case 'R': pieceType = Rook;
                    if (playerNumber == 0) _whiteRooks |= squareMask;
                    else _blackRooks |= squareMask;
                    break;
                case 'Q': pieceType = Queen;
                    if (playerNumber == 0) _whiteQueens |= squareMask;
                    else _blackQueens |= squareMask;
                    break;
                case 'K': pieceType = King;
                    if (playerNumber == 0) _whiteKing |= squareMask;
                    else _blackKing |= squareMask;
                    break;
                default: pieceType = Pawn; break;
            }
            
            Bit* piece = PieceForPlayer(playerNumber, pieceType);
            ChessSquare* squarePtr = _grid->getSquare(col, row);
            piece->setPosition(squarePtr->getPosition());
            squarePtr->setBit(piece);
            
            col++;
        }
    }
}

// ============================================================================
// PLAYER & TURN MANAGEMENT
// ============================================================================

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    int currentPlayerNum = getCurrentPlayer()->playerNumber();
    int pieceColor = (bit.gameTag() >= 128) ? 1 : 0;
    return (pieceColor == currentPlayerNum);
}

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    ChessSquare* srcSquare = dynamic_cast<ChessSquare*>(&src);
    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(&dst);
    
    if (!srcSquare || !dstSquare) return false;
    
    int fromSquare = srcSquare->getRow() * 8 + srcSquare->getColumn();
    int toSquare = dstSquare->getRow() * 8 + dstSquare->getColumn();
    
    if (fromSquare == toSquare) return false;
    
    int gameTag = bit.gameTag();
    ChessPiece pieceType = static_cast<ChessPiece>(gameTag % 128);
    
    if (pieceType != Pawn && pieceType != Knight && pieceType != King)
        return false;
    
    updateBitboardsFromGrid();
    
    char color = (getCurrentPlayer()->playerNumber() == 0) ? 'w' : 'b';
    std::vector<BitMove> moves;
    
    if (color == 'w') {
        generatePawnMoves(moves, 'w');
        generateKnightMoves(moves, 'w');
        generateKingMoves(moves, 'w');
    } else {
        generatePawnMoves(moves, 'b');
        generateKnightMoves(moves, 'b');
        generateKingMoves(moves, 'b');
    }
    
    for (const auto& move : moves)
        if (move.from == fromSquare && move.to == toSquare)
            return true;
    
    return false;
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

void Chess::makeMove(const BitMove& move) {
    updateBitboardsFromGrid();
    
    ChessPiece pieceType = static_cast<ChessPiece>(move.piece);
    char movingColor = (getCurrentPlayer()->playerNumber() == 0) ? 'w' : 'b';
    
    uint64_t fromMask = 1ULL << move.from;
    uint64_t toMask = 1ULL << move.to;
    
    if (movingColor == 'w') {
        switch (pieceType) {
            case Pawn: _whitePawns &= ~fromMask; _whitePawns |= toMask; break;
            case Knight: _whiteKnights &= ~fromMask; _whiteKnights |= toMask; break;
            case King: _whiteKing &= ~fromMask; _whiteKing |= toMask; break;
            default: break;
        }
        _blackPawns &= ~toMask;
        _blackKnights &= ~toMask;
        _blackBishops &= ~toMask;
        _blackRooks &= ~toMask;
        _blackQueens &= ~toMask;
        _blackKing &= ~toMask;
    } else {
        switch (pieceType) {
            case Pawn: _blackPawns &= ~fromMask; _blackPawns |= toMask; break;
            case Knight: _blackKnights &= ~fromMask; _blackKnights |= toMask; break;
            case King: _blackKing &= ~fromMask; _blackKing |= toMask; break;
            default: break;
        }
        _whitePawns &= ~toMask;
        _whiteKnights &= ~toMask;
        _whiteBishops &= ~toMask;
        _whiteRooks &= ~toMask;
        _whiteQueens &= ~toMask;
        _whiteKing &= ~toMask;
    }
    
    updateGridFromBitboards();
    endTurn();
}

void Chess::moveCompleted(Bit* bit, BitHolder* src, BitHolder* dst)
{
    if (!bit || !src || !dst) return;
    
    ChessSquare* srcSquare = dynamic_cast<ChessSquare*>(src);
    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(dst);
    
    if (!srcSquare || !dstSquare) return;
    
    int fromSquare = srcSquare->getRow() * 8 + srcSquare->getColumn();
    int toSquare = dstSquare->getRow() * 8 + dstSquare->getColumn();
    
    int gameTag = bit->gameTag();
    ChessPiece pieceType = static_cast<ChessPiece>(gameTag % 128);
    
    BitMove move(fromSquare, toSquare, pieceType);
    
    updateBitboardsFromGrid();
    
    uint64_t fromMask = 1ULL << fromSquare;
    uint64_t toMask = 1ULL << toSquare;
    
    if (gameTag < 128) {
        switch (pieceType) {
            case Pawn: _whitePawns &= ~fromMask; _whitePawns |= toMask; break;
            case Knight: _whiteKnights &= ~fromMask; _whiteKnights |= toMask; break;
            case King: _whiteKing &= ~fromMask; _whiteKing |= toMask; break;
            default: break;
        }
        _blackPawns &= ~toMask;
        _blackKnights &= ~toMask;
        _blackBishops &= ~toMask;
        _blackRooks &= ~toMask;
        _blackQueens &= ~toMask;
        _blackKing &= ~toMask;
    } else {
        switch (pieceType) {
            case Pawn: _blackPawns &= ~fromMask; _blackPawns |= toMask; break;
            case Knight: _blackKnights &= ~fromMask; _blackKnights |= toMask; break;
            case King: _blackKing &= ~fromMask; _blackKing |= toMask; break;
            default: break;
        }
        _whitePawns &= ~toMask;
        _whiteKnights &= ~toMask;
        _whiteBishops &= ~toMask;
        _whiteRooks &= ~toMask;
        _whiteQueens &= ~toMask;
        _whiteKing &= ~toMask;
    }
    
    endTurn();
}

void Chess::undoMove(const BitMove& move, Bit* capturedPiece) {
    endTurn();
}

Player* Chess::checkForWinner() {
    updateBitboardsFromGrid();
    if (_whiteKing == 0) return getPlayerAt(1);
    if (_blackKing == 0) return getPlayerAt(0);
    return nullptr;
}

bool Chess::checkForDraw() {
    return generateMovesForCurrentPlayer().empty();
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };
    Bit* bit = new Bit();
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);
    bit->setGameTag(piece + (playerNumber == 0 ? 0 : 128));
    return bit;
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) return nullptr;
    auto square = _grid->getSquare(x, y);
    return (square && square->bit()) ? square->bit()->getOwner() : nullptr;
}

// ============================================================================
// STATE STRING METHODS
// ============================================================================

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            s += pieceNotation(x, y);
    return s;
}

void Chess::setStateString(const std::string &s)
{
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            _grid->getSquare(x, y)->setBit(nullptr);
}

char Chess::pieceNotation(int x, int y) const {
    const char *wpieces = "0PNBRQK";
    const char *bpieces = "0pnbrqk";
    Bit *bit = _grid->getSquare(x, y)->bit();
    return bit ? (bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128]) : '0';
}

bool Chess::isValidMove(int fromSquare, int toSquare, ChessPiece pieceType, char color)
{
    std::vector<BitMove> moves = generateMovesForCurrentPlayer();
    for (const auto& move : moves)
        if (move.from == fromSquare && move.to == toSquare && move.piece == pieceType)
            return true;
    return false;
}

// ============================================================================
// BITBOARD MANAGEMENT
// ============================================================================

void Chess::updateBitboardsFromGrid() {
    _whitePawns = _whiteKnights = _whiteBishops = _whiteRooks = _whiteQueens = _whiteKing = 0;
    _blackPawns = _blackKnights = _blackBishops = _blackRooks = _blackQueens = _blackKing = 0;
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            Bit* piece = _grid->getSquare(col, row)->bit();
            if (!piece) continue;
            
            int square = row * 8 + col;
            int gameTag = piece->gameTag();
            ChessPiece pieceType = static_cast<ChessPiece>(gameTag % 128);
            uint64_t mask = 1ULL << square;
            
            if (gameTag < 128) {
                switch (pieceType) {
                    case Pawn: _whitePawns |= mask; break;
                    case Knight: _whiteKnights |= mask; break;
                    case Bishop: _whiteBishops |= mask; break;
                    case Rook: _whiteRooks |= mask; break;
                    case Queen: _whiteQueens |= mask; break;
                    case King: _whiteKing |= mask; break;
                    default: break;
                }
            } else {
                switch (pieceType) {
                    case Pawn: _blackPawns |= mask; break;
                    case Knight: _blackKnights |= mask; break;
                    case Bishop: _blackBishops |= mask; break;
                    case Rook: _blackRooks |= mask; break;
                    case Queen: _blackQueens |= mask; break;
                    case King: _blackKing |= mask; break;
                    default: break;
                }
            }
        }
    }
}

void Chess::updateGridFromBitboards() {
    for (int row = 0; row < 8; row++)
        for (int col = 0; col < 8; col++)
            _grid->getSquare(col, row)->setBit(nullptr);
    
    auto placePieces = [this](uint64_t bitboard, int playerNumber, ChessPiece pieceType) {
        if (bitboard == 0) return;
        BitboardElement bb(bitboard);
        bb.forEachBit([this, playerNumber, pieceType](int square) {
            int row = square / 8;
            int col = square % 8;
            Bit* piece = PieceForPlayer(playerNumber, pieceType);
            ChessSquare* squarePtr = _grid->getSquare(col, row);
            piece->setPosition(squarePtr->getPosition());
            squarePtr->setBit(piece);
        });
    };
    
    placePieces(_whitePawns, 0, Pawn);
    placePieces(_whiteKnights, 0, Knight);
    placePieces(_whiteBishops, 0, Bishop);
    placePieces(_whiteRooks, 0, Rook);
    placePieces(_whiteQueens, 0, Queen);
    placePieces(_whiteKing, 0, King);
    placePieces(_blackPawns, 1, Pawn);
    placePieces(_blackKnights, 1, Knight);
    placePieces(_blackBishops, 1, Bishop);
    placePieces(_blackRooks, 1, Rook);
    placePieces(_blackQueens, 1, Queen);
    placePieces(_blackKing, 1, King);
}

uint64_t Chess::getWhitePieces() const {
    return _whitePawns | _whiteKnights | _whiteBishops | _whiteRooks | _whiteQueens | _whiteKing;
}

uint64_t Chess::getBlackPieces() const {
    return _blackPawns | _blackKnights | _blackBishops | _blackRooks | _blackQueens | _blackKing;
}

uint64_t Chess::getAllPieces() const {
    return getWhitePieces() | getBlackPieces();
}

std::vector<BitMove> Chess::generateMovesForCurrentPlayer() {
    std::vector<BitMove> moves;
    moves.reserve(40);
    
    updateBitboardsFromGrid();
    
    char color = (getCurrentPlayer()->playerNumber() == 0) ? 'w' : 'b';
    
    generatePawnMoves(moves, color);
    generateKnightMoves(moves, color);
    generateKingMoves(moves, color);
    
    return moves;
}

// ============================================================================
// PAWN MOVE GENERATION
// ============================================================================

void Chess::generatePawnMoves(std::vector<BitMove>& moves, char color) {
    uint64_t pawns = (color == 'w') ? _whitePawns : _blackPawns;
    uint64_t emptySquares = ~getAllPieces();
    uint64_t enemyPieces = (color == 'w') ? getBlackPieces() : getWhitePieces();
    
    constexpr uint64_t NOT_A_FILE = 0xFEFEFEFEFEFEFEFEULL;
    constexpr uint64_t NOT_H_FILE = 0x7F7F7F7F7F7F7F7FULL;
    constexpr uint64_t RANK_2 = 0x000000000000FF00ULL;
    constexpr uint64_t RANK_7 = 0x00FF000000000000ULL;
    
    if (pawns == 0) return;
    
    if (color == 'w') {
        uint64_t singlePushes = (pawns << 8) & emptySquares;
        BitboardElement singlePushBB(singlePushes);
        singlePushBB.forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare - 8, toSquare, Pawn);
        });
        
        uint64_t pawnsOnRank2 = pawns & RANK_2;
        if (pawnsOnRank2) {
            uint64_t oneStep = (pawnsOnRank2 << 8) & emptySquares;
            uint64_t twoStep = (oneStep << 8) & emptySquares;
            BitboardElement doublePushBB(twoStep);
            doublePushBB.forEachBit([&](int toSquare) {
                moves.emplace_back(toSquare - 16, toSquare, Pawn);
            });
        }
        
        uint64_t capturesLeft = ((pawns & NOT_A_FILE) << 7) & enemyPieces;
        BitboardElement capturesLeftBB(capturesLeft);
        capturesLeftBB.forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare - 7, toSquare, Pawn);
        });
        
        uint64_t capturesRight = ((pawns & NOT_H_FILE) << 9) & enemyPieces;
        BitboardElement capturesRightBB(capturesRight);
        capturesRightBB.forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare - 9, toSquare, Pawn);
        });
    } else {
        uint64_t singlePushes = (pawns >> 8) & emptySquares;
        BitboardElement singlePushBB(singlePushes);
        singlePushBB.forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare + 8, toSquare, Pawn);
        });
        
        uint64_t pawnsOnRank7 = pawns & RANK_7;
        if (pawnsOnRank7) {
            uint64_t oneStep = (pawnsOnRank7 >> 8) & emptySquares;
            uint64_t twoStep = (oneStep >> 8) & emptySquares;
            BitboardElement doublePushBB(twoStep);
            doublePushBB.forEachBit([&](int toSquare) {
                moves.emplace_back(toSquare + 16, toSquare, Pawn);
            });
        }
        
        uint64_t capturesLeft = ((pawns & NOT_H_FILE) >> 7) & enemyPieces;
        BitboardElement capturesLeftBB(capturesLeft);
        capturesLeftBB.forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare + 7, toSquare, Pawn);
        });
        
        uint64_t capturesRight = ((pawns & NOT_A_FILE) >> 9) & enemyPieces;
        BitboardElement capturesRightBB(capturesRight);
        capturesRightBB.forEachBit([&](int toSquare) {
            moves.emplace_back(toSquare + 9, toSquare, Pawn);
        });
    }
}

// ============================================================================
// KNIGHT MOVE GENERATION
// ============================================================================

void Chess::generateKnightMoves(std::vector<BitMove>& moves, char color) {
    uint64_t knights = (color == 'w') ? _whiteKnights : _blackKnights;
    uint64_t friendlyPieces = (color == 'w') ? getWhitePieces() : getBlackPieces();
    
    if (knights == 0) return;
    
    BitboardElement knightBB(knights);
    knightBB.forEachBit([&](int fromSquare) {
        uint64_t attacks = KnightAttacks[fromSquare] & ~friendlyPieces;
        BitboardElement attackBB(attacks);
        attackBB.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, Knight);
        });
    });
}

// ============================================================================
// KING MOVE GENERATION
// ============================================================================

void Chess::generateKingMoves(std::vector<BitMove>& moves, char color) {
    uint64_t king = (color == 'w') ? _whiteKing : _blackKing;
    uint64_t friendlyPieces = (color == 'w') ? getWhitePieces() : getBlackPieces();
    
    if (king == 0) return;
    
    BitboardElement kingBB(king);
    kingBB.forEachBit([&](int fromSquare) {
        uint64_t attacks = KingAttacks[fromSquare] & ~friendlyPieces;
        BitboardElement attackBB(attacks);
        attackBB.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, King);
        });
    });
}