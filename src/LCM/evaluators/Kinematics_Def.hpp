//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#include "amb.hpp"

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

#include <Intrepid_MiniTensor.h>

#include <typeinfo>

namespace LCM {

  //----------------------------------------------------------------------------
  template<typename EvalT, typename Traits>
  Kinematics<EvalT, Traits>::
  Kinematics(Teuchos::ParameterList& p,
             const Teuchos::RCP<Albany::Layouts>& dl) :
    grad_u_   (p.get<std::string>("Gradient QP Variable Name"),dl->qp_tensor),
    weights_  (p.get<std::string>("Weights Name"),dl->qp_scalar),
    def_grad_ (p.get<std::string>("DefGrad Name"),dl->qp_tensor),
    j_        (p.get<std::string>("DetDefGrad Name"),dl->qp_scalar),
    weighted_average_(p.get<bool>("Weighted Volume Average J", false)),
    alpha_(p.get<RealType>("Average J Stabilization Parameter", 0.0)),
    needs_vel_grad_(false),
    needs_strain_(false)
  {
    if ( p.isType<bool>("Velocity Gradient Flag") )
      needs_vel_grad_ = p.get<bool>("Velocity Gradient Flag");
    if ( p.isType<std::string>("Strain Name") ) {
      needs_strain_ = true;
      PHX::MDField<ScalarT,Cell,QuadPoint,Dim,Dim>
        tmp(p.get<std::string>("Strain Name"),dl->qp_tensor);
      strain_ = tmp;
      this->addEvaluatedField(strain_);
    }

    std::vector<PHX::DataLayout::size_type> dims;
    dl->qp_tensor->dimensions(dims);
    num_pts_  = dims[1];
    num_dims_ = dims[2];

    this->addDependentField(grad_u_);
    this->addDependentField(weights_);

    this->addEvaluatedField(def_grad_);
    this->addEvaluatedField(j_);

    if (needs_vel_grad_) {
      PHX::MDField<ScalarT,Cell,QuadPoint,Dim,Dim>
        tmp(p.get<std::string>("Velocity Gradient Name"),dl->qp_tensor);
      vel_grad_ = tmp;
      this->addEvaluatedField(vel_grad_);
    }

    this->setName("Kinematics"+PHX::typeAsString<PHX::Device>());

   
#ifndef NO_KOKKOS_ALBANY
    //Allocationg additional data for Kokkos functors
    ddims_.push_back(24);
    F=PHX::MDField<ScalarT,Dim,Dim>("F",Teuchos::rcp(new PHX::MDALayout<Dim,Dim>(num_dims_,num_dims_)));   
    strain=PHX::MDField<ScalarT,Dim,Dim>("strain",Teuchos::rcp(new PHX::MDALayout<Dim,Dim>(num_dims_,num_dims_)));
    gradu=PHX::MDField<ScalarT,Dim,Dim>("gradu",Teuchos::rcp(new PHX::MDALayout<Dim,Dim>(num_dims_,num_dims_)));
    tgradu=PHX::MDField<ScalarT,Dim,Dim>("tgradu",Teuchos::rcp(new PHX::MDALayout<Dim,Dim>(num_dims_,num_dims_)));
    Itensor=PHX::MDField<ScalarT,Dim,Dim>("Itensor",Teuchos::rcp(new PHX::MDALayout<Dim,Dim>(num_dims_,num_dims_)));

    F.setFieldData(ViewFactory::buildView(F.fieldTag(),ddims_));
    strain.setFieldData(ViewFactory::buildView(strain.fieldTag(),ddims_));
    gradu.setFieldData(ViewFactory::buildView(gradu.fieldTag(),ddims_));
    tgradu.setFieldData(ViewFactory::buildView(tgradu.fieldTag(),ddims_));
    Itensor.setFieldData(ViewFactory::buildView(Itensor.fieldTag(),ddims_));
#endif 
 }

  //----------------------------------------------------------------------------
  template<typename EvalT, typename Traits>
  void Kinematics<EvalT, Traits>::
  postRegistrationSetup(typename Traits::SetupData d,
                        PHX::FieldManager<Traits>& fm)
  {
    this->utils.setFieldData(weights_,fm);
    this->utils.setFieldData(def_grad_,fm);
    this->utils.setFieldData(j_,fm);
    this->utils.setFieldData(grad_u_,fm);
    if (needs_strain_) this->utils.setFieldData(strain_,fm);
    if (needs_vel_grad_) this->utils.setFieldData(vel_grad_,fm);
  }

