#include "Albany_BasisOps.hpp"

#include "Albany_EpetraMVDenseMatrixView.hpp"

#include "Epetra_BlockMap.h"
#include "Epetra_LocalMap.h"

#include "Epetra_Operator.h"

#include "Epetra_SerialDenseMatrix.h"
#include "Epetra_SerialDenseSolver.h"

#include "Teuchos_Assert.hpp"

namespace Albany {

Epetra_LocalMap createComponentMap(const Epetra_MultiVector &projector)
{
  return Epetra_LocalMap(projector.NumVectors(), 0, projector.Comm());
}

void dualize(const Epetra_MultiVector &primal, Epetra_MultiVector &dual)
{
  // 1) A <- primal^T * dual
  const Epetra_LocalMap componentMap = createComponentMap(dual);
  Epetra_MultiVector product(componentMap, primal.NumVectors(), false);
  {
    const int ierr = reduce(dual, primal, product);
    TEUCHOS_ASSERT(ierr == 0);
  }

  // 2) A <- A^{-1}
  {
    Epetra_SerialDenseMatrix matrix = localDenseMatrixView(product);
    Epetra_SerialDenseSolver solver;
    {
      const int ierr = solver.SetMatrix(matrix);
      TEUCHOS_ASSERT(ierr == 0);
    }
    {
      const int ierr = solver.Invert();
      TEUCHOS_ASSERT(ierr == 0);
    }
  }

  // 3) dual <- dual * A
  const Epetra_MultiVector dual_copy(dual);
  dual.Multiply('N', 'N', 1.0, dual_copy, product, 0.0);
}

void dualize(const Epetra_MultiVector &primal, const Epetra_Operator &metric, Epetra_MultiVector &result)
{
  const int ierr = metric.Apply(primal, result);
  TEUCHOS_ASSERT(ierr == 0);
  dualize(primal, result);
}

} // end namespace Albany
