#pragma once

#define BASE_SCORE 1000000

inline char MirrorPos(char P)
{
	return (63 - P);
	//return ((7 - (P >> 3)) << 3) | (P & 7);
}

struct FAIPiece
{
	char Pos;
	FPieceDesc* Desc;
	bool bFirstMove : 1, bOpponent : 1, bDead : 1;

	FAIPiece()
		: bDead(false)
	{
	}
	inline void Set(char P, FPieceDesc* D, bool bFirst, bool bOpp)
	{
		Pos = P;
		Desc = D;
		bFirstMove = bFirst;
		bOpponent = bOpp;
	}
};
struct FAITask : public FAIMoveTest
{
	FAITask* NextWait;
	thread* Task;

	FAITask(FCheckPoint* P, char Index);
	void WaitForFinish()
	{
		if (Task)
			Task->join();
	}
};

struct FCheckPoint
{
	char Num;
	char Depth;
	FAIPiece Pieces[32];
	int BestScore;
	char BestStart, BestEnd;
	bool bDidMirror, bOpponentTurn;
	mutex BestMutex;
	FAITask* FirstWait;

	FCheckPoint()
		: Num(0), Depth(0), bDidMirror(false), bOpponentTurn(false), FirstWait(NULL)
	{}
	FCheckPoint(FCheckPoint* Parent, char StartMove, char EndMove)
		: Num(Parent->Num), Depth(Parent->Depth+1), bDidMirror(false), bOpponentTurn(!Parent->bOpponentTurn), FirstWait(NULL)
	{
		memcpy(&Pieces, &Parent->Pieces, sizeof(Pieces));

		for (char i = 0; i < Num; ++i)
		{
			if (Pieces[i].bDead)
				continue;

			if (Pieces[i].Pos == StartMove)
			{
				Pieces[i].bFirstMove = false;
				Pieces[i].Pos = EndMove;
			}
			else if (Pieces[i].Pos == EndMove)
				Pieces[i].bDead = true;
			Pieces[i].Pos = MirrorPos(Pieces[i].Pos);
			Pieces[i].bOpponent = !Pieces[i].bOpponent;
		}
	}
	~FCheckPoint()
	{
		if (FirstWait)
		{
			FAITask* NT;
			for (FAITask* T = FirstWait; T; T = NT)
			{
				NT = T->NextWait;
				delete T;
			}
			FirstWait = NULL;
		}
	}

