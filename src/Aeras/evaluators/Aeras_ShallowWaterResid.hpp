//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef AERAS_SHALLOWWATERRESID_HPP
#define AERAS_SHALLOWWATERRESID_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Albany_Layouts.hpp"
#include "Sacado_ParameterAccessor.hpp"

#include <Shards_CellTopology.hpp>
#include <Intrepid_Basis.hpp>
#include <Intrepid_Cubature.hpp>

namespace Aeras {
/** \brief ShallowWater equation Residual for atmospheric modeling

    This evaluator computes the residual of the ShallowWater equation for
    atmospheric dynamics.

*/

template<typename EvalT, typename Traits>
class ShallowWaterResid : public PHX::EvaluatorWithBaseImpl<Traits>,
                   public PHX::EvaluatorDerived<EvalT, Traits>,
                   public Sacado::ParameterAccessor<EvalT, SPL_Traits>  {

public:
  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  ShallowWaterResid(const Teuchos::ParameterList& p,
                const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

  ScalarT& getValue(const std::string &n);

private:

  // Input:
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint> wBF;
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> wGradBF;
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim> U;  //vecDim works but its really Dim+1
  PHX::MDField<ScalarT,Cell,Node,VecDim> UNodal;
  PHX::MDField<ScalarT,Cell,Node,VecDim> UDotDotNodal;
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim,Dim> Ugrad;
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim> UDot;
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim> UDotDot;
  Teuchos::RCP<shards::CellTopology> cellType;

  PHX::MDField<ScalarT,Cell,QuadPoint> mountainHeight;

  PHX::MDField<MeshScalarT,Cell,QuadPoint> weighted_measure;

  PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim,Dim> jacobian;
  PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim,Dim> jacobian_inv;
  PHX::MDField<MeshScalarT,Cell,QuadPoint> jacobian_det;
  Intrepid::FieldContainer<RealType>    grad_at_cub_points;
  PHX::MDField<ScalarT,Cell,Node,VecDim> hyperviscosity;

  // Output:
  PHX::MDField<ScalarT,Cell,Node,VecDim> Residual;


  bool usePrescribedVelocity;
  bool useExplHyperviscosity;
  bool useImplHyperviscosity;
  bool plotVorticity;
                    
  Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > intrepidBasis;
  Teuchos::RCP<Intrepid::Cubature<RealType> > cubature;
  Intrepid::FieldContainer<RealType>    refPoints;
  Intrepid::FieldContainer<RealType>    refWeights;
#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
  Intrepid::FieldContainer<MeshScalarT>  nodal_jacobian;
  Intrepid::FieldContainer<MeshScalarT>  nodal_inv_jacobian;
  Intrepid::FieldContainer<MeshScalarT>  nodal_det_j;
#endif
  PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim>   sphere_coord;
  PHX::MDField<ScalarT,Cell,Node> lambda_nodal;
  PHX::MDField<ScalarT,Cell,Node> theta_nodal;
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim> source;

  ScalarT gravity; // gravity parameter -- Sacado-ized for sensitivities
  ScalarT Omega;   //rotation of earth  -- Sacado-ized for sensitivities

  double RRadius;   // 1/radius_of_earth
  double AlphaAngle;
  bool doNotDampRotation;

  int numNodes;
  int numQPs;
  int numDims;
  int vecDim;
  int spatialDim;

  //OG: this is temporary
  double sHvTau;

#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
  void divergence(const Intrepid::FieldContainer<ScalarT>  & fieldAtNodes,
      std::size_t cell, Intrepid::FieldContainer<ScalarT>  & div);

  //gradient returns vector in physical basis
  void gradient(const Intrepid::FieldContainer<ScalarT>  & fieldAtNodes,
      std::size_t cell, Intrepid::FieldContainer<ScalarT>  & gradField);

  // curl only returns the component in the radial direction
  void curl(const Intrepid::FieldContainer<ScalarT>  & fieldAtNodes,
      std::size_t cell, Intrepid::FieldContainer<ScalarT>  & curl);

  void fill_nodal_metrics(std::size_t cell);

  void get_coriolis(std::size_t cell, Intrepid::FieldContainer<ScalarT>  & coriolis);

  std::vector<LO> qpToNodeMap; 
  std::vector<LO> nodeToQPMap; 

#else
public:

  Kokkos::View<MeshScalarT***, PHX::Device> nodal_jacobian;
  Kokkos::View<MeshScalarT***, PHX::Device> nodal_inv_jacobian;
  Kokkos::View<MeshScalarT*, PHX::Device> nodal_det_j;
  Kokkos::View<MeshScalarT*, PHX::Device> refWeights_Kokkos;
  Kokkos::View<MeshScalarT***, PHX::Device> grad_at_cub_points_Kokkos;
  Kokkos::View<MeshScalarT**, PHX::Device> refPoints_kokkos;
 
 typedef PHX::KokkosViewFactory<ScalarT,PHX::Device> ViewFactory;

 PHX::MDField<ScalarT,Node, Dim> huAtNodes;
 PHX::MDField<ScalarT,QuadPoint> div_hU;
 PHX::MDField<ScalarT,Node> kineticEnergyAtNodes;
 PHX::MDField<ScalarT,QuadPoint, Dim> gradKineticEnergy;
 PHX::MDField<ScalarT,Node> potentialEnergyAtNodes;
 PHX::MDField<ScalarT,QuadPoint, Dim> gradPotentialEnergy;
 PHX::MDField<ScalarT,Node, Dim> uAtNodes;
 PHX::MDField<ScalarT,QuadPoint> curlU;
 PHX::MDField<ScalarT,QuadPoint> coriolis;

 PHX::MDField<ScalarT,Node> surf;
 PHX::MDField<ScalarT,Node> surftilde;
 PHX::MDField<ScalarT,QuadPoint, Dim> hgradNodes;
 PHX::MDField<ScalarT,QuadPoint, Dim> htildegradNodes;

// PHX::MDField<ScalarT,Node> ucomp;
// PHX::MDField<ScalarT,Node> vcomp;
// PHX::MDField<ScalarT,Node> utildecomp;
// PHX::MDField<ScalarT,Node> vtildecomp;

// PHX::MDField<ScalarT,QuadPoint, Dim> ugradNodes;
// PHX::MDField<ScalarT,QuadPoint, Dim> vgradNodes;
// PHX::MDField<ScalarT,QuadPoint, Dim> utildegradNodes;
// PHX::MDField<ScalarT,QuadPoint, Dim> vtildegradNodes;

 PHX::MDField<ScalarT,Node> uX, uY, uZ, utX, utY,utZ;
 PHX::MDField<ScalarT,QuadPoint, Dim> uXgradNodes, uYgradNodes, uZgradNodes;
 PHX::MDField<ScalarT,QuadPoint, Dim> utXgradNodes, utYgradNodes, utZgradNodes;

 PHX::MDField<ScalarT,Node, Dim> vcontra;

 std::vector<LO> qpToNodeMap;
 std::vector<LO> nodeToQPMap;
 Kokkos::View<int*, PHX::Device> nodeToQPMap_Kokkos;

 double a, myPi;

 KOKKOS_INLINE_FUNCTION
 void divergence(const PHX::MDField<ScalarT,Node, Dim>  & fieldAtNodes,
      const int cell) const;

// KOKKOS_INLINE_FUNCTION
// void gradient(const Intrepid::FieldContainer<ScalarT>  & fieldAtNodes,
//      int cell, Intrepid::FieldContainer<ScalarT>  & gradField)const;



 KOKKOS_INLINE_FUNCTION
 void curl(const int &cell)const;

 KOKKOS_INLINE_FUNCTION 
 void fill_nodal_metrics (const int &cell) const;
  
 KOKKOS_INLINE_FUNCTION
 void get_coriolis(const int &cell)const;

 typedef Kokkos::View<int***, PHX::Device>::execution_space ExecutionSpace;

 struct ShallowWaterResid_VecDim3_usePrescribedVelocity_Tag{};
 struct ShallowWaterResid_VecDim3_no_usePrescribedVelocity_Tag{};
 struct ShallowWaterResid_VecDim3_no_usePrescribedVelocity_explHV_Tag{};
 //The following are for hyperviscosity
 struct ShallowWaterResid_VecDim4_Tag{};
 struct ShallowWaterResid_VecDim6_Tag{};

 typedef Kokkos::RangePolicy<ExecutionSpace, ShallowWaterResid_VecDim3_usePrescribedVelocity_Tag> ShallowWaterResid_VecDim3_usePrescribedVelocity_Policy;
 typedef Kokkos::RangePolicy<ExecutionSpace, ShallowWaterResid_VecDim3_no_usePrescribedVelocity_Tag> ShallowWaterResid_VecDim3_no_usePrescribedVelocity_Policy;
 typedef Kokkos::RangePolicy<ExecutionSpace, ShallowWaterResid_VecDim4_Tag> ShallowWaterResid_VecDim4_Policy;
 typedef Kokkos::RangePolicy<ExecutionSpace, ShallowWaterResid_VecDim6_Tag> ShallowWaterResid_VecDim6_Policy;

 typedef Kokkos::RangePolicy<ExecutionSpace, ShallowWaterResid_VecDim3_no_usePrescribedVelocity_explHV_Tag> ShallowWaterResid_VecDim3_no_usePrescribedVelocity_explHV_Policy;

 KOKKOS_INLINE_FUNCTION
 void operator() (const ShallowWaterResid_VecDim3_usePrescribedVelocity_Tag& tag, const int& cell) const;
 KOKKOS_INLINE_FUNCTION
 void operator() (const ShallowWaterResid_VecDim3_no_usePrescribedVelocity_Tag& tag, const int& cell) const; 
 KOKKOS_INLINE_FUNCTION
 void operator() (const ShallowWaterResid_VecDim4_Tag& tag, const int& cell) const;
 KOKKOS_INLINE_FUNCTION
 void operator() (const ShallowWaterResid_VecDim6_Tag& tag, const int& cell) const; 
 
 KOKKOS_INLINE_FUNCTION
 void operator() (const ShallowWaterResid_VecDim3_no_usePrescribedVelocity_explHV_Tag& tag, const int& cell) const;

 KOKKOS_INLINE_FUNCTION
 void compute_huAtNodes_vecDim3(const int& cell) const;
 
 KOKKOS_INLINE_FUNCTION 
 void compute_Residual0(const int& cell) const;
 KOKKOS_INLINE_FUNCTION 
 void compute_Residual0_useHyperViscosity(const int& cell) const;
 KOKKOS_INLINE_FUNCTION 
 void compute_Residual3(const int& cell) const;

#endif
};

// Warning: these maps are a temporary fix, introduced by Steve Bova,
// to use the correct node ordering for node-point quadrature.  This
// should go away when spectral elements are fully implemented for
// Aeras.
//const int qpToNodeMap[9] = {0, 4, 1, 7, 8, 5, 3, 6, 2};
//const int nodeToQPMap[9] = {0, 2, 8, 6, 1, 5, 7, 3, 4};
// const int qpToNodeMap[4] = {0, 1, 3, 2};
// const int nodeToQPMap[4] = {0, 1, 3, 2};
// const int qpToNodeMap[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
// const int nodeToQPMap[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
}

#endif
