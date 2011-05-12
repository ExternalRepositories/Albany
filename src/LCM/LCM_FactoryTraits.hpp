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


#ifndef LCM_FACTORY_TRAITS_HPP
#define LCM_FACTORY_TRAITS_HPP

// User Defined Evaluator Types
#include "PHAL_Constant.hpp"
#include "PHAL_Dirichlet.hpp"
#include "PHAL_GatherSolution.hpp"
#include "PHAL_ScatterResidual.hpp"
#include "PHAL_Source.hpp"
#include "PHAL_ThermalConductivity.hpp"
#include "PHAL_ComputeBasisFunctions.hpp"
#include "PHAL_DOFInterpolation.hpp"
#include "PHAL_DOFGradInterpolation.hpp"
#include "PHAL_DOFVecInterpolation.hpp"
#include "PHAL_DOFVecGradInterpolation.hpp"
#include "PHAL_MapToPhysicalFrame.hpp"
#include "PHAL_HelmholtzResid.hpp"
#include "PHAL_HeatEqResid.hpp"
#include "PHAL_GatherCoordinateVector.hpp"
#include "PHAL_JouleHeating.hpp"
#include "PHAL_SaveStateField.hpp"

#include "LCM/evaluators/Stress.hpp"
#ifdef ALBANY_LAME
#include "LCM/evaluators/LameStress.hpp"
#endif
#include "LCM/evaluators/Strain.hpp"
#include "LCM/evaluators/ElasticModulus.hpp"
#include "LCM/evaluators/ElasticityResid.hpp"
#include "LCM/evaluators/PoissonsRatio.hpp"
#include "LCM/evaluators/DefGrad.hpp"
#include "LCM/evaluators/RCG.hpp"
#include "LCM/evaluators/LCG.hpp"
#include "LCM/evaluators/Neohookean.hpp"
#include "LCM/evaluators/J2Stress.hpp"
#include "LCM/evaluators/TLElasResid.hpp"
#include "LCM/evaluators/EnergyPotential.hpp"
#include "LCM/evaluators/HardeningModulus.hpp"
#include "LCM/evaluators/YieldStrength.hpp"
#include "LCM/evaluators/PisdWdF.hpp"
#include "LCM/evaluators/DamageResid.hpp"
#include "LCM/evaluators/J2Damage.hpp"
#include "LCM/evaluators/DamageLS.hpp"
#include "LCM/evaluators/SaturationModulus.hpp"
#include "LCM/evaluators/SaturationExponent.hpp"
#include "LCM/evaluators/Localization.hpp"
#include "LCM/evaluators/DamageSource.hpp"
#include "LCM/evaluators/ShearModulus.hpp"
#include "LCM/evaluators/BulkModulus.hpp"
#include "LCM/evaluators/KfieldBC.hpp"


#include "boost/mpl/vector/vector50.hpp"
#include "boost/mpl/placeholders.hpp"
using namespace boost::mpl::placeholders;

/*! \brief Struct to define Evaluator objects for the EvaluatorFactory.
    
    Preconditions:
    - You must provide a boost::mpl::vector named EvaluatorTypes that contain all 
    Evaluator objects that you wish the factory to build.  Do not confuse evaluator types 
    (concrete instances of evaluator objects) with evaluation types (types of evaluations 
    to perform, i.e., Residual, Jacobian). 

*/
namespace LCM {

template<typename Traits>
struct FactoryTraits {
  
