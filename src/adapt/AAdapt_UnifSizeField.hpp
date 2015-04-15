//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef AADAPT_UNIFSIZEFIELD_HPP
#define AADAPT_UNIFSIZEFIELD_HPP

#include "Albany_PUMIDiscretization.hpp"
#include <ma.h>
#include "Albany_StateManager.hpp"
#include "AAdapt_MeshSizeField.hpp"

namespace AAdapt {

class UnifSizeField : public ma::IsotropicFunction, public MeshSizeField {

  public:
    UnifSizeField(const Teuchos::RCP<Albany::AbstractPUMIDiscretization>& disc);

    ~UnifSizeField();

    double getValue(ma::Entity* v);

    void setParams(const Teuchos::RCP<Teuchos::ParameterList>& p);

    void computeError();

    void copyInputFields() {}
    void freeInputFields() {}
    void freeSizeField() {}

  private:

    Teuchos::RCP<const Teuchos_Comm> commT;

    double elem_size;

};

}

#endif

