//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#include "Schwarz_StatelessObserverImpl.hpp"

#include "Albany_AbstractDiscretization.hpp"

#include "Teuchos_TimeMonitor.hpp"

#include <string>

namespace LCM {

StatelessObserverImpl::
StatelessObserverImpl (const Teuchos::RCP<Albany::Application> &app)
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

Teuchos::RCP<const Tpetra_Map>
StatelessObserverImpl::getNonOverlappedMapT () const {
  return app_->getMapT();
}

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

} // namespace LCM
