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

namespace LCM {

//**********************************************************************
template<typename EvalT, typename Traits>
J2Stress<EvalT, Traits>::
J2Stress(const Teuchos::ParameterList& p) :
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
  stress           (p.get<std::string>                   ("Stress Name"),
	            p.get<Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout") )
{
  // Pull out numQPs and numDims from a Layout
  Teuchos::RCP<PHX::DataLayout> tensor_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  tensor_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];
  //int worksetSize = dims[0];
  std::size_t worksetSize = dims[0];

  this->addDependentField(defgrad);
  this->addDependentField(J);
  this->addDependentField(elasticModulus);
  this->addDependentField(poissonsRatio);
  this->addDependentField(yieldStrength);
  this->addDependentField(hardeningModulus);
  // PoissonRatio not used in 1D stress calc
  //  if (numDims>1) this->addDependentField(poissonsRatio);

  this->addEvaluatedField(stress);

  // scratch space FCs
  be.resize(numDims, numDims);
  s.resize(numDims, numDims);
  N.resize(numDims, numDims);
  A.resize(numDims, numDims);
  expA.resize(numDims, numDims);
  Fp.resize(worksetSize, numQPs, numDims, numDims);
  Fpinv.resize(worksetSize, numQPs, numDims, numDims);
  FpinvT.resize(worksetSize, numQPs, numDims, numDims);
  Cpinv.resize(worksetSize, numQPs, numDims, numDims);
  eqps.resize(worksetSize, numQPs);
  tmp.resize(numDims, numDims);
  tmp2.resize(numDims, numDims);

  this->setName("Stress"+PHX::TypeString<EvalT>::value);

}

//**********************************************************************
template<typename EvalT, typename Traits>
void J2Stress<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(stress,fm);
  this->utils.setFieldData(defgrad,fm);
  this->utils.setFieldData(J,fm);
  this->utils.setFieldData(elasticModulus,fm);
  this->utils.setFieldData(hardeningModulus,fm);
  this->utils.setFieldData(yieldStrength,fm);
  if (numDims>1) this->utils.setFieldData(poissonsRatio,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void J2Stress<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  typedef Intrepid::FunctionSpaceTools FST;
  typedef Intrepid::RealSpaceTools<ScalarT> RST;

  ScalarT kappa;
  ScalarT mu, mubar;
  ScalarT K;
  ScalarT Y;
  ScalarT Jm23;
  ScalarT trace;
  ScalarT smag2, smag, f, p, dgam;

  //bool saveState = (workset.newState != Teuchos::null);
  //std::cout << "saveState: " << saveState << std::endl;
  //saveState = (workset.oldState != Teuchos::null);
  //std::cout << "saveState: " << saveState << std::endl;
  Albany::StateVariables& newState = *workset.newState;
  Albany::StateVariables  oldState = *workset.oldState;
  Intrepid::FieldContainer<RealType>& Fpold   = *oldState["Fp"];
  Intrepid::FieldContainer<RealType>& eqpsold = *oldState["eqps"];
  Intrepid::FieldContainer<RealType>& FP      = *newState["Fp"];
  Intrepid::FieldContainer<RealType>& EQPS    = *newState["eqps"];
  Intrepid::FieldContainer<RealType>& STRESS  = *newState["stress"];

  // compute Cp_{n}^{-1}
  RST::inverse(Fpinv, Fpold);
  RST::transpose(FpinvT, Fpinv);
  FST::tensorMultiplyDataData<ScalarT>(Cpinv, Fpinv, FpinvT);

  // std::cout << "F:\n";
  // for (std::size_t cell=0; cell < workset.numCells; ++cell)
  // {
  //   for (std::size_t qp=0; qp < numQPs; ++qp)
  //   {
  //     for (std::size_t i=0; i < numDims; ++i)
  // 	for (std::size_t j=0; j < numDims; ++j)
  // 	  std::cout << Sacado::ScalarValue<ScalarT>::eval(defgrad(cell,qp,i,j)) << " ";
  //   }
  //   std::cout << std::endl;      
  // }
  // std::cout << std::endl;      

  // std::cout << "Fpold:\n";
  // for (std::size_t cell=0; cell < workset.numCells; ++cell)
  // {
  //   for (std::size_t qp=0; qp < numQPs; ++qp)
  //   {
  //     for (std::size_t i=0; i < numDims; ++i)
  // 	for (std::size_t j=0; j < numDims; ++j)
  // 	  std::cout << Sacado::ScalarValue<ScalarT>::eval(Fpold(cell,qp,i,j)) << " ";
  //   }
  //   std::cout << std::endl;      
  // }
  // std::cout << std::endl;      
    
  // std::cout << "Cpinv:\n";
  // for (std::size_t cell=0; cell < workset.numCells; ++cell)
  // {
  //   for (std::size_t qp=0; qp < numQPs; ++qp)
  //   {
  //     for (std::size_t i=0; i < numDims; ++i)
  // 	for (std::size_t j=0; j < numDims; ++j)
  // 	  std::cout << Sacado::ScalarValue<ScalarT>::eval(Cpinv(cell,qp,i,j)) << " ";
  //   }
  //   std::cout << std::endl;      
  // }
  // std::cout << std::endl;      


  for (std::size_t cell=0; cell < workset.numCells; ++cell) 
  {
    for (std::size_t qp=0; qp < numQPs; ++qp) 
    {
      // local parameters
      kappa = elasticModulus(cell,qp) / ( 3. * ( 1. - 2. * poissonsRatio(cell,qp) ) );
      mu    = elasticModulus(cell,qp) / ( 2. * ( 1. + poissonsRatio(cell,qp) ) );
      K     = hardeningModulus(cell,qp);
      Y     = yieldStrength(cell,qp);
      Jm23  = std::pow( J(cell,qp), -2./3. );

      // std::cout << "kappa: " << Sacado::ScalarValue<ScalarT>::eval(kappa) << std::endl;
      // std::cout << "mu   : " << Sacado::ScalarValue<ScalarT>::eval(mu) << std::endl;
      // std::cout << "K    : " << Sacado::ScalarValue<ScalarT>::eval(K) << std::endl;
      // std::cout << "Y    : " << Sacao::ScalarValue<ScalarT>::eval(Y) << std::endl;
      be.initialize(0.0);
      // Compute Trial State      
      for (std::size_t i=0; i < numDims; ++i)
      {	
	for (std::size_t j=0; j < numDims; ++j)
	{
	  for (std::size_t p=0; p < numDims; ++p)
	  {
	    for (std::size_t q=0; q < numDims; ++q)
	    {
	      be(i,j) += Jm23 * defgrad(cell,qp,i,p) * Cpinv(cell,qp,p,q) * defgrad(cell,qp,j,q);
	    }
	  }
	}
      } 
      
      // std::cout << "be: \n" << be;
      
      trace = 0.0;
      for (std::size_t i=0; i < numDims; ++i)
	trace += be(i,i);
      trace /= numDims;
      mubar = trace*mu;
      for (std::size_t i=0; i < numDims; ++i)
      {	
	for (std::size_t j=0; j < numDims; ++j)
	{
	  s(i,j) = mu * be(i,j);
	}
	s(i,i) -= mu * trace;
      }	  
      
      // std::cout << "s: \n" << s;

      // check for yielding
      // smag = s.norm();
      smag2 = 0.0;
      for (std::size_t i=0; i < numDims; ++i)	
	for (std::size_t j=0; j < numDims; ++j)
	  smag2 += s(i,j) * s(i,j);
      smag = std::sqrt(smag2);
      
      f = smag - sqrt(2./3.)*( K * eqpsold(cell,qp) + Y );

      // std::cout << "smag : " << Sacado::ScalarValue<ScalarT>::eval(smag) << std::endl;
      // std::cout << "eqpsold: " << Sacado::ScalarValue<ScalarT>::eval(eqpsold(cell,qp)) << std::endl;
      // std::cout << "K      : " << Sacado::ScalarValue<ScalarT>::eval(K) << std::endl;
      // std::cout << "Y      : " << Sacado::ScalarValue<ScalarT>::eval(Y) << std::endl;
      // std::cout << "f      : " << Sacado::ScalarValue<ScalarT>::eval(f) << std::endl;

      if (f > 1E-12)
      {
	// return mapping algorithm
	dgam = ( f / ( 2. * mubar) ) / ( 1. + K / ( 3. * mubar ) );

	// plastic direction
	for (std::size_t i=0; i < numDims; ++i)	
	  for (std::size_t j=0; j < numDims; ++j)
	    N(i,j) = (1/smag) * s(i,j);

	for (std::size_t i=0; i < numDims; ++i)	
	  for (std::size_t j=0; j < numDims; ++j)
	    s(i,j) -= 2. * mubar * dgam * N(i,j);

	// update eqps
	eqps(cell,qp) = eqpsold(cell,qp) + sqrt(2./3.) * dgam;

	// exponential map to get Fp
	for (std::size_t i=0; i < numDims; ++i)	
	  for (std::size_t j=0; j < numDims; ++j)
	    A(i,j) = dgam * N(i,j);

	exponential_map(expA, A);

	// std::cout << "expA: \n";
	// for (std::size_t i=0; i < numDims; ++i)	
	//   for (std::size_t j=0; j < numDims; ++j)
	//     std::cout << Sacado::ScalarValue<ScalarT>::eval(expA(i,j)) << " ";
	// std::cout << std::endl;
		  
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
      p = kappa * ( J(cell,qp) - 1 / ( J(cell,qp) ) );
      
      // compute stress
      for (std::size_t i=0; i < numDims; ++i)	
      {
	for (std::size_t j=0; j < numDims; ++j)
	{
	  stress(cell,qp,i,j) = s(i,j) / J(cell,qp);
	}
	stress(cell,qp,i,i) += p;
      }
    }
  }

  // std::cout << "Fp: \n";
  // Save Stress as State Variable
  for (std::size_t cell=0; cell < workset.numCells; ++cell) 
  {
    for (std::size_t qp=0; qp < numQPs; ++qp) 
    {
      EQPS(cell,qp) = Sacado::ScalarValue<ScalarT>::eval(eqps(cell,qp));
      for (std::size_t i=0; i < numDims; ++i)
      {
	for (std::size_t j=0; j < numDims; ++j)
	{
	  STRESS(cell,qp,i,j) = Sacado::ScalarValue<ScalarT>::eval(stress(cell,qp,i,j));
	  FP(cell,qp,i,j) = Sacado::ScalarValue<ScalarT>::eval(Fp(cell,qp,i,j));
	  // std::cout << FP(cell,qp,i,j) << " ";
	}
      }
      // std::cout << std::endl;
    }
    // std::cout << std::endl;
  }
}
//**********************************************************************
template<typename EvalT, typename Traits>
void 
J2Stress<EvalT, Traits>::exponential_map(Intrepid::FieldContainer<ScalarT> & expA, const Intrepid::FieldContainer<ScalarT> A)
{
  tmp.initialize(0.0);
  expA.initialize(0.0);

  bool converged = false;
  ScalarT norm0 = norm(A);

  for (std::size_t i=0; i < numDims; ++i)
  {
    tmp(i,i) = 1.0;
  }

  ScalarT k = 0.0;
  while (!converged)
  {
    // expA += tmp
    for (std::size_t i=0; i < numDims; ++i)
      for (std::size_t j=0; j < numDims; ++j)
	expA(i,j) += tmp(i,j);

    tmp2.initialize(0.0);
    for (std::size_t i=0; i < numDims; ++i)
      for (std::size_t j=0; j < numDims; ++j)
	for (std::size_t p=0; p < numDims; ++p)
	  tmp2(i,j) += A(i,p) * tmp(p,j);

    // tmp = tmp2
    k = k + 1.0;
    for (std::size_t i=0; i < numDims; ++i)
      for (std::size_t j=0; j < numDims; ++j)
	tmp(i,j) = (1/k) * tmp2(i,j);

    if (norm(tmp)/norm0 < 1.E-14 ) converged = true;
    
    TEST_FOR_EXCEPTION( k > 50.0, std::logic_error,
			std::endl << "Error in exponential map, k = " << k << std::endl);
    
  }
}
//**********************************************************************
template<typename EvalT, typename Traits>
typename EvalT::ScalarT 
J2Stress<EvalT, Traits>::norm(Intrepid::FieldContainer<ScalarT> A)
{
  ScalarT max(0.0), colsum;

  for (std::size_t i(0); i < numDims; ++i)
  {
    colsum = 0.0;
    for (std::size_t j(0); j < numDims; ++j)
      colsum += A(i,j);
    max = (colsum > max) ? colsum : max;  
  }

  return max;
}
//**********************************************************************
} // end LCM

