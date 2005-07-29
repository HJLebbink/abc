/**CFile****************************************************************

  FileName    [fpgaTruth.c]

  PackageName [MVSIS 1.3: Multi-valued logic synthesis system.]

  Synopsis    [Technology mapping for variable-size-LUT FPGAs.]

  Author      [MVSIS Group]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 2.0. Started - August 18, 2004.]

  Revision    [$Id: fpgaTruth.c,v 1.4 2005/01/23 06:59:42 alanmi Exp $]

***********************************************************************/

#include "fpgaInt.h"
#include "cudd.h"

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFITIONS                           ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Recursively derives the truth table for the cut.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
DdNode * Fpga_TruthsCutBdd_rec( DdManager * dd, Fpga_Cut_t * pCut, Fpga_NodeVec_t * vVisited )
{
    DdNode * bFunc, * bFunc0, * bFunc1;
    assert( !Fpga_IsComplement(pCut) );
    // if the cut is visited, return the result
    if ( pCut->uSign )
        return (DdNode *)pCut->uSign;
    // compute the functions of the children
    bFunc0 = Fpga_TruthsCutBdd_rec( dd, Fpga_CutRegular(pCut->pOne), vVisited );   Cudd_Ref( bFunc0 );
    bFunc0 = Cudd_NotCond( bFunc0, Fpga_CutIsComplement(pCut->pOne) );
    bFunc1 = Fpga_TruthsCutBdd_rec( dd, Fpga_CutRegular(pCut->pTwo), vVisited );   Cudd_Ref( bFunc1 );
    bFunc1 = Cudd_NotCond( bFunc1, Fpga_CutIsComplement(pCut->pTwo) );
    // get the function of the cut
    bFunc  = Cudd_bddAnd( dd, bFunc0, bFunc1 );   Cudd_Ref( bFunc );
    bFunc  = Cudd_NotCond( bFunc, pCut->Phase );
    Cudd_RecursiveDeref( dd, bFunc0 );
    Cudd_RecursiveDeref( dd, bFunc1 );
    assert( pCut->uSign == 0 );
    pCut->uSign = (unsigned)bFunc;
    // add this cut to the visited list
    Fpga_NodeVecPush( vVisited, (Fpga_Node_t *)pCut );
    return bFunc;
}

/**Function*************************************************************

  Synopsis    [Derives the truth table for one cut.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void * Fpga_TruthsCutBdd( void * dd, Fpga_Cut_t * pCut )
{
    Fpga_NodeVec_t * vVisited;
    DdNode * bFunc;
    int i;
    assert( pCut->nLeaves > 1 );
    // set the leaf variables
    for ( i = 0; i < pCut->nLeaves; i++ )
        pCut->ppLeaves[i]->pCuts->uSign = (unsigned)Cudd_bddIthVar( dd, i );
    // recursively compute the function
    vVisited = Fpga_NodeVecAlloc( 10 );
    bFunc = Fpga_TruthsCutBdd_rec( dd, pCut, vVisited );   Cudd_Ref( bFunc );
    // clean the intermediate BDDs
    for ( i = 0; i < pCut->nLeaves; i++ )
        pCut->ppLeaves[i]->pCuts->uSign = 0;
    for ( i = 0; i < vVisited->nSize; i++ )
    {
        pCut = (Fpga_Cut_t *)vVisited->pArray[i];
        Cudd_RecursiveDeref( dd, (DdNode*)pCut->uSign );
        pCut->uSign = 0;
    }
    Fpga_NodeVecFree( vVisited );
    Cudd_Deref( bFunc );
    return bFunc;
}


////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


