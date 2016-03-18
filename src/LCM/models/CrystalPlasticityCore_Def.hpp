//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <boost/math/special_functions/fpclassify.hpp>


///
/// Verify that constitutive update has preserved finite values
///
template<Intrepid2::Index NumDimT, typename ArgT>
void
CP::confirmTensorSanity(
    Intrepid2::Tensor<ArgT, NumDimT> const & input,
    std::string const & message)
{
  int dim = input.get_dimension();
  for (int i = 0; i < dim; i++) {
    for (int j = 0; j < dim; j++) {
      assert(boost::math::isfinite(Sacado::ScalarValue<ArgT>::eval(input(i, j)))==true);
      // Disabling this capability for release.
      // We will revisit this option when we can cut the time step from the constitutive model.
      /* if (!boost::math::isfinite(
          Sacado::ScalarValue<ArgT>::eval(input(i, j)))) {
        std::string msg =
            "**** Invalid data detected in CP::confirmTensorSanity(): "
                + message;
        TEUCHOS_TEST_FOR_EXCEPTION(
            !boost::math::isfinite(
                Sacado::ScalarValue<ArgT>::eval(input(i, j))),
            std::logic_error,
            msg);
      } */
    }
  }
}



///
/// Update the plastic quantities
///
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, typename DataT,
    typename ArgT>
void
CP::applySlipIncrement(
    std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems,
    DataT dt,
    Intrepid2::Vector<DataT, NumSlipT> const & slip_n,
    Intrepid2::Vector<ArgT, NumSlipT> const & slip_np1,
    Intrepid2::Tensor<DataT, NumDimT> const & Fp_n,
    Intrepid2::Tensor<ArgT, NumDimT> & Lp_np1,
    Intrepid2::Tensor<ArgT, NumDimT> & Fp_np1)
{
  Intrepid2::Index const num_slip = slip_n.get_dimension();
  Intrepid2::Index const num_dim = Fp_n.get_dimension();

  Intrepid2::Tensor<ArgT, NumDimT> exp_L_dt(num_dim);

  Lp_np1.fill(Intrepid2::ZEROS);
  for (int s(0); s < num_slip; ++s) {

    // calculate plastic velocity gradient
    if(dt > 0){
      Lp_np1 += (slip_np1[s] - slip_n[s])/dt * slip_systems[s].projector_;
    }
  }

  CP::confirmTensorSanity<NumDimT>(Lp_np1, "Lp_np1 in applySlipIncrement().");

  // update plastic deformation gradient
  // F^{p}_{n+1} = exp(L_{n+1} * delta t) F^{p}_{n}
  exp_L_dt = Intrepid2::exp(Lp_np1 * dt);
  Fp_np1 = exp_L_dt * Fp_n;

  CP::confirmTensorSanity<NumDimT>(Fp_np1, "Fp_np1 in applySlipIncrement()");
}




///
/// Evolve the hardnesses
///
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, typename DataT,
    typename ArgT>
void
CP::updateHardness(
    std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems,
    DataT dt,
    Intrepid2::Vector<ArgT, NumSlipT> const & rate_slip,
    Intrepid2::Vector<DataT, NumSlipT> const & hardness_n,
    Intrepid2::Vector<ArgT, NumSlipT> & hardness_np1)
{
  Intrepid2::Index const num_slip = rate_slip.get_dimension();

  if(num_slip == 0) {
    return;
  }

  using HARDENING = CP::HardeningBase<NumDimT, NumSlipT, DataT, ArgT>;

  std::unique_ptr<HARDENING>
  phardening = 
    CP::hardeningFactory<NumDimT, NumSlipT, DataT, ArgT>(
      slip_systems[0].hardening_law);

  HARDENING &
  hardening = *phardening;
 
  hardening.createLatentMatrix(slip_systems);
  hardening.harden(slip_systems, dt, rate_slip, hardness_n, hardness_np1);

  return;

}




///
/// Update the plastic slips
///
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, typename DataT,
    typename ArgT>
