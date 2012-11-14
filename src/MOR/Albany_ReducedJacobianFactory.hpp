//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#ifndef ALBANY_REDUCEDJACOBIANFACTORY_HPP
#define ALBANY_REDUCEDJACOBIANFACTORY_HPP

#include "Epetra_CrsGraph.h"

class Epetra_Operator;
class Epetra_CrsMatrix;
class Epetra_MultiVector;

#include "Teuchos_RCP.hpp"

namespace Albany {

class ReducedJacobianFactory {
public:
  explicit ReducedJacobianFactory(const Teuchos::RCP<const Epetra_MultiVector> &rightProjector);

  Teuchos::RCP<const Epetra_MultiVector> rightProjector() const { return rightProjector_; }
  Teuchos::RCP<const Epetra_MultiVector> premultipliedRightProjector() const { return premultipliedRightProjector_; }

  void fullJacobianIs(const Epetra_Operator &op);

  Teuchos::RCP<Epetra_CrsMatrix> reducedMatrixNew() const;
  const Epetra_CrsMatrix &reducedMatrix(const Epetra_MultiVector &leftProjector, Epetra_CrsMatrix &result) const;

private:
  Teuchos::RCP<const Epetra_MultiVector> rightProjector_;
  Teuchos::RCP<Epetra_MultiVector> premultipliedRightProjector_;

  Epetra_CrsGraph reducedGraph_;

  bool isMasterProcess() const;
};

} // namespace Albany

#endif /* ALBANY_REDUCEDJACOBIANFACTORY_HPP */
