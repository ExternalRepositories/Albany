//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#include "Albany_ObserverImpl.hpp"

#include "Albany_AbstractDiscretization.hpp"
#if defined(ALBANY_EPETRA)
# include "AAdapt_AdaptiveSolutionManager.hpp"
#endif

#include "Teuchos_TimeMonitor.hpp"
#include "Teuchos_Ptr.hpp"
#include "Petra_Converters.hpp"

#ifdef ALBANY_PERIDIGM
#if defined(ALBANY_EPETRA)
#include "PeridigmManager.hpp"
#endif
#endif

#include <string>

namespace Albany {

ObserverImpl::
ObserverImpl (const Teuchos::RCP<Application> &app)
  : StatelessObserverImpl(app)
{}

#if defined(ALBANY_EPETRA)
void ObserverImpl::observeSolution (
  double stamp, const Epetra_Vector& nonOverlappedSolution,
  const Teuchos::Ptr<const Epetra_Vector>& nonOverlappedSolutionDot)
{
  // If solution == "Steady" or "Continuation", we need to update the solution
  // from the initial guess prior to writing it out, or we will not get the
  // proper state of things like "Stress" in the Exodus file.
  {
    // Evaluate state field manager
    if(nonOverlappedSolutionDot != Teuchos::null)
      app_->evaluateStateFieldManager(stamp, nonOverlappedSolutionDot.get(),
                                      NULL, nonOverlappedSolution);
    else
      app_->evaluateStateFieldManager(stamp, NULL, NULL, nonOverlappedSolution);

    // Renames the New state as the Old state in preparation for the next step
    app_->getStateMgr().updateStates();

#ifdef ALBANY_PERIDIGM
#if defined(ALBANY_EPETRA)
    LCM::PeridigmManager& peridigmManager = LCM::PeridigmManager::self();
    double obcFunctional = peridigmManager.obcEvaluateFunctional();
    peridigmManager.writePeridigmSubModel(stamp);
    peridigmManager.updateState();

    int myPID = nonOverlappedSolution.Map().Comm().MyPID();
    if(myPID == 0)
      std::cout << "\nPERIDIGM-ALBANY OPTIMIZATION-BASED COUPLING FUNCTIONAL VALUE = " << obcFunctional << "\n" << std::endl;
#endif
#endif
  }

  //! update distributed parameters in the mesh
  Teuchos::RCP<DistParamLib> distParamLib = app_->getDistParamLib();
  distParamLib->scatter();
  DistParamLib::const_iterator it;
  Teuchos::RCP<const Teuchos::Comm<int> > commT = app_->getComm();
  Teuchos::RCP<Epetra_Comm>
    comm = Albany::createEpetraCommFromTeuchosComm(commT);
  for(it = distParamLib->begin(); it != distParamLib->end(); ++it) {
    Teuchos::RCP<Epetra_Vector> epetra_vec;
    Petra::TpetraVector_To_EpetraVector(it->second->overlapped_vector(),
                                        epetra_vec, comm);
    app_->getDiscretization()->setField(*epetra_vec, it->second->name(),
                                        /*overlapped*/ true);
  }

  StatelessObserverImpl::observeSolution(stamp, nonOverlappedSolution,
                                         nonOverlappedSolutionDot);
}
#endif

void ObserverImpl::observeSolutionT(
  double stamp, const Tpetra_Vector &nonOverlappedSolutionT,
  const Teuchos::Ptr<const Tpetra_Vector>& nonOverlappedSolutionDotT)
{
  app_->evaluateStateFieldManagerT(stamp, nonOverlappedSolutionDotT,
                                   Teuchos::null, nonOverlappedSolutionT);
  app_->getStateMgr().updateStates();

  StatelessObserverImpl::observeSolutionT(stamp, nonOverlappedSolutionT,
                                          nonOverlappedSolutionDotT);
}

} // namespace Albany