void
CP::updateSlip(
    std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems,
    DataT dt,
    Intrepid2::Vector<ArgT, NumSlipT> const & hardness,
    Intrepid2::Vector<ArgT, NumSlipT> const & shear,
    Intrepid2::Vector<DataT, NumSlipT> const & slip_n,
    Intrepid2::Vector<ArgT, NumSlipT> & slip_np1)
{
  Intrepid2::Index const num_slip_sys = slip_n.get_dimension();

  for (int slip_sys(0); slip_sys < num_slip_sys; ++slip_sys) {

    // Material properties
    DataT const tauC = slip_systems[slip_sys].tau_critical_;
    DataT const m = slip_systems[slip_sys].exponent_rate_;
    DataT const g0 = slip_systems[slip_sys].rate_slip_reference_;

    // Compute slip increment
    ArgT const temp = shear[slip_sys] / (tauC + hardness[slip_sys]);
    slip_np1[slip_sys] = slip_n[slip_sys] + 
      dt * g0 * std::pow(std::fabs(temp), m-1) * temp;

  }

}



///
/// Compute the stresses 
///
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, typename DataT,
    typename ArgT, typename DataS>
void
CP::computeStress(
    std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems,
    Intrepid2::Tensor4<DataS, NumDimT> const & C,
    Intrepid2::Tensor<DataS, NumDimT> const & F,
    Intrepid2::Tensor<DataT, NumDimT> const & Fp,
    Intrepid2::Tensor<ArgT, NumDimT> & sigma,
    Intrepid2::Tensor<ArgT, NumDimT> & S,
    Intrepid2::Vector<ArgT, NumSlipT> & shear)
{
  Intrepid2::Index const num_dim = F.get_dimension();
  Intrepid2::Index const num_slip = shear.get_dimension();

  Intrepid2::Tensor<ArgT, NumDimT> Fe(num_dim);
  Intrepid2::Tensor<ArgT, NumDimT> Ee(num_dim);
  Intrepid2::Tensor<ArgT, NumDimT> Ce(num_dim);

  // Saint Venant–Kirchhoff model
  Fe = F * Intrepid2::inverse(Fp);
  Ce = Intrepid2::transpose(Fe) * Fe;
  Ee = 0.5 * (Ce - Intrepid2::identity<ArgT, NumDimT>(num_dim));
  S = Intrepid2::dotdot(C, Ee);
  sigma = (1.0 / Intrepid2::det(Fe)) * Fe * S * Intrepid2::transpose(Fe);
  CP::confirmTensorSanity<NumDimT>(
      sigma,
      "Cauchy stress in CrystalPlasticityNLS::computeStress()");

  // Compute resolved shear stresses
  for (int s(0); s < num_slip; ++s) {
    shear[s] = Intrepid2::dotdot(slip_systems[s].projector_, Ce * S);
  }
}



//
//! Construct elasticity tensor
//
template<Intrepid2::Index NumDimT, typename DataT, typename ArgT>
void
CP::computeCubicElasticityTensor(
    DataT c11, 
    DataT c12, 
    DataT c44,
    Intrepid2::Tensor4<ArgT, NumDimT> & C)
{

  Intrepid2::Index const num_dims = C.get_dimension();

  C.fill(Intrepid2::ZEROS);

  for (Intrepid2::Index dim_i = 0; dim_i < num_dims; ++dim_i) {
    C(dim_i, dim_i, dim_i, dim_i) = c11;
    for (Intrepid2::Index dim_j = dim_i + 1; dim_j < num_dims; ++dim_j) {
      C(dim_i, dim_i, dim_j, dim_j) = c12;
      C(dim_j, dim_j, dim_i, dim_i) = C(dim_i, dim_i, dim_j, dim_j);
      C(dim_i, dim_j, dim_i, dim_j) = c44;
      C(dim_j, dim_i, dim_j, dim_i) = C(dim_i, dim_j, dim_i, dim_j);
      C(dim_i, dim_j, dim_j, dim_i) = C(dim_i, dim_j, dim_i, dim_j);
      C(dim_j, dim_i, dim_i, dim_j) = C(dim_i, dim_j, dim_i, dim_j);
    }
  }
}


//
// Define nonlinear system based on residual of slip values
//
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, typename EvalT>
CP::CrystalPlasticityNLS<NumDimT, NumSlipT, EvalT>::CrystalPlasticityNLS(
      Intrepid2::Tensor4<ArgT, NumDimT> const & C,
      std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems,
      Intrepid2::Tensor<RealType, NumDimT> const & Fp_n,
      Intrepid2::Vector<RealType, NumSlipT> const & hardness_n,
      Intrepid2::Vector<RealType, NumSlipT> const & slip_n,
      Intrepid2::Tensor<ArgT, NumDimT> const & F_np1,
      RealType dt)
  :
      C_(C), slip_systems_(slip_systems), Fp_n_(Fp_n), hardness_n_(hardness_n),
      slip_n_(slip_n),
      F_np1_(F_np1), dt_(dt)
{
  num_dim_ = Fp_n_.get_dimension();
  num_slip_ = hardness_n_.get_dimension();
}

