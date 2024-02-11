// Chessy.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <time.h>
#include <windows.h>

typedef signed char SBYTE;

using namespace std;

#include "ThreadsX.h"

enum EPiece : char
{
	TYPE_None = 0x00,
	TYPE_Pawn = 0x01,
	TYPE_Horse = 0x02,
	TYPE_Rook = 0x03,
	TYPE_Bishop = 0x04,
	TYPE_Queen = 0x05,
	TYPE_King = 0x06,
	TYPE_Max = 0x07,

	TEAM_AI = 0x08,
	PSTATE_FirstMove = 0x10, // Entity hasn't moved yet.
};
enum EGameState : char
{
	STATE_None = 0x00,
	STATE_ATurn = 0x01,
	STATE_BTurn = 0x02,
	STATE_CheckA = 0x04,
	STATE_CheckB = 0x08,
	STATE_MateA = 0x10,
	STATE_MateB = 0x20,

	NEWGAME_AIFirst = STATE_ATurn,
	NEWGAME_HumanFirst = STATE_BTurn,
};

struct FMovePair
{
	SBYTE X, Y;

	FMovePair(SBYTE inX, SBYTE inY)
		: X(inX), Y(inY)
	{}
	inline bool CanApply(char Pos, char* Result) const
	{
		SBYTE x = (Pos & 7);
		SBYTE y = (Pos >> 3);
		x += X;
		y += Y;
		if (x >= 0 && x < 8 && y >= 0 && y < 8)
		{
			if (Result)
				*Result = x | (y << 3);
			return true;
		}
		return false;
	}
};

struct FGameBoard
{
	char Board[8 * 8];
	char GameState;
	struct FPieceDesc* PieceDescs[TYPE_Max];

	FGameBoard()
		: GameState(STATE_None)
	{
		for (char i = 0; i < TYPE_Max; ++i)
			PieceDescs[i] = nullptr;
	}
	void InitDescriptors();
	void AIMove();

	void PrintBoard();

	void CommitMove(char Start, char End);

	void NewGame(bool bAIStart)
	{
		memset(Board, TYPE_None, (8 * 8 * sizeof(char)));
		char i;
		Board[0] = TYPE_Rook;
		Board[1] = TYPE_Horse;
		Board[2] = TYPE_Bishop;
		Board[3] = TYPE_Queen;
		Board[4] = TYPE_King;
		Board[5] = TYPE_Bishop;
		Board[6] = TYPE_Horse;
		Board[7] = TYPE_Rook;
		for (i = (8); i < (8 + 8); ++i)
			Board[i] = TYPE_Pawn;
		for (i = (8 * 6); i < (8 * 6 + 8); ++i)
			Board[i] = TYPE_Pawn;
		Board[(8 * 7) + 0] = TYPE_Rook;
		Board[(8 * 7) + 1] = TYPE_Horse;
		Board[(8 * 7) + 2] = TYPE_Bishop;
		Board[(8 * 7) + 3] = TYPE_Queen;
		Board[(8 * 7) + 4] = TYPE_King;
		Board[(8 * 7) + 5] = TYPE_Bishop;
		Board[(8 * 7) + 6] = TYPE_Horse;
		Board[(8 * 7) + 7] = TYPE_Rook;

		for (i = 0; i < (8 * 2); ++i)
			Board[i] |= PSTATE_FirstMove;
		for (i = (8 * 6); i < (8 * 8); ++i)
			Board[i] |= PSTATE_FirstMove;

		if (!bAIStart)
		{
			GameState = NEWGAME_HumanFirst;
			for (i = (8 * 6); i < (8 * 8); ++i)
				Board[i] |= TEAM_AI;
			cout << "Your turn to start! ";
		}
		else
		{
			GameState = NEWGAME_AIFirst;
			for (i = 0; i < (8 * 2); ++i)
				Board[i] |= TEAM_AI;
			cout << "AI start!" << endl;
		}
	}
	inline char GetEntity(char Space) const
	{
		return (Board[Space] & TYPE_Max);
	}
	inline bool IsAI(char Space) const
	{
		return (Board[Space] & TEAM_AI) != 0;
	}
	inline bool IsFirstMove(char Space) const
	{
		return (Board[Space] & PSTATE_FirstMove) != 0;
	}
	inline bool IsOccupied(char Space) const
	{
		return ((Board[Space] & TYPE_Max) != TYPE_None);
	}
};

struct FCheckPoint;
struct FAIPiece;

enum ETestFlags : char
{
	FLAGS_Empty = 0x01,
	FLAGS_ATeam = 0x02,
	FLAGS_BTeam = 0x04,
	FLAGS_Absolute = 0x8,

	FLAGS_EnemyOrEmpty = (FLAGS_Empty | FLAGS_BTeam),
};

struct FAIMoveTest
{
	FCheckPoint* Callback;
	FAIPiece* Piece;
	char X, Y;

