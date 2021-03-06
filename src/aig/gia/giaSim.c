/**CFile****************************************************************

  FileName    [giaSim.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Scalable AIG package.]

  Synopsis    [Fast sequential simulator.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: giaSim.c,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/

#include "gia.h"
#include "misc/util/utilTruth.h"

ABC_NAMESPACE_IMPL_START


////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static inline unsigned * Gia_SimData( Gia_ManSim_t * p, int i )    { return p->pDataSim + i * p->nWords;    }
static inline unsigned * Gia_SimDataCi( Gia_ManSim_t * p, int i )  { return p->pDataSimCis + i * p->nWords; }
static inline unsigned * Gia_SimDataCo( Gia_ManSim_t * p, int i )  { return p->pDataSimCos + i * p->nWords; }

unsigned * Gia_SimDataExt( Gia_ManSim_t * p, int i )    { return Gia_SimData(p, i);    }
unsigned * Gia_SimDataCiExt( Gia_ManSim_t * p, int i )  { return Gia_SimDataCi(p, i);  }
unsigned * Gia_SimDataCoExt( Gia_ManSim_t * p, int i )  { return Gia_SimDataCo(p, i);  }

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////


/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSimCollect_rec( Gia_Man_t * pGia, Gia_Obj_t * pObj, Vec_Int_t * vVec )
{
    Vec_IntPush( vVec, Gia_ObjToLit(pGia, pObj) );
    if ( Gia_IsComplement(pObj) || Gia_ObjIsCi(pObj) )
        return;
    assert( Gia_ObjIsAnd(pObj) );
    Gia_ManSimCollect_rec( pGia, Gia_ObjChild0(pObj), vVec );
    Gia_ManSimCollect_rec( pGia, Gia_ObjChild1(pObj), vVec );
}

/**Function*************************************************************

  Synopsis    [Derives signal implications.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSimCollect( Gia_Man_t * pGia, Gia_Obj_t * pObj, Vec_Int_t * vVec )
{
    Vec_IntClear( vVec );
    Gia_ManSimCollect_rec( pGia, pObj, vVec );
    Vec_IntUniqify( vVec );
}

/**Function*************************************************************

  Synopsis    [Finds signals, which reset flops to have constant values.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Int_t * Gia_ManSimDeriveResets( Gia_Man_t * pGia )
{
    int nImpLimit = 5;
    Vec_Int_t * vResult;
    Vec_Int_t * vCountLits, * vSuperGate;
    Gia_Obj_t * pObj;
    int i, k, Lit, Count;
    int Counter0 = 0, Counter1 = 0;
    int CounterPi0 = 0, CounterPi1 = 0;
    abctime clk = Abc_Clock();

    // create reset counters for each literal
    vCountLits = Vec_IntStart( 2 * Gia_ManObjNum(pGia) );

    // collect implications for each flop input driver
    vSuperGate = Vec_IntAlloc( 1000 );
    Gia_ManForEachRi( pGia, pObj, i )
    {
        if ( Gia_ObjFaninId0p(pGia, pObj) == 0 )
            continue;
        Vec_IntAddToEntry( vCountLits, Gia_ObjToLit(pGia, Gia_ObjChild0(pObj)), 1 );
        Gia_ManSimCollect( pGia, Gia_ObjFanin0(pObj), vSuperGate );
        Vec_IntForEachEntry( vSuperGate, Lit, k )
            Vec_IntAddToEntry( vCountLits, Lit, 1 );
    }
    Vec_IntFree( vSuperGate );

    // label signals whose counter if more than the limit
    vResult = Vec_IntStartFull( Gia_ManObjNum(pGia) );
    Vec_IntForEachEntry( vCountLits, Count, Lit )
    {
        if ( Count < nImpLimit )
            continue;
        pObj = Gia_ManObj( pGia, Abc_Lit2Var(Lit) );
        if ( Abc_LitIsCompl(Lit) ) // const 0
        {
//            Ssm_ObjSetLogic0( pObj );
            Vec_IntWriteEntry( vResult, Abc_Lit2Var(Lit), 0 );
            CounterPi0 += Gia_ObjIsPi(pGia, pObj);
            Counter0++;
        }
        else
        {
//            Ssm_ObjSetLogic1( pObj );
            Vec_IntWriteEntry( vResult, Abc_Lit2Var(Lit), 1 );
            CounterPi1 += Gia_ObjIsPi(pGia, pObj);
            Counter1++;
        }
//        if ( Gia_ObjIsPi(pGia, pObj) )
//            printf( "%d ", Count );
    }
//    printf( "\n" );
    Vec_IntFree( vCountLits );

    printf( "Logic0 = %d (%d). Logic1 = %d (%d). ", Counter0, CounterPi0, Counter1, CounterPi1 );
    Abc_PrintTime( 1, "Time", Abc_Clock() - clk );
    return vResult;
}


/**Function*************************************************************

  Synopsis    [This procedure sets default parameters.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSimSetDefaultParams( Gia_ParSim_t * p )
{
    memset( p, 0, sizeof(Gia_ParSim_t) );
    // user-controlled parameters
    p->nWords       =   8;    // the number of machine words
    p->nIters       =  32;    // the number of timeframes
    p->RandSeed     =   0;    // the seed to generate random numbers
    p->TimeLimit    =  60;    // time limit in seconds
    p->fCheckMiter  =   0;    // check if miter outputs are non-zero 
    p->fVerbose     =   0;    // enables verbose output
    p->iOutFail     =  -1;    // index of the failed output
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSimDelete( Gia_ManSim_t * p )
{
    Vec_IntFreeP( &p->vConsts );
    Vec_IntFreeP( &p->vCis2Ids );
    Gia_ManStopP( &p->pAig );
    ABC_FREE( p->pDataSim );
    ABC_FREE( p->pDataSimCis );
    ABC_FREE( p->pDataSimCos );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Creates fast simulation manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Gia_ManSim_t * Gia_ManSimCreate( Gia_Man_t * pAig, Gia_ParSim_t * pPars )
{
    Gia_ManSim_t * p;
    int Entry, i;
    p = ABC_ALLOC( Gia_ManSim_t, 1 );
    memset( p, 0, sizeof(Gia_ManSim_t) );
    // look for reset signals
    if ( pPars->fVerbose )
        p->vConsts = Gia_ManSimDeriveResets( pAig );
    // derive the frontier
    p->pAig   = Gia_ManFront( pAig );
    p->pPars  = pPars;
    p->nWords = pPars->nWords;
    p->pDataSim = ABC_ALLOC( unsigned, p->nWords * p->pAig->nFront );
    p->pDataSimCis = ABC_ALLOC( unsigned, p->nWords * Gia_ManCiNum(p->pAig) );
    p->pDataSimCos = ABC_ALLOC( unsigned, p->nWords * Gia_ManCoNum(p->pAig) );
    if ( !p->pDataSim || !p->pDataSimCis || !p->pDataSimCos )
    { 
        Abc_Print( 1, "Simulator could not allocate %.2f GB for simulation info.\n", 
            4.0 * p->nWords * (p->pAig->nFront + Gia_ManCiNum(p->pAig) + Gia_ManCoNum(p->pAig)) / (1<<30) );
        Gia_ManSimDelete( p );
        return NULL;
    }
    p->vCis2Ids = Vec_IntAlloc( Gia_ManCiNum(p->pAig) );
    Vec_IntForEachEntry( pAig->vCis, Entry, i )
        Vec_IntPush( p->vCis2Ids, i );  //  do we need p->vCis2Ids?
    if ( pPars->fVerbose )
    Abc_Print( 1, "AIG = %7.2f MB.   Front mem = %7.2f MB.  Other mem = %7.2f MB.\n", 
        12.0*Gia_ManObjNum(p->pAig)/(1<<20), 
        4.0*p->nWords*p->pAig->nFront/(1<<20), 
        4.0*p->nWords*(Gia_ManCiNum(p->pAig) + Gia_ManCoNum(p->pAig))/(1<<20) );

    return p;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void Gia_ManSimInfoRandom( Gia_ManSim_t * p, unsigned * pInfo )
{
    int w;
    for ( w = p->nWords-1; w >= 0; w-- )
        pInfo[w] = Gia_ManRandom( 0 );
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void Gia_ManSimInfoZero( Gia_ManSim_t * p, unsigned * pInfo )
{
    int w;
    for ( w = p->nWords-1; w >= 0; w-- )
        pInfo[w] = 0;
}

/**Function*************************************************************

  Synopsis    [Returns index of the first pattern that failed.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int Gia_ManSimInfoIsZero( Gia_ManSim_t * p, unsigned * pInfo )
{
    int w;
    for ( w = 0; w < p->nWords; w++ )
        if ( pInfo[w] )
            return 32*w + Gia_WordFindFirstBit( pInfo[w] );
    return -1;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void Gia_ManSimInfoOne( Gia_ManSim_t * p, unsigned * pInfo )
{
    int w;
    for ( w = p->nWords-1; w >= 0; w-- )
        pInfo[w] = ~0;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void Gia_ManSimInfoCopy( Gia_ManSim_t * p, unsigned * pInfo, unsigned * pInfo0 )
{
    int w;
    for ( w = p->nWords-1; w >= 0; w-- )
        pInfo[w] = pInfo0[w];
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void Gia_ManSimulateCi( Gia_ManSim_t * p, Gia_Obj_t * pObj, int iCi )
{
    unsigned * pInfo  = Gia_SimData( p, Gia_ObjValue(pObj) );
    unsigned * pInfo0 = Gia_SimDataCi( p, iCi );
    int w;
    for ( w = p->nWords-1; w >= 0; w-- )
        pInfo[w] = pInfo0[w];
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void Gia_ManSimulateCo( Gia_ManSim_t * p, int iCo, Gia_Obj_t * pObj )
{
    unsigned * pInfo  = Gia_SimDataCo( p, iCo );
    unsigned * pInfo0 = Gia_SimData( p, Gia_ObjDiff0(pObj) );
    int w;
    if ( Gia_ObjFaninC0(pObj) )
        for ( w = p->nWords-1; w >= 0; w-- )
            pInfo[w] = ~pInfo0[w];
    else 
        for ( w = p->nWords-1; w >= 0; w-- )
            pInfo[w] = pInfo0[w];
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void Gia_ManSimulateNode( Gia_ManSim_t * p, Gia_Obj_t * pObj )
{
    unsigned * pInfo  = Gia_SimData( p, Gia_ObjValue(pObj) );
    unsigned * pInfo0 = Gia_SimData( p, Gia_ObjDiff0(pObj) );
    unsigned * pInfo1 = Gia_SimData( p, Gia_ObjDiff1(pObj) );
    int w;
    if ( Gia_ObjFaninC0(pObj) )
    {
        if (  Gia_ObjFaninC1(pObj) )
            for ( w = p->nWords-1; w >= 0; w-- )
                pInfo[w] = ~(pInfo0[w] | pInfo1[w]);
        else 
            for ( w = p->nWords-1; w >= 0; w-- )
                pInfo[w] = ~pInfo0[w] & pInfo1[w];
    }
    else 
    {
        if (  Gia_ObjFaninC1(pObj) )
            for ( w = p->nWords-1; w >= 0; w-- )
                pInfo[w] = pInfo0[w] & ~pInfo1[w];
        else 
            for ( w = p->nWords-1; w >= 0; w-- )
                pInfo[w] = pInfo0[w] & pInfo1[w];
    }
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSimInfoInit( Gia_ManSim_t * p )
{
    int iPioNum, i;
    Vec_IntForEachEntry( p->vCis2Ids, iPioNum, i )
    {
        if ( iPioNum < Gia_ManPiNum(p->pAig) )
            Gia_ManSimInfoRandom( p, Gia_SimDataCi(p, i) );
        else
            Gia_ManSimInfoZero( p, Gia_SimDataCi(p, i) );
    }
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSimInfoTransfer( Gia_ManSim_t * p )
{
    int iPioNum, i;
    Vec_IntForEachEntry( p->vCis2Ids, iPioNum, i )
    {
        if ( iPioNum < Gia_ManPiNum(p->pAig) )
            Gia_ManSimInfoRandom( p, Gia_SimDataCi(p, i) );
        else
            Gia_ManSimInfoCopy( p, Gia_SimDataCi(p, i), Gia_SimDataCo(p, Gia_ManPoNum(p->pAig)+iPioNum-Gia_ManPiNum(p->pAig)) );
    }
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSimulateRound( Gia_ManSim_t * p )
{
    Gia_Obj_t * pObj;
    int i, iCis = 0, iCos = 0;
    assert( p->pAig->nFront > 0 );
    assert( Gia_ManConst0(p->pAig)->Value == 0 );
    Gia_ManSimInfoZero( p, Gia_SimData(p, 0) );
    Gia_ManForEachObj1( p->pAig, pObj, i )
    {
        if ( Gia_ObjIsAndOrConst0(pObj) )
        {
            assert( Gia_ObjValue(pObj) < p->pAig->nFront );
            Gia_ManSimulateNode( p, pObj );
        }
        else if ( Gia_ObjIsCo(pObj) )
        {
            assert( Gia_ObjValue(pObj) == GIA_NONE );
            Gia_ManSimulateCo( p, iCos++, pObj );
        }
        else // if ( Gia_ObjIsCi(pObj) )
        {
            assert( Gia_ObjValue(pObj) < p->pAig->nFront );
            Gia_ManSimulateCi( p, pObj, iCis++ );
        }
    }
    assert( Gia_ManCiNum(p->pAig) == iCis );
    assert( Gia_ManCoNum(p->pAig) == iCos );
}

/**Function*************************************************************

  Synopsis    [Returns index of the PO and pattern that failed it.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline int Gia_ManCheckPos( Gia_ManSim_t * p, int * piPo, int * piPat )
{
    int i, iPat;
    for ( i = 0; i < Gia_ManPoNum(p->pAig); i++ )
    {
        iPat = Gia_ManSimInfoIsZero( p, Gia_SimDataCo(p, i) );
        if ( iPat >= 0 )
        {
            *piPo = i;
            *piPat = iPat;
            return 1;
        }
    }
    return 0;
}

/**Function*************************************************************

  Synopsis    [Returns the counter-example.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Abc_Cex_t * Gia_ManGenerateCounter( Gia_Man_t * pAig, int iFrame, int iOut, int nWords, int iPat, Vec_Int_t * vCis2Ids )
{
    Abc_Cex_t * p;
    unsigned * pData;
    int f, i, w, iPioId, Counter;
    p = Abc_CexAlloc( Gia_ManRegNum(pAig), Gia_ManPiNum(pAig), iFrame+1 );
    p->iFrame = iFrame;
    p->iPo    = iOut;
    // fill in the binary data
    Counter = p->nRegs;
    pData = ABC_ALLOC( unsigned, nWords );
    for ( f = 0; f <= iFrame; f++, Counter += p->nPis )
    for ( i = 0; i < Gia_ManPiNum(pAig); i++ )
    {
        iPioId = Vec_IntEntry( vCis2Ids, i );
        if ( iPioId >= p->nPis )
            continue;
        for ( w = nWords-1; w >= 0; w-- )
            pData[w] = Gia_ManRandom( 0 );
        if ( Abc_InfoHasBit( pData, iPat ) )
            Abc_InfoSetBit( p->pData, Counter + iPioId );
    }
    ABC_FREE( pData );
    return p;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManResetRandom( Gia_ParSim_t * pPars )
{
    int i;
    Gia_ManRandom( 1 );
    for ( i = 0; i < pPars->RandSeed; i++ )
        Gia_ManRandom( 0 );
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ManSimSimulate( Gia_Man_t * pAig, Gia_ParSim_t * pPars )
{
    extern int Gia_ManSimSimulateEquiv( Gia_Man_t * pAig, Gia_ParSim_t * pPars );
    Gia_ManSim_t * p;
    abctime clkTotal = Abc_Clock();
    int i, iOut, iPat, RetValue = 0;
    abctime nTimeToStop = pPars->TimeLimit ? pPars->TimeLimit * CLOCKS_PER_SEC + Abc_Clock(): 0;
    if ( pAig->pReprs && pAig->pNexts )
        return Gia_ManSimSimulateEquiv( pAig, pPars );
    ABC_FREE( pAig->pCexSeq );
    p = Gia_ManSimCreate( pAig, pPars );
    Gia_ManResetRandom( pPars );
    Gia_ManSimInfoInit( p );
    for ( i = 0; i < pPars->nIters; i++ )
    {
        Gia_ManSimulateRound( p );
        if ( pPars->fVerbose )
        {
            Abc_Print( 1, "Frame %4d out of %4d and timeout %3d sec. ", i+1, pPars->nIters, pPars->TimeLimit );
            Abc_Print( 1, "Time = %7.2f sec\r", (1.0*Abc_Clock()-clkTotal)/CLOCKS_PER_SEC );
        }
        if ( pPars->fCheckMiter && Gia_ManCheckPos( p, &iOut, &iPat ) )
        {
            Gia_ManResetRandom( pPars );
            pPars->iOutFail = iOut;
            pAig->pCexSeq = Gia_ManGenerateCounter( pAig, i, iOut, p->nWords, iPat, p->vCis2Ids );
            Abc_Print( 1, "Output %d of miter \"%s\" was asserted in frame %d.  ", iOut, pAig->pName, i );
            if ( !Gia_ManVerifyCex( pAig, pAig->pCexSeq, 0 ) )
            {
//                Abc_Print( 1, "\n" );
                Abc_Print( 1, "\nGenerated counter-example is INVALID.                    " );
//                Abc_Print( 1, "\n" );
            }
            else
            {
//                Abc_Print( 1, "\n" );
//                if ( pPars->fVerbose )
//                Abc_Print( 1, "\nGenerated counter-example is verified correctly.         " );
//                Abc_Print( 1, "\n" );
            }
            RetValue = 1;
            break;
        }
        if ( Abc_Clock() > nTimeToStop )
        {
            i++;
            break;
        }
        if ( i < pPars->nIters - 1 )
            Gia_ManSimInfoTransfer( p );
    }
    Gia_ManSimDelete( p );
    if ( pAig->pCexSeq == NULL )
        Abc_Print( 1, "No bug detected after simulating %d frames with %d words.  ", i, pPars->nWords );
    Abc_PrintTime( 1, "Time", Abc_Clock() - clkTotal );
    return RetValue;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Int_t * Gia_ManSimReadFile( char * pFileIn )
{
    int c;
    Vec_Int_t * vPat;
    FILE * pFile = fopen( pFileIn, "rb" );
    if ( pFile == NULL )
    {
        printf( "Cannot open input file.\n" );
        return NULL;
    }
    vPat = Vec_IntAlloc( 1000 );
    while ( (c = fgetc(pFile)) != EOF )
        if ( c == '0' || c == '1' )
            Vec_IntPush( vPat, c - '0' );
    fclose( pFile );
    return vPat;
}
int Gia_ManSimWriteFile( char * pFileOut, Vec_Int_t * vPat, int nOuts )
{
    int c, i;
    FILE * pFile = fopen( pFileOut, "wb" );
    if ( pFile == NULL )
    {
        printf( "Cannot open output file.\n" );
        return 0;
    }
    assert( Vec_IntSize(vPat) % nOuts == 0 );
    Vec_IntForEachEntry( vPat, c, i )
    {
        fputc( '0' + c, pFile );
        if ( i % nOuts == nOuts - 1 )
            fputc( '\n', pFile );
    }
    fclose( pFile );
    return 1;
}
Vec_Int_t * Gia_ManSimSimulateOne( Gia_Man_t * p, Vec_Int_t * vPat )
{
    Vec_Int_t * vPatOut;
    Gia_Obj_t * pObj, * pObjRo;
    int i, k, f;
    assert( Vec_IntSize(vPat) % Gia_ManPiNum(p) == 0 );
    Gia_ManConst0(p)->fMark1 = 0;
    Gia_ManForEachRo( p, pObj, i )
        pObj->fMark1 = 0;
    vPatOut = Vec_IntAlloc( 1000 );
    for ( k = f = 0; f < Vec_IntSize(vPat) / Gia_ManPiNum(p); f++ )
    {
        Gia_ManForEachPi( p, pObj, i )
            pObj->fMark1 = Vec_IntEntry( vPat, k++ );
        Gia_ManForEachAnd( p, pObj, i )
            pObj->fMark1 = (Gia_ObjFanin0(pObj)->fMark1 ^ Gia_ObjFaninC0(pObj)) & (Gia_ObjFanin1(pObj)->fMark1 ^ Gia_ObjFaninC1(pObj));
        Gia_ManForEachCo( p, pObj, i )
            pObj->fMark1 = (Gia_ObjFanin0(pObj)->fMark1 ^ Gia_ObjFaninC0(pObj));
        Gia_ManForEachPo( p, pObj, i )
            Vec_IntPush( vPatOut, pObj->fMark1 );
        Gia_ManForEachRiRo( p, pObj, pObjRo, i )
            pObjRo->fMark1 = pObj->fMark1;
    }
    assert( k == Vec_IntSize(vPat) );
    Gia_ManForEachObj( p, pObj, i )
        pObj->fMark1 = 0;
    return vPatOut;
}
void Gia_ManSimSimulatePattern( Gia_Man_t * p, char * pFileIn, char * pFileOut )
{
    Vec_Int_t * vPat, * vPatOut;
    vPat = Gia_ManSimReadFile( pFileIn );
    if ( vPat == NULL )
        return;
    if ( Vec_IntSize(vPat) % Gia_ManPiNum(p) )
    {
        printf( "The number of 0s and 1s in the input file (%d) does not evenly divide by the number of primary inputs (%d).\n", 
            Vec_IntSize(vPat), Gia_ManPiNum(p) );
        Vec_IntFree( vPat );
        return;
    }
    vPatOut = Gia_ManSimSimulateOne( p, vPat );
    if ( Gia_ManSimWriteFile( pFileOut, vPatOut, Gia_ManPoNum(p) ) )
        printf( "Output patterns are written into file \"%s\".\n", pFileOut );
    Vec_IntFree( vPat );
    Vec_IntFree( vPatOut );
}


/**Function*************************************************************

  Synopsis    [Bit-parallel simulation during AIG construction.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline word * Gia_ManBuiltInDataPi( Gia_Man_t * p, int iCiId )
{
    return Vec_WrdEntryP( p->vSimsPi, p->nSimWords * iCiId );
}
static inline word * Gia_ManBuiltInData( Gia_Man_t * p, int iObj )
{
    return Vec_WrdEntryP( p->vSims, p->nSimWords * iObj );
}
static inline void Gia_ManBuiltInDataPrint( Gia_Man_t * p, int iObj )
{
//    printf( "Obj %6d : ", iObj ); Extra_PrintBinary( stdout, Gia_ManBuiltInData(p, iObj), p->nSimWords * 64 ); printf( "\n" );
}
void Gia_ManBuiltInSimStart( Gia_Man_t * p, int nWords, int nObjs )
{
    int i, k;
    assert( !p->fBuiltInSim );
    assert( Gia_ManAndNum(p) == 0 );
    p->fBuiltInSim = 1;
    p->iPatsPi = 0;
    p->iPastPiMax = 0;
    p->nSimWords = nWords;
    p->nSimWordsMax = 8;
    Gia_ManRandomW( 1 );
    // init PI care info
    p->vSimsPi = Vec_WrdAlloc( p->nSimWords * Gia_ManCiNum(p) );
    Vec_WrdFill( p->vSimsPi, p->nSimWords * Gia_ManCiNum(p), 0 );
    // init object sim info
    p->vSims = Vec_WrdAlloc( p->nSimWords * nObjs );
    Vec_WrdFill( p->vSims, p->nSimWords, 0 );
    for ( i = 0; i < Gia_ManCiNum(p); i++ )
        for ( k = 0; k < p->nSimWords; k++ )
            Vec_WrdPush( p->vSims, Gia_ManRandomW(0) );
}
void Gia_ManBuiltInSimPerformInt( Gia_Man_t * p, int iObj )
{
    Gia_Obj_t * pObj = Gia_ManObj( p, iObj ); int w;
    word * pInfo  = Gia_ManBuiltInData( p, iObj ); 
    word * pInfo0 = Gia_ManBuiltInData( p, Gia_ObjFaninId0(pObj, iObj) );
    word * pInfo1 = Gia_ManBuiltInData( p, Gia_ObjFaninId1(pObj, iObj) ); 
    assert( p->fBuiltInSim || p->fIncrSim );
    if ( Gia_ObjFaninC0(pObj) )
    {
        if (  Gia_ObjFaninC1(pObj) )
            for ( w = 0; w < p->nSimWords; w++ )
                pInfo[w] = ~(pInfo0[w] | pInfo1[w]);
        else 
            for ( w = 0; w < p->nSimWords; w++ )
                pInfo[w] = ~pInfo0[w] & pInfo1[w];
    }
    else 
    {
        if (  Gia_ObjFaninC1(pObj) )
            for ( w = 0; w < p->nSimWords; w++ )
                pInfo[w] = pInfo0[w] & ~pInfo1[w];
        else 
            for ( w = 0; w < p->nSimWords; w++ )
                pInfo[w] = pInfo0[w] & pInfo1[w];
    }
    assert( Vec_WrdSize(p->vSims) == Gia_ManObjNum(p) * p->nSimWords );
}
void Gia_ManBuiltInSimPerform( Gia_Man_t * p, int iObj )
{
    int w;
    for ( w = 0; w < p->nSimWords; w++ )
        Vec_WrdPush( p->vSims, 0 );
    Gia_ManBuiltInSimPerformInt( p, iObj );
}

void Gia_ManBuiltInSimResimulateCone_rec( Gia_Man_t * p, int iObj )
{
    Gia_Obj_t * pObj;
    if ( Gia_ObjIsTravIdCurrentId(p, iObj) )
        return;
    Gia_ObjSetTravIdCurrentId(p, iObj);
    pObj = Gia_ManObj( p, iObj );
    if ( Gia_ObjIsCi(pObj) )
        return;
    assert( Gia_ObjIsAnd(pObj) );
    Gia_ManBuiltInSimResimulateCone_rec( p, Gia_ObjFaninId0(pObj, iObj) );
    Gia_ManBuiltInSimResimulateCone_rec( p, Gia_ObjFaninId1(pObj, iObj) );
    Gia_ManBuiltInSimPerformInt( p, iObj );
}
void Gia_ManBuiltInSimResimulateCone( Gia_Man_t * p, int iLit0, int iLit1 )
{
    Gia_ManIncrementTravId( p );
    Gia_ManBuiltInSimResimulateCone_rec( p, Abc_Lit2Var(iLit0) );
    Gia_ManBuiltInSimResimulateCone_rec( p, Abc_Lit2Var(iLit1) );
}
void Gia_ManBuiltInSimResimulate( Gia_Man_t * p )
{
    Gia_Obj_t * pObj;  int iObj;
    Gia_ManForEachAnd( p, pObj, iObj )
        Gia_ManBuiltInSimPerformInt( p, iObj );
}

int Gia_ManBuiltInSimCheckOver( Gia_Man_t * p, int iLit0, int iLit1 )
{
    word * pInfo0 = Gia_ManBuiltInData( p, Abc_Lit2Var(iLit0) );
    word * pInfo1 = Gia_ManBuiltInData( p, Abc_Lit2Var(iLit1) ); int w;
    assert( p->fBuiltInSim || p->fIncrSim );

//    printf( "%d %d\n", Abc_LitIsCompl(iLit0), Abc_LitIsCompl(iLit1) );
//    Extra_PrintBinary( stdout, pInfo0, 64*p->nSimWords ); printf( "\n" );
//    Extra_PrintBinary( stdout, pInfo1, 64*p->nSimWords ); printf( "\n" );
//    printf( "\n" );

    if ( Abc_LitIsCompl(iLit0) )
    {
        if (  Abc_LitIsCompl(iLit1) )
        {
            for ( w = 0; w < p->nSimWords; w++ )
                if ( ~pInfo0[w] & ~pInfo1[w] )
                    return 1;
        }
        else 
        {
            for ( w = 0; w < p->nSimWords; w++ )
                if ( ~pInfo0[w] & pInfo1[w] )
                    return 1;
        }
    }
    else 
    {
        if (  Abc_LitIsCompl(iLit1) )
        {
            for ( w = 0; w < p->nSimWords; w++ )
                if ( pInfo0[w] & ~pInfo1[w] )
                    return 1;
        }
        else 
        {
            for ( w = 0; w < p->nSimWords; w++ )
                if ( pInfo0[w] & pInfo1[w] )
                    return 1;
        }
    }
    return 0;
}
int Gia_ManBuiltInSimCheckEqual( Gia_Man_t * p, int iLit0, int iLit1 )
{
    word * pInfo0 = Gia_ManBuiltInData( p, Abc_Lit2Var(iLit0) );
    word * pInfo1 = Gia_ManBuiltInData( p, Abc_Lit2Var(iLit1) ); int w;
    assert( p->fBuiltInSim || p->fIncrSim );

//    printf( "%d %d\n", Abc_LitIsCompl(iLit0), Abc_LitIsCompl(iLit1) );
//    Extra_PrintBinary( stdout, pInfo0, 64*p->nSimWords ); printf( "\n" );
//    Extra_PrintBinary( stdout, pInfo1, 64*p->nSimWords ); printf( "\n" );
//    printf( "\n" );

    if ( Abc_LitIsCompl(iLit0) )
    {
        if (  Abc_LitIsCompl(iLit1) )
        {
            for ( w = 0; w < p->nSimWords; w++ )
                if ( ~pInfo0[w] != ~pInfo1[w] )
                    return 0;
        }
        else 
        {
            for ( w = 0; w < p->nSimWords; w++ )
                if ( ~pInfo0[w] != pInfo1[w] )
                    return 0;
        }
    }
    else 
    {
        if (  Abc_LitIsCompl(iLit1) )
        {
            for ( w = 0; w < p->nSimWords; w++ )
                if ( pInfo0[w] != ~pInfo1[w] )
                    return 0;
        }
        else 
        {
            for ( w = 0; w < p->nSimWords; w++ )
                if ( pInfo0[w] != pInfo1[w] )
                    return 0;
        }
    }
    return 1;
}

int Gia_ManBuiltInSimPack( Gia_Man_t * p, Vec_Int_t * vPat )
{
    int i, k, iLit;
    assert( Vec_IntSize(vPat) > 0 );
    //printf( "%d ", Vec_IntSize(vPat) );
    for ( i = 0; i < p->iPatsPi; i++ )
    {
        Vec_IntForEachEntry( vPat, iLit, k )
            if ( Abc_TtGetBit(Gia_ManBuiltInDataPi(p, Abc_Lit2Var(iLit)), i) && 
                 Abc_TtGetBit(Gia_ManBuiltInData(p, 1+Abc_Lit2Var(iLit)), i) == Abc_LitIsCompl(iLit) )
                break;
        if ( k == Vec_IntSize(vPat) )
            return i; // success
    }
    return -1; // failure
}
// adds PI pattern to storage; the pattern comes in terms of CiIds
int Gia_ManBuiltInSimAddPat( Gia_Man_t * p, Vec_Int_t * vPat )
{
    int Period = 0xF;
    int fOverflow = p->iPatsPi == 64 * p->nSimWords && p->nSimWords == p->nSimWordsMax;
    int k, iLit, iPat = Gia_ManBuiltInSimPack( p, vPat );
    if ( iPat == -1 )
    {
        if ( fOverflow )
        {
            if ( (p->iPastPiMax & Period) == 0 )
                Gia_ManBuiltInSimResimulate( p );
            iPat = p->iPastPiMax;
            //if ( p->iPastPiMax == 64 * p->nSimWordsMax - 1 )
            //    printf( "Completed the cycle.\n" );
            p->iPastPiMax = p->iPastPiMax == 64 * p->nSimWordsMax - 1 ? 0 : p->iPastPiMax + 1;
        }
        else
        {
            if ( p->iPatsPi && (p->iPatsPi & Period) == 0 )
                Gia_ManBuiltInSimResimulate( p );
            if ( p->iPatsPi == 64 * p->nSimWords )
            {
                Vec_Wrd_t * vTemp = Vec_WrdAlloc( 2 * Vec_WrdSize(p->vSims) ); 
                word Data; int w, Count = 0, iObj = 0;
                Vec_WrdForEachEntry( p->vSims, Data, w )
                {
                    Vec_WrdPush( vTemp, Data );
                    if ( ++Count == p->nSimWords )
                    {
                        Gia_Obj_t * pObj = Gia_ManObj(p, iObj++);
                        if ( Gia_ObjIsCi(pObj) )
                            Vec_WrdPush( vTemp, Gia_ManRandomW(0) ); // Vec_WrdPush( vTemp, 0 );
                        else if ( Gia_ObjIsAnd(pObj) )
                            Vec_WrdPush( vTemp, pObj->fPhase ? ~(word)0 : 0 );
                        else
                            Vec_WrdPush( vTemp, 0 );
                        Count = 0;
                    }
                }
                assert( iObj == Gia_ManObjNum(p) );
                Vec_WrdFree( p->vSims );
                p->vSims = vTemp;

                vTemp = Vec_WrdAlloc( 2 * Vec_WrdSize(p->vSimsPi) ); 
                Count = 0;
                Vec_WrdForEachEntry( p->vSimsPi, Data, w )
                {
                    Vec_WrdPush( vTemp, Data );
                    if ( ++Count == p->nSimWords )
                    {
                        Vec_WrdPush( vTemp, 0 );
                        Count = 0;
                    }
                }
                Vec_WrdFree( p->vSimsPi );
                p->vSimsPi = vTemp;

                // update the word count
                p->nSimWords++;
                assert( Vec_WrdSize(p->vSims)   == p->nSimWords * Gia_ManObjNum(p) );
                assert( Vec_WrdSize(p->vSimsPi) == p->nSimWords * Gia_ManCiNum(p)  );
                //printf( "Resizing to %d words.\n", p->nSimWords );
            }
            iPat = p->iPatsPi++;
        }
    }
    assert( iPat >= 0 && iPat < p->iPatsPi );
    // add literals 
    if ( fOverflow )
    {
        int iVar;
        Vec_IntForEachEntry( &p->vSuppVars, iVar, k )
            if ( Abc_TtGetBit(Gia_ManBuiltInDataPi(p, iVar), iPat) )
                 Abc_TtXorBit(Gia_ManBuiltInDataPi(p, iVar), iPat);
        Vec_IntForEachEntry( vPat, iLit, k )
        {
            if ( Abc_TtGetBit(Gia_ManBuiltInData(p, 1+Abc_Lit2Var(iLit)), iPat) == Abc_LitIsCompl(iLit) )
            Abc_TtXorBit(Gia_ManBuiltInData(p, 1+Abc_Lit2Var(iLit)), iPat);
            Abc_TtXorBit(Gia_ManBuiltInDataPi(p, Abc_Lit2Var(iLit)), iPat);
        }
    }
    else
    {
        Vec_IntForEachEntry( vPat, iLit, k )
        {
            if ( Abc_TtGetBit(Gia_ManBuiltInDataPi(p, Abc_Lit2Var(iLit)), iPat) )
                assert( Abc_TtGetBit(Gia_ManBuiltInData(p, 1+Abc_Lit2Var(iLit)), iPat) != Abc_LitIsCompl(iLit) );
            else 
            {
                if ( Abc_TtGetBit(Gia_ManBuiltInData(p, 1+Abc_Lit2Var(iLit)), iPat) == Abc_LitIsCompl(iLit) )
                Abc_TtXorBit(Gia_ManBuiltInData(p, 1+Abc_Lit2Var(iLit)), iPat);
                Abc_TtXorBit(Gia_ManBuiltInDataPi(p, Abc_Lit2Var(iLit)), iPat);
            }
        }
    }
    return 1;
}

/**Function*************************************************************

  Synopsis    [Finds a satisfying assignment.]

  Description [Returns 1 if a sat assignment is found; 0 otherwise.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ManObjCheckSat_rec( Gia_Man_t * p, int iLit, Vec_Int_t * vObjs )
{
    int iObj = Abc_Lit2Var(iLit);
    Gia_Obj_t * pObj = Gia_ManObj( p, iObj );
    if ( pObj->fMark0 )
        return pObj->fMark1 == (unsigned)Abc_LitIsCompl(iLit);
    pObj->fMark0 = 1;
    pObj->fMark1 = Abc_LitIsCompl(iLit);
    Vec_IntPush( vObjs, iObj );
    if ( Gia_ObjIsAnd(pObj) )
    {
        if ( pObj->fMark1 == 0 ) // node value is 1
        {
            if ( !Gia_ManObjCheckSat_rec( p, Gia_ObjFaninLit0(pObj, iObj), vObjs ) )   return 0;
            if ( !Gia_ManObjCheckSat_rec( p, Gia_ObjFaninLit1(pObj, iObj), vObjs ) )   return 0;
        }
        else
        {
            if ( !Gia_ManObjCheckSat_rec( p, Abc_LitNot(Gia_ObjFaninLit0(pObj, iObj)), vObjs ) )   return 0;
        }
    }
    return 1;
}
int Gia_ManObjCheckOverlap1( Gia_Man_t * p, int iLit0, int iLit1, Vec_Int_t * vObjs )
{
    Gia_Obj_t * pObj;
    int i, Res0, Res1 = 0;
//    Gia_ManForEachObj( p, pObj, i )
//        assert( pObj->fMark0 == 0 && pObj->fMark1 == 0 );
    Vec_IntClear( vObjs );
    Res0 = Gia_ManObjCheckSat_rec( p, iLit0, vObjs );
    if ( Res0 )
        Res1 = Gia_ManObjCheckSat_rec( p, iLit1, vObjs );
    Gia_ManForEachObjVec( vObjs, p, pObj, i )
        pObj->fMark0 = pObj->fMark1 = 0;
    return Res0 && Res1;
}
int Gia_ManObjCheckOverlap( Gia_Man_t * p, int iLit0, int iLit1, Vec_Int_t * vObjs )
{
    if ( Gia_ManObjCheckOverlap1( p, iLit0, iLit1, vObjs ) )
        return 1;
    return Gia_ManObjCheckOverlap1( p, iLit1, iLit0, vObjs );
}





/**Function*************************************************************

  Synopsis    [Bit-parallel simulation during AIG construction.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManIncrSimUpdate( Gia_Man_t * p )
{
    int i, k;
    // extend timestamp info
    assert( Vec_IntSize(p->vTimeStamps) <= Gia_ManObjNum(p) );
    Vec_IntFillExtra( p->vTimeStamps, Gia_ManObjNum(p), 0 );
    // extend simulation info
    assert( Vec_WrdSize(p->vSims) <= Gia_ManObjNum(p) * p->nSimWords );
    Vec_WrdFillExtra( p->vSims,  Gia_ManObjNum(p) * p->nSimWords,  0 );
    // extend PI info
    assert( p->iNextPi <= Gia_ManCiNum(p) );
    for ( i = p->iNextPi; i < Gia_ManCiNum(p); i++ )
    {
        word * pSims = Gia_ManBuiltInData( p, Gia_ManCiIdToId(p, i) );
        for ( k = 0; k < p->nSimWords; k++ )
            pSims[k] = Gia_ManRandomW(0);
    }
    p->iNextPi = Gia_ManCiNum(p);
}
void Gia_ManIncrSimStart( Gia_Man_t * p, int nWords, int nObjs )
{
    assert( !p->fIncrSim );
    p->fIncrSim  = 1;
    p->iPatsPi   = 0;
    p->nSimWords = nWords;
    // init time stamps
    p->iTimeStamp  = 1;
    p->vTimeStamps = Vec_IntAlloc( p->nSimWords );
    // init object sim info
    p->iNextPi = 0;       
    p->vSims   = Vec_WrdAlloc( p->nSimWords * nObjs );
    Gia_ManRandomW( 1 );
}
void Gia_ManIncrSimStop( Gia_Man_t * p )
{
    assert( p->fIncrSim );
    p->fIncrSim   = 0;
    p->iPatsPi    = 0;
    p->nSimWords  = 0;
    p->iTimeStamp = 1;
    Vec_IntFreeP( &p->vTimeStamps );
    Vec_WrdFreeP( &p->vSims );
}
void Gia_ManIncrSimSet( Gia_Man_t * p, Vec_Int_t * vObjLits )
{
    int i, iLit; 
    assert( Vec_IntSize(vObjLits) > 0 );
    p->iTimeStamp++;
    Vec_IntForEachEntry( vObjLits, iLit, i )
    {
        word * pSims = Gia_ManBuiltInData( p, Abc_Lit2Var(iLit) );
        if ( Gia_ObjIsAnd(Gia_ManObj(p, Abc_Lit2Var(iLit))) )
            continue;
        //assert( Vec_IntEntry(p->vTimeStamps, Abc_Lit2Var(iLit)) == p->iTimeStamp-1 );
        Vec_IntWriteEntry(p->vTimeStamps, Abc_Lit2Var(iLit), p->iTimeStamp);
        if ( Abc_TtGetBit(pSims, p->iPatsPi) == Abc_LitIsCompl(iLit) )
             Abc_TtXorBit(pSims, p->iPatsPi);
    }
    p->iPatsPi = (p->iPatsPi == p->nSimWords * 64 - 1) ? 0 : p->iPatsPi + 1;
}
void Gia_ManIncrSimCone_rec( Gia_Man_t * p, int iObj )
{
    Gia_Obj_t * pObj = Gia_ManObj( p, iObj );
    if ( Gia_ObjIsCi(pObj) )
        return;
    if ( Vec_IntEntry(p->vTimeStamps, iObj) == p->iTimeStamp )
        return;
    assert( Vec_IntEntry(p->vTimeStamps, iObj) < p->iTimeStamp );
    Vec_IntWriteEntry( p->vTimeStamps, iObj, p->iTimeStamp );
    assert( Gia_ObjIsAnd(pObj) );
    Gia_ManIncrSimCone_rec( p, Gia_ObjFaninId0(pObj, iObj) );
    Gia_ManIncrSimCone_rec( p, Gia_ObjFaninId1(pObj, iObj) );
    Gia_ManBuiltInSimPerformInt( p, iObj );
}
int Gia_ManIncrSimCheckOver( Gia_Man_t * p, int iLit0, int iLit1 )
{
    assert( iLit0 > 1 && iLit1 > 1 );
    Gia_ManIncrSimUpdate( p );
    Gia_ManIncrSimCone_rec( p, Abc_Lit2Var(iLit0) );
    Gia_ManIncrSimCone_rec( p, Abc_Lit2Var(iLit1) );
//    return 0; // disable
    return Gia_ManBuiltInSimCheckOver( p, iLit0, iLit1 );
}
int Gia_ManIncrSimCheckEqual( Gia_Man_t * p, int iLit0, int iLit1 )
{
    assert( iLit0 > 1 && iLit1 > 1 );
    Gia_ManIncrSimUpdate( p );
    Gia_ManIncrSimCone_rec( p, Abc_Lit2Var(iLit0) );
    Gia_ManIncrSimCone_rec( p, Abc_Lit2Var(iLit1) );
//    return 1; // disable
    return Gia_ManBuiltInSimCheckEqual( p, iLit0, iLit1 );
}





/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Wrd_t * Gia_ManSimPatGenRandom( int nWords )
{
    Vec_Wrd_t * vSims = Vec_WrdAlloc( nWords ); int i;
    for ( i = 0; i < nWords; i++ )
        Vec_WrdPush( vSims, Gia_ManRandomW(0) );
    return vSims;
}
void Gia_ManSimPatAssignInputs( Gia_Man_t * p, int nWords, Vec_Wrd_t * vSims, Vec_Wrd_t * vSimsIn )
{
    int i, Id;
    assert( Vec_WrdSize(vSims)   == nWords * Gia_ManObjNum(p) );
    assert( Vec_WrdSize(vSimsIn) == nWords * Gia_ManCiNum(p) );
    Gia_ManForEachCiId( p, Id, i )
        memcpy( Vec_WrdEntryP(vSims, Id*nWords), Vec_WrdEntryP(vSimsIn, i*nWords), sizeof(word)*nWords );
}
static inline void Gia_ManSimPatSimAnd( Gia_Man_t * p, int i, Gia_Obj_t * pObj, int nWords, Vec_Wrd_t * vSims )
{
    word pComps[2] = { 0, ~(word)0 };
    word Diff0 = pComps[Gia_ObjFaninC0(pObj)];
    word Diff1 = pComps[Gia_ObjFaninC1(pObj)];
    word * pSims  = Vec_WrdArray(vSims);
    word * pSims0 = pSims + nWords*Gia_ObjFaninId0(pObj, i);
    word * pSims1 = pSims + nWords*Gia_ObjFaninId1(pObj, i);
    word * pSims2 = pSims + nWords*i; int w;
    for ( w = 0; w < nWords; w++ )
        pSims2[w] = (pSims0[w] ^ Diff0) & (pSims1[w] ^ Diff1);
}
static inline void Gia_ManSimPatSimPo( Gia_Man_t * p, int i, Gia_Obj_t * pObj, int nWords, Vec_Wrd_t * vSims )
{
    word pComps[2] = { 0, ~(word)0 };
    word Diff0     = pComps[Gia_ObjFaninC0(pObj)];
    word * pSims   = Vec_WrdArray(vSims);
    word * pSims0  = pSims + nWords*Gia_ObjFaninId0(pObj, i);
    word * pSims2  = pSims + nWords*i; int w;
    for ( w = 0; w < nWords; w++ )
        pSims2[w] = (pSims0[w] ^ Diff0);
}
Vec_Wrd_t * Gia_ManSimPatSim( Gia_Man_t * pGia )
{
    Gia_Obj_t * pObj;
    int i, nWords = Vec_WrdSize(pGia->vSimsPi) / Gia_ManCiNum(pGia);
    Vec_Wrd_t * vSims = Vec_WrdStart( Gia_ManObjNum(pGia) * nWords );
    assert( Vec_WrdSize(pGia->vSimsPi) % Gia_ManCiNum(pGia) == 0 );
    Gia_ManSimPatAssignInputs( pGia, nWords, vSims, pGia->vSimsPi );
    Gia_ManForEachAnd( pGia, pObj, i ) 
        Gia_ManSimPatSimAnd( pGia, i, pObj, nWords, vSims );
    Gia_ManForEachCo( pGia, pObj, i )
        Gia_ManSimPatSimPo( pGia, Gia_ObjId(pGia, pObj), pObj, nWords, vSims );
    return vSims;
}


/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/\
Vec_Wrd_t * Gia_ManSimCombine( int nInputs, Vec_Wrd_t * vBase, Vec_Wrd_t * vAddOn, int nWordsUse )
{
    int nWordsBase  = Vec_WrdSize(vBase)  / nInputs;
    int nWordsAddOn = Vec_WrdSize(vAddOn) / nInputs; int i, w;
    Vec_Wrd_t * vSimsIn = Vec_WrdAlloc( nInputs * (nWordsBase + nWordsUse) );
    assert( Vec_WrdSize(vBase)  % nInputs == 0 );
    assert( Vec_WrdSize(vAddOn) % nInputs == 0 );
    assert( nWordsUse <= nWordsAddOn );
    for ( i = 0; i < nInputs; i++ )
    {
        word * pSimsB = Vec_WrdEntryP( vBase,  i * nWordsBase );
        word * pSimsA = Vec_WrdEntryP( vAddOn, i * nWordsAddOn );
        for ( w = 0; w < nWordsBase; w++ )
            Vec_WrdPush( vSimsIn, pSimsB[w] );
        for ( w = 0; w < nWordsUse; w++ )
            Vec_WrdPush( vSimsIn, pSimsA[w] );
    }
    assert( Vec_WrdSize(vSimsIn) == Vec_WrdCap(vSimsIn) );
    return vSimsIn;
}
int Gia_ManSimBitPackOne( int nWords, Vec_Wrd_t * vSimsIn, Vec_Wrd_t * vSimsCare, int iPat, int * pLits, int nLits )
{
    word * pSimsI, * pSimsC; int i, k;
    for ( i = 0; i < iPat; i++ )
    {
        for ( k = 0; k < nLits; k++ )
        {
            int iVar = Abc_Lit2Var( pLits[k] );
            pSimsI = Vec_WrdEntryP( vSimsIn,   nWords * iVar );
            pSimsC = Vec_WrdEntryP( vSimsCare, nWords * iVar );
            if ( Abc_TtGetBit(pSimsC, i) && (Abc_TtGetBit(pSimsI, i) == Abc_LitIsCompl(pLits[k])) )
                break;
        }
        if ( k == nLits )
            break;
    }
    for ( k = 0; k < nLits; k++ )
    {
        int iVar = Abc_Lit2Var( pLits[k] );
        pSimsI = Vec_WrdEntryP( vSimsIn,   nWords * iVar );
        pSimsC = Vec_WrdEntryP( vSimsCare, nWords * iVar );
        if ( !Abc_TtGetBit(pSimsC, i) && Abc_TtGetBit(pSimsI, i) == Abc_LitIsCompl(pLits[k]) )
            Abc_TtXorBit( pSimsI, i );
        Abc_TtSetBit( pSimsC, i );
        assert( Abc_TtGetBit(pSimsC, i) && (Abc_TtGetBit(pSimsI, i) != Abc_LitIsCompl(pLits[k])) );
    }
    return (int)(i == iPat);
}
Vec_Wrd_t * Gia_ManSimBitPacking( Gia_Man_t * p, Vec_Int_t * vCexStore, int nCexes )
{
    int c, iCur = 0, iPat = 0;
    int nWordsMax = Abc_Bit6WordNum( nCexes ); 
    Vec_Wrd_t * vSimsIn   = Gia_ManSimPatGenRandom( Gia_ManCiNum(p) * nWordsMax );
    Vec_Wrd_t * vSimsCare = Vec_WrdStart( Gia_ManCiNum(p) * nWordsMax );
    Vec_Wrd_t * vSimsRes  = NULL;
    for ( c = 0; c < nCexes; c++ )
    {
        int Out  = Vec_IntEntry( vCexStore, iCur++ );
        int Size = Vec_IntEntry( vCexStore, iCur++ );
        iPat += Gia_ManSimBitPackOne( nWordsMax, vSimsIn, vSimsCare, iPat, Vec_IntEntryP(vCexStore, iCur), Size );
        iCur += Size;
        assert( iPat <= nCexes );
        Out = 0;
    }
    printf( "Compressed %d CEXes into %d test patterns.\n", nCexes, iPat );
    assert( iCur == Vec_IntSize(vCexStore) );
    vSimsRes = Gia_ManSimCombine( Gia_ManCiNum(p), p->vSimsPi, vSimsIn, Abc_Bit6WordNum(iPat+1) );
    printf( "Combined %d words of the original info with %d words of additional info.\n", 
        Vec_WrdSize(p->vSimsPi) / Gia_ManCiNum(p), Abc_Bit6WordNum(iPat+1) );
    Vec_WrdFree( vSimsIn );
    Vec_WrdFree( vSimsCare );
    return vSimsRes;
}


/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ManSimPatHashPatterns( Gia_Man_t * p, int nWords, Vec_Wrd_t * vSims, int * pnC0, int * pnC1 )
{
    Gia_Obj_t * pObj; 
    int i, nUnique;
    Vec_Mem_t * vStore;
    vStore = Vec_MemAlloc( nWords, 12 ); // 2^12 N-word entries per page
    Vec_MemHashAlloc( vStore, 1 << 12 );
    Gia_ManForEachCand( p, pObj, i )
    {
        word * pSim = Vec_WrdEntryP(vSims, i*nWords);
        if ( pnC0 && Abc_TtIsConst0(pSim, nWords) )
            (*pnC0)++;
        if ( pnC1 && Abc_TtIsConst1(pSim, nWords) )
            (*pnC1)++;
        Vec_MemHashInsert( vStore, pSim );
    }
    nUnique = Vec_MemEntryNum( vStore );
    Vec_MemHashFree( vStore );
    Vec_MemFree( vStore );
    return nUnique;
}
Gia_Man_t * Gia_ManSimPatGenMiter( Gia_Man_t * p, Vec_Wrd_t * vSims )
{
    Gia_Man_t * pNew;
    Gia_Obj_t * pObj;
    int i, nWords = Vec_WrdSize(vSims) / Gia_ManObjNum(p);
    pNew = Gia_ManStart( Gia_ManObjNum(p) + Gia_ManCoNum(p) );
    Gia_ManHashStart( pNew );
    Gia_ManConst0(p)->Value = 0;
    Gia_ManForEachCi( p, pObj, i )
        pObj->Value = Gia_ManAppendCi( pNew );
    Gia_ManForEachAnd( p, pObj, i )
        pObj->Value = Gia_ManHashAnd( pNew, Gia_ObjFanin0Copy(pObj), Gia_ObjFanin1Copy(pObj) );
    Gia_ManForEachAnd( p, pObj, i )
    {
        word * pSim = Vec_WrdEntryP(vSims, i*nWords);
        if ( Abc_TtIsConst0(pSim, nWords) )
            Gia_ManAppendCo( pNew, Abc_LitNotCond(pObj->Value, 0) );
        if ( Abc_TtIsConst1(pSim, nWords) )
            Gia_ManAppendCo( pNew, Abc_LitNotCond(pObj->Value, 1) );
    }
    Gia_ManHashStop( pNew );
    return pNew;
}


/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSimPatWriteOne( FILE * pFile, word * pSim, int nWords )
{
    int k, Digit, nDigits = nWords*16;
    for ( k = 0; k < nDigits; k++ )
    {
        Digit = (int)((pSim[k/16] >> ((k%16) * 4)) & 15);
        if ( Digit < 10 )
            fprintf( pFile, "%d", Digit );
        else
            fprintf( pFile, "%c", 'A' + Digit-10 );
    }
    fprintf( pFile, "\n" );
}
void Gia_ManSimPatWrite( char * pFileName, Vec_Wrd_t * vSimsIn, int nWords )
{
    int i, nNodes = Vec_WrdSize(vSimsIn) / nWords;
    FILE * pFile = fopen( pFileName, "wb" );
    if ( pFile == NULL )
    {
        printf( "Cannot open file \"%s\" for writing.\n", pFileName );
        return;
    }
    assert( Vec_WrdSize(vSimsIn) % nWords == 0 );
    for ( i = 0; i < nNodes; i++ )
        Gia_ManSimPatWriteOne( pFile, Vec_WrdEntryP(vSimsIn, i*nWords), nWords );
    fclose( pFile );
    printf( "Written %d words of simulation data.\n", nWords );
}
int Gia_ManSimPatReadOne( char c )
{
    int Digit = 0;
    if ( c >= '0' && c <= '9' )
        Digit = c - '0';
    else if ( c >= 'A' && c <= 'F' )
        Digit = c - 'A' + 10;
    else if ( c >= 'a' && c <= 'f' )
        Digit = c - 'a' + 10;
    else assert( 0 );
    assert( Digit >= 0 && Digit < 16 );
    return Digit;
}
Vec_Wrd_t * Gia_ManSimPatRead( char * pFileName )
{
    Vec_Wrd_t * vSimsIn = NULL; 
    int c, nWords = -1, nChars = 0; word Num = 0;
    FILE * pFile = fopen( pFileName, "rb" );
    if ( pFile == NULL )
    {
        printf( "Cannot open file \"%s\" for reading.\n", pFileName );
        return NULL;
    }
    vSimsIn = Vec_WrdAlloc( 1000 );
    while ( (c = fgetc(pFile)) != EOF )
    {
        if ( c == '\n' && nWords == -1 )
            nWords = Vec_WrdSize(vSimsIn);
        if ( c == '\n' || c == '\r' || c == '\t' || c == ' ' )
            continue;
        Num |= (word)Gia_ManSimPatReadOne((char)c) << (nChars * 4);
        if ( ++nChars < 16 )
            continue;
        Vec_WrdPush( vSimsIn, Num );
        nChars = 0; 
        Num = 0;
    }
    assert( Vec_WrdSize(vSimsIn) % nWords == 0 );
    fclose( pFile );
    printf( "Read %d words of simulation data.\n", nWords );
    return vSimsIn;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ManSimProfile( Gia_Man_t * pGia )
{
    Vec_Wrd_t * vSims = Gia_ManSimPatSim( pGia );
    int nWords = Vec_WrdSize(vSims) / Gia_ManObjNum(pGia);
    int nC0s = 0, nC1s = 0, nUnique = Gia_ManSimPatHashPatterns( pGia, nWords, vSims, &nC0s, &nC1s );
    printf( "Simulating %d words leads to %d unique objects (%.2f %% out of %d), Const0 = %d. Const1 = %d.\n", 
        nWords, nUnique, 100.0*nUnique/Gia_ManCandNum(pGia), Gia_ManCandNum(pGia), nC0s, nC1s );
    Vec_WrdFree( vSims );
}
void Gia_ManSimPat( Gia_Man_t * p, int nWords0, int fVerbose )
{
    extern Vec_Int_t * Cbs2_ManSolveMiterNc( Gia_Man_t * pAig, int nConfs, Vec_Str_t ** pvStatus, int fVerbose );
    int i, Status, Counts[3] = {0};
    Gia_Man_t * pGia;
    Vec_Wrd_t * vSimsIn = NULL;
    Vec_Str_t * vStatus = NULL;
    Vec_Int_t * vCexStore = NULL;
    Vec_Wrd_t * vSims = Gia_ManSimPatSim( p );
    Gia_ManSimProfile( p );
    pGia  = Gia_ManSimPatGenMiter( p, vSims );
    vCexStore = Cbs2_ManSolveMiterNc( pGia, 1000, &vStatus, 0 );
    Gia_ManStop( pGia );
    Vec_StrForEachEntry( vStatus, Status, i )
    {
        assert( Status >= -1 && Status <= 1 );
        Counts[Status+1]++;
    }
    printf( "Total = %d : SAT = %d.  UNSAT = %d.  UNDEC = %d.\n", Counts[1]+Counts[2]+Counts[0], Counts[1], Counts[2], Counts[0] );
    if ( Counts[1] == 0 )
        printf( "There are no counter-examples.  No need for more simulation.\n" );
    else
    {
        vSimsIn = Gia_ManSimBitPacking( p, vCexStore, Counts[1] );
        Vec_WrdFreeP( &p->vSimsPi );
        p->vSimsPi = vSimsIn;
        Gia_ManSimProfile( p );
    }
    Vec_StrFree( vStatus );
    Vec_IntFree( vCexStore );
    Vec_WrdFree( vSims );
}





typedef struct Gia_SimRsbMan_t_ Gia_SimRsbMan_t;
struct Gia_SimRsbMan_t_
{
    Gia_Man_t *    pGia;
    Vec_Int_t *    vTfo;
    Vec_Int_t *    vCands;
    Vec_Int_t *    vFanins;
    Vec_Int_t *    vFanins2;
    Vec_Wrd_t *    vSimsObj;
    Vec_Wrd_t *    vSimsObj2;
    int            nWords;
    word *         pFunc[3];
};

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
Gia_SimRsbMan_t * Gia_SimRsbAlloc( Gia_Man_t * pGia )
{
    Gia_SimRsbMan_t * p = ABC_CALLOC( Gia_SimRsbMan_t, 1 );
    p->pGia      = pGia;
    p->nWords    = Vec_WrdSize(pGia->vSimsPi) / Gia_ManCiNum(pGia); assert( Vec_WrdSize(pGia->vSimsPi) % Gia_ManCiNum(pGia) == 0 );
    p->pFunc[0]  = ABC_CALLOC( word, p->nWords );
    p->pFunc[1]  = ABC_CALLOC( word, p->nWords );
    p->pFunc[2]  = ABC_CALLOC( word, p->nWords );
    p->vTfo      = Vec_IntAlloc( 1000 );
    p->vCands    = Vec_IntAlloc( 1000 );
    p->vFanins   = Vec_IntAlloc( 10 );
    p->vFanins2  = Vec_IntAlloc( 10 );
    p->vSimsObj  = Gia_ManSimPatSim( pGia );
    p->vSimsObj2 = Vec_WrdStart( Vec_WrdSize(p->vSimsObj) );
    assert( p->nWords == Vec_WrdSize(p->vSimsObj) / Gia_ManObjNum(pGia) );
    Gia_ManStaticFanoutStart( pGia );
    return p;
}
void Gia_SimRsbFree( Gia_SimRsbMan_t * p )
{
    Gia_ManStaticFanoutStop( p->pGia );
    Vec_IntFree( p->vTfo );
    Vec_IntFree( p->vCands );
    Vec_IntFree( p->vFanins );
    Vec_IntFree( p->vFanins2 );
    Vec_WrdFree( p->vSimsObj );
    Vec_WrdFree( p->vSimsObj2 );
    ABC_FREE( p->pFunc[0] );
    ABC_FREE( p->pFunc[1] );
    ABC_FREE( p->pFunc[2] );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_SimRsbTfo_rec( Gia_Man_t * p, int iObj, int iFanout, Vec_Int_t * vTfo )
{
    int i, iFan;
    if ( Gia_ObjIsTravIdCurrentId(p, iObj) )
        return;
    Gia_ObjSetTravIdCurrentId(p, iObj);
    Gia_ObjForEachFanoutStaticId( p, iObj, iFan, i )
        if ( iFanout == -1 || iFan == iFanout )
            Gia_SimRsbTfo_rec( p, iFan, -1, vTfo );
    Vec_IntPush( vTfo, iObj );
}
Vec_Int_t * Gia_SimRsbTfo( Gia_SimRsbMan_t * p, int iObj, int iFanout )
{
    assert( iObj > 0 );
    Vec_IntClear( p->vTfo );
    Gia_ManIncrementTravId( p->pGia );
    Gia_SimRsbTfo_rec( p->pGia, iObj, iFanout, p->vTfo );
    assert( Vec_IntEntryLast(p->vTfo) == iObj );
    Vec_IntPop( p->vTfo );
    Vec_IntReverseOrder( p->vTfo );
    Vec_IntSort( p->vTfo, 0 );
    return p->vTfo;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
word * Gia_SimRsbFunc( Gia_SimRsbMan_t * p, int iObj, Vec_Int_t * vFanins, int fOnSet )
{
    int nTruthWords = Abc_Truth6WordNum( Vec_IntSize(vFanins) );
    word * pTruth   = ABC_CALLOC( word, nTruthWords ); 
    word * pFunc    = Vec_WrdEntryP( p->vSimsObj, p->nWords*iObj );
    word * pFanins[16] = {NULL};  int s, b, iMint, i, iFanin;
    assert( Vec_IntSize(vFanins) <= 16 );
    Vec_IntForEachEntry( vFanins, iFanin, i )
        pFanins[i] = Vec_WrdEntryP( p->vSimsObj, p->nWords*iFanin );
    for ( s = 0; s < 64*p->nWords; s++ )
    {
        if ( !Abc_TtGetBit(p->pFunc[2], s) || Abc_TtGetBit(pFunc, s) != fOnSet )
            continue;
        iMint = 0;
        for ( b = 0; b < Vec_IntSize(vFanins); b++ )
            if ( Abc_TtGetBit(pFanins[b], s) )
                iMint |= 1 << b;
        Abc_TtSetBit( pTruth, iMint );
    }
    return pTruth;
}
int Gia_SimRsbResubVerify( Gia_SimRsbMan_t * p, int iObj, Vec_Int_t * vFanins )
{
    word * pTruth0 = Gia_SimRsbFunc( p, iObj, p->vFanins, 0 );
    word * pTruth1 = Gia_SimRsbFunc( p, iObj, p->vFanins, 1 );
    int Res = !Abc_TtIntersect( pTruth0, pTruth1, p->nWords, 0 );
    ABC_FREE( pTruth0 );
    ABC_FREE( pTruth1 );
    return Res;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
static inline void Gia_SimRsbSimAndCareSet( Gia_Man_t * p, int i, Gia_Obj_t * pObj, int nWords, Vec_Wrd_t * vSims, Vec_Wrd_t * vSims2 )
{
    word pComps[2] = { 0, ~(word)0 };
    word Diff0 = pComps[Gia_ObjFaninC0(pObj)];
    word Diff1 = pComps[Gia_ObjFaninC1(pObj)];
    Vec_Wrd_t * vSims0 = Gia_ObjIsTravIdCurrentId(p, Gia_ObjFaninId0(pObj, i)) ? vSims2 : vSims;
    Vec_Wrd_t * vSims1 = Gia_ObjIsTravIdCurrentId(p, Gia_ObjFaninId1(pObj, i)) ? vSims2 : vSims;
    word * pSims0 = Vec_WrdEntryP( vSims0, nWords*Gia_ObjFaninId0(pObj, i) );
    word * pSims1 = Vec_WrdEntryP( vSims1, nWords*Gia_ObjFaninId1(pObj, i) );
    word * pSims2 = Vec_WrdEntryP( vSims2, nWords*i ); int w;
    for ( w = 0; w < nWords; w++ )
        pSims2[w] = (pSims0[w] ^ Diff0) & (pSims1[w] ^ Diff1);
}
word * Gia_SimRsbCareSet( Gia_SimRsbMan_t * p, int iObj, Vec_Int_t * vTfo )
{
    word * pSims  = Vec_WrdEntryP( p->vSimsObj,  p->nWords*iObj );
    word * pSims2 = Vec_WrdEntryP( p->vSimsObj2, p->nWords*iObj );  int iNode, i;
    Abc_TtCopy( pSims2, pSims, p->nWords, 1 );
    Abc_TtClear( p->pFunc[2], p->nWords );
    Vec_IntForEachEntry( vTfo, iNode, i )
    {
        Gia_Obj_t * pNode = Gia_ManObj(p->pGia, iNode);
        if ( Gia_ObjIsAnd(pNode) )
            Gia_SimRsbSimAndCareSet( p->pGia, iNode, pNode, p->nWords, p->vSimsObj, p->vSimsObj2 );
        else if ( Gia_ObjIsCo(pNode) )
        {
            word * pSimsA = Vec_WrdEntryP( p->vSimsObj,  p->nWords*Gia_ObjFaninId0p(p->pGia, pNode) );
            word * pSimsB = Vec_WrdEntryP( p->vSimsObj2, p->nWords*Gia_ObjFaninId0p(p->pGia, pNode) );
            Abc_TtOrXor( p->pFunc[2], pSimsA, pSimsB, p->nWords );
        }
        else assert( 0 );
    }
    return p->pFunc[2];
}


/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
void Gia_ObjSimCollect( Gia_SimRsbMan_t * p )
{
    int i, k, iTemp, iFanout;
    Vec_IntClear( p->vFanins2 );
    assert( Vec_IntSize(p->vFanins) > 0 );
    Vec_IntForEachEntry( p->vFanins, iTemp, i )
    {
        Gia_Obj_t * pObj = Gia_ManObj( p->pGia, iTemp );
        if ( Gia_ObjIsAnd(pObj) && !Gia_ObjIsTravIdCurrentId( p->pGia, Gia_ObjFaninId0(pObj, iTemp) ) )
            Vec_IntPush( p->vFanins2, Gia_ObjFaninId0(pObj, iTemp) );
        if ( Gia_ObjIsAnd(pObj) && !Gia_ObjIsTravIdCurrentId( p->pGia, Gia_ObjFaninId1(pObj, iTemp) ) )
            Vec_IntPush( p->vFanins2, Gia_ObjFaninId1(pObj, iTemp) );
        Gia_ObjForEachFanoutStaticId( p->pGia, iTemp, iFanout, k )
            if ( Gia_ObjIsAnd(Gia_ManObj(p->pGia, iFanout)) && !Gia_ObjIsTravIdCurrentId( p->pGia, iFanout ) )
                Vec_IntPush( p->vFanins2, iFanout );
    }
}
Vec_Int_t * Gia_ObjSimCands( Gia_SimRsbMan_t * p, int iObj, int nCands )
{
    assert( iObj > 0 );
    assert( Gia_ObjIsAnd(Gia_ManObj(p->pGia, iObj)) );
    Vec_IntClear( p->vCands );
    Vec_IntFill( p->vFanins, 1, iObj );
    while ( Vec_IntSize(p->vFanins) > 0 && Vec_IntSize(p->vCands) < nCands )
    {
        int i, iTemp;
        Vec_IntForEachEntry( p->vFanins, iTemp, i )
            Gia_ObjSetTravIdCurrentId( p->pGia, iTemp );
        Gia_ObjSimCollect( p ); // p->vFanins -> p->vFanins2
        Vec_IntAppend( p->vCands, p->vFanins2 );
        ABC_SWAP( Vec_Int_t *, p->vFanins, p->vFanins2 );
    }
    assert( Vec_IntSize(p->vFanins) == 0 || Vec_IntSize(p->vCands) >= nCands );
    if ( Vec_IntSize(p->vCands) > nCands )
        Vec_IntShrink( p->vCands, nCands );
    return p->vCands;
}

/**Function*************************************************************

  Synopsis    []

  Description []

  SideEffects []

  SeeAlso     []

***********************************************************************/
int Gia_ObjSimRsb( Gia_SimRsbMan_t * p, int iObj, int nCands, int fVerbose, int * pnBufs, int * pnInvs )
{
    int i, iCand, RetValue = 0;
    Vec_Int_t * vTfo   = Gia_SimRsbTfo( p, iObj, -1 );
    word * pCareSet    = Gia_SimRsbCareSet( p, iObj, vTfo );
    word * pFunc       = Vec_WrdEntryP( p->vSimsObj, p->nWords*iObj );
    Vec_Int_t * vCands = Gia_ObjSimCands( p, iObj, nCands );
    Abc_TtAndSharp( p->pFunc[0], pCareSet, pFunc, p->nWords, 1 );
    Abc_TtAndSharp( p->pFunc[1], pCareSet, pFunc, p->nWords, 0 );

/*
printf( "Considering node %d with %d candidates:\n", iObj, Vec_IntSize(vCands) );
Vec_IntPrint( vTfo );
Vec_IntPrint( vCands );
Extra_PrintBinary( stdout, (unsigned *)pCareSet,    64 );  printf( "\n" );
Extra_PrintBinary( stdout, (unsigned *)pFunc,       64 );  printf( "\n" );
Extra_PrintBinary( stdout, (unsigned *)p->pFunc[0], 64 );  printf( "\n" );
Extra_PrintBinary( stdout, (unsigned *)p->pFunc[1], 64 );  printf( "\n" );
*/
    Vec_IntForEachEntry( vCands, iCand, i )
    {
        word * pDiv = Vec_WrdEntryP( p->vSimsObj, p->nWords*iCand );
        if ( !Abc_TtIntersect(pDiv, p->pFunc[0], p->nWords, 0) &&
             !Abc_TtIntersect(pDiv, p->pFunc[1], p->nWords, 1) )
            { (*pnBufs)++; if ( fVerbose ) printf( "Level %3d : %d = buf(%d)\n", Gia_ObjLevelId(p->pGia, iObj), iObj, iCand ); RetValue = 1; }
        if ( !Abc_TtIntersect(pDiv, p->pFunc[0], p->nWords, 1) &&
             !Abc_TtIntersect(pDiv, p->pFunc[1], p->nWords, 0) )
            { (*pnInvs)++; if ( fVerbose ) printf( "Level %3d : %d = inv(%d)\n", Gia_ObjLevelId(p->pGia, iObj), iObj, iCand ); RetValue = 1; }
    }
    return RetValue;
}

int Gia_ManSimRsb( Gia_Man_t * pGia, int nCands, int fVerbose )
{
    abctime clk = Abc_Clock();
    Gia_Obj_t * pObj; int iObj, nCount = 0, nBufs = 0, nInvs = 0;
    Gia_SimRsbMan_t * p = Gia_SimRsbAlloc( pGia );
    assert( pGia->vSimsPi != NULL );
    Gia_ManLevelNum( pGia );
    Gia_ManForEachAnd( pGia, pObj, iObj )
        //if ( iObj == 6 )
        nCount += Gia_ObjSimRsb( p, iObj, nCands, fVerbose, &nBufs, &nInvs );
    printf( "Can resubstitute %d nodes (%.2f %% out of %d) (Bufs = %d Invs = %d)  ", 
        nCount, 100.0*nCount/Gia_ManAndNum(pGia), Gia_ManAndNum(pGia), nBufs, nInvs );
    Abc_PrintTime( 1, "Time", Abc_Clock() - clk );
    Gia_SimRsbFree( p );
    return nCount;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END