template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, typename EvalT>
template<typename T, Intrepid2::Index N>
T
CP::CrystalPlasticityNLS<NumDimT, NumSlipT, EvalT>::value(
    Intrepid2::Vector<T, N> const & x)
{
  return Intrepid2::Function_Base<
  CrystalPlasticityNLS<NumDimT, NumSlipT, EvalT>, ArgT>::value(
      *this,
      x);
}

template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, typename EvalT>
template<typename T, Intrepid2::Index N>
Intrepid2::Vector<T, N>
CP::CrystalPlasticityNLS<NumDimT, NumSlipT, EvalT>::gradient(
    Intrepid2::Vector<T, N> const & x)
{
  // Get a convenience reference to the failed flag in case it is used more
  // than once.
  bool &
  failed = Intrepid2::Function_Base<
  CrystalPlasticityNLS<NumDimT, NumSlipT, EvalT>, ArgT>::failed;

  // Tensor mechanical state variables
  Intrepid2::Tensor<T, NumDimT> Fp_np1(num_dim_);
  Intrepid2::Tensor<T, NumDimT> Lp_np1(num_dim_);
  Intrepid2::Tensor<T, NumDimT> sigma_np1(num_dim_);
  Intrepid2::Tensor<T, NumDimT> S_np1(num_dim_);

  // Slip system state variables
  Intrepid2::Vector<T, NumSlipT> hardness_np1(num_slip_);
  Intrepid2::Vector<T, NumSlipT> slip_np1(num_slip_);
  Intrepid2::Vector<T, NumSlipT> slip_computed(num_slip_);
  Intrepid2::Vector<T, NumSlipT> shear_np1(num_slip_);
  Intrepid2::Vector<T, NumSlipT> rate_slip(num_slip_);
  RealType norm_slip_residual_;

  auto const
  num_unknowns = x.get_dimension();

  Intrepid2::Vector<T, N> residual(num_unknowns);

  // Return immediately if something failed catastrophically.
  if (failed == true) {
    return residual;
  }

  Intrepid2::Tensor<T, NumDimT> const
  F_np1_peeled = LCM::peel_tensor<EvalT, T, N, NumDimT>()(F_np1_);

  Intrepid2::Tensor4<T, NumDimT> const
  C_peeled = LCM::peel_tensor4<EvalT, T, N, NumDimT>()(C_);

  for (int i = 0; i< num_slip_; ++i){
    slip_np1[i] = x[i];
  }

  if(dt_ > 0.0){
    rate_slip = (slip_np1 - slip_n_) / dt_;
  }
  else{
    rate_slip.fill(Intrepid2::ZEROS);
  }

  // Ensure that the slip increment is bounded
   if (Intrepid2::norm(rate_slip * dt_) > 1.0) {
       failed =  true;
       return residual;
   }
  // Compute Lp_np1, and Fp_np1
  CP::applySlipIncrement<NumDimT, NumSlipT>(
      slip_systems_,
      dt_,
      slip_n_,
      slip_np1,
      Fp_n_,
      Lp_np1,
      Fp_np1);

  // Compute sigma_np1, S_np1, and shear_np1
  CP::computeStress<NumDimT, NumSlipT>(
      slip_systems_,
      C_peeled,
      F_np1_peeled,
      Fp_np1,
      sigma_np1,
      S_np1,
      shear_np1);

  // Compute hardness_np1
  CP::updateHardness<NumDimT, NumSlipT>(
      slip_systems_,
      dt_,
      rate_slip,
      hardness_n_,
      hardness_np1);

  // Compute slips
  CP::updateSlip<NumDimT, NumSlipT>(
      slip_systems_,
      dt_,
      hardness_np1,
      shear_np1,
      slip_n_,
      slip_computed);

  for (int i = 0; i< num_slip_; ++i){
    residual[i] = slip_np1[i] - slip_computed[i];
  }
  
  return residual;

}

// Nonlinear system, residual based on slip increments
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, typename EvalT>
template<typename T, Intrepid2::Index N>
Intrepid2::Tensor<T, N>
CP::CrystalPlasticityNLS<NumDimT, NumSlipT, EvalT>::hessian(
    Intrepid2::Vector<T, N> const & x)
{
  return Intrepid2::Function_Base<
      CrystalPlasticityNLS<NumDimT, NumSlipT, EvalT>, ArgT>::hessian(
      *this,
      x);
}


