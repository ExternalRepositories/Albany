//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <vector>
#include <string>

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

namespace PHAL {


template<typename EvalT, typename Traits>
GatherAuxData<EvalT,Traits>::
GatherAuxData(const Teuchos::ParameterList& p,
                   const Teuchos::RCP<Albany::Layouts>& dl)
{ 
  
  
  std::string field_name = p.get<std::string>("Field Name"); 
  auxDataIndex = p.get<int>("Aux Data Vector Index");

  PHX::MDField<ScalarT,Cell,Node> f(field_name ,dl->node_scalar);
  vector_data = f;
  this->addEvaluatedField(vector_data);

  char buf[200];
  sprintf(buf, "Gather Aux Data %d to %s", (int)auxDataIndex, field_name.c_str());
  this->setName(buf + PHX::TypeString<EvalT>::value);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void GatherAuxData<EvalT,Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(vector_data,fm);
  numNodes = vector_data.dimension(1);
}

// **********************************************************************

template<typename EvalT, typename Traits>
void GatherAuxData<EvalT,Traits>::
evaluateFields(typename Traits::EvalData workset)
{ 
  const Epetra_Vector& v = *((*(workset.auxDataPtr))(auxDataIndex));

  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID = workset.wsElNodeEqID[cell];
    
    for(std::size_t node =0; node < this->numNodes; ++node) {
      int offsetIntoVec = nodeID[node][0]; // neq==1 hardwired
      this->vector_data(cell,node) = v[offsetIntoVec];
    }
  }

}

// **********************************************************************

}
