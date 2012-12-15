//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#ifndef ALBANY_FULLSTATERECONSTRUCTOR_HPP
#define ALBANY_FULLSTATERECONSTRUCTOR_HPP

#include "NOX_Epetra_Observer.H"

#include "Epetra_Vector.h"

#include "Teuchos_RCP.hpp"

namespace Albany {

class ReducedSpace;

class FullStateReconstructor : public NOX::Epetra::Observer {
public:
  FullStateReconstructor(
      const Teuchos::RCP<const ReducedSpace> &reducedSpace,
      const Teuchos::RCP<NOX::Epetra::Observer> &decoratedObserver);

  //! Calls underlying observer then evaluates projection error
  virtual void observeSolution(const Epetra_Vector& solution);

  //! Calls underlying observer then evaluates projection error
  virtual void observeSolution(const Epetra_Vector& solution, double time_or_param_val);

private:
  Teuchos::RCP<const ReducedSpace> reducedSpace_;
  Teuchos::RCP<NOX::Epetra::Observer> decoratedObserver_;

  Epetra_Vector lastFullSolution_;
  void computeLastFullSolution(const Epetra_Vector& reducedSolution);

  // Disallow copy & assignment
  FullStateReconstructor(const FullStateReconstructor &);
  FullStateReconstructor operator=(const FullStateReconstructor &);
};

} // end namespace Albany

#endif /* ALBANY_FULLSTATERECONSTRUCTOR_HPP */