//
// Define nonlinear system based on residual of slip and hardness values
//
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, typename EvalT>
CP::ResidualSlipHardnessNLS<NumDimT, NumSlipT, EvalT>::ResidualSlipHardnessNLS(
      Intrepid2::Tensor4<ArgT, NumDimT> const & C,
      std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems,
      Intrepid2::Tensor<RealType, NumDimT> const & Fp_n,
      Intrepid2::Vector<RealType, NumSlipT> const & hardness_n,
      Intrepid2::Vector<RealType, NumSlipT> const & slip_n,
      Intrepid2::Tensor<ArgT, NumDimT> const & F_np1,
      RealType dt)
  :
      C_(C), slip_systems_(slip_systems), Fp_n_(Fp_n), hardness_n_(hardness_n),
      slip_n_(slip_n),
      F_np1_(F_np1), dt_(dt)
{
  num_dim_ = Fp_n_.get_dimension();
  num_slip_ = hardness_n_.get_dimension();
}

template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, typename EvalT>
template<typename T, Intrepid2::Index N>
T
CP::ResidualSlipHardnessNLS<NumDimT, NumSlipT, EvalT>::value(
    Intrepid2::Vector<T, N> const & x)
{
  return Intrepid2::Function_Base<
  ResidualSlipHardnessNLS<NumDimT, NumSlipT, EvalT>, ArgT>::value(
      *this,
      x);
}

template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, typename EvalT>
template<typename T, Intrepid2::Index N>
Intrepid2::Vector<T, N>
CP::ResidualSlipHardnessNLS<NumDimT, NumSlipT, EvalT>::gradient(
    Intrepid2::Vector<T, N> const & x) const
{
  // Tensor mechanical state variables
  Intrepid2::Tensor<T, NumDimT> Fp_np1(num_dim_);
  Intrepid2::Tensor<T, NumDimT> Lp_np1(num_dim_);
  Intrepid2::Tensor<T, NumDimT> sigma_np1(num_dim_);
  Intrepid2::Tensor<T, NumDimT> S_np1(num_dim_);

  // Slip system state variables
  Intrepid2::Vector<T, NumSlipT> hardness_np1(num_slip_);
  Intrepid2::Vector<T, NumSlipT> hardness_computed(num_slip_);
  Intrepid2::Vector<T, NumSlipT> slip_np1(num_slip_);
  Intrepid2::Vector<T, NumSlipT> slip_computed(num_slip_);
  Intrepid2::Vector<T, NumSlipT> shear_np1(num_slip_);
  Intrepid2::Vector<T, NumSlipT> slip_residual(num_slip_);
  Intrepid2::Vector<T, NumSlipT> rate_slip(num_slip_);
  RealType norm_slip_residual_;

  auto const
  num_unknowns = x.get_dimension();

  Intrepid2::Vector<T, N> residual(num_unknowns);

  Intrepid2::Tensor<T, NumDimT> const
  F_np1_peeled = LCM::peel_tensor<EvalT, T, N, NumDimT>()(F_np1_);

  Intrepid2::Tensor4<T, NumDimT> const
  C_peeled = LCM::peel_tensor4<EvalT, T, N, NumDimT>()(C_);

  for (int i = 0; i< num_slip_; ++i){
    slip_np1[i] = x[i];
    hardness_np1[i] = x[i + num_slip_];
  }

  if(dt_ > 0.0){
    rate_slip = (slip_np1 - slip_n_) / dt_;
  }
  else{
    rate_slip.fill(Intrepid2::ZEROS);
  }

  // Compute Lp_np1, and Fp_np1
  CP::applySlipIncrement<NumDimT, NumSlipT>(
      slip_systems_,
      dt_,
      slip_n_,
      slip_np1,
      Fp_n_,
      Lp_np1,
      Fp_np1);
  
  // Compute sigma_np1, S_np1, and shear_np1
  CP::computeStress<NumDimT, NumSlipT>(
      slip_systems_,
      C_peeled,
      F_np1_peeled,
      Fp_np1,
      sigma_np1,
      S_np1,
      shear_np1);

  // Compute hardness_np1
  CP::updateHardness<NumDimT, NumSlipT>(
      slip_systems_,
      dt_,
      rate_slip,
      hardness_n_,
      hardness_computed);

  // Compute slips
  CP::updateSlip<NumDimT, NumSlipT>(
      slip_systems_,
      dt_,
      hardness_computed,
      shear_np1,
      slip_n_,
      slip_computed);

  for (int i = 0; i< num_slip_; ++i){
    residual[i] = slip_np1[i] - slip_computed[i];
    residual[i + num_slip_] = hardness_np1[i] - hardness_computed[i];
  }
  
  return residual;
}

