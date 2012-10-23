//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#ifndef ALBANY_RYTHMOSPROJECTIONERROROBSERVER_HPP
#define ALBANY_RYTHMOSPROJECTIONERROROBSERVER_HPP

#include "Rythmos_IntegrationObserverBase.hpp"

#include "Albany_ProjectionError.hpp"

class Epetra_Map;

#include "Teuchos_ParameterList.hpp"

namespace Albany {

class RythmosProjectionErrorObserver : public Rythmos::IntegrationObserverBase<double> {
public:
  explicit RythmosProjectionErrorObserver(const Teuchos::RCP<Teuchos::ParameterList> &params,
                                          const Teuchos::RCP<const Epetra_Map> &stateMap);

  // Overridden
  virtual Teuchos::RCP<Rythmos::IntegrationObserverBase<double> > cloneIntegrationObserver() const;

  virtual void resetIntegrationObserver(const Rythmos::TimeRange<double> &integrationTimeDomain);

  virtual void observeStartTimeStep(
    const Rythmos::StepperBase<double> &stepper,
    const Rythmos::StepControlInfo<double> &stepCtrlInfo,
    const int timeStepIter);

  virtual void observeCompletedTimeStep(
    const Rythmos::StepperBase<double> &stepper,
    const Rythmos::StepControlInfo<double> &stepCtrlInfo,
    const int timeStepIter);

private:
  ProjectionError projectionError_;
  Teuchos::RCP<const Epetra_Map> stateMap_;

  virtual void observeTimeStep(
    const Rythmos::StepperBase<double> &stepper,
    const Rythmos::StepControlInfo<double> &stepCtrlInfo,
    const int timeStepIter);
};

} // namespace Albany

#endif /*ALBANY_RYTHMOSPROJECTIONERROROBSERVER_HPP*/
