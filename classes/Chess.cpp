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
            int square = SQUARE(row, col);  // Using SQUARE macro

            char upperC = std::toupper(c);
            switch (upperC) {
                case 'P': pieceType = Pawn;   break;
                case 'N': pieceType = Knight; break;
                case 'B': pieceType = Bishop; break;
                case 'R': pieceType = Rook;   break;
                case 'Q': pieceType = Queen;  break;
                case 'K': pieceType = King;   break;
                default:  pieceType = Pawn;   break;
            }
            // Using SET_BIT macro
            SET_BIT(getBitboard(pieceType, playerNumber), square);
            
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
    
    int fromSquare = SQUARE(srcSquare->getRow(), srcSquare->getColumn());  // Using SQUARE macro
    int toSquare = SQUARE(dstSquare->getRow(), dstSquare->getColumn());    // Using SQUARE macro
    
    if (fromSquare == toSquare) return false;
    
    int gameTag = bit.gameTag();
    ChessPiece pieceType = static_cast<ChessPiece>(gameTag % 128);
    
    if (pieceType != Pawn && pieceType != Knight && pieceType != King)
        return false;
    
    updateBitboardsFromGrid();
    
    char color = (getCurrentPlayer()->playerNumber() == 0) ? 'w' : 'b';
    std::vector<BitMove> moves;
    
    // Generate all moves for the current color directly
    generatePawnMoves(moves, color);
    generateKnightMoves(moves, color);
    generateKingMoves(moves, color);
    
    for (const auto& move : moves)
        if (move.from == fromSquare && move.to == toSquare)
            return true;
    
    return false;
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

// Helper function to apply a move to bitboards
void Chess::applyMoveToBitboards(const BitMove& move, int playerNumber) {
    ChessPiece pieceType = static_cast<ChessPiece>(move.piece);
    int enemyPlayer = (playerNumber == 0) ? 1 : 0;
    
    // Clear the source square
    CLEAR_BIT(getBitboard(pieceType, playerNumber), move.from);
    
    // Set the destination square
    SET_BIT(getBitboard(pieceType, playerNumber), move.to);
    
    // Clear any enemy piece at the destination (capture)
    static const ChessPiece types[6] = {Pawn, Knight, Bishop, Rook, Queen, King};
    for (int i = 0; i < 6; i++) {
        CLEAR_BIT(getBitboard(types[i], enemyPlayer), move.to);
    }
}

void Chess::makeMove(const BitMove& move) {
    updateBitboardsFromGrid();
    
    int currentPlayer = getCurrentPlayer()->playerNumber();
    applyMoveToBitboards(move, currentPlayer);
    
    updateGridFromBitboards();
    endTurn();
}

void Chess::moveCompleted(Bit* bit, BitHolder* src, BitHolder* dst)
{
    if (!bit || !src || !dst) return;
    
    ChessSquare* srcSquare = dynamic_cast<ChessSquare*>(src);
    ChessSquare* dstSquare = dynamic_cast<ChessSquare*>(dst);
    
    if (!srcSquare || !dstSquare) return;
    
    int fromSquare = SQUARE(srcSquare->getRow(), srcSquare->getColumn());  // Using SQUARE macro
    int toSquare = SQUARE(dstSquare->getRow(), dstSquare->getColumn());    // Using SQUARE macro
    
    int gameTag = bit->gameTag();
    ChessPiece pieceType = static_cast<ChessPiece>(gameTag % 128);
    int playerNumber = (gameTag < 128) ? 0 : 1;
    
    BitMove move(fromSquare, toSquare, pieceType);
    
    updateBitboardsFromGrid();
    applyMoveToBitboards(move, playerNumber);
    
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
            
            int square = SQUARE(row, col);  // Using SQUARE macro
            int gameTag = piece->gameTag();
            ChessPiece pieceType = static_cast<ChessPiece>(gameTag % 128);
            
            // Using SET_BIT macro
            SET_BIT(getBitboard(pieceType, gameTag < 128 ? 0 : 1), square);
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
    
    static const ChessPiece types[6] = {Pawn, Knight, Bishop, Rook, Queen, King};
    for (int player = 0; player < 2; player++)
        for (int i = 0; i < 6; i++)
            placePieces(getBitboard(types[i], player), player, types[i]);
}

uint64_t& Chess::getBitboard(ChessPiece pieceType, int playerNumber) {
    if (playerNumber == 0) {
        switch (pieceType) {
            case Pawn:   return _whitePawns;
            case Knight: return _whiteKnights;
            case Bishop: return _whiteBishops;
            case Rook:   return _whiteRooks;
            case Queen:  return _whiteQueens;
            case King:   return _whiteKing;
            default:     return _whitePawns;
        }
    } else {
        switch (pieceType) {
            case Pawn:   return _blackPawns;
            case Knight: return _blackKnights;
            case Bishop: return _blackBishops;
            case Rook:   return _blackRooks;
            case Queen:  return _blackQueens;
            case King:   return _blackKing;
            default:     return _blackPawns;
        }
    }
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

void Chess::addPawnBitboardMovesToList(std::vector<BitMove>& moves, uint64_t bitboard, int shift) {
    BitboardElement(bitboard).forEachBit([&](int toSquare) {
        int fromSquare = toSquare - shift;
        moves.emplace_back(fromSquare, toSquare, Pawn);
    });
}

void Chess::generatePawnMoves(std::vector<BitMove>& moves, char color) {
    uint64_t pawns = (color == 'w') ? _whitePawns : _blackPawns;
    if (pawns == 0) return;

    uint64_t empty = ~getAllPieces();
    uint64_t enemy = (color == 'w') ? getBlackPieces() : getWhitePieces();

    constexpr uint64_t Rank3 = 0x0000000000FF0000ULL;
    constexpr uint64_t Rank6 = 0x0000FF0000000000ULL;

    // Using directional macros for pawn moves
    uint64_t singleMoves = (color == 'w') ? NORTH(pawns) & empty : SOUTH(pawns) & empty;
    uint64_t doubleMoves = (color == 'w') ? NORTH(singleMoves & Rank3) & empty 
                                           : SOUTH(singleMoves & Rank6) & empty;
    uint64_t capturesLeft  = (color == 'w') ? NORTH_WEST(pawns) & enemy : SOUTH_WEST(pawns) & enemy;
    uint64_t capturesRight = (color == 'w') ? NORTH_EAST(pawns) & enemy : SOUTH_EAST(pawns) & enemy;

    int shiftForward      = (color == 'w') ?  8  : -8;
    int shiftDouble       = (color == 'w') ?  16 : -16;
    int shiftCaptureLeft  = (color == 'w') ?  7  : -7;
    int shiftCaptureRight = (color == 'w') ?  9  : -9;

    addPawnBitboardMovesToList(moves, singleMoves,   shiftForward);
    addPawnBitboardMovesToList(moves, doubleMoves,   shiftDouble);
    addPawnBitboardMovesToList(moves, capturesLeft,  shiftCaptureLeft);
    addPawnBitboardMovesToList(moves, capturesRight, shiftCaptureRight);
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
        // Using pre-calculated knight attacks from magic bitboards
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
        // Using pre-calculated king attacks from magic bitboards
        uint64_t attacks = KingAttacks[fromSquare] & ~friendlyPieces;
        BitboardElement attackBB(attacks);
        attackBB.forEachBit([&](int toSquare) {
            moves.emplace_back(fromSquare, toSquare, King);
        });
    });
}