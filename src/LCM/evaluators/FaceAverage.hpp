/*
 * FaceAverage.hpp
 *
 *  Created on: Jul 27, 2012
 *      Author: jrthune
 */

#ifndef FACE_AVERAGE_HPP_
#define FACE_AVERAGE_HPP_

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Intrepid_CellTools.hpp"
#include "Intrepid_Cubature.hpp"

namespace LCM {
/** \brief Computes the face average of a nodal value
 *
 * \param[in] nodal variable
 * \param[out] Face averaged variable
 */

template<typename EvalT, typename Traits>
class FaceAverage : public PHX::EvaluatorWithBaseImpl<Traits>,
  public PHX::EvaluatorDerived<EvalT, Traits>  {

  public:

      FaceAverage(const Teuchos::ParameterList& p);

      void postRegistrationSetup(typename Traits::SetupData d,
                        PHX::FieldManager<Traits>& vm);

      void evaluateFields(typename Traits::EvalData d);

    private:

      typedef typename EvalT::ScalarT ScalarT;
      typedef typename EvalT::MeshScalarT MeshScalarT;

      unsigned int numNodes;
      unsigned int numDims;
      unsigned int numFaces;
      unsigned int numComp; // length of the vector
      unsigned int worksetSize;
      unsigned int faceDim;
      unsigned int numFaceNodes;
      unsigned int numQPs;

      // Input:
      // Coordinates in the reference configuration
      PHX::MDField<ScalarT,Cell,Vertex,Dim> coordinates;

      // The field that was projected to the nodes
      PHX::MDField<ScalarT,Cell,Node,VecDim> projected;
      //Numerical integration rule
      Teuchos::RCP<Intrepid::Cubature<RealType> > cubature;
      // FE basis
      Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > intrepidBasis;
      // The cell type
      Teuchos::RCP<shards::CellTopology> cellType;

      //Output:
      // As a test, output the face average of the nodal field
      PHX::MDField<ScalarT,Cell,Face,VecDim> faceAve;

      // This is in here to trick the code to run the evaluator - does absolutely nothing
      PHX::MDField<ScalarT,Cell> temp;

      // For creating the quadrature weights
      Intrepid::FieldContainer<RealType> refPoints;
      Intrepid::FieldContainer<RealType> refWeights;
      Intrepid::FieldContainer<RealType> refValues;

      // Face topology data
      const struct CellTopologyData_Subcell * sides;

};

} // namespace LCM


#endif /* FACEAVERAGE_HPP_ */
