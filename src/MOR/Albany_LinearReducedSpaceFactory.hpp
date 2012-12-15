//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#ifndef ALBANY_LINEARREDUCEDSPACEFACTORY_HPP
#define ALBANY_LINEARREDUCEDSPACEFACTORY_HPP

#include "Teuchos_ParameterList.hpp"
#include "Teuchos_RCP.hpp"

#include <string>
#include <map>

class Epetra_MultiVector;

namespace Albany {

class LinearReducedSpace;

class LinearReducedSpaceFactory {
public:
  LinearReducedSpaceFactory();

  Teuchos::RCP<LinearReducedSpace> create(const Teuchos::RCP<Teuchos::ParameterList> &params);

  class BasisProvider;
  void extend(const std::string &id, const Teuchos::RCP<BasisProvider> &provider);

private:
  typedef std::map<std::string, Teuchos::RCP<BasisProvider> > BasisProviderMap;
  BasisProviderMap mvProviders_;
};

class LinearReducedSpaceFactory::BasisProvider {
public:
  virtual Teuchos::RCP<Epetra_MultiVector> operator()(const Teuchos::RCP<Teuchos::ParameterList> &params) = 0;
  virtual ~BasisProvider() {}
};

} // end namepsace Albany

#endif /* ALBANY_LINEARREDUCEDSPACEFACTORY_HPP */
