//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

//IK, 9/12/14: no Epetra!

#ifndef SCHWARZ_PIROOBSERVERT_HPP
#define SCHWARZ_PIROOBSERVERT_HPP

#include "Piro_ObserverBase.hpp"

#include "Albany_DataTypes.hpp"
#include "Albany_Application.hpp"
#include "Schwarz_ObserverImpl.hpp"
#include "SchwarzMultiscale.hpp"

#include "Teuchos_RCP.hpp"

namespace LCM {

class Schwarz_PiroObserverT : public Piro::ObserverBase<ST> {
public:
  explicit Schwarz_PiroObserverT(const Teuchos::RCP<Albany::Application> &app, const Teuchos::RCP<SchwarzMultiscale>& cs_model);

  virtual void observeSolution(
      const Thyra::VectorBase<ST> &solution);

  virtual void observeSolution(
      const Thyra::VectorBase<ST> &solution,
      const ST stamp);

  virtual void observeSolution(
      const Thyra::VectorBase<ST> &solution,
      const Thyra::VectorBase<ST> &solution_dot,
      const ST stamp);

private:
  void observeSolutionImpl(
      const Thyra::VectorBase<ST> &solution,
      const ST defaultStamp);

  void observeSolutionImpl(
      const Thyra::VectorBase<ST> &solution,
      const Thyra::VectorBase<ST> &solution_dot,
      const ST defaultStamp);

  void observeTpetraSolutionImpl(
      const Tpetra_Vector &solution,
      Teuchos::Ptr<const Tpetra_Vector> solution_dot,
      const ST defaultStamp);

  Teuchos::RCP<ObserverImpl> impl_;

  Teuchos::ArrayRCP<Teuchos::RCP<Albany::Application> > apps_;
};

} // namespace Albany

#endif /*SCHWARZ_PIROOBSERVERT_HPP*/
