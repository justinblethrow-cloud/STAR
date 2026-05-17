/*
 * inserts sequences into the SA
 * returns number of SA indexes inserted
 */
#include "insertSeqSA.h"
#include "ErrorWarning.h"
#include "SuffixArrayFuns.h"
#include "SequenceFuns.h"
#include "serviceFuns.cpp"
#include "streamFuns.h"
#include "binarySearch2.h"
#include "funCompareUintAndSuffixes.h"
#include "funCompareUintAndSuffixesMemcmp.h"
#include <cmath>
#include "sortSuffixesBucket.h"

namespace {
// Update the prefix table for newly inserted suffixes without rebuilding it from the expanded SA.
void insertSeqSAi(PackedArray &SAi, char **seq1, uint64 *indArray, uint64 nInd, Parameters &P, Genome &mapGen)
{
    time_t rawtime;

    for (uint iL=0; iL < mapGen.pGe.gSAindexNbases; iL++) {
        uint64 iSeq=0;
        uint ind0=mapGen.genomeSAindexStart[iL]-1;//last index that was present in the old genome

        for (uint ii=mapGen.genomeSAindexStart[iL]; ii<mapGen.genomeSAindexStart[iL+1]; ii++) {
            uint iSA1=SAi[ii];
            uint iSA2=iSA1 & mapGen.SAiMarkNmask & mapGen.SAiMarkAbsentMask;

            if (iSeq<nInd && (iSA1 & mapGen.SAiMarkAbsentMaskC)>0) {
                uint64 iSeq1=iSeq;
                int64 ind1=funCalcSAi(seq1[0]+indArray[2*iSeq+1], iL);
                while (iSeq<nInd && ind1 < (int64)(ii-mapGen.genomeSAindexStart[iL]) && indArray[2*iSeq]<=iSA2) {
                    ++iSeq;
                    if (iSeq<nInd) {
                        ind1=funCalcSAi(seq1[0]+indArray[2*iSeq+1], iL);
                    };
                };

                if (iSeq<nInd && ind1 == (int64)(ii-mapGen.genomeSAindexStart[iL])) {
                    SAi.writePacked(ii, indArray[2*iSeq]+iSeq);
                    for (uint ii0=ind0+1; ii0<ii; ii0++) {
                        SAi.writePacked(ii0, (indArray[2*iSeq]+iSeq) | mapGen.SAiMarkAbsentMaskC);
                    };
                    ++iSeq;
                    ind0=ii;
                } else {
                    iSeq=iSeq1;
                };
            } else {
                while (iSeq<nInd && indArray[2*iSeq]<iSA2) {
                    ++iSeq;
                };

                while (iSeq<nInd && indArray[2*iSeq]==iSA2) {
                    if (funCalcSAi(seq1[0]+indArray[2*iSeq+1], iL) >= (int64)(ii-mapGen.genomeSAindexStart[iL])) {
                        break;
                    };
                    ++iSeq;
                };

                SAi.writePacked(ii, iSA1+iSeq);

                for (uint ii0=ind0+1; ii0<ii; ii0++) {
                    SAi.writePacked(ii0, (iSA2+iSeq) | mapGen.SAiMarkAbsentMaskC);
                };
                ind0=ii;
            };
        };
    };

    for (uint64 iSeq=0; iSeq<nInd; iSeq++) {
        int64 ind1=0;
        for (uint iL=0; iL < mapGen.pGe.gSAindexNbases; iL++) {
            uint g=(uint) seq1[0][indArray[2*iSeq+1]+iL];
            ind1 <<= 2;
            if (g>3) {
                for (uint iL1=iL; iL1 < mapGen.pGe.gSAindexNbases; iL1++) {
                    ind1 += 3;
                    int64 ind2=mapGen.genomeSAindexStart[iL1]+ind1;
                    for (; ind2>=0; ind2--) {
                        if ((SAi[ind2] & mapGen.SAiMarkAbsentMaskC)==0) {
                            break;
                        };
                    };
                    SAi.writePacked(ind2, SAi[ind2] | mapGen.SAiMarkNmaskC);
                    ind1 <<= 2;
                };
                break;
            } else {
                ind1 += g;
            };
        };
    };

    time(&rawtime);
    P.inOut->logMain << timeMonthDayTime(rawtime) << "   Finished incremental SAi" << endl;
}
}

