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


#include <fstream>
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Sacado_ParameterRegistration.hpp"
#include "Albany_Utils.hpp"

namespace LCM {

template<typename EvalT, typename Traits>
BiotModulus<EvalT, Traits>::
BiotModulus(Teuchos::ParameterList& p) :
  biotModulus(p.get<std::string>("Biot Modulus Name"),
		 p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout"))
{
  Teuchos::ParameterList* elmd_list = 
    p.get<Teuchos::ParameterList*>("Parameter List");

  Teuchos::RCP<PHX::DataLayout> vector_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];

  Teuchos::RCP<ParamLib> paramLib = 
    p.get< Teuchos::RCP<ParamLib> >("Parameter Library", Teuchos::null);

  std::string type = elmd_list->get("Biot Modulus Type", "Constant");
  if (type == "Constant") {
    is_constant = true;
    constant_value = elmd_list->get("Value", 1.0);

    // Add Biot Modulus as a Sacado-ized parameter
    new Sacado::ParameterRegistration<EvalT, SPL_Traits>(
	"Biot Modulus", this, paramLib);
  }
  else if (type == "Truncated KL Expansion") {
    is_constant = false;
    PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim>
      fx(p.get<string>("QP Coordinate Vector Name"), vector_dl);
    coordVec = fx;
    this->addDependentField(coordVec);

    exp_rf_kl = 
      Teuchos::rcp(new Stokhos::KL::ExponentialRandomField<MeshScalarT>(*elmd_list));
    int num_KL = exp_rf_kl->stochasticDimension();

    // Add KL random variables as Sacado-ized parameters
    rv.resize(num_KL);
    for (int i=0; i<num_KL; i++) {
      std::string ss = Albany::strint("Biot Modulus KL Random Variable",i);
      new Sacado::ParameterRegistration<EvalT, SPL_Traits>(ss, this, paramLib);
      rv[i] = elmd_list->get(ss, 0.0);
    }
  }
  else {
	  TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
		       "Invalid Biot modulus type " << type);
  } 

  if ( p.isType<string>("Porosity Name") ) {
     Teuchos::RCP<PHX::DataLayout> scalar_dl =
       p.get< Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout");
     PHX::MDField<ScalarT,Cell,QuadPoint>
       tp(p.get<string>("Porosity Name"), scalar_dl);
     porosity = tp;
     this->addDependentField(porosity);
     isPoroElastic = true;
     FluidBulkModulus = elmd_list->get("Fluid Bulk Modulus Value", 10.0e9);
     new Sacado::ParameterRegistration<EvalT, SPL_Traits>(
                                 "Skeleton Bulk Modulus Parameter Value", this, paramLib);
     GrainBulkModulus = elmd_list->get("Grain Bulk Modulus Value", 10.0e12); // typically Kgrain >> Kskeleton
     new Sacado::ParameterRegistration<EvalT, SPL_Traits>(
                                     "Grain Bulk Modulus Value", this, paramLib);
   }
   else {
     isPoroElastic=false;
     FluidBulkModulus=10.0e9; // temp value..need to change
     GrainBulkModulus = 10.0e12;  // temp value need to change
   }

  if ( p.isType<string>("Biot Coefficient Name") ) {
     Teuchos::RCP<PHX::DataLayout> scalar_dl =
       p.get< Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout");
     PHX::MDField<ScalarT,Cell,QuadPoint>
       btp(p.get<string>("Biot Coefficient Name"), scalar_dl);
     biotCoefficient = btp;
     this->addDependentField(biotCoefficient);
  }


  this->addEvaluatedField(biotModulus);
  this->setName("Biot Modulus"+PHX::TypeString<EvalT>::value);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void BiotModulus<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(biotModulus,fm);
  if (!is_constant) this->utils.setFieldData(coordVec,fm);
//  if (isThermoElastic) this->utils.setFieldData(Temperature,fm);
  if(isPoroElastic) this->utils.setFieldData(porosity,fm);
  if(isPoroElastic) this->utils.setFieldData(biotCoefficient,fm);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void BiotModulus<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  std::size_t numCells = workset.numCells;

  if (is_constant) {
    for (std::size_t cell=0; cell < numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
    	  biotModulus(cell,qp) = constant_value;
      }
    }
  }
  else {
    for (std::size_t cell=0; cell < numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
	Teuchos::Array<MeshScalarT> point(numDims);
	for (std::size_t i=0; i<numDims; i++)
	  point[i] = Sacado::ScalarValue<MeshScalarT>::eval(coordVec(cell,qp,i));
			biotModulus(cell,qp) = exp_rf_kl->evaluate(point, rv);
      }
    }
  }
  if (isPoroElastic) {
    for (std::size_t cell=0; cell < numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
    	    // 1/M = (B-phi)/Ks + phi/Kf
    	    biotModulus(cell,qp) = 1/((biotCoefficient(cell,qp) - porosity(cell,qp))/GrainBulkModulus
    	    		                  + porosity(cell,qp)/FluidBulkModulus);

      }
    }
  }
}

// **********************************************************************
template<typename EvalT,typename Traits>
typename BiotModulus<EvalT,Traits>::ScalarT&
BiotModulus<EvalT,Traits>::getValue(const std::string &n)
{
  if (n == "Biot Modulus")
    return constant_value;
  else if (n == "Fluid Bulk Modulus Value")
     return FluidBulkModulus;
   else if (n == "Grain Bulk Modulus Value")
       return GrainBulkModulus;
  for (int i=0; i<rv.size(); i++) {
    if (n == Albany::strint("Biot Modulus KL Random Variable",i))
      return rv[i];
  }
  TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
		     std::endl <<
		     "Error! Logic error in getting paramter " << n
		     << " in BiotModulus::getValue()" << std::endl);
  return constant_value;
}

// **********************************************************************
// **********************************************************************
}