  //----------------------------------------------------------------------------
#define wse(TYPE)                                                       \
  void writestuff (                                                     \
    const LCM::Kinematics<PHAL::AlbanyTraits::TYPE, PHAL::AlbanyTraits>& k, \
    PHAL::AlbanyTraits::EvalData workset) {}
wse(Jacobian);
wse(Tangent);
wse(DistParamDeriv);

void writestuff (
  const Kinematics<PHAL::AlbanyTraits::Residual, PHAL::AlbanyTraits>& k,
  PHAL::AlbanyTraits::EvalData workset)
{
  if (amb::print_level() < 2) return;
  const int nc = workset.numCells, nd = k.num_dims_, np = k.num_pts_;
  amb_write_mdfield2(k.j_, "k_j", nc, np);
  amb_write_mdfield4(k.def_grad_, "k_def_grad", nc, np, nd, nd);
  amb_write_mdfield4(k.grad_u_, "k_grad_u", nc, np, nd, nd);
}
//----------------------------------------------------------------------------
//Kokkos kernels
//
template<typename EvalT, typename Traits>
template <class ArrayT>
KOKKOS_INLINE_FUNCTION
const typename Kinematics<EvalT, Traits>::ScalarT 
Kinematics<EvalT, Traits>::
det(const ArrayT &A) const
{
  const int dimension = A.dimension(0);

  ScalarT s = 0.0;

  switch (dimension) {

    default:
    {
     TEUCHOS_TEST_FOR_EXCEPTION( !( (dimension == 2) || (dimension == 3) ), std::invalid_argument,
                                  ">>> ERROR (LCM Kinematics): Det function is defined only for rank-2 or 3 .");
    }
    break;

    case 3:
      s = -A(0,2)*A(1,1)*A(2,0) + A(0,1)*A(1,2)*A(2,0) +
           A(0,2)*A(1,0)*A(2,1) - A(0,0)*A(1,2)*A(2,1) -
           A(0,1)*A(1,0)*A(2,2) + A(0,0)*A(1,1)*A(2,2);
      break;

    case 2:
      s = A(0,0) * A(1,1) - A(1,0) * A(0,1);
      break;

  }

  return s;
}

template<typename EvalT, typename Traits>
template <class ArrayT>
KOKKOS_INLINE_FUNCTION
const ArrayT Kinematics<EvalT, Traits>::
transpose(const ArrayT& A) const
{
  const int dimension = A.dimension(0);

  ArrayT B = A;
      for (int i = 0; i < dimension; ++i) {
        for (int j = i + 1; j < dimension; ++j) {
           B(i,j)=A(j,i);
        }
      }
  return B;
}

template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void Kinematics<EvalT, Traits>::
compute_defgrad(const int cell) const{

 for (int ii=0; ii<num_dims_; ii++){
     for (int j=0; j<num_dims_; j++){
      Itensor(ii,j)=0.0;
      if (ii==j) Itensor(ii,j)=1;
     }
  }

    for (int pt(0); pt < num_pts_; ++pt) {
     
      for (int ii=0; ii<num_dims_; ii++){
         for (int j=0; j<num_dims_; j++){
            gradu(ii,j)=grad_u_(cell,pt,ii,j);
            F(ii,j) = Itensor(ii,j) + gradu(ii,j);
         }
      }
     
     j_(cell,pt)=det(F);
      for (int i(0); i < num_dims_; ++i) 
          for (int j(0); j < num_dims_; ++j) 
            def_grad_(cell,pt,i,j) = F(i,j);
    }

}

template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void Kinematics<EvalT, Traits>::
compute_weighted_average(const int cell) const{

   ScalarT jbar, weighted_jbar, volume;
        jbar = 0.0;
        volume = 0.0;
        for (int pt(0); pt < num_pts_; ++pt) {
          jbar += weights_(cell,pt) * std::log( j_(cell,pt) );
          volume += weights_(cell,pt);
        }
        jbar /= volume;

        for (int pt(0); pt < num_pts_; ++pt) {
          weighted_jbar =
            std::exp( (1-alpha_) * jbar + alpha_ * std::log( j_(cell,pt) ) );
           for (int i=0; i<num_dims_; i++){
               for (int j=0; j<num_dims_; j++){
                 F(i,j)=def_grad_(cell, pt,i,j);
                 F(i,j)=F(i,j)*std::pow( (weighted_jbar/j_(cell,pt)), 1./3. );
               }
           }
          j_(cell,pt) = weighted_jbar;
          for (int i(0); i < num_dims_; ++i) {
            for (int j(0); j < num_dims_; ++j) {
              def_grad_(cell,pt,i,j) = F(i,j);
            }
          }
        }
}

template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void Kinematics<EvalT, Traits>::
compute_strain(const int cell) const{
     for (int pt(0); pt < num_pts_; ++pt) {
         // tgradu=transpose(gradu);
          for (int i=0; i<num_dims_; i++){
            for (int j=0; j<num_dims_; j++){
                F(i,j)=grad_u_(cell, pt,i,j);
                strain(i,j) = 0.5 * (gradu(i,j) + gradu(j,i));
            }
          }
          for (int i(0); i < num_dims_; ++i) {
            for (int j(0); j < num_dims_; ++j) {
              strain_(cell,pt,i,j) = strain(i,j);
            }
          }
        }
}

template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void Kinematics<EvalT, Traits>::
operator() (const kinematic_Tag& tag, const int& i) const{
  
  this->compute_defgrad(i);
 
}

template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void Kinematics<EvalT, Traits>::
operator() (const kinematic_weighted_average_Tag& tag, const int& i) const{

  this->compute_defgrad(i);
  this->compute_weighted_average(i);

}

template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void Kinematics<EvalT, Traits>::
operator() (const kinematic_needs_strain_Tag& tag, const int& i) const{

   this->compute_defgrad(i);
   this->compute_strain(i);  

}

template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void Kinematics<EvalT, Traits>::
operator() (const kinematic_weighted_average_needs_strain_Tag& tag, const int& i) const{

  this->compute_defgrad(i);
  this->compute_weighted_average(i);
  this->compute_strain(i);  

}

//----------------------------------------------------------------------------
  template<typename EvalT, typename Traits>
  void Kinematics<EvalT, Traits>::
  evaluateFields(typename Traits::EvalData workset)
  {

#ifdef NO_KOKKOS_ALBANY
    Intrepid::Tensor<ScalarT> F(num_dims_), strain(num_dims_), gradu(num_dims_);
    Intrepid::Tensor<ScalarT> I(Intrepid::eye<ScalarT>(num_dims_));

    // Compute DefGrad tensor from displacement gradient
    for (int cell(0); cell < workset.numCells; ++cell) {
      for (int pt(0); pt < num_pts_; ++pt) {
        gradu.fill( grad_u_,cell,pt,0,0);
        F = I + gradu;
        j_(cell,pt) = Intrepid::det(F);
        for (int i(0); i < num_dims_; ++i) {
          for (int j(0); j < num_dims_; ++j) {
            def_grad_(cell,pt,i,j) = F(i,j);
          }
        }
      }
    }

    if (weighted_average_) {
      ScalarT jbar, weighted_jbar, volume;
      for (int cell(0); cell < workset.numCells; ++cell) {
        jbar = 0.0;
        volume = 0.0;
        for (int pt(0); pt < num_pts_; ++pt) {
          jbar += weights_(cell,pt) * std::log( j_(cell,pt) );
          volume += weights_(cell,pt);
        }
        jbar /= volume;

        for (int pt(0); pt < num_pts_; ++pt) {
          weighted_jbar = 
            std::exp( (1-alpha_) * jbar + alpha_ * std::log( j_(cell,pt) ) );
          F.fill( def_grad_,cell,pt,0,0);
          F = F*std::pow( (weighted_jbar/j_(cell,pt)), 1./3. );
          j_(cell,pt) = weighted_jbar;
          for (int i(0); i < num_dims_; ++i) {
            for (int j(0); j < num_dims_; ++j) {
              def_grad_(cell,pt,i,j) = F(i,j);
            }
          }
        }
      }
    }

    if (needs_strain_) {
      for (int cell(0); cell < workset.numCells; ++cell) {
        for (int pt(0); pt < num_pts_; ++pt) {
          gradu.fill( grad_u_,cell,pt,0,0);
          strain = 0.5 * (gradu + Intrepid::transpose(gradu));
          for (int i(0); i < num_dims_; ++i) {
            for (int j(0); j < num_dims_; ++j) {
              strain_(cell,pt,i,j) = strain(i,j);
            }
          }
        }
      }
    }
#else

  if (weighted_average_) {
    if (needs_strain_) Kokkos::parallel_for(kinematic_weighted_average_needs_strain_Policy(0,workset.numCells),*this);
   else Kokkos::parallel_for(kinematic_weighted_average_Policy(0,workset.numCells),*this);
  }
  else{
   if (needs_strain_) Kokkos::parallel_for(kinematic_needs_strain_Policy(0,workset.numCells),*this);
   else Kokkos::parallel_for(kinematic_Policy(0,workset.numCells),*this);
  }

#endif
   // writestuff(*this, workset);
  }
  //----------------------------------------------------------------------------
}
