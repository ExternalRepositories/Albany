//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Schwarz_CoupledJacobian.hpp"
#include "Teuchos_ParameterListExceptions.hpp"
#include "Teuchos_TestForException.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Albany_Utils.hpp"
#include "Thyra_DefaultBlockedLinearOp.hpp"

//#include "Tpetra_LocalMap.h"

#define WRITE_TO_MATRIX_MARKET

static int c3 = 0; 
static int c4 = 0; 

using Thyra::PhysicallyBlockedLinearOpBase;


LCM::Schwarz_CoupledJacobian::Schwarz_CoupledJacobian(const Teuchos::RCP<const Teuchos_Comm>& commT)
{
  std::cout << __PRETTY_FUNCTION__ << "\n"; 
  commT_ = commT;
  blockedOp_ = Thyra::defaultBlockedLinearOp<ST>();
}

LCM::Schwarz_CoupledJacobian::~Schwarz_CoupledJacobian()
{
}

//getThyraCoupledJacobian method is similar analogous to getThyraMatrix in panzer 
//(Panzer_BlockedTpetraLinearObjFactory_impl.hpp).
Teuchos::RCP<Thyra::LinearOpBase<ST> > 
LCM::Schwarz_CoupledJacobian::
getThyraCoupledJacobian(Teuchos::Array<Teuchos::RCP<Tpetra_CrsMatrix> >jacs, 
                        Teuchos::Array<Teuchos::RCP<LCM::Schwarz_BoundaryJacobian> > jacs_boundary) const 
{
  std::cout << __PRETTY_FUNCTION__ << "\n"; 
  std::size_t blockDim = jacs.size(); 

#ifdef WRITE_TO_MATRIX_MARKET
  char name[100];  //create string for file name
  sprintf(name, "Jac0_%i.mm", c3);
//write individual model jacobians to matrix market for debug
  Tpetra_MatrixMarket_Writer::writeSparseFile(name, jacs[0]);
  c3++; 
  if (blockDim > 1) 
    Tpetra_MatrixMarket_Writer::writeSparseFile("Jac1_%i.mm", jacs[1]);
#endif
   // get the block dimension
   // this operator will be square
   blockedOp_->beginBlockFill(blockDim,blockDim);

   // loop over each block
   for(std::size_t i=0;i<blockDim;i++) {
     for(std::size_t j=0;j<blockDim;j++) {
        // build (i,j) block matrix and add it to blocked operator
        if (i == j) { //diagonal blocks
          Teuchos::RCP<Thyra::LinearOpBase<ST> > block = Thyra::createLinearOp<ST,LO,GO,KokkosNode>(jacs[i]);
          blockedOp_->setNonconstBlock(i,j,block);
        }
        //FIXME: add off-diagonal blocks
     }
   }

   // all done
   blockedOp_->endBlockFill();

   return blockedOp_;
}


