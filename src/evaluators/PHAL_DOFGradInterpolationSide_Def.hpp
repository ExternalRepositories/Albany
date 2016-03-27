//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

#include "Intrepid_FunctionSpaceTools.hpp"

namespace PHAL {

//**********************************************************************
template<typename EvalT, typename Traits, typename ScalarT>
DOFGradInterpolationSideBase<EvalT, Traits, ScalarT>::
DOFGradInterpolationSideBase(const Teuchos::ParameterList& p,
                         const Teuchos::RCP<Albany::Layouts>& dl) :
  sideSetName (p.get<std::string> ("Side Set Name")),
  val_node    (p.get<std::string> ("Variable Name"), dl->side_node_scalar),
  gradBF      (p.get<std::string> ("Gradient BF Name"), dl->side_node_qp_gradient),
  grad_qp      (p.get<std::string> ("Gradient Variable Name"), dl->side_qp_gradient )
{
  this->addDependentField(val_node);
  this->addDependentField(gradBF);
  this->addEvaluatedField(grad_qp);

  this->setName("DOFGradInterpolationSideBase" );

  std::vector<PHX::DataLayout::size_type> dims;
  gradBF.fieldTag().dataLayout().dimensions(dims);
  numSideNodes = dims[2];
  numSideQPs   = dims[3];
  numDims      = dims[4];
}

//**********************************************************************
template<typename EvalT, typename Traits, typename ScalarT>
void DOFGradInterpolationSideBase<EvalT, Traits, ScalarT>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(val_node,fm);
  this->utils.setFieldData(gradBF,fm);
  this->utils.setFieldData(grad_qp,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits, typename ScalarT>
void DOFGradInterpolationSideBase<EvalT, Traits, ScalarT>::
evaluateFields(typename Traits::EvalData workset)
{
  if (workset.sideSets->find(sideSetName)==workset.sideSets->end())
    return;

  const std::vector<Albany::SideStruct>& sideSet = workset.sideSets->at(sideSetName);
  for (auto const& it_side : sideSet)
  {
    // Get the local data of side and cell
    const int cell = it_side.elem_LID;
    const int side = it_side.side_local_id;

    for (int qp=0; qp<numSideQPs; ++qp)
    {
      for (int dim=0; dim<numDims; ++dim)
      {
        grad_qp(cell,side,qp,dim) = 0.;
        for (int node=0; node<numSideNodes; ++node)
        {
          grad_qp(cell,side,qp,dim) += val_node(cell,side,node) * gradBF(cell,side,node,qp,dim);
        }
      }
    }
  }
}

} // Namespace PHAL
