//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#ifndef SCHWARZ_STATELESSOBSERVERIMPL_HPP
#define SCHWARZ_STATELESSOBSERVERIMPL_HPP

#include "Albany_Application.hpp"
#include "Albany_DataTypes.hpp"

#include "Teuchos_RCP.hpp"
#include "Teuchos_Ptr.hpp"

#include "Teuchos_Time.hpp"

namespace LCM {

/*! \brief Implementation to observe the solution without updating state
 *         information.
 *
 * When LOCA completes solve(), a number of things happen, some with side
 * effects: eigendata are computed and saved, response functions are evaluated,
 * and printSolution is called. Previously, ObserverImpl called
 * updateStates. This meant that it could only be called after the RFs were
 * evaluated. Moreover, if eigedata were saved, the state field manager would be
 * evaluated with the eigenvectors as solutions, which gives meaningless results
 * for the _new states, and then updateStates would copy that meaningless data
 * from _new to _old.
 *
 * This class is at least part of a solution to this problem. Eigendata are now
 * saved using this stateless observer impl, so updateStates is not called. The
 * order in LOCA at present follows:
 *     solve;
 *     optionally compute and save (Epetra only) eigendata: no side effect
 *       except writing files;
 *     postProcessContinuationStep: eval RF;
 *     printSolution: eval sfm, updateStates, write to exo file.
 * Problems remain in how LOCA::AdaptiveStepper and Albany interact, but I think
 * LOCA::Stepper and Albany may be entirely OK now in terms of sequencing and
 * updating state.
 *
 * It probably would have been a better design to make a StatefulObserver
 * subclassed from an (assumed, as NOX/LOCA do) stateless one. However, that
 * would change the name of a class already in wide use, which I don't want to
 * do. Instead, NOXStatelessObserver will start with just one user (Epetra
 * eigendata saver), and NOXObserver will continue to behave as it always has.
 */
class StatelessObserverImpl {
public:
  explicit StatelessObserverImpl(Teuchos::ArrayRCP<Teuchos::RCP<Albany::Application> > &apps);

  RealType getTimeParamValueOrDefault(RealType defaultValue) const;

  virtual void observeSolutionT(
    double stamp, Teuchos::Array<Teuchos::RCP<const Tpetra_Vector > > nonOverlappedSolutionsT,
    Teuchos::Array<Teuchos::RCP<const Tpetra_Vector> > nonOverlappedSolutionDotsT);

protected:
  Teuchos::RCP<Teuchos::Time> sol_out_time_;
  Teuchos::ArrayRCP<Teuchos::RCP<Albany::Application> > apps_;
  int n_models_; 

private:
  StatelessObserverImpl(const StatelessObserverImpl&);
  StatelessObserverImpl& operator=(const StatelessObserverImpl&);
};

} // namespace LCM

#endif // SCHWARZ_STATELESSOBSERVERIMPL_HPP