uint insertSeqSA(PackedArray & SA, PackedArray & SA1, PackedArray & SAi, char * G1, uint64 nG, uint64 nG1, uint64 nG2, Parameters & P, Genome &mapGen)
{//insert new sequences into the SA

    uint GstrandBit1 = (uint) floor(log(nG+nG1)/log(2))+1;
    if (GstrandBit1<32) GstrandBit1=32; //TODO: use simple access function for SA
    if ( GstrandBit1+1 > SA.wordLength)
    {//sequence is too long - GstrandBit changed
        ostringstream errOut;
        errOut << "EXITING because of FATAL ERROR: cannot insert sequence on the fly because of strand GstrandBit problem\n";
        errOut << "SOLUTION: please contact STAR author at https://groups.google.com/forum/#!forum/rna-star\n";
        exitWithError(errOut.str(),std::cerr, P.inOut->logMain, EXIT_CODE_GENOME_FILES, P);
    };

    uint N2bit= 1LLU << (SA.wordLength-1);
    uint strandMask=~N2bit;
    for (uint64 isa=0;isa<SA.length; isa++)
    {
        uint64 ind1=SA[isa];
        if ( (ind1 & N2bit)>0 )
        {//- strand
            if ( (ind1 & strandMask)>=nG2 )
            {//the first nG bases
                ind1+=nG1; //reverse complementary indices are all shifted by the length of the sequence
                SA.writePacked(isa,ind1);
            };
        } else
        {//+ strand
            if ( ind1>=nG )
            {//the last nG2 bases
                ind1+=nG1; //reverse complementary indices are all shifted by the length of the sequence
                SA.writePacked(isa,ind1);
            };
        };
    };

    char** seq1=new char*[2];

    #define GENOME_endFillL 16
    char* seqq=new char [4*nG1+3*GENOME_endFillL];//ends shouldbe filled with 5 to mark boundaries

    seq1[0]=seqq+GENOME_endFillL;//TODO: avoid defining an extra array, use reverse search
    seq1[1]=seqq+2*GENOME_endFillL+2*nG1;

    memset(seqq,GENOME_spacingChar,GENOME_endFillL);
    memset(seqq+2*nG1+GENOME_endFillL,GENOME_spacingChar,GENOME_endFillL);
    memset(seqq+4*nG1+2*GENOME_endFillL,GENOME_spacingChar,GENOME_endFillL);

    memcpy(seq1[0], G1, nG1);
    for (uint ii=0; ii<nG1; ii++)
    {//reverse complement sequence
        seq1[0][2*nG1-1-ii]=seq1[0][ii]<4 ? 3-seq1[0][ii] : seq1[0][ii];
    };
    complementSeqNumbers(seq1[0], seq1[1], 2*nG1);//complement

    uint64* indArray=new uint64[nG1*2*2+2];// for each base, 1st number - insertion place in SA, 2nd number - index, *2 for reverse compl


    #pragma omp parallel num_threads(P.runThreadN)
    #pragma omp for schedule (dynamic,1000)
    for (uint ii=0; ii<2*nG1; ii++) {//find insertion points for each of the sequences

        if (seq1[0][ii]>3)
        {//no index for suffices starting with N
            indArray[ii*2]=-1;
        } else
        {
            indArray[ii*2] =  suffixArraySearch1(mapGen, seq1, ii, 10000, nG, (ii<nG1 ? true:false), 0, SA.length-1, 0) ;
            indArray[ii*2+1] = ii;
        };
    };

    uint64 nInd=0;//true number of new indices
    for (uint ii=0; ii<2*nG1; ii++) {//remove entries that cannot be inserted, this cannot be done in the parallel cycle above
        if (indArray[ii*2]!= (uint) -1) {
            indArray[nInd*2]=indArray[ii*2];
            indArray[nInd*2+1]=indArray[ii*2+1];
            ++nInd;
        };
    };

    time_t rawtime;
    time ( &rawtime );
    P.inOut->logMain  << timeMonthDayTime(rawtime) << "   Finished SA search, number of new SA indices = "<<nInd<<endl;

    /*//old-debug
    uint64* indArray1=new uint64[nG1*2*2+2];
    memcpy((void*) indArray1, (void*) indArray, 8*(nG1*2*2+2));
    g_funCompareUintAndSuffixes_G=seq1[0];
    qsort((void*) indArray1, nInd, 2*sizeof(uint64), funCompareUintAndSuffixes);
    time ( &rawtime );
    P.inOut->logMain  << timeMonthDayTime(rawtime) << "   Finished qsort - old " <<endl;
    */

    g_funCompareUintAndSuffixesMemcmp_G=seq1[0];
    g_funCompareUintAndSuffixesMemcmp_L=mapGen.pGe.gSuffixLengthMax/sizeof(uint64_t);
    qsort((void*) indArray, nInd, 2*sizeof(uint64_t), funCompareUintAndSuffixesMemcmp);

//     qsort((void*) indArray, nInd, 2*sizeof(uint64), funCompareUint2);
    time ( &rawtime );
    P.inOut->logMain  << timeMonthDayTime(rawtime) << "   Finished qsort" <<endl;



    /*//new sorting, 2-step: qsort for indArray, bucket sort for suffixes
    qsort((void*) indArray, nInd, 2*sizeof(uint64), funCompareUint2);
    time ( &rawtime );
    P.inOut->logMain  << timeMonthDayTime(rawtime) << "   Finished qsort"<<nInd<<endl;

    sortSuffixesBucket(seq1[0], (void*) indArray, nInd, 2*sizeof(uint64));
    time ( &rawtime );
    P.inOut->logMain  << timeMonthDayTime(rawtime) << "   Finished ordering suffixes"<<nInd<<endl;
    */

    /* //debug
    for (int ii=0;ii<2*nInd;ii++)
    {
        if (indArray[ii]!=indArray1[ii])
        {
            cout << ii <<"   "<< indArray[ii]  <<"   "<< indArray1[ii] <<endl;
        };
    };
    */

    time ( &rawtime );
    P.inOut->logMain  << timeMonthDayTime(rawtime) << "   Finished sorting SA indices"<<endl;

    indArray[2*nInd]=-999; //mark the last junction
    indArray[2*nInd+1]=-999; //mark the last junction

    SA1.defineBits(SA.wordLength,SA.length+nInd);

    /*testing
    PackedArray SAo;
    SAo.defineBits(mapGen.GstrandBit+1,mapGen.nSA+nInd);
    SAo.allocateArray();
    ifstream oldSAin("./DirTrue/SA");
    oldSAin.read(SAo.charArray,SAo.lengthByte);
    oldSAin.close();
    */

    uint isa1=0, isa2=0;
    for (uint isa=0;isa<SA.length;isa++) {
        while (isa==indArray[isa1*2]) {//insert new index before the existing index
            uint ind1=indArray[isa1*2+1];
            if (ind1<nG1) {
                ind1+=nG;
            } else {//reverse strand
                ind1=(ind1-nG1+nG2) | N2bit;
            };
            SA1.writePacked(isa2,ind1);
            /*testing
            if (SA1[isa2]!=SAo[isa2]) {
               cout <<isa2 <<" "<< SA1[isa2]<<" "<<SAo[isa2]<<endl;
               //sleep(100);
            };
            */
            ++isa2; ++isa1;

        };

        SA1.writePacked(isa2,SA[isa]); //TODO make sure that the first sj index is not before the first array index
            /*testing
            if (SA1[isa2]!=SAo[isa2]) {
               cout <<isa2 <<" "<< SA1[isa2]<<" "<<SAo[isa2]<<endl;
               //sleep(100);
            };
            */
        ++isa2;
    };
    for (;isa1<nInd;isa1++)
    {//insert the last indices
        uint ind1=indArray[isa1*2+1];
        if (ind1<nG1)
        {
            ind1+=nG;
        } else
        {//reverse strand
            ind1=(ind1-nG1+nG2) | N2bit;
        };
        SA1.writePacked(isa2,ind1);
        ++isa2;
    };

    time ( &rawtime );
    P.inOut->logMain  << timeMonthDayTime(rawtime) << "   Finished inserting SA indices" <<endl;

    insertSeqSAi(SAi, seq1, indArray, nInd, P, mapGen);

    //change parameters, most parameters are already re-defined in sjdbPrepare.cpp
    SA.defineBits(mapGen.GstrandBit+1,SA.length+nInd);//same as SA2
    SA.pointArray(SA1.charArray);
    mapGen.nSA=SA.length;
    mapGen.nSAbyte=SA.lengthByte;

//     mapGen.sjGstart=mapGen.chrStart[mapGen.nChrReal];
//     memcpy(G+mapGen.chrStart[mapGen.nChrReal],seq1[0], nseq1[0]);


    return nInd;
};
