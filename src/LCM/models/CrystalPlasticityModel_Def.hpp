//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Albany_Utils.hpp"
#include <boost/math/special_functions/fpclassify.hpp>

//#define  PRINT_DEBUG
//#define  PRINT_OUTPUT
//#define  DECOUPLE
#define CP_HARDENING
#include <typeinfo>
#include <Sacado_Traits.hpp>
namespace LCM
{

//------------------------------------------------------------------------------
template<typename EvalT, typename Traits>
CrystalPlasticityModel<EvalT, Traits>::
CrystalPlasticityModel(Teuchos::ParameterList* p,
    const Teuchos::RCP<Albany::Layouts>& dl) :
    LCM::ConstitutiveModel<EvalT, Traits>(p, dl),
    num_slip_(p->get<int>("Number of Slip Systems", 0))
{
  slip_systems_.resize(num_slip_);

#ifdef PRINT_DEBUG
  std::cout << ">>> in cp constructor\n";
  std::cout << ">>> parameter list:\n" << *p << std::endl;
#endif

  Teuchos::ParameterList e_list = p->sublist("Crystal Elasticity");
  // assuming cubic symmetry
  c11_ = e_list.get<RealType>("C11");
  c12_ = e_list.get<RealType>("C12");
  c44_ = e_list.get<RealType>("C44");
  Intrepid::Tensor4<RealType> C(num_dims_);
  C.fill(Intrepid::ZEROS);
  for (int i = 0; i < num_dims_; ++i) {
    C(i,i,i,i) = c11_;
    for (int j = i+1; j < num_dims_; ++j) {
      C(i,i,j,j) = C(j,j,i,i) = c12_;
      C(i,j,i,j) = C(j,i,j,i) = C(i,j,j,i) = C(j,i,i,j) = c44_;
    }
  }
// NOTE check if basis is given else default
// NOTE default to coordinate axes and also construct 3rd direction if only 2 given
  orientation_.set_dimension(num_dims_);
  for (int i = 0; i < num_dims_; ++i) {
    std::vector<RealType> b_temp = e_list.get<Teuchos::Array<RealType> >(Albany::strint("Basis Vector", i+1)).toVector();
    RealType norm = 0.;
    for (int j = 0; j < num_dims_; ++j) {
      norm += b_temp[j]*b_temp[j];
    }
// NOTE check zero, rh system
// Filling columns of transformation with basis vectors
// We are forming R^{T} which is equivalent to the direction cosine matrix
    norm = 1./std::sqrt(norm);
    for (int j = 0; j < num_dims_; ++j) {
      orientation_(j,i) = b_temp[j]*norm;
    }
  }

// print rotation tensor employed for transformations
#ifdef PRINT_DEBUG
  std::cout << ">>> orientation_ :\n" << orientation_ << std::endl;
#endif

  // rotate elastic tensor and slip systems to match given orientation
  C_ = Intrepid::kronecker(orientation_,C);
  for (int num_ss=0; num_ss < num_slip_; ++num_ss) {
    Teuchos::ParameterList ss_list = p->sublist(Albany::strint("Slip System", num_ss+1));

    // Obtain and normalize slip directions. Miller indices need to be normalized.
    std::vector<RealType> s_temp = ss_list.get<Teuchos::Array<RealType> >("Slip Direction").toVector();
    Intrepid::Vector<RealType> s_temp_normalized(num_dims_,&s_temp[0]);
    s_temp_normalized = Intrepid::unit(s_temp_normalized);
    slip_systems_[num_ss].s_ = orientation_*s_temp_normalized;

    // Obtain and normal slip normals. Miller indices need to be normalized.
    std::vector<RealType> n_temp = ss_list.get<Teuchos::Array<RealType> >("Slip Normal").toVector();
    Intrepid::Vector<RealType> n_temp_normalized(num_dims_,&n_temp[0]);
    n_temp_normalized = Intrepid::unit(n_temp_normalized);
    slip_systems_[num_ss].n_ = orientation_*n_temp_normalized;

    // print each slip direction and slip normal after transformation
    #ifdef PRINT_DEBUG
      std::cout << ">>> slip direction " << num_ss + 1 << ": " << slip_systems_[num_ss].s_ << std::endl;
      std::cout << ">>> slip normal " << num_ss + 1 << ": " << slip_systems_[num_ss].n_ << std::endl;
    #endif

    slip_systems_[num_ss].projector_ = Intrepid::dyad(slip_systems_[num_ss].s_, slip_systems_[num_ss].n_);

    // print projector
    #ifdef PRINT_DEBUG
      std::cout << ">>> projector_ " << num_ss + 1 << ": " << slip_systems_[num_ss].projector_ << std::endl;
    #endif

    slip_systems_[num_ss].tau_critical_ = ss_list.get<RealType>("Tau Critical");
    slip_systems_[num_ss].gamma_dot_0_ = ss_list.get<RealType>("Gamma Dot");
    slip_systems_[num_ss].gamma_exp_ = ss_list.get<RealType>("Gamma Exponent");
    slip_systems_[num_ss].H_         = ss_list.get<RealType>("Hardening",0);
  }
#ifdef PRINT_DEBUG
  std::cout << "<<< done with parameter list\n";
#endif

  // retrive appropriate field name strings (ref to problems/FieldNameMap)
  std::string cauchy_string = (*field_name_map_)["Cauchy_Stress"];
  std::string Fp_string = (*field_name_map_)["Fp"];
  std::string L_string = (*field_name_map_)["Velocity_Gradient"]; 
  std::string F_string = (*field_name_map_)["F"];
  std::string J_string = (*field_name_map_)["J"];
  std::string source_string = (*field_name_map_)["Mechanical_Source"];

  // define the dependent fields
  this->dep_field_map_.insert(std::make_pair(F_string, dl->qp_tensor));
  this->dep_field_map_.insert(std::make_pair(J_string, dl->qp_scalar));
  this->dep_field_map_.insert(std::make_pair("Delta Time", dl->workset_scalar));

  // define the evaluated fields
  this->eval_field_map_.insert(std::make_pair(cauchy_string, dl->qp_tensor));
  this->eval_field_map_.insert(std::make_pair(Fp_string, dl->qp_tensor));
  this->eval_field_map_.insert(std::make_pair(L_string, dl->qp_tensor));
  this->eval_field_map_.insert(std::make_pair(source_string, dl->qp_scalar));
  this->eval_field_map_.insert(std::make_pair("Time", dl->workset_scalar));

  // define the state variables
  //
  // stress
  this->num_state_variables_++;
  this->state_var_names_.push_back(cauchy_string);
  this->state_var_layouts_.push_back(dl->qp_tensor);
  this->state_var_init_types_.push_back("scalar");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(false);
  this->state_var_output_flags_.push_back(p->get<bool>("Output Cauchy Stress", false));
  //
  // Fp
  this->num_state_variables_++;
  this->state_var_names_.push_back(Fp_string);
  this->state_var_layouts_.push_back(dl->qp_tensor);
  this->state_var_init_types_.push_back("identity");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(true);
  this->state_var_output_flags_.push_back(p->get<bool>("Output Fp", false));
  //
  // L
  this->num_state_variables_++;
  this->state_var_names_.push_back(L_string);
  this->state_var_layouts_.push_back(dl->qp_tensor);
  this->state_var_init_types_.push_back("identity");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(true);
  this->state_var_output_flags_.push_back(p->get<bool>("Output L", false));
  //
  // mechanical source (body force)
  this->num_state_variables_++;
  this->state_var_names_.push_back(source_string);
  this->state_var_layouts_.push_back(dl->qp_scalar);
  this->state_var_init_types_.push_back("scalar");
  this->state_var_init_values_.push_back(0.0);
  this->state_var_old_state_flags_.push_back(false);
  this->state_var_output_flags_.push_back(p->get<bool>("Output Mechanical Source", false));
  //
  // gammas
#ifdef CP_HARDENING
  for (int num_ss=0; num_ss < num_slip_; ++num_ss) {
    std::string g = Albany::strint("gamma_", num_ss+1);
    std::string gamma_string = (*field_name_map_)[g];
    this->eval_field_map_.insert(std::make_pair(gamma_string, dl->qp_scalar));
    this->num_state_variables_++;
    this->state_var_names_.push_back(gamma_string);
    this->state_var_layouts_.push_back(dl->qp_scalar);
    this->state_var_init_types_.push_back("scalar");
    this->state_var_init_values_.push_back(0.0);
    this->state_var_old_state_flags_.push_back(true);
    this->state_var_output_flags_.push_back(p->get<bool>("Output "+gamma_string , false));
  }
#endif

#ifdef PRINT_DEBUG
  std::cout << "<<< done in cp constructor\n";
#endif
}
//------------------------------------------------------------------------------
template<typename EvalT, typename Traits>
void CrystalPlasticityModel<EvalT, Traits>::
computeState(typename Traits::EvalData workset,
    std::map<std::string, Teuchos::RCP<PHX::MDField<ScalarT> > > dep_fields,
    std::map<std::string, Teuchos::RCP<PHX::MDField<ScalarT> > > eval_fields)
{
#ifdef PRINT_DEBUG
  std::cout << ">>> in cp compute state\n";
#endif
  // retrive appropriate field name strings
  std::string cauchy_string = (*field_name_map_)["Cauchy_Stress"];
  std::string Fp_string = (*field_name_map_)["Fp"];
  std::string L_string = (*field_name_map_)["Velocity_Gradient"];
  std::string source_string = (*field_name_map_)["Mechanical_Source"];
  std::string F_string = (*field_name_map_)["F"];
  std::string J_string = (*field_name_map_)["J"];

  // extract dependent MDFields
  PHX::MDField<ScalarT> def_grad = *dep_fields[F_string];
  PHX::MDField<ScalarT> J = *dep_fields[J_string];
  PHX::MDField<ScalarT> delta_time = *dep_fields["Delta Time"];

  // extract evaluated MDFields
  PHX::MDField<ScalarT> stress = *eval_fields[cauchy_string];
  PHX::MDField<ScalarT> plastic_deformation = *eval_fields[Fp_string];
  PHX::MDField<ScalarT> velocity_gradient = *eval_fields[L_string];
  PHX::MDField<ScalarT> source = *eval_fields[source_string];
  PHX::MDField<ScalarT> time = *eval_fields["Time"];
#ifdef CP_HARDENING
  std::vector<Teuchos::RCP<PHX::MDField<ScalarT> > > slips;
  std::vector<Albany::MDArray * > previous_slips;
  for (int num_ss=0; num_ss < num_slip_; ++num_ss) {
    std::string g = Albany::strint("gamma_", num_ss+1);
    std::string gamma_string = (*field_name_map_)[g];
    slips.push_back(eval_fields[gamma_string]);
    previous_slips.push_back(&((*workset.stateArrayPtr)[gamma_string + "_old"]));
  }
#endif

  // get state variables
  Albany::MDArray previous_plastic_deformation = (*workset.stateArrayPtr)[Fp_string + "_old"];

  ScalarT tau, gamma, dgamma;
  ScalarT g0, tauC, m, H;
  ScalarT dt = delta_time(0);
  ScalarT tcurrent = time(0);
  Intrepid::Tensor<ScalarT> Fp_temp(num_dims_);
  Intrepid::Tensor<ScalarT> F(num_dims_), Fp(num_dims_);
  Intrepid::Tensor<ScalarT> sigma(num_dims_), S(num_dims_);
  Intrepid::Tensor<ScalarT> L(num_dims_), expL(num_dims_);
  Intrepid::Tensor<RealType> P(num_dims_);
  I_=Intrepid::eye<RealType>(num_dims_);

#ifdef PRINT_OUTPUT
  std::ofstream out("output.dat", std::fstream::app);
#endif

  for (int cell(0); cell < workset.numCells; ++cell) {
    for (int pt(0); pt < num_pts_; ++pt) {
#ifdef PRINT_OUTPUT
//    std::cout << ">>> cell " << cell << " point " << pt << " <<<\n";
#endif
      // fill local tensors
      F.fill(def_grad, cell, pt, 0, 0);
      for (int i(0); i < num_dims_; ++i) {
        for (int j(0); j < num_dims_; ++j) {
          Fp(i, j) = ScalarT(previous_plastic_deformation(cell, pt, i, j));
        }
      }

      // compute stress 
      computeStress(F,Fp,sigma,S);
#ifdef PRINT_OUTPUT
      double dgammas[24];
      double taus[24];
#endif
      // compute velocity gradient
      L.fill(Intrepid::ZEROS);
      if (num_slip_ >0) { // crystal plasticity
        for (int s(0); s < num_slip_; ++s) {
          P  = slip_systems_[s].projector_; 
          // compute resolved shear stresses
          tau = Intrepid::dotdot(P,S);
          int sign = tau < 0 ? -1 : 1;
          // compute  dgammas
          g0   = slip_systems_[s].gamma_dot_0_;
          tauC = slip_systems_[s].tau_critical_;
          m    = slip_systems_[s].gamma_exp_;
          H    = slip_systems_[s].H_;
#ifdef CP_HARDENING
          PHX::MDField<ScalarT> slip  = *(slips[s]);
          Albany::MDArray previous_slip  = *(previous_slips[s]);
          slip(cell,pt) = previous_slip(cell,pt);
          gamma = slip(cell, pt);
#else
          gamma = 0.;
#endif
          ScalarT t1 = std::fabs(tau /(tauC+H*std::fabs(gamma)));
          dgamma = dt*g0*std::fabs(std::pow(t1,m))*sign;
#ifdef CP_HARDENING
          slip(cell, pt) += dgamma;
#endif
          L += (dgamma* P);
#ifdef PRINT_OUTPUT
          dgammas[s] = Sacado::ScalarValue<ScalarT>::eval(dgamma);
          taus[s] = Sacado::ScalarValue<ScalarT>::eval(tau);
#endif
        }
        // update plastic deformation gradient
        expL = Intrepid::exp(L);
        Fp_temp = expL * Fp;
        Fp = Fp_temp;
        // recompute stress
        computeStress(F,Fp,sigma,S);
      }
      source(cell, pt) = 0.0;
      for (int i(0); i < num_dims_; ++i) {
        for (int j(0); j < num_dims_; ++j) {

	  // Check for NaN and Inf
	  // DJL this check could/should eventually be made to run with debug builds only
	  TEUCHOS_TEST_FOR_EXCEPTION(!boost::math::isfinite(Fp(i,j)), std::logic_error,
				     "\n****Error, NaN detected in CrystalPlasticityModel Fp");
	  TEUCHOS_TEST_FOR_EXCEPTION(!boost::math::isfinite(sigma(i,j)), std::logic_error,
				     "\n****Error, NaN detected in CrystalPlasticityModel sigma");
	  TEUCHOS_TEST_FOR_EXCEPTION(!boost::math::isfinite(L(i,j)), std::logic_error,
				     "\n****Error, NaN detected in CrystalPlasticityModel L");

          plastic_deformation(cell, pt, i, j) = Fp(i, j);
          stress(cell, pt, i, j) = sigma(i, j);
          velocity_gradient(cell, pt, i, j) = L(i, j);
        }
      }
#ifdef PRINT_OUTPUT
      if (cell == 0 && pt == 0) {
      out << std::setprecision(12) << Sacado::ScalarValue<ScalarT>::eval(tcurrent) << " ";
      for (int i(0); i < num_dims_; ++i) {
        for (int j(0); j < num_dims_; ++j) {
          out << std::setprecision(12) <<  Sacado::ScalarValue<ScalarT>::eval(F(i,j)) << " ";
        }
      }
      for (int i(0); i < num_dims_; ++i) {
        for (int j(0); j < num_dims_; ++j) {
          out << std::setprecision(12) << Sacado::ScalarValue<ScalarT>::eval(Fp(i,j)) << " ";
        }
      }
      for (int i(0); i < num_dims_; ++i) {
        for (int j(0); j < num_dims_; ++j) {
          out << std::setprecision(12) <<  Sacado::ScalarValue<ScalarT>::eval(sigma(i,j)) << " ";
        }
      }
      for (int i(0); i < num_dims_; ++i) {
        for (int j(0); j < num_dims_; ++j) {
          out << std::setprecision(12) <<  Sacado::ScalarValue<ScalarT>::eval(L(i,j)) << " ";
        }
      }
      for (int s(0); s < num_slip_; ++s) {
        out << std::setprecision(12) <<  dgammas[s] << " ";
      }
      for (int s(0); s < num_slip_; ++s) {
        out << std::setprecision(12) <<  taus[s] << " ";
      }
      out << "\n";
      }
#endif
    }
  }
#ifdef PRINT_DEBUG
  std::cout << "<<< done in cp compute state\n" << std::flush;
#endif
}
//------------------------------------------------------------------------------
template<typename EvalT, typename Traits>
void CrystalPlasticityModel<EvalT, Traits>::
computeStress(Intrepid::Tensor<ScalarT> const & F,
              Intrepid::Tensor<ScalarT> const & Fp,
              Intrepid::Tensor<ScalarT>       & sigma, 
              Intrepid::Tensor<ScalarT>       & S) 

{
  // Saint Venant–Kirchhoff model
  Fpinv_ = Intrepid::inverse(Fp);
#ifdef DECOUPLE
  std::cout << "ELASTIC STRESS ONLY\n";
  Fe_ = F;
#else
  Fe_ = F * Fpinv_;
#endif
  E_ = 0.5*( Intrepid::transpose(Fe_) * Fe_ - I_);
  S = Intrepid::dotdot(C_,E_);
  sigma = (1.0 / Intrepid::det(F) ) * F * S * Intrepid::transpose(F);
}
//------------------------------------------------------------------------------
}
