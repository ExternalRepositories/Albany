/********************************************************************\
*            Albany, Copyright (2010) Sandia Corporation             *
*                                                                    *
* Notice: This computer software was prepared by Sandia Corporation, *
* hereinafter the Contractor, under Contract DE-AC04-94AL85000 with  *
* the Department of Energy (DOE). All rights in the computer software*
* are reserved by DOE on behalf of the United States Government and  *
* the Contractor as provided in the Contract. You are authorized to  *
* use this computer software for Governmental purposes but it is not *
* to be released or distributed to the public. NEITHER THE GOVERNMENT*
* NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR      *
* ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This notice    *
* including this sentence must appear on any copies of this software.*
*    Questions to Andy Salinger, agsalin@sandia.gov                  *
\********************************************************************/


#ifndef PETRA_CONVERTERS_H
#define PETRA_CONVERTERS_H

#ifdef ALBANY_MPI
  #define Albany_MPI_Comm MPI_Comm
  #define Albany_MPI_COMM_WORLD MPI_COMM_WORLD
  #define Albany_MPI_COMM_NULL MPI_COMM_NULL
  #include "Epetra_MpiComm.h"
  #include "Teuchos_DefaultMpiComm.hpp"
#else
  #define Albany_MPI_Comm int
  #define Albany_MPI_COMM_WORLD 0  // This is compatible with Dakota
  #define Albany_MPI_COMM_NULL 99
  #include "Epetra_SerialComm.h"
  #include "Teuchos_DefaultSerialComm.hpp"
#endif
#include "Teuchos_RCP.hpp"
#include "Albany_DataTypes.hpp"
#include "Epetra_Vector.h" 
#include "Epetra_CrsGraph.h"
#include "Epetra_CrsMatrix.h"

namespace Petra {


//TpetraMap_To_EpetraMap: takes in Tpetra::Map object, converts it to its equivalent Epetra_Map object, 
//and returns an RCP pointer to this Epetra_Map
Teuchos::RCP<Epetra_Map> TpetraMap_To_EpetraMap(const Teuchos::RCP<const Tpetra_Map>& tpetraMap_, 
                                                      const Teuchos::RCP<const Epetra_Comm>& comm_);

//EpetraMap_To_TpetraMap: takes in Epetra_Map object, converts it to its equivalent Tpetra::Map object, 
//and returns an RCP pointer to this Tpetra::Map
Teuchos::RCP<const Tpetra_Map> EpetraMap_To_TpetraMap(const Teuchos::RCP<const Epetra_Map>& epetraMap_, 
                                                      Teuchos::RCP<const Tpetra::Comm<int> >& comm_, 
                                                      Teuchos::RCP<KokkosNode>& nodeT_);

//TpetraCrsGraph_To_TpetraCrsGraph: takes in Tpetra::CrsGraph object, converts it to its equivalent Epetra_CrsGraph object, 
//and returns an RCP pointer to this Epetra_CrsGraph
Teuchos::RCP<Epetra_CrsGraph> TpetraCrsGraph_To_EpetraCrsGraph(const Teuchos::RCP<const Tpetra_CrsGraph>& tpetraCrsGraph_, 
                                                                     const Teuchos::RCP<const Epetra_Comm>& comm_);


//TpetraCrsMatrix_To_CrsMatrix: copies Tpetra::CrsMatrix object into its analogous 
//Epetra_CrsMatrix object 
void TpetraCrsMatrix_To_EpetraCrsMatrix(Teuchos::RCP<Tpetra_CrsMatrix>& tpetraCrsMatrix_,
                                                  Epetra_CrsMatrix& epetraCrsMatrix_,
                                                  const Teuchos::RCP<const Epetra_Comm>& comm_);


//TpetraVector_To_EpetraVector: copies Tpetra::Vector object into its analogous 
//Epetra_Vector object 
void TpetraVector_To_EpetraVector(const Teuchos::RCP<const Tpetra_Vector>& tpetraVector_,
                                  Epetra_Vector& epetraVector_, const Teuchos::RCP<const Epetra_Comm>& comm_); 

//EpetraVector_To_TpetraVectorConst: copies const Epetra_Vector to const Tpetra_Vector
Teuchos::RCP<const Tpetra_Vector> EpetraVector_To_TpetraVectorConst(const Epetra_Vector& epetraVector_, 
                                                               Teuchos::RCP<const Teuchos::Comm<int> >& commT_,
                                                               Teuchos::RCP<KokkosNode>& nodeT_); 

//EpetraVector_To_TpetraVectorNonConst: copies non-const Epetra_Vector to non-const Tpetra_Vector
Teuchos::RCP<Tpetra_Vector> EpetraVector_To_TpetraVectorNonConst(const Epetra_Vector& epetraVector_, 
                                                               Teuchos::RCP<const Teuchos::Comm<int> >& commT_,
                                                               Teuchos::RCP<KokkosNode>& nodeT_);

//TpetraMultiVector_To_EpetraMultiVector: copies Tpetra::MultiVector object into its analogous 
//Epetra_MultiVector object 
void TpetraMultiVector_To_EpetraMultiVector(const Teuchos::RCP<const Tpetra_MultiVector>& tpetraMV_,
                                  Epetra_MultiVector& epetraMV_, const Teuchos::RCP<const Epetra_Comm>& comm_); 

//EpetraMultiVector_To_TpetraMultiVectorConst: copies Epetra_MultiVector to const Tpetra_MultiVector
Teuchos::RCP<Tpetra_MultiVector> EpetraMultiVector_To_TpetraMultiVector(const Epetra_MultiVector& epetraMV_, 
                                                               Teuchos::RCP<const Teuchos::Comm<int> >& commT_,
                                                               Teuchos::RCP<KokkosNode>& nodeT_);

//EpetraCrsMatrix_To_TpetraCrsMatrix: copies Epetra_CrsMatrix to its analogous Tpetra_CrsMatrix
Teuchos::RCP<Tpetra_CrsMatrix> EpetraCrsMatrix_To_TpetraCrsMatrix(Epetra_CrsMatrix& epetraMatrix_,
                                                               Teuchos::RCP<const Teuchos::Comm<int> >& commT_,
                                                               Teuchos::RCP<KokkosNode>& nodeT_);

}
#endif //PETRA_CONVERTERS
