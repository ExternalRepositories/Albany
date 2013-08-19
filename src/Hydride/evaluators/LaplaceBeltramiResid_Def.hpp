//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

namespace PHAL {

//**********************************************************************
template<typename EvalT, typename Traits>
LaplaceBeltramiResid<EvalT, Traits>::
LaplaceBeltramiResid(const Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl) :
  solnVec(p.get<std::string> ("Solution Vector Name"), dl->node_vector),
  Gc            (p.get<std::string> ("Contravariant Metric Tensor Name"), dl->qp_tensor),
  cubature(p.get<Teuchos::RCP <Intrepid::Cubature<RealType> > >("Cubature")),
  cellType(p.get<Teuchos::RCP <shards::CellTopology> > ("Cell Type")),
  intrepidBasis(p.get<Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > > ("Intrepid Basis")),
  solnResidual(p.get<std::string> ("Residual Name"), dl->node_vector) {

  this->addDependentField(Gc);
  this->addDependentField(solnVec);
  this->addEvaluatedField(solnResidual);

  std::vector<PHX::DataLayout::size_type> dims;
  dl->node_qp_vector->dimensions(dims);
  worksetSize = dims[0];
  numNodes = dims[1];
  numQPs  = dims[2];
  numDims = dims[3];

  // Allocate Temporary FieldContainers
  grad_at_cub_points.resize(numNodes, numQPs, numDims);
  refPoints.resize(numQPs, numDims);
  refWeights.resize(numQPs);
  jacobian.resize(worksetSize, numQPs, numDims, numDims);
  jacobian_det.resize(worksetSize, numQPs);

  // Pre-Calculate reference element quantitites
  cubature->getCubature(refPoints, refWeights);
  intrepidBasis->getValues(grad_at_cub_points, refPoints, Intrepid::OPERATOR_GRAD);

  this->setName("LaplaceBeltramiResid" + PHX::TypeString<EvalT>::value);

}

//**********************************************************************
template<typename EvalT, typename Traits>
void LaplaceBeltramiResid<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm) {

  this->utils.setFieldData(solnVec, fm);
  this->utils.setFieldData(Gc, fm);

  this->utils.setFieldData(solnResidual, fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void LaplaceBeltramiResid<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset) {

  // Need to be ScalarT!
  Intrepid::CellTools<ScalarT>::setJacobian(jacobian, refPoints, solnVec, *cellType);
  Intrepid::CellTools<ScalarT>::setJacobianDet(jacobian_det, jacobian);

    for(std::size_t cell = 0; cell < workset.numCells; ++cell) {
      for(std::size_t node_a = 0; node_a < numNodes; ++node_a) {

        for(std::size_t eq = 0; eq < numDims; eq++)
          solnResidual(cell, node_a, eq) = 0.0;

        for(std::size_t qp = 0; qp < numQPs; ++qp) {
          for(std::size_t node_b = 0; node_b < numNodes; ++node_b) {

            ScalarT kk = 0.0;

            for(std::size_t i = 0; i < numDims; i++) {
              for(std::size_t j = 0; j < numDims; j++) {

                kk += Gc(cell, qp, i, j) * grad_at_cub_points(node_a, qp, i) * grad_at_cub_points(node_b, qp, j);

              }
            }

            for(std::size_t eq = 0; eq < numDims; eq++) {

              solnResidual(cell, node_a, eq) +=
                kk * solnVec(cell, node_b, eq) * jacobian_det(cell, qp) * refWeights(qp);

            }
          }
        }
      }
    }

}

//**********************************************************************
}

