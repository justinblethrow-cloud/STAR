#ifndef CODE_GenomeInsertIdentity
#define CODE_GenomeInsertIdentity

#include "IncludeDefine.h"

class Genome;
class Parameters;

string genomeInsertBaseIdentityFromDirectory(const string &directory, uint threadN, Parameters &P);
string genomeInsertBaseIdentityFromLoaded(Genome &genome);
string genomeInsertFileIdentity(const string &path, uint threadN, Parameters &P);

#endif
