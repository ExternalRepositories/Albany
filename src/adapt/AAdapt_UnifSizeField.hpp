//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#ifndef AADAPT_UNIFSIZEFIELD_HPP
#define AADAPT_UNIFSIZEFIELD_HPP

#include "AlbPUMI_FMDBDiscretization.hpp"
#include <ma.h>
#include "Albany_StateManager.hpp"

namespace AAdapt {

class UnifSizeField : public ma::IsotropicFunction {

  public:
    UnifSizeField(const Teuchos::RCP<AlbPUMI::AbstractPUMIDiscretization>& disc);

    ~UnifSizeField();

    double getValue(ma::Entity* v);

    void setParams(
		   double element_size, double err_bound,
		   const std::string state_var_name);

    void computeError();

    void copyInputFields() {};
    void freeInputFields() {};
    void freeSizeField() {};

  private:

    Teuchos::RCP<const Teuchos_Comm> commT;

    double elem_size;

};

}

#endif

