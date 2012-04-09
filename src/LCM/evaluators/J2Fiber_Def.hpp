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

#include "Intrepid_FunctionSpaceTools.hpp"
#include <typeinfo>
namespace LCM {

//**********************************************************************
template<typename EvalT, typename Traits>
J2Fiber<EvalT, Traits>::
J2Fiber(const Teuchos::ParameterList& p) :
  defgrad          (p.get<std::string>                   ("DefGrad Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout") ),
  J                (p.get<std::string>                   ("DetDefGrad Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  elasticModulus   (p.get<std::string>                   ("Elastic Modulus Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  poissonsRatio    (p.get<std::string>                   ("Poissons Ratio Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  yieldStrength    (p.get<std::string>                   ("Yield Strength Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  hardeningModulus (p.get<std::string>                   ("Hardening Modulus Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  satMod           (p.get<std::string>                   ("Saturation Modulus Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  satExp           (p.get<std::string>                   ("Saturation Exponent Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  stress           (p.get<std::string>                   ("Stress Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout") ),
  Fp               (p.get<std::string>                   ("Fp Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout") ),
  eqps             (p.get<std::string>                   ("Eqps Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  energy_J2         (p.get<std::string>                   ("Energy_J2 Name"),
				p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  energy_f1        (p.get<std::string>                   ("Energy_f1 Name"),
				p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  energy_f2        (p.get<std::string>                   ("Energy_f2 Name"),
				p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  xiinf_J2          (p.get<RealType>("xiinf_J2 Name")),
  tau_J2            (p.get<RealType>("tau_J2 Name")),
  k_f1          	(p.get<RealType>("k_f1 Name")),
  q_f1          	(p.get<RealType>("q_f1 Name")),
  vol_f1          	(p.get<RealType>("vol_f1 Name")),
  xiinf_f1          (p.get<RealType>("xiinf_f1 Name")),
  tau_f1          	(p.get<RealType>("tau_f1 Name")),
  Mx_f1          	(p.get<RealType>("Mx_f1 Name")),
  My_f1          	(p.get<RealType>("My_f1 Name")),
  Mz_f1          	(p.get<RealType>("Mz_f1 Name")),
  k_f2          	(p.get<RealType>("k_f2 Name")),
  q_f2          	(p.get<RealType>("q_f2 Name")),
  vol_f2          	(p.get<RealType>("vol_f2 Name")),
  xiinf_f2          (p.get<RealType>("xiinf_f2 Name")),
  tau_f2          	(p.get<RealType>("tau_f2 Name")),
  Mx_f2          	(p.get<RealType>("Mx_f2 Name")),
  My_f2          	(p.get<RealType>("My_f2 Name")),
  Mz_f2          	(p.get<RealType>("Mz_f2 Name"))
{
  // Pull out numQPs and numDims from a Layout
  Teuchos::RCP<PHX::DataLayout> tensor_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  tensor_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];
  worksetSize = dims[0];

  this->addDependentField(defgrad);
  this->addDependentField(J);
  this->addDependentField(elasticModulus);
  this->addDependentField(poissonsRatio);
  this->addDependentField(yieldStrength);
  this->addDependentField(hardeningModulus);
  this->addDependentField(satMod);  
  this->addDependentField(satExp);
  // PoissonRatio not used in 1D stress calc
  //  if (numDims>1) this->addDependentField(poissonsRatio);

  fpName = p.get<std::string>("Fp Name")+"_old";
  eqpsName = p.get<std::string>("Eqps Name")+"_old";

  energy_J2Name = p.get<std::string>("Energy_J2 Name")+"_old";
  energy_f1Name = p.get<std::string>("Energy_f1 Name")+"_old";
  energy_f2Name = p.get<std::string>("Energy_f2 Name")+"_old";

  this->addEvaluatedField(stress);
  this->addEvaluatedField(Fp);
  this->addEvaluatedField(eqps);

  this->addEvaluatedField(energy_J2);
  this->addEvaluatedField(energy_f1);
  this->addEvaluatedField(energy_f2);

  // scratch space FCs
//  be.resize(numDims, numDims);
//  s.resize(numDims, numDims);
//  N.resize(numDims, numDims);
//  A.resize(numDims, numDims);
//  expA.resize(numDims, numDims);
  Fpinv.resize(worksetSize, numQPs, numDims, numDims);
  FpinvT.resize(worksetSize, numQPs, numDims, numDims);
  Cpinv.resize(worksetSize, numQPs, numDims, numDims);

  this->setName("Stress"+PHX::TypeString<EvalT>::value);

}

//**********************************************************************
template<typename EvalT, typename Traits>
void J2Fiber<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(stress,fm);
  this->utils.setFieldData(defgrad,fm);
  this->utils.setFieldData(J,fm);
  this->utils.setFieldData(elasticModulus,fm);
  this->utils.setFieldData(hardeningModulus,fm);
  this->utils.setFieldData(yieldStrength,fm);
  this->utils.setFieldData(satMod,fm);
  this->utils.setFieldData(satExp,fm);
  this->utils.setFieldData(Fp,fm);
  this->utils.setFieldData(eqps,fm);

  this->utils.setFieldData(energy_J2,fm);
  this->utils.setFieldData(energy_f1,fm);
  this->utils.setFieldData(energy_f2,fm);

  if (numDims>1) this->utils.setFieldData(poissonsRatio,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void J2Fiber<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  typedef Intrepid::FunctionSpaceTools FST;
  typedef Intrepid::RealSpaceTools<ScalarT> RST;

  ScalarT kappa;
  ScalarT mu, mubar;
  ScalarT K, Y, siginf, delta;
  ScalarT Jm23;
  ScalarT trace, trd3;
  ScalarT smag, f, p, dgam;
  ScalarT sq23 = std::sqrt(2./3.);

  ScalarT alpha_J2, alpha_f1, alpha_f2;
  ScalarT xi_J2, xi_f1, xi_f2;

  // previous state
  Albany::MDArray Fpold = (*workset.stateArrayPtr)[fpName];
  Albany::MDArray eqpsold = (*workset.stateArrayPtr)[eqpsName];
  Albany::MDArray energy_J2old = (*workset.stateArrayPtr)[energy_J2Name];
  Albany::MDArray energy_f1old = (*workset.stateArrayPtr)[energy_f1Name];
  Albany::MDArray energy_f2old = (*workset.stateArrayPtr)[energy_f2Name];

//  // compute Cp_{n}^{-1}
//  // AGS MAY NEED TO ALLICATE Fpinv FpinvT Cpinv  with actual workse size
//  // to prevent going past the end of Fpold.
  RST::inverse(Fpinv, Fpold);
  RST::transpose(FpinvT, Fpinv);
  FST::tensorMultiplyDataData<ScalarT>(Cpinv, Fpinv, FpinvT);

  for (std::size_t cell=0; cell < workset.numCells; ++cell)
  {
    for (std::size_t qp=0; qp < numQPs; ++qp)
    {
 // local parameters
      kappa  = elasticModulus(cell,qp) / ( 3. * ( 1. - 2. * poissonsRatio(cell,qp) ) );
      mu     = elasticModulus(cell,qp) / ( 2. * ( 1. + poissonsRatio(cell,qp) ) );
      K      = hardeningModulus(cell,qp);
      Y      = yieldStrength(cell,qp);
      siginf = satMod(cell,qp);
      delta  = satExp(cell,qp);
      Jm23   = std::pow( J(cell,qp), -2./3. );

      be.clear();
      // Compute Trial State
      for (std::size_t i=0; i < numDims; ++i)
    	  for (std::size_t j=0; j < numDims; ++j)
    		  for (std::size_t p=0; p < numDims; ++p)
    			  for (std::size_t q=0; q < numDims; ++q)
    				  be(i,j) += Jm23 * defgrad(cell,qp,i,p) * Cpinv(cell,qp,p,q) * defgrad(cell,qp,j,q);

      trace = LCM::trace(be);
      trd3 = trace / numDims;
      mubar = trd3*mu;

      s = mu*(be - trd3 * eye<ScalarT>());

      // check for yielding
      smag = LCM::norm(s);

      f = smag - sq23 * ( Y + K * eqpsold(cell,qp) + siginf * ( 1. - std::exp( -delta * eqpsold(cell,qp) ) ) );

      if (f > 1E-12)
      {
		// return mapping algorithm
		bool converged = false;
		ScalarT g = f;
		ScalarT H = K * eqpsold(cell,qp) + siginf*( 1. - std::exp( -delta * eqpsold(cell,qp) ) );
		ScalarT dg = ( -2. * mubar ) * ( 1. + H / ( 3. * mubar ) );
		ScalarT dH = 0.0;
		ScalarT alpha = 0.0;
		ScalarT res = 0.0;
		int count = 0;
		dgam = 0.0;

		while (!converged)
		{
		  count++;

		  dgam -= g/dg;

		  alpha = eqpsold(cell,qp) + sq23 * dgam;

		  H = K * alpha + siginf*( 1. - std::exp( -delta * alpha ) );
		  dH = K + delta * siginf * std::exp( -delta * alpha );

		  g = smag -  ( 2. * mubar * dgam + sq23 * ( Y + H ) );
		  dg = -2. * mubar * ( 1. + dH / ( 3. * mubar ) );

		  res = std::abs(g);
		  if ( res < 1.e-11 || res/f < 1.E-11 )
			converged = true;

		  TEUCHOS_TEST_FOR_EXCEPTION( count > 20, std::runtime_error,
						  std::endl << "Error in return mapping, count = " << count <<
										  "\nres = " << res <<
										  "\nrelres = " << res/f <<
										  "\ng = " << g <<
										  "\ndg = " << dg <<
										  "\nalpha = " << alpha << std::endl);

			}

        // plastic direction
        N = ScalarT(1./smag) * s;

        s -= ScalarT(2. * mubar * dgam) * N;

        // update eqps
        eqps(cell,qp) = alpha;

        // exponential map to get Fp
        A = dgam * N;
        expA = LCM::exp<ScalarT>(A);

        for (std::size_t i=0; i < numDims; ++i)
        {
          for (std::size_t j=0; j < numDims; ++j)
          {
            Fp(cell,qp,i,j) = 0.0;
            for (std::size_t p=0; p < numDims; ++p)
            {
              Fp(cell,qp,i,j) += expA(i,p) * Fpold(cell,qp,p,j);
            }
          }
        }
      }
      else
      {
        // set state variables to old values
        eqps(cell, qp) = eqpsold(cell,qp);
        for (std::size_t i=0; i < numDims; ++i)
          for (std::size_t j=0; j < numDims; ++j)
            Fp(cell,qp,i,j) = Fpold(cell,qp,i,j);
      }

      // compute pressure
      p = 0.5 * kappa * ( J(cell,qp) - 1. / ( J(cell,qp) ) );

      // compute stress
      for (std::size_t i=0; i < numDims; ++i)
      {
        for (std::size_t j=0; j < numDims; ++j)
        {
          stress(cell,qp,i,j) = s(i,j) / J(cell,qp);
        }
        stress(cell,qp,i,i) += p;
      }

      // update be
      be = ScalarT(1./mu) * s + trd3 * eye<ScalarT>();
      // compute energy for J2 stress
      energy_J2(cell,qp) = 0.5*kappa*(0.5*(J(cell,qp)*J(cell,qp)-1.0)-std::log(J(cell,qp)))
	+ 0.5*mu*(trace - 3.0);

      // damage term in J2.
      alpha_J2 = energy_J2old(cell,qp);
      if(energy_J2(cell,qp) > alpha_J2)
    	  alpha_J2 = energy_J2(cell,qp);

      xi_J2 = xiinf_J2 * (1 - std::exp(-alpha_J2/tau_J2));

//-----------compute stress in Fibers

      LCM::Tensor<ScalarT> F;
      for (std::size_t i=0; i < numDims; ++i)
        for (std::size_t j=0; j < numDims; ++j)
        	F(i,j) = defgrad(cell,qp,i,j);

      LCM::Tensor<ScalarT> C = LCM::dot(LCM::transpose(F), F);
      ScalarT Mx1 = ScalarT(Mx_f1), My1 = ScalarT(My_f1), Mz1 = ScalarT(Mz_f1);
      ScalarT Mx2 = ScalarT(Mx_f2), My2 = ScalarT(My_f2), Mz2 = ScalarT(Mz_f2);

      LCM::Vector<ScalarT> M1(Mx1, My1, Mz1);
      LCM::Vector<ScalarT> M2(Mx2, My2, Mz2);

      //LCM::Vector<ScalarT> CdotM1 = LCM::dot(C,M1);
	  ScalarT I4_f1 = LCM::dot(M1, LCM::dot(C, M1));
	  ScalarT I4_f2 = LCM::dot(M2, LCM::dot(C, M2));
	  LCM::Tensor<ScalarT> M1dyadM1 = dyad(M1,M1);
	  LCM::Tensor<ScalarT> M2dyadM2 = dyad(M2,M2);

	  // undamaged stress (2nd PK stress)
	  LCM::Tensor<ScalarT> S0_f1, S0_f2;
	  S0_f1 = ScalarT(4.0 * k_f1 * (I4_f1 - 1.0) * std::exp(q_f1 * (I4_f1 - 1) * (I4_f1 - 1))) * M1dyadM1;
	  S0_f2 = ScalarT(4.0 * k_f2 * (I4_f2 - 1.0) * std::exp(q_f2 * (I4_f2 - 1) * (I4_f2 - 1))) * M2dyadM2;

	  // compute energy for fibers
	  energy_f1(cell,qp) = k_f1 * (std::exp(q_f1 * (I4_f1 - 1) * (I4_f1 - 1)) - 1) / q_f1;
	  energy_f2(cell,qp) = k_f2 * (std::exp(q_f2 * (I4_f2 - 1) * (I4_f2 - 1)) - 1) / q_f2;

	  // Cauchy stress
	  LCM::Tensor<ScalarT> stress_f1, stress_f2;
	  stress_f1 = ScalarT(1.0/J(cell,qp)) * LCM::dot(F, LCM::dot(S0_f1, LCM::transpose(F)));
	  stress_f2 = ScalarT(1.0/J(cell,qp)) * LCM::dot(F, LCM::dot(S0_f2, LCM::transpose(F)));

	  // maximum thermodynamic forces
      alpha_f1 = energy_f1old(cell,qp);
      alpha_f2 = energy_f2old(cell,qp);

      if(energy_f1(cell,qp) > alpha_f1)
    	  alpha_f1 = energy_f1(cell,qp);

      if(energy_f2(cell,qp) > alpha_f2)
    	  alpha_f2 = energy_f2(cell,qp);

	  // damage term in fibers
      xi_f1 = xiinf_f1 * (1 - std::exp(-alpha_f1/tau_f1));
      xi_f2 = xiinf_f2 * (1 - std::exp(-alpha_f2/tau_f2));

	  // total Cauchy stress (J2, Fibers)
      for (std::size_t i=0; i < numDims; ++i)
      {
        for (std::size_t j=0; j < numDims; ++j)
        {
          stress(cell,qp,i,j) = (1-vol_f1-vol_f2) * (1-xi_J2) * stress(cell, qp, i, j)
        		  + vol_f1 * (1-xi_f1) * stress_f1(i,j)
        		  + vol_f2 * (1-xi_f2) * stress_f2(i,j);
        }
      }

    }// end of loop over qp
  }// end of loop over cell

  // Since Intrepid will later perform calculations on the entire workset size
  // and not just the used portion, we must fill the excess with reasonable
  // values. Leaving this out leads to inversion of 0 tensors.
  for (std::size_t cell=workset.numCells; cell < worksetSize; ++cell)
    for (std::size_t qp=0; qp < numQPs; ++qp)
      for (std::size_t i=0; i < numDims; ++i)
          Fp(cell,qp,i,i) = 1.0;
}
//**********************************************************************

//**********************************************************************
} // end LCM

