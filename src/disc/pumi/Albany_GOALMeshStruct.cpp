//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Albany_GOALMeshStruct.hpp"
#include <apfShape.h>

Albany::GOALMeshStruct::GOALMeshStruct(
    const Teuchos::RCP<Teuchos::ParameterList>& params,
		const Teuchos::RCP<const Teuchos_Comm>& commT) :
  PUMIMeshStruct(params, commT)
{
  polynomialOrder = params->get<int>("Polynomial Order", 1);
  shape = apf::getHierarchic(polynomialOrder);
  for (int ps=0; ps < meshSpecs.size(); ++ps)
    meshSpecs[ps]->polynomialOrder = polynomialOrder;
}

Albany::GOALMeshStruct::~GOALMeshStruct()
{
}

Albany::AbstractMeshStruct::msType
Albany::GOALMeshStruct::meshSpecsType()
{
  return GOAL_MS;
}

apf::Field*
Albany::GOALMeshStruct::createNodalField(char const* name, int valueType)
{
  return apf::createField(this->mesh, name, valueType, shape);
}
