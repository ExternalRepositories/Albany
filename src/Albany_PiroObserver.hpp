//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef ALBANY_PIROOBSERVER_HPP
#define ALBANY_PIROOBSERVER_HPP

#include "Piro_ObserverBase.hpp"

#include "Albany_Application.hpp"
#include "Albany_ObserverImpl.hpp"

#include "Teuchos_RCP.hpp"

namespace Albany {

class PiroObserver : public Piro::ObserverBase<double> {
public:
  explicit PiroObserver(const Teuchos::RCP<Application> &app);

  virtual void observeSolution(const Thyra::VectorBase<double> &solution);

private:
  ObserverImpl impl_;
};

} // namespace Albany

#endif /*ALBANY_PIROOBSERVER_HPP*/
