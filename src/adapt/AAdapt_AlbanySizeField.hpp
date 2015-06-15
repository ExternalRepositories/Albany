//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#ifndef AADAPT_ALBANYSIZEFIELD_HPP
#define AADAPT_ALBANYSIZEFIELD_HPP

#include "AAdapt_MeshSizeField.hpp"

/*
An Albany evaluator calculates the size field and passes it to this class
to perform the adaptation.
*/

namespace AAdapt {

class AlbanySizeField : public ma::IsotropicFunction, public MeshSizeField {

  public:

    AlbanySizeField(const Teuchos::RCP<Albany::AbstractPUMIDiscretization>& disc);

    ~AlbanySizeField();

    double getValue(ma::Entity* v);

    void setParams(const Teuchos::RCP<Teuchos::ParameterList>& p);

    void computeError();

    void copyInputFields();
    void freeInputFields() {}
    void freeSizeField() {}

  private:

    double elem_size;
    double averageEdgeLength;

};

}

#endif

