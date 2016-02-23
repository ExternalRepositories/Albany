//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#include "AAdapt_UnifSizeField.hpp"
#include "Albany_PUMIMeshStruct.hpp"

AAdapt::UnifSizeField::UnifSizeField(const Teuchos::RCP<Albany::APFDiscretization>& disc) :
  MeshSizeField(disc) {
}

AAdapt::UnifSizeField::
~UnifSizeField() {
}

void
AAdapt::UnifSizeField::configure(const Teuchos::RCP<Teuchos::ParameterList>& adapt_params_)
{

  ma::IsotropicFunction*
    isf = dynamic_cast<ma::IsotropicFunction*>(&unifIsoFunc);
  ma::Input *in = ma::configure(mesh_struct->getMesh(), isf);

  in->maximumIterations = adapt_params_->get<int>("Max Number of Mesh Adapt Iterations", 1);
  //do not snap on deformation problems even if the model supports it
  in->shouldSnap = false;

  setMAInputParams(adapt_params_, in);

  ma::adapt(in);

}

void
AAdapt::UnifSizeField::computeError() {
}


void
AAdapt::UnifSizeField::setParams(
    const Teuchos::RCP<Teuchos::ParameterList>& p) {
  unifIsoFunc.elem_size = p->get<double>("Target Element Size", 0.1);
}

