#ifndef CODE_GenomeInsertOverlay
#define CODE_GenomeInsertOverlay

#include "IncludeDefine.h"

class Parameters;

bool genomeInsertOverlayLoad(Parameters &P);
void genomeInsertOutputPrepare(Parameters &P);
void genomeInsertOverlayWrite(Parameters &P);
void genomeInsertOutputFinalize(Parameters &P);
string genomeInsertDeltaFilePath(const string &dir);

#endif