// Nonlinear system, residual based on slip increments and hardnesses
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, typename EvalT>
template<typename T, Intrepid2::Index N>
Intrepid2::Tensor<T, N>
CP::ResidualSlipHardnessNLS<NumDimT, NumSlipT, EvalT>::hessian(
    Intrepid2::Vector<T, N> const & x)
{
  return Intrepid2::Function_Base<
      ResidualSlipHardnessNLS<NumDimT, NumSlipT, EvalT>, ArgT>::hessian(
      *this,
      x);
}

//
// Linear hardening with recovery
//
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, 
  typename DataT, typename ArgT>
void
CP::LinearMinusRecoveryHardening<NumDimT, NumSlipT, DataT, ArgT>::
createLatentMatrix(
  std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems)
{
  Intrepid2::Index const num_slip_sys = slip_systems.size();
  latent_matrix.set_dimension(num_slip_sys);
  latent_matrix.fill(Intrepid2::ONES);

  return;
}

//
// 
//
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, 
  typename DataT, typename ArgT>
void
CP::LinearMinusRecoveryHardening<NumDimT, NumSlipT, DataT, ArgT>::
harden(
  std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems,
  DataT dt,
  Intrepid2::Vector<ArgT, NumSlipT> const & rate_slip,
  Intrepid2::Vector<DataT, NumSlipT> const & hardness_n,
  Intrepid2::Vector<ArgT, NumSlipT> & hardness_np1)
{
  Intrepid2::Index const num_slip_sys = hardness_n.get_dimension();

  Intrepid2::Vector<ArgT, NumSlipT> rate_slip_abs(num_slip_sys);
  for (int slip_sys(0); slip_sys < num_slip_sys; ++slip_sys) {
    rate_slip_abs(slip_sys) = std::fabs(rate_slip(slip_sys));
  }

  Intrepid2::Vector<ArgT, NumSlipT> const 
    driver_hardening = latent_matrix * rate_slip_abs;

  for (int slip_sys(0); slip_sys < num_slip_sys; ++slip_sys) {

    RealType const H = slip_systems[slip_sys].H_;
    RealType const Rd = slip_systems[slip_sys].Rd_;  

    if (Rd > 0.0) {
      RealType const 
        effective_slip_n = -1.0/Rd * std::log(1.0 - Rd/H * hardness_n[slip_sys]);
      hardness_np1[slip_sys] = H / Rd * (1.0 - 
        std::exp(-Rd * (effective_slip_n + dt * driver_hardening[slip_sys])));  
    }
    else {
      hardness_np1[slip_sys] = 
        hardness_n[slip_sys] + H * dt * driver_hardening[slip_sys];
    }

//    hardness_np1[slip_sys] = hardness_n[slip_sys] +
//      dt * (H - Rd * (hardness_n[slip_sys])) * driver_hardening[slip_sys];

  } 

  return;
}

//
// Saturation hardening
//
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, 
  typename DataT, typename ArgT>
void
CP::SaturationHardening<NumDimT, NumSlipT, DataT, ArgT>::
createLatentMatrix(
  std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems)
{
  Intrepid2::Index const num_slip_sys = slip_systems.size();
  latent_matrix.set_dimension(num_slip_sys);

  for (int slip_sys_i(0); slip_sys_i < num_slip_sys; ++slip_sys_i) {

    for (int slip_sys_j(0); slip_sys_j < num_slip_sys; ++slip_sys_j) {

      latent_matrix(slip_sys_i, slip_sys_j) = 
        std::fabs(Intrepid2::dotdot(
          Intrepid2::sym(slip_systems[slip_sys_i].projector_),
          Intrepid2::sym(slip_systems[slip_sys_j].projector_)));

    }

  }

  return;
}

//
// 
//
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, 
  typename DataT, typename ArgT>