  static const int id_dirichlet                 =  0;
  static const int id_gather_solution           =  1;
  static const int id_gather_coordinate_vector  =  2;
  static const int id_scatter_residual          =  3;
  static const int id_compute_basis_functions   =  4;
  static const int id_dof_interpolation         =  5;
  static const int id_dof_grad_interpolation    =  6;
  static const int id_dofvec_interpolation      =  7;
  static const int id_dofvec_grad_interpolation =  8;
  static const int id_map_to_physical_frame     =  9;
  static const int id_source                    = 10;
  static const int id_thermal_conductivity      = 11;
  static const int id_helmholtzresid            = 12;
  static const int id_heateqresid               = 13;
  static const int id_constant                  = 14;
  static const int id_dirichlet_aggregator      = 15;
  static const int id_jouleheating              = 16;
  static const int id_elastic_modulus           = 17;
  static const int id_stress                    = 18;
  static const int id_strain                    = 19;
  static const int id_elasticityresid           = 20;
  static const int id_poissons_ratio            = 21;
  static const int id_defgrad                   = 22;
  static const int id_rcg                       = 23;
  static const int id_lcg                       = 24;
  static const int id_neohookean_stress         = 25;
  static const int id_tl_elas_resid             = 26;
  static const int id_j2_stress                 = 27;
  static const int id_energy_potential          = 28;
  static const int id_hardening_modulus         = 29;
  static const int id_yield_strength            = 30;
  static const int id_pisdwdf_stress            = 31;
  static const int id_damage_resid              = 32;
  static const int id_j2_damage                 = 33;
  static const int id_damage_ls                 = 34;
  static const int id_sat_mod                   = 35;
  static const int id_sat_exp                   = 36;
  static const int id_localization              = 37;
  static const int id_damage_source             = 38;
  static const int id_bulk_modulus              = 39;
  static const int id_shear_modulus             = 40;
  static const int id_kfield_bc                 = 41;
  static const int id_savestatefield            = 42;
  // JTO - leave lame stress at the bottom for the convention below to be most effective
  static const int id_lame_stress               = 43;

#ifndef ALBANY_LAME
  typedef boost::mpl::vector43<
#else
  typedef boost::mpl::vector44<
#endif
    PHAL::Dirichlet<_,Traits>,                //  0
    PHAL::GatherSolution<_,Traits>,           //  1
    PHAL::GatherCoordinateVector<_,Traits>,   //  2
    PHAL::ScatterResidual<_,Traits>,          //  3
    PHAL::ComputeBasisFunctions<_,Traits>,    //  4
    PHAL::DOFInterpolation<_,Traits>,         //  5
    PHAL::DOFGradInterpolation<_,Traits>,     //  6
    PHAL::DOFVecInterpolation<_,Traits>,      //  7
    PHAL::DOFVecGradInterpolation<_,Traits>,  //  8
    PHAL::MapToPhysicalFrame<_,Traits>,       //  9
    PHAL::Source<_,Traits>,                   // 10
    PHAL::ThermalConductivity<_,Traits>,      // 11
    PHAL::HelmholtzResid<_,Traits>,           // 12
    PHAL::HeatEqResid<_,Traits>,              // 13
    PHAL::Constant<_,Traits>,                 // 14
    PHAL::DirichletAggregator<_,Traits>,      // 15
    PHAL::JouleHeating<_,Traits>,             // 16
    LCM::ElasticModulus<_,Traits>,            // 17
    LCM::Stress<_,Traits>,                    // 18
    LCM::Strain<_,Traits>,                    // 19
    LCM::ElasticityResid<_,Traits>,           // 20
    LCM::PoissonsRatio<_,Traits>,             // 21
    LCM::DefGrad<_,Traits>,                   // 22
    LCM::RCG<_,Traits>,                       // 23
    LCM::LCG<_,Traits>,                       // 24
    LCM::Neohookean<_,Traits>,                // 25
    LCM::TLElasResid<_,Traits>,               // 26
    LCM::J2Stress<_,Traits>,                  // 27
    LCM::EnergyPotential<_,Traits>,           // 28
    LCM::HardeningModulus<_,Traits>,          // 29
    LCM::YieldStrength<_,Traits>,             // 30
    LCM::PisdWdF<_,Traits>,                   // 31
    LCM::DamageResid<_,Traits>,               // 32
    LCM::J2Damage<_,Traits>,                  // 33
    LCM::DamageLS<_,Traits>,                  // 34
    LCM::SaturationModulus<_,Traits>,         // 35
    LCM::SaturationExponent<_,Traits>,        // 36
    LCM::Localization<_,Traits>,              // 37
    LCM::DamageSource<_,Traits>,              // 38
    LCM::BulkModulus<_,Traits>,               // 39
    LCM::ShearModulus<_,Traits>,              // 40
    LCM::KfieldBC<_,Traits>,                  // 41
    PHAL::SaveStateField<_,Traits>             // 42
#ifdef ALBANY_LAME
    ,LCM::LameStress<_,Traits>                // 43
#endif
    > EvaluatorTypes;
};
}

#endif

