//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#include "Albany_StatelessObserverImpl.hpp"

#include "Albany_AbstractDiscretization.hpp"
#ifdef ALBANY_EPETRA
#include "AAdapt_AdaptiveSolutionManager.hpp"
#endif

#include "Teuchos_TimeMonitor.hpp"

#include <string>

namespace Albany {

StatelessObserverImpl::
StatelessObserverImpl (const Teuchos::RCP<Application> &app)
  : app_(app),
  solOutTime_(Teuchos::TimeMonitor::getNewTimer("Albany: Output to File"))
{}

RealType StatelessObserverImpl::
getTimeParamValueOrDefault (RealType defaultValue) const {
  const std::string label("Time");

  return (app_->getParamLib()->isParameter(label)) ?
    app_->getParamLib()->getRealValue<PHAL::AlbanyTraits::Residual>(label) :
    defaultValue;
}

#ifdef ALBANY_EPETRA
const Epetra_Map& StatelessObserverImpl::getNonOverlappedMap () const {
  return *app_->getMap();
}
#endif

Teuchos::RCP<const Tpetra_Map>
StatelessObserverImpl::getNonOverlappedMapT () const {
  return app_->getMapT();
}

#ifdef ALBANY_EPETRA
void StatelessObserverImpl::observeSolution (
  double stamp, const Epetra_Vector &nonOverlappedSolution,
  const Teuchos::Ptr<const Epetra_Vector>& nonOverlappedSolutionDot)
{
  Teuchos::TimeMonitor timer(*solOutTime_);
  const Teuchos::Ptr<const Epetra_Vector> overlappedSolution(
    app_->getAdaptSolMgr()->getOverlapSolution(nonOverlappedSolution));
  app_->getDiscretization()->writeSolution(*overlappedSolution, stamp,
                                           /*overlapped =*/ true);
}
#endif

void StatelessObserverImpl::observeSolutionT (
  double stamp, const Tpetra_Vector &nonOverlappedSolutionT,
  const Teuchos::Ptr<const Tpetra_Vector>& nonOverlappedSolutionDotT)
{
  Teuchos::TimeMonitor timer(*solOutTime_);
  const Teuchos::RCP<const Tpetra_Vector> overlappedSolutionT =
    app_->getOverlapSolutionT(nonOverlappedSolutionT);
  app_->getDiscretization()->writeSolutionT(
    *overlappedSolutionT, stamp, /*overlapped =*/ true);
}

} // namespace Albany