void
CP::SaturationHardening<NumDimT, NumSlipT, DataT, ArgT>::
harden(
  std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems,
  DataT dt,
  Intrepid2::Vector<ArgT, NumSlipT> const & rate_slip,
  Intrepid2::Vector<DataT, NumSlipT> const & hardness_n,
  Intrepid2::Vector<ArgT, NumSlipT> & hardness_np1)
{
  Intrepid2::Index const num_slip_sys = hardness_n.get_dimension();

  Intrepid2::Vector<ArgT, NumSlipT> rate_slip_abs(num_slip_sys);
  for (int slip_sys(0); slip_sys < num_slip_sys; ++slip_sys) {
    rate_slip_abs(slip_sys) = std::fabs(rate_slip(slip_sys));
  }

  Intrepid2::Vector<ArgT, NumSlipT> const 
    driver_hardening = latent_matrix * rate_slip_abs;

  for (int slip_sys(0); slip_sys < num_slip_sys; ++slip_sys) {

    DataT const rate_slip_reference = 
      slip_systems[slip_sys].rate_slip_reference_;
    DataT const stress_saturation_initial = 
      slip_systems[slip_sys].stress_saturation_initial_;
    DataT const rate_hardening = 
      slip_systems[slip_sys].rate_hardening_;
    DataT const resistance_slip_initial = 
      slip_systems[slip_sys].resistance_slip_initial_;
    DataT const exponent_saturation = 
      slip_systems[slip_sys].exponent_saturation_;  

    ArgT effective_slip_rate{0.0};

    // calculate effective slip increment
    for (int slip_sys(0); slip_sys < num_slip_sys; ++slip_sys) {
      effective_slip_rate += fabs(rate_slip[slip_sys]);
    }

    ArgT stress_saturation{stress_saturation_initial};

    if (exponent_saturation > 0.0) {
      stress_saturation = stress_saturation_initial * std::pow(
        effective_slip_rate / rate_slip_reference, exponent_saturation);
    }

    hardness_np1[slip_sys] = hardness_n[slip_sys] + dt * rate_hardening *
      (stress_saturation - hardness_n[slip_sys] - resistance_slip_initial) / 
      (stress_saturation - resistance_slip_initial) * driver_hardening[slip_sys];

  } 

  return;
}

//
// Dislocation-density based hardening
//
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, 
  typename DataT, typename ArgT>
void
CP::DislocationDensityHardening<NumDimT, NumSlipT, DataT, ArgT>::
createLatentMatrix(
  std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems)
{
  Intrepid2::Index const num_slip_sys = slip_systems.size();
  latent_matrix.set_dimension(num_slip_sys);
  latent_matrix.fill(Intrepid2::ZEROS);

  return;
}

//
// 
//
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, 
  typename DataT, typename ArgT>
void
CP::DislocationDensityHardening<NumDimT, NumSlipT, DataT, ArgT>::
harden(
  std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems,
  DataT dt,
  Intrepid2::Vector<ArgT, NumSlipT> const & rate_slip,
  Intrepid2::Vector<DataT, NumSlipT> const & hardness_n,
  Intrepid2::Vector<ArgT, NumSlipT> & hardness_np1)
{
  Intrepid2::Index const num_slip_sys = hardness_n.get_dimension();
  
  for (int slip_sys(0); slip_sys < num_slip_sys; ++slip_sys) {
    hardness_np1[slip_sys] = hardness_n[slip_sys];
  }
}

//
// No hardening
//
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, 
  typename DataT, typename ArgT>
void
CP::NoHardening<NumDimT, NumSlipT, DataT, ArgT>::
createLatentMatrix(
  std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems)
{
  Intrepid2::Index const num_slip_sys = slip_systems.size();
  latent_matrix.set_dimension(num_slip_sys);
  latent_matrix.fill(Intrepid2::ZEROS);

  return;
}

//
// 
//
template<Intrepid2::Index NumDimT, Intrepid2::Index NumSlipT, 
  typename DataT, typename ArgT>
void
CP::NoHardening<NumDimT, NumSlipT, DataT, ArgT>::
harden(
  std::vector<CP::SlipSystemStruct<NumDimT, NumSlipT> > const & slip_systems,
  DataT dt,
  Intrepid2::Vector<ArgT, NumSlipT> const & rate_slip,
  Intrepid2::Vector<DataT, NumSlipT> const & hardness_n,
  Intrepid2::Vector<ArgT, NumSlipT> & hardness_np1)
{
  Intrepid2::Index const num_slip_sys = hardness_n.get_dimension();
  
  for (int slip_sys(0); slip_sys < num_slip_sys; ++slip_sys) {
    hardness_np1[slip_sys] = hardness_n[slip_sys];
  }
}
