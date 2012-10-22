/********************************************************************\
*            Albany, Copyright (2010) Sandia Corporation             *
 *                                                                    *
 * Notice: This computer software was prepared by Sandia Corporation, *
 * hereinafter the Contractor, under Contract DE-AC04-94AL85000 with  *
 * the Department of Energy (DOE). All rights in the computer software*
 * are reserved by DOE on behalf of the United States Government and  *
 * the Contractor as provided in the Contract. You are authorized to  *
 * use this computer software for Governmental purposes but it is not *
 * to be released or distributed to the public. NEITHER THE GOVERNMENT*
 * NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR      *
 * ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This notice    *
 * including this sentence must appear on any copies of this software.*
 *    Questions to Andy Salinger, agsalin@sandia.gov                  *
 \********************************************************************/

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

#include "Tensor.h"

namespace LCM {

//**********************************************************************
  template<typename EvalT, typename Traits>
  SurfaceVectorJump<EvalT, Traits>::
  SurfaceVectorJump(const Teuchos::ParameterList& p,
                    const Teuchos::RCP<Albany::Layouts>& dl) :
      cubature     (p.get<Teuchos::RCP<Intrepid::Cubature<RealType> > >("Cubature")), 
      intrepidBasis(p.get<Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > >("Intrepid Basis")), 
      vector       (p.get<std::string>("Vector Name"),dl->node_vector),
      jump         (p.get<std::string>("Vector Jump Name"),dl->qp_vector)
  {
    this->addDependentField(vector);

    this->addEvaluatedField(jump);

    this->setName("Surface Vector Jump" + PHX::TypeString<EvalT>::value);

    std::vector<PHX::DataLayout::size_type> dims;
    dl->node_vector->dimensions(dims);
    worksetSize = dims[0];
    numNodes = dims[1];
    numDims = dims[2];

    numQPs = cubature->getNumPoints();

    numPlaneNodes = numNodes / 2;
    numPlaneDims = numDims - 1;

#ifdef ALBANY_VERBOSE
    std::cout << "in Surface Vector Jump" << std::endl;
    std::cout << " numPlaneNodes: " << numPlaneNodes << std::endl;
    std::cout << " numPlaneDims: " << numPlaneDims << std::endl;
    std::cout << " numQPs: " << numQPs << std::endl;
    std::cout << " cubature->getNumPoints(): " << cubature->getNumPoints() << std::endl;
    std::cout << " cubature->getDimension(): " << cubature->getDimension() << std::endl;
#endif
    
    // Allocate Temporary FieldContainers
    refValues.resize(numPlaneNodes, numQPs);
    refGrads.resize(numPlaneNodes, numQPs, numPlaneDims);
    refPoints.resize(numQPs, numPlaneDims);
    refWeights.resize(numQPs);

    // Pre-Calculate reference element quantitites
    cubature->getCubature(refPoints, refWeights);
    intrepidBasis->getValues(refValues, refPoints, Intrepid::OPERATOR_VALUE);
    intrepidBasis->getValues(refGrads, refPoints, Intrepid::OPERATOR_GRAD);
  }

  //**********************************************************************
  template<typename EvalT, typename Traits>
  void SurfaceVectorJump<EvalT, Traits>::postRegistrationSetup(
      typename Traits::SetupData d, PHX::FieldManager<Traits>& fm)
  {
    this->utils.setFieldData(vector, fm);
    this->utils.setFieldData(jump, fm);
  }

  //**********************************************************************
  template<typename EvalT, typename Traits>
  void SurfaceVectorJump<EvalT, Traits>::evaluateFields(
      typename Traits::EvalData workset)
  {
    LCM::Vector<ScalarT> vecA(0, 0, 0), vecB(0, 0, 0), vecJump(0, 0, 0);

    for (std::size_t cell = 0; cell < workset.numCells; ++cell) {
      for (std::size_t pt = 0; pt < numQPs; ++pt) {
        vecA.clear();
        vecB.clear();
        for (std::size_t node = 0; node < numPlaneNodes; ++node) {
          int topNode = node + numPlaneNodes;
          vecA += LCM::Vector<ScalarT>(
              refValues(node, pt) * vector(cell, node, 0),
              refValues(node, pt) * vector(cell, node, 1),
              refValues(node, pt) * vector(cell, node, 2));
          vecB += LCM::Vector<ScalarT>(
              refValues(node, pt) * vector(cell, topNode, 0),
              refValues(node, pt) * vector(cell, topNode, 1),
              refValues(node, pt) * vector(cell, topNode, 2));
        }
        vecJump = vecB - vecA;
        jump(cell, pt, 0) = vecJump(0);
        jump(cell, pt, 1) = vecJump(1);
        jump(cell, pt, 2) = vecJump(2);
      }
    }
  }

//**********************************************************************
}