	void SetupFrom(FGameBoard* B, bool bAITurn)
	{
		char Type;
		bDidMirror = (B->GameState & STATE_BTurn) != 0;
		for (char i = 0; i < (8 * 8); ++i)
		{
			Type = B->GetEntity(i);
			if (Type != TYPE_None)
				Pieces[Num++].Set(bDidMirror ? MirrorPos(i) : i, B->PieceDescs[Type], B->IsFirstMove(i), B->IsAI(i) != bAITurn);
		}
	}
	inline FAIPiece* FindPiece(char Space)
	{
		char i;
		for (i = 0; i < Num; ++i)
			if (!Pieces[i].bDead && Pieces[i].Pos == Space)
				return &Pieces[i];
		return nullptr;
	}
	int TestAllMoves(char* iStart, char* iEnd)
	{
		BestStart = 127;
		if (Depth==0)
		{
			for (char i = 0; i < Num; ++i)
				if (!Pieces[i].bDead && !Pieces[i].bOpponent)
					new FAITask(this, i);

			// Wait for all tasks to complete...
			for (FAITask* T = FirstWait; T; T = T->NextWait)
				T->WaitForFinish();
		}
		else
		{
			for (char i = 0; i < Num; ++i)
				if (!Pieces[i].bDead && !Pieces[i].bOpponent)
				{
					FAIMoveTest AI(this, &Pieces[i]);
					AI.Piece->Desc->IterateMoves(&AI);
				}
		}

		if (BestStart == 127)
			return -1;
		if (iStart)
		{
			if (bDidMirror)
				*iStart = MirrorPos(BestStart);
			else *iStart = BestStart;
		}
		if (iEnd)
		{
			if (bDidMirror)
				*iEnd = MirrorPos(BestEnd);
			else *iEnd = BestEnd;
		}
		if (BestScore == -1)
			BestScore = 0;
		return BestScore;
	}
	int TestMove(FAIMoveTest* AI, int EndX, int EndY, char Flags = FLAGS_EnemyOrEmpty)
	{
		char newPos;
		if (Flags & FLAGS_Absolute)
			newPos = EndX;
		else
		{
			int NewX = int(AI->X) + EndX;
			int NewY = int(AI->Y) + EndY;
			if (NewX < 0 || NewX>7 || NewY < 0 || NewY>7)
				return 0;
			newPos = (NewX | (NewY << 3)) & 0xFF;
		}

		FAIPiece* got = FindPiece(newPos);
		if (got)
		{
			if (got->bOpponent)
			{
				if (!(Flags & FLAGS_BTeam))
					return 0;
			}
			else if (!(Flags & FLAGS_ATeam))
				return 0;
		}
		else if (!(Flags & FLAGS_Empty))
			return 0;
		
		int Score;
		if (Depth == 0)
			Score = BASE_SCORE + (rand() & 63);
		else Score = 0;

		if (got)
			Score += (got->Desc->GetAIPriority(this, got) * 200);
		Score += (AI->Piece->Desc->GetAIPostMovePriority(AI, newPos) * 100);

		if (bOpponentTurn)
			Score = -Score;

		if (Depth < 5)
		{
			// Iterate to next suggested move to see how it plays out.
			FCheckPoint* SubMove = new FCheckPoint(this, AI->Piece->Pos, newPos);
			Score += SubMove->TestAllMoves(NULL, NULL);
			delete SubMove;
		}
		BestMutex.lock();
		if (BestStart == 127 || (bOpponentTurn ? (Score < BestScore) : (Score > BestScore)))
		{
			BestScore = Score;
			BestStart = AI->Piece->Pos;
			BestEnd = newPos;
		}
		BestMutex.unlock();

		return (got != nullptr) ? 2 : 1;
	}
};
struct FAILogic
{
	FGameBoard* Board;
	FCheckPoint MainCheck;

	FAILogic(FGameBoard* B)
		: Board(B)
	{
		MainCheck.SetupFrom(B, true);
	}
	void CreateMove()
	{
		char iStart, iEnd;
		int Score = MainCheck.TestAllMoves(&iStart, &iEnd);
		if (Score == -1)
		{
			cout << "AI unable to make any moves at all!" << endl;
			return;
		}
		cout << "AI[" << (Score - BASE_SCORE) << "] ";
		Board->CommitMove(iStart, iEnd);
	}
};

void UpdateWorkers(int Dir)
{
	static int NumWorkers = 0;
	static mutex TitleMutex;
	static TCHAR TitleStr[64];

	TitleMutex.lock();
	NumWorkers += Dir;
	swprintf_s(TitleStr, TEXT("Working %i..."), NumWorkers);
	SetConsoleTitle(TitleStr);
	TitleMutex.unlock();
}

void Task_Main(FAITask* AI)
{
	UpdateWorkers(1);
	AI->Piece->Desc->IterateMoves(AI);
	UpdateWorkers(-1);
}
FAITask::FAITask(FCheckPoint* P, char Index)
	: FAIMoveTest(P, &P->Pieces[Index]), Task(NULL)
{
	NextWait = P->FirstWait;
	P->FirstWait = this;
	Task = new thread(Task_Main, this);
}

FAIMoveTest::FAIMoveTest(FCheckPoint* C, FAIPiece* P)
	: Callback(C), Piece(P), X(P->Pos & 7), Y(P->Pos >> 3)
{}
inline int FAIMoveTest::TestMove(int EndX, int EndY, char Flags)
{
	return Callback->TestMove(this, EndX, EndY, Flags);
}

