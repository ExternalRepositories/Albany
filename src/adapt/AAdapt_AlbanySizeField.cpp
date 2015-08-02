//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#include "AAdapt_AlbanySizeField.hpp"
#include "Albany_PUMIMeshStruct.hpp"

#include "Albany_Utils.hpp"
#include <apf.h>

AAdapt::AlbanySizeField::AlbanySizeField(const Teuchos::RCP<Albany::APFDiscretization>& disc) :
  apfDisc(disc), MeshSizeField(disc) {
}

AAdapt::AlbanySizeField::
~AlbanySizeField() {
}

void
AAdapt::AlbanySizeField::configure(const Teuchos::RCP<Teuchos::ParameterList>& adapt_params_)
{

// Print the fields we see

/* Debugging text
  for(size_t i = 0; i < mesh_struct->getMesh()->countFields(); i++){

    apf::Field* field_n = mesh_struct->getMesh()->getField(i);
    std::cout << "AlbanySizeField: found field name: " << apf::getName(field_n) << std::endl;
  }
*/

  apf::Field* field = mesh_struct->getMesh()->findField("proj_nodal_IsoMeshSizeField");
  TEUCHOS_TEST_FOR_EXCEPTION(field == NULL, std::logic_error, "Cannot find proj_nodal_IsoMeshSizeField");

  ma::Input *in = ma::configure(mesh_struct->getMesh(), field);

  in->maximumIterations = adapt_params_->get<int>("Max Number of Mesh Adapt Iterations", 1);
  //do not snap on deformation problems even if the model supports it
  in->shouldSnap = false;

  setMAInputParams(adapt_params_, in);

  ma::adapt(in);

}

void
AAdapt::AlbanySizeField::copyInputFields() {

  TEUCHOS_TEST_FOR_EXCEPTION(
      mesh_struct->nodal_data_base.is_null(), std::logic_error,
      "Mesh Adapt: Attempting to adapt the mesh to a size field and none are defined");

  const Teuchos::RCP<Albany::NodeFieldContainer>
    node_states = mesh_struct->nodal_data_base->getNodeContainer();
  apf::Mesh2* const m = mesh_struct->getMesh();

  for (Albany::NodeFieldContainer::iterator nfs = node_states->begin();
       nfs != node_states->end(); ++nfs) {
    Teuchos::RCP<Albany::PUMINodeDataBase<RealType> >
      nd = Teuchos::rcp_dynamic_cast<Albany::PUMINodeDataBase<RealType>>(
        nfs->second);
    TEUCHOS_TEST_FOR_EXCEPTION(
      nd.is_null(), std::logic_error,
      "A node field container is not a PUMINodeDataBase");

    if ( nd->name != "proj_nodal_IsoMeshSizeField") continue;

    int value_type, nentries;
    const int spdim = mesh_struct->numDim;
    switch (nd->ndims()) {
    case 0: value_type = apf::SCALAR; nentries = 1; break;
    case 1: value_type = apf::VECTOR; nentries = spdim; break;
    case 2: value_type = apf::MATRIX; nentries = spdim*spdim; break;
    default:
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                                 "dim is not in {1,2,3}");
    }
//    std::cout << "Adapt - Saving Node Field: " << nd->name << std::endl;
    apf::Field* f = apf::createFieldOn(m, nd->name.c_str(), value_type);
    apfDisc->setField(nd->name.c_str(), &nd->buffer[0], false, 0, nentries);
    return;
  }
}

void
AAdapt::AlbanySizeField::freeInputFields() {

//  std::cout << "Adapt - destroying Node Field: proj_nodal_IsoMeshSizeField" << std::endl;
  apf::destroyField(mesh_struct->getMesh()->findField("proj_nodal_IsoMeshSizeField"));

}
