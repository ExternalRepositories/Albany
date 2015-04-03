//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#include <Intrepid_MiniTensor.h>

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

namespace LCM {

//**********************************************************************
template<typename EvalT, typename Traits>
SurfaceVectorJump<EvalT, Traits>::
SurfaceVectorJump(const Teuchos::ParameterList & p,
    const Teuchos::RCP<Albany::Layouts> & dl) :
    cubature_(p.get<Teuchos::RCP<Intrepid::Cubature<RealType> > >("Cubature")),
    intrepid_basis_(
        p
            .get<
                Teuchos::RCP<
                    Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > >(
            "Intrepid Basis")),
    vector_(p.get<std::string>("Vector Name"), dl->node_vector),
    jump_(p.get<std::string>("Vector Jump Name"), dl->qp_vector)
{
  this->addDependentField(vector_);

  this->addEvaluatedField(jump_);

  this->setName("Surface Vector Jump" + PHX::typeAsString<EvalT>());

  std::vector<PHX::DataLayout::size_type> dims;
  dl->node_vector->dimensions(dims);
  workset_size_ = dims[0];
  numNodes = dims[1];
  numDims = dims[2];

  numQPs = cubature_->getNumPoints();

  numPlaneNodes = numNodes / 2;
  numPlaneDims = numDims - 1;

#ifdef ALBANY_VERBOSE
  std::cout << "in Surface Vector Jump" << std::endl;
  std::cout << " numPlaneNodes: " << numPlaneNodes << std::endl;
  std::cout << " numPlaneDims: " << numPlaneDims << std::endl;
  std::cout << " numQPs: " << numQPs << std::endl;
  std::cout << " cubature->getNumPoints(): " << cubature_->getNumPoints() << std::endl;
  std::cout << " cubature->getDimension(): " << cubature_->getDimension() << std::endl;
#endif

  // Allocate Temporary FieldContainers
  ref_values_.resize(numPlaneNodes, numQPs);
  ref_grads_.resize(numPlaneNodes, numQPs, numPlaneDims);
  ref_points_.resize(numQPs, numPlaneDims);
  ref_weights_.resize(numQPs);

  // Pre-Calculate reference element quantitites
  cubature_->getCubature(ref_points_, ref_weights_);
  intrepid_basis_->getValues(ref_values_, ref_points_, Intrepid::OPERATOR_VALUE);
  intrepid_basis_->getValues(ref_grads_, ref_points_, Intrepid::OPERATOR_GRAD);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void SurfaceVectorJump<EvalT, Traits>::postRegistrationSetup(
    typename Traits::SetupData d, PHX::FieldManager<Traits> & fm)
{
  this->utils.setFieldData(vector_, fm);
  this->utils.setFieldData(jump_, fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void SurfaceVectorJump<EvalT, Traits>::evaluateFields(
    typename Traits::EvalData workset)
{
  Intrepid::Vector<ScalarT> vecA(0, 0, 0), vecB(0, 0, 0), vecJump(0, 0, 0);

  for (int cell = 0; cell < workset.numCells; ++cell) {
    for (int pt = 0; pt < numQPs; ++pt) {
      vecA.clear();
      vecB.clear();
      for (int node = 0; node < numPlaneNodes; ++node) {
        int topNode = node + numPlaneNodes;
        vecA += Intrepid::Vector<ScalarT>(
            ref_values_(node, pt) * vector_(cell, node, 0),
            ref_values_(node, pt) * vector_(cell, node, 1),
            ref_values_(node, pt) * vector_(cell, node, 2));
        vecB += Intrepid::Vector<ScalarT>(
            ref_values_(node, pt) * vector_(cell, topNode, 0),
            ref_values_(node, pt) * vector_(cell, topNode, 1),
            ref_values_(node, pt) * vector_(cell, topNode, 2));
      }
      vecJump = vecB - vecA;
      jump_(cell, pt, 0) = vecJump(0);
      jump_(cell, pt, 1) = vecJump(1);
      jump_(cell, pt, 2) = vecJump(2);
    }
  }
}

//**********************************************************************
}