void FPawn::IterateMoves(FAIMoveTest* AI)
{
	AI->TestMove(0, 1, FLAGS_Empty);
	if (AI->Piece->bFirstMove)
		AI->TestMove(0, 2, FLAGS_Empty);
	AI->TestMove(-1, 1, FLAGS_BTeam);
	AI->TestMove(1, 1, FLAGS_BTeam);
}
int FPawn::GetAIPriority(FCheckPoint* AI, FAIPiece* Piece)
{
	char Height = (Piece->Pos >> 3);
	if (Piece->bOpponent)
		Height = 7 - Height;
	if (Height < 5)
		return 10;
	if (Height == 5)
		return 25;
	if (Height == 6)
		return 50;
	return 100;
}
int FPawn::GetAIPostMovePriority(FAIMoveTest* AI, char NewPos)
{
	char Height = (NewPos >> 3);
	if (AI->Piece->bOpponent)
		Height = 7 - Height;
	if (Height < 5)
		return 1;
	if (Height == 5)
		return 2;
	if (Height == 6)
		return 4;
	return 8;
}
void FHorse::IterateMoves(FAIMoveTest* AI)
{
	for (char iDir = 0; iDir < 8; ++iDir)
	{
		const FMovePair& MP = HorseMoveSet[iDir];
		AI->TestMove(MP.X, MP.Y, FLAGS_EnemyOrEmpty);
	}
}
int FHorse::GetAIPriority(FCheckPoint* AI, FAIPiece* Piece)
{
	return 25;
}
void FRook::IterateMoves(FAIMoveTest* AI)
{
	for (char iDir = 0; iDir < 4; ++iDir)
	{
		const FMovePair& MP = QueenMoveSet[iDir];
		for (SBYTE Step = 1; Step <= 8; ++Step)
		{
			if (AI->TestMove(MP.X * Step, MP.Y * Step, FLAGS_EnemyOrEmpty) != 1)
				break;
		}
	}
}
int FRook::GetAIPriority(FCheckPoint* AI, FAIPiece* Piece)
{
	return 75;
}
void FBishop::IterateMoves(FAIMoveTest* AI)
{
	for (char iDir = 4; iDir < 8; ++iDir)
	{
		const FMovePair& MP = QueenMoveSet[iDir];
		for (SBYTE Step = 1; Step <= 8; ++Step)
		{
			if (AI->TestMove(MP.X * Step, MP.Y * Step, FLAGS_EnemyOrEmpty) != 1)
				break;
		}
	}
}
int FBishop::GetAIPriority(FCheckPoint* AI, FAIPiece* Piece)
{
	return 70;
}
void FKing::IterateMoves(FAIMoveTest* AI)
{
	for (char iDir = 0; iDir < 8; ++iDir)
	{
		const FMovePair& MP = QueenMoveSet[iDir];
		AI->TestMove(MP.X, MP.Y, FLAGS_EnemyOrEmpty);
	}
}
int FKing::GetAIPriority(FCheckPoint* AI, FAIPiece* Piece)
{
	return 800;
}
void FQueen::IterateMoves(FAIMoveTest* AI)
{
	for (char iDir = 0; iDir < 8; ++iDir)
	{
		const FMovePair& MP = QueenMoveSet[iDir];
		for (SBYTE Step = 1; Step <= 8; ++Step)
		{
			if (AI->TestMove(MP.X * Step, MP.Y * Step, FLAGS_EnemyOrEmpty) != 1)
				break;
		}
	}
}
int FQueen::GetAIPriority(FCheckPoint* AI, FAIPiece* Piece)
{
	return 100;
}

void FGameBoard::AIMove()
{
	FAILogic AI(this);
	AI.CreateMove();
}
