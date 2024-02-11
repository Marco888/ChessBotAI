#pragma once

struct FNullType : public FPieceDesc
{
	EPiece GetType() const
	{
		return TYPE_None;
	}
	const char* GetName() const
	{
		return "NULL";
	}
	const char* GetIdentifier(bool bAI) const
	{
		return " ";
	}

	int GetAIPriority(FCheckPoint* AI, FAIPiece* Piece)
	{
		return 0;
	}
	bool MakeMove(char Start, char End)
	{
		return false;
	}
	void IterateMoves(FAIMoveTest* AI)
	{}
};
struct FPawn : public FPieceDesc
{
	EPiece GetType() const
	{
		return TYPE_Pawn;
	}
	const char* GetName() const
	{
		return "Pawn";
	}
	const char* GetIdentifier(bool bAI) const
	{
		return bAI ? "P" : "p";
	}

	void PostMove(char End)
	{
		if (MoveUp(End) ? (End >= (8 * 7)) : (End < 8))
		{
			cout << "Yikes! Pawn transformed into a Queen!" << endl;
			GameBoard->Board[End] = TYPE_Queen | (GameBoard->Board[End] & TEAM_AI);
		}
	}
	bool MakeMove(char Start, char End)
	{
		if (MoveUp(Start))
		{
			if (IsFirstMove(Start) && End == (Start + (8 * 2)) && !IsOccupied(End))
				return true;
			if (End == (Start + 8) && !IsOccupied(End))
				return true;
			if ((End == (Start + 7) || End == (Start + 9)) && IsOccupied(End) && IsAI(Start) != IsAI(End))
				return true;
		}
		else
		{
			if (IsFirstMove(Start) && End == (Start - (8 * 2)) && !IsOccupied(End))
				return true;
			if (End == (Start - 8) && !IsOccupied(End))
				return true;
			if ((End == (Start - 7) || End == (Start - 9)) && IsOccupied(End) && IsAI(Start) != IsAI(End))
				return true;
		}
		return false;
	}
	int GetAIPriority(FCheckPoint* AI, FAIPiece* Piece);
	int GetAIPostMovePriority(FAIMoveTest* AI, char NewPos);
	void IterateMoves(FAIMoveTest* AI);
};

static FMovePair QueenMoveSet[8] =
{ FMovePair(1,0), FMovePair(-1,0), FMovePair(0,1), FMovePair(0,-1), FMovePair(1,1), FMovePair(-1,1), FMovePair(1,-1), FMovePair(-1,-1) };

struct FQueen : public FPieceDesc
{
	EPiece GetType() const
	{
		return TYPE_Queen;
	}
	const char* GetName() const
	{
		return "Queen";
	}
	const char* GetIdentifier(bool bAI) const
	{
		return bAI ? "Q" : "q";
	}

	bool MakeMove(char Start, char End)
	{
		char Test;
		for (char iDir = 0; iDir < 8; ++iDir)
		{
			const FMovePair& MP = QueenMoveSet[iDir];
			Test = Start;
			for (char Step = 0; Step < 8; ++Step)
			{
				if (!MP.CanApply(Test, &Test))
					break;
				if (Test == End)
				{
					if (IsOccupied(End))
						return (IsAI(Start) != IsAI(End));
					return true;
				}
				else if (IsOccupied(Test))
					break;
			}
		}
		return false;
	}
	int GetAIPriority(FCheckPoint* AI, FAIPiece* Piece);
	void IterateMoves(FAIMoveTest* AI);
};

static FMovePair HorseMoveSet[8] =
{ FMovePair(2,1), FMovePair(2,-1), FMovePair(-2,1), FMovePair(-2,-1), FMovePair(1,2), FMovePair(-1,2), FMovePair(1,-2), FMovePair(-1,-2) };

struct FHorse : public FPieceDesc
{
	EPiece GetType() const
	{
		return TYPE_Horse;
	}
	const char* GetName() const
	{
		return "Horse";
	}
	const char* GetIdentifier(bool bAI) const
	{
		return bAI ? "H" : "h";
	}

	bool MakeMove(char Start, char End)
	{
		char Test;
		for (char iDir = 0; iDir < 8; ++iDir)
		{
			const FMovePair& MP = HorseMoveSet[iDir];
			Test = Start;
			if (MP.CanApply(Test, &Test) && Test == End)
			{
				if (IsOccupied(End))
					return (IsAI(Start) != IsAI(End));
				return true;
			}
		}
		return false;
	}
	int GetAIPriority(FCheckPoint* AI, FAIPiece* Piece);
	void IterateMoves(FAIMoveTest* AI);
};

