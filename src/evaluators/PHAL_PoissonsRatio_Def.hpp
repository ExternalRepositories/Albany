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

template<typename EvalT, typename Traits>
PoissonsRatio<EvalT, Traits>::
PoissonsRatio(Teuchos::ParameterList& p) :
  poissonsRatio(p.get<std::string>("QP Variable Name"),
      p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout"))
{
  Teuchos::ParameterList* pr_list = 
    p.get<Teuchos::ParameterList*>("Parameter List");
  
  Teuchos::RCP<ParamLib> paramLib = 
    p.get< Teuchos::RCP<ParamLib> >("Parameter Library", Teuchos::null);

  Teuchos::RCP<PHX::DataLayout> vector_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];

  std::string type = pr_list->get("Poissons Ratio Type", "Constant");
  if (type == "Constant") {
    is_constant = true;
    constant_value = pr_list->get("Value", 1.0);

    // Add Poissons Ratio as a Sacado-ized parameter
    new Sacado::ParameterRegistration<EvalT, SPL_Traits>(
      "Poissons Ratio", this, paramLib);
  }
  else if (type == "Truncated KL Expansion") {
    is_constant = false;
    PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim>
      fx(p.get<string>("QP Coordinate Vector Name"), vector_dl);
    coordVec = fx;
    this->addDependentField(coordVec);

    exp_rf_kl = 
      Teuchos::rcp(new Stokhos::KL::ExponentialRandomField<MeshScalarT>(*pr_list));
    int num_KL = exp_rf_kl->stochasticDimension();

    // Add KL random variables as Sacado-ized parameters
    rv.resize(num_KL);
    for (int i=0; i<num_KL; i++) {
      std::stringstream ss;
      ss << "Poissons Ratio KL Random Variable " << i;
      new Sacado::ParameterRegistration<EvalT, SPL_Traits>(ss.str(), this, 
							   paramLib);
      rv[i] = pr_list->get(ss.str(), 0.0);
    }
  }
  else {
    TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
		       "Invalid Poissons ratio type " << type);
  } 

  // Optional dependence on Temperature (nu = nu_ + dnudT * T)
  // Switched ON by sending Temperature field in p

  if ( p.isType<string>("QP Temperature Name") ) {
    Teuchos::RCP<PHX::DataLayout> scalar_dl =
      p.get< Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout");
    PHX::MDField<ScalarT,Cell,QuadPoint>
      tmp(p.get<string>("QP Temperature Name"), scalar_dl);
    Temperature = tmp;
    this->addDependentField(Temperature);
    isThermoElastic = true;
    dnudT_value = pr_list->get("dnudT Value", 0.0);
    new Sacado::ParameterRegistration<EvalT, SPL_Traits>(
                                "dnudT Value", this, paramLib);
  }
  else {
    isThermoElastic=false;
    dnudT_value=0.0;
  }


  this->addEvaluatedField(poissonsRatio);
  this->setName("Poissons Ratio");
}

// **********************************************************************
template<typename EvalT, typename Traits>
void PoissonsRatio<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(poissonsRatio,fm);
  if (!is_constant) this->utils.setFieldData(coordVec,fm);
  if (isThermoElastic) this->utils.setFieldData(Temperature,fm);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void PoissonsRatio<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  int numCells = workset.numCells;

  if (is_constant) {
    for (std::size_t cell=0; cell < numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
	poissonsRatio(cell,qp) = constant_value;
      }
    }
  }
  else {
    for (std::size_t cell=0; cell < numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
	Teuchos::Array<MeshScalarT> point(numDims);
	for (std::size_t i=0; i<numDims; i++)
	  point[i] = Sacado::ScalarValue<MeshScalarT>::eval(coordVec(cell,qp,i));
	poissonsRatio(cell,qp) = exp_rf_kl->evaluate(point, rv);
      }
    }
  }
  if (isThermoElastic) {
    for (std::size_t cell=0; cell < numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
        poissonsRatio(cell,qp) += dnudT_value * Temperature(cell,qp);
      }
    }
  }
}

// **********************************************************************
template<typename EvalT,typename Traits>
typename PoissonsRatio<EvalT,Traits>::ScalarT& 
PoissonsRatio<EvalT,Traits>::getValue(const std::string &n)
{
  if (n=="Poissons Ratio")
    return constant_value;
  else if (n == "dnudT Value")
    return dnudT_value;
  for (int i=0; i<rv.size(); i++) {
    std::stringstream ss;
    ss << "Poissons Ratio KL Random Variable " << i;
    if (n == ss.str())
      return rv[i];
  }
  TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
		     std::endl <<
		     "Error! Logic error in getting paramter " << n
		     << " in PoissonsRatio::getValue()" << std::endl);
  return constant_value;
}

// **********************************************************************
// **********************************************************************