	FAIMoveTest(FCheckPoint* C, FAIPiece* P);
	int TestMove(int EndX, int EndY, char Flags = FLAGS_EnemyOrEmpty);
};

struct FPieceDesc
{
	static FGameBoard* GameBoard;

	virtual EPiece GetType() const = 0;
	virtual const char* GetName() const = 0;
	virtual const char* GetIdentifier(bool bAI) const = 0;

	inline bool IsAI(char Space) const
	{
		return GameBoard->IsAI(Space);
	}
	inline bool MoveUp(char Space) const
	{
		return (GameBoard->GameState & STATE_ATurn) ? GameBoard->IsAI(Space) : !GameBoard->IsAI(Space);
	}
	inline bool IsFirstMove(char Space) const
	{
		return GameBoard->IsFirstMove(Space);
	}
	inline bool IsOccupied(char Space) const
	{
		return GameBoard->IsOccupied(Space);
	}

	virtual int GetAIPriority(FCheckPoint* AI, FAIPiece* Piece) = 0;
	virtual int GetAIPostMovePriority(FAIMoveTest* AI, char NewPos) { return 0; }
	virtual bool MakeMove(char Start, char End) = 0;
	virtual void PostMove(char End) {}
	virtual void IterateMoves(FAIMoveTest* AI) = 0;
};
FGameBoard* FPieceDesc::GameBoard = nullptr;

#include "PiecesList.h"
#include "AIHandle.h"

inline bool IsDigit(char ch)
{
	return (ch >= '1' && ch <= '8');
}
inline bool IsAl(char ch)
{
	return (ch >= 'A' && ch <= 'H') || (ch >= 'a' && ch <= 'h');
}

inline char FromDigit(char ch)
{
	return (ch - '1');
}
inline char FromAl(char ch)
{
	return (ch >= 'A' && ch <= 'H') ? (ch - 'A') : (ch - 'a');
}
inline const char* GetAreaName(char Space)
{
	static char Res[8][3];
	static char iCur = 0;
	char* R = &Res[iCur++][0];
	if (iCur == 8)
		iCur = 0;
	R[0] = (Space >> 3) + '1';
	R[1] = (Space & 7) + 'A';
	R[2] = 0;
	return R;
}

void FGameBoard::CommitMove(char Start, char End)
{
	char Piece = GetEntity(Start);
	if (IsOccupied(End))
		cout << "Move '" << PieceDescs[Piece]->GetName() << "' from " << GetAreaName(Start) << " to " << GetAreaName(End) << " and ate enemy '" << PieceDescs[GetEntity(End)]->GetName() << "'" << endl;
	else cout << "Move '" << PieceDescs[Piece]->GetName() << "' from " << GetAreaName(Start) << " to " << GetAreaName(End) << endl;
	char AIFlag = Board[Start] & TEAM_AI;
	Board[Start] = 0;
	Board[End] = Piece | AIFlag;
	PieceDescs[Piece]->PostMove(End);
}

int main()
{
	srand(uint32_t(time(NULL)));
	FGameBoard Board;
	Board.InitDescriptors();
	cout << "ChessMaster!" << endl << "/exit = Close app, /new - New game, /newF - New game your first move, /r - Undo move, <row+column><row+colum> = make move (2D4D)." << endl;

	while (1)
	{
		cout << "Cmd: ";
		string cmd;
		cin >> cmd;

		if (cmd == "/exit")
			break;
		else if (cmd == "/new" || cmd == "/newF")
		{
			Board.NewGame(cmd == "/new");
			if (Board.GameState & STATE_ATurn)
				Board.AIMove();
			Board.PrintBoard();
		}
		else if (IsDigit(cmd[0]) && IsAl(cmd[1]) && IsDigit(cmd[2]) && IsAl(cmd[3]))
		{
			if (Board.GameState == STATE_None)
			{
				cout << "Game not active, use /new or /newF!" << endl;
			}
			else if (Board.GameState >= STATE_MateA)
			{
				cout << "Game already ended, use /new or /newF!" << endl;
			}
			else
			{
				char iStart = (FromDigit(cmd[0]) * 8) + FromAl(cmd[1]);
				char iEnd = (FromDigit(cmd[2]) * 8) + FromAl(cmd[3]);
				char Piece = Board.GetEntity(iStart);

				if (Piece == TYPE_None)
					cout << "No piece found at that spot!" << endl;
				else if(Board.IsAI(iStart))
					cout << "Can't move AI piece!" << endl;
				else if(!Board.PieceDescs[Piece]->MakeMove(iStart,iEnd))
					cout << "Illegal move! (dest occupied by " << Board.PieceDescs[Board.GetEntity(iEnd)]->GetName() << ")" << endl;
				else
				{
					Board.CommitMove(iStart, iEnd);
					Board.AIMove();
					Board.PrintBoard();
				}
			}
		}
		else
		{
			cout << "\rUnknown command '" << cmd << "'! ";
		}
	}

	cout << endl << "Goodbye!" << endl;
	system("pause");
	return 0;
}