struct FRook : public FPieceDesc
{
	EPiece GetType() const
	{
		return TYPE_Rook;
	}
	const char* GetName() const
	{
		return "Rook";
	}
	const char* GetIdentifier(bool bAI) const
	{
		return bAI ? "R" : "r";
	}

	bool MakeMove(char Start, char End)
	{
		char Test;
		for (char iDir = 0; iDir < 4; ++iDir)
		{
			const FMovePair& MP = QueenMoveSet[iDir];
			Test = Start;
			for (char Step = 0; Step < 8; ++Step)
			{
				if (!MP.CanApply(Test, &Test))
					break;
				if (Test == End)
				{
					if (IsOccupied(End))
						return (IsAI(Start) != IsAI(End));
					return true;
				}
				else if (IsOccupied(Test))
					break;
			}
		}
		return false;
	}
	int GetAIPriority(FCheckPoint* AI, FAIPiece* Piece);
	void IterateMoves(FAIMoveTest* AI);
};
struct FBishop : public FPieceDesc
{
	EPiece GetType() const
	{
		return TYPE_Bishop;
	}
	const char* GetName() const
	{
		return "Bishop";
	}
	const char* GetIdentifier(bool bAI) const
	{
		return bAI ? "B" : "b";
	}

	bool MakeMove(char Start, char End)
	{
		char Test;
		for (char iDir = 4; iDir < 8; ++iDir)
		{
			const FMovePair& MP = QueenMoveSet[iDir];
			Test = Start;
			for (char Step = 0; Step < 8; ++Step)
			{
				if (!MP.CanApply(Test, &Test))
					break;
				if (Test == End)
				{
					if (IsOccupied(End))
						return (IsAI(Start) != IsAI(End));
					return true;
				}
				else if (IsOccupied(Test))
					break;
			}
		}
		return false;
	}
	int GetAIPriority(FCheckPoint* AI, FAIPiece* Piece);
	void IterateMoves(FAIMoveTest* AI);
};
struct FKing : public FPieceDesc
{
	EPiece GetType() const
	{
		return TYPE_King;
	}
	const char* GetName() const
	{
		return "King";
	}
	const char* GetIdentifier(bool bAI) const
	{
		return bAI ? "K" : "k";
	}

	bool MakeMove(char Start, char End)
	{
		char Test;
		for (char iDir = 0; iDir < 8; ++iDir)
		{
			const FMovePair& MP = QueenMoveSet[iDir];
			if (MP.CanApply(Start, &Test) && Test==End)
			{
				if (IsOccupied(End))
					return (IsAI(Start) != IsAI(End));
				return true;
			}
		}
		return false;
	}
	int GetAIPriority(FCheckPoint* AI, FAIPiece* Piece);
	void IterateMoves(FAIMoveTest* AI);
};

void FGameBoard::InitDescriptors()
{
	FPieceDesc::GameBoard = this;
	PieceDescs[TYPE_None] = new FNullType;
	PieceDescs[TYPE_Pawn] = new FPawn;
	PieceDescs[TYPE_Horse] = new FHorse;
	PieceDescs[TYPE_Rook] = new FRook;
	PieceDescs[TYPE_Bishop] = new FBishop;
	PieceDescs[TYPE_Queen] = new FQueen;
	PieceDescs[TYPE_King] = new FKing;
}
void FGameBoard::PrintBoard()
{
	cout << endl << " *-*-*-*-*-*-*-*-*" << endl;
	for (char r = 0; r < 8; ++r)
	{
		cout << int(8-r) << "|";
		for (char c = 0; c < 8; ++c)
		{
			char type = Board[((7 - r) << 3) | c];
			char piec = type & TYPE_Max;
			if (piec == TYPE_None)
			{
				cout << " |";
				continue;
			}
			cout << PieceDescs[piec]->GetIdentifier((type & TEAM_AI) != 0) << "|";
		}
		cout << endl << " *-*-*-*-*-*-*-*-*" << endl;
	}
	cout << "  ";
	char str[2] = { 0,0 };
	for (char c = 0; c < 8; ++c)
	{
		str[0] = c + 'A';
		cout << str << " ";
	}
	cout << endl;
}
