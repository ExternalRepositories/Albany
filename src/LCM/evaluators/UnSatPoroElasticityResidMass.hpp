//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef UNSAT_POROELASTICITYRESIDMASS_HPP
#define UNSAT_POROELASTICITYRESIDMASS_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Intrepid_CellTools.hpp"
#include "Intrepid_Cubature.hpp"

namespace LCM {
/** \brief Finite Element Interpolation Evaluator

    This evaluator computes residual of the
    balance of mass for unsaturated
    poromechanics problem

**/

template<typename EvalT, typename Traits>
class UnSatPoroElasticityResidMass : public PHX::EvaluatorWithBaseImpl<Traits>,
				public PHX::EvaluatorDerived<EvalT, Traits>  {

public:

  UnSatPoroElasticityResidMass(const Teuchos::ParameterList& p);

  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  // Input:
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint> wBF;
  PHX::MDField<ScalarT,Cell,QuadPoint> porePressure;
  PHX::MDField<ScalarT,Cell,QuadPoint> Tdot;
  PHX::MDField<ScalarT,Cell,QuadPoint> stabParameter;
  PHX::MDField<ScalarT,Cell,QuadPoint> ThermalCond;
  PHX::MDField<ScalarT,Cell,QuadPoint> vgPermeability;
  PHX::MDField<ScalarT,Cell,QuadPoint> vgSat;
  PHX::MDField<ScalarT,Cell,QuadPoint> porosity;
  PHX::MDField<ScalarT,Cell,QuadPoint> biotCoefficient;
  PHX::MDField<ScalarT,Cell,QuadPoint> biotModulus;
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> wGradBF;
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim> TGrad;
  PHX::MDField<ScalarT,Cell,QuadPoint> Source;
  Teuchos::Array<double> convectionVels;
  PHX::MDField<ScalarT,Cell,QuadPoint> rhoCp;
  PHX::MDField<ScalarT,Cell,QuadPoint> Absorption;
  PHX::MDField<ScalarT,Cell,QuadPoint,Dim,Dim> strain;

  // stabilization term
  PHX::MDField<MeshScalarT,Cell,Vertex,Dim> coordVec;
  Teuchos::RCP<Intrepid::Cubature<RealType> > cubature;
  Teuchos::RCP<shards::CellTopology> cellType;
  PHX::MDField<MeshScalarT,Cell,QuadPoint> weights;

  // Time
  PHX::MDField<ScalarT,Dummy> deltaTime;

  //Data from previous time step
  std::string strainName, porePressureName, porosityName,
              satName;



  bool haveSource;
  bool haveConvection;
  bool haveAbsorption;
  bool enableTransient;
  bool haverhoCp;
  unsigned int numNodes;
  unsigned int numQPs;
  unsigned int numDims;
  unsigned int worksetSize;

  // Temporary FieldContainers
  Intrepid::FieldContainer<ScalarT> flux;
  Intrepid::FieldContainer<ScalarT> fluxdt;
  Intrepid::FieldContainer<ScalarT> pterm;
  Intrepid::FieldContainer<ScalarT> tpterm;
  Intrepid::FieldContainer<ScalarT> aterm;
  // Temporary FieldContainers
  Intrepid::FieldContainer<RealType> refPoints;
  Intrepid::FieldContainer<RealType> refWeights;
  Intrepid::FieldContainer<MeshScalarT> jacobian;
  Intrepid::FieldContainer<MeshScalarT> jacobian_inv;
  Intrepid::FieldContainer<MeshScalarT> Gc;


  ScalarT porePbar, vol;
  ScalarT trialPbar;


  // Output:
  PHX::MDField<ScalarT,Cell,Node> TResidual;


};
}

#endif
