//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <fstream>
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Sacado_ParameterRegistration.hpp"
#include "Albany_Utils.hpp"

namespace LCM {

//**********************************************************************
template<typename EvalT, typename Traits>
BulkModulus<EvalT, Traits>::
BulkModulus(Teuchos::ParameterList& p) :
  bulkModulus(p.get<std::string>("QP Variable Name"),
	      p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout"))
{
  Teuchos::ParameterList* bmd_list = 
    p.get<Teuchos::ParameterList*>("Parameter List");

  Teuchos::RCP<PHX::DataLayout> vector_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];

  Teuchos::RCP<ParamLib> paramLib = 
    p.get< Teuchos::RCP<ParamLib> >("Parameter Library", Teuchos::null);

  std::string type = bmd_list->get("Bulk Modulus Type", "Constant");
  if (type == "Constant") {
    is_constant = true;
    constant_value = bmd_list->get("Value", 1.0);

    // Add Bulk Modulus as a Sacado-ized parameter
    this->registerSacadoParameter("Bulk Modulus", paramLib);
  }
  else if (type == "Truncated KL Expansion") {
    is_constant = false;
    PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim>
      fx(p.get<std::string>("QP Coordinate Vector Name"), vector_dl);
    coordVec = fx;
    this->addDependentField(coordVec);

    exp_rf_kl = 
      Teuchos::rcp(new Stokhos::KL::ExponentialRandomField<MeshScalarT>(*bmd_list));
    int num_KL = exp_rf_kl->stochasticDimension();

    // Add KL random variables as Sacado-ized parameters
    rv.resize(num_KL);
    for (int i=0; i<num_KL; i++) {
      std::string ss = Albany::strint("Bulk Modulus KL Random Variable",i);
      this->registerSacadoParameter(ss, paramLib);
      rv[i] = bmd_list->get(ss, 0.0);
    }
  }
  else {
    TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
			       "Invalid bulk modulus type " << type);
  } 

  if ( p.isType<std::string>("QP Temperature Name") ) {
    Teuchos::RCP<PHX::DataLayout> scalar_dl =
      p.get< Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout");
    PHX::MDField<ScalarT,Cell,QuadPoint>
      tmp(p.get<std::string>("QP Temperature Name"), scalar_dl);
    Temperature = tmp;
    this->addDependentField(Temperature);
    isThermoElastic = true;
    dKdT_value = bmd_list->get("dKdT Value", 0.0);
    refTemp = p.get<RealType>("Reference Temperature", 0.0);
    this->registerSacadoParameter("dKdT Value", paramLib);
  }
  else {
    isThermoElastic=false;
    dKdT_value=0.0;
  }

  this->addEvaluatedField(bulkModulus);
  this->setName("Bulk Modulus"+PHX::TypeString<EvalT>::value);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void BulkModulus<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(bulkModulus,fm);
  if (!is_constant) this->utils.setFieldData(coordVec,fm);
  if (isThermoElastic) this->utils.setFieldData(Temperature,fm);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void BulkModulus<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  if (is_constant) {
    for (unsigned int cell=0; cell < workset.numCells; ++cell) {
      for (unsigned int qp=0; qp < numQPs; ++qp) {
	bulkModulus(cell,qp) = constant_value;
      }
    }
  }
  else {
    for (unsigned int cell=0; cell < workset.numCells; ++cell) {
      for (unsigned int qp=0; qp < numQPs; ++qp) {
	Teuchos::Array<MeshScalarT> point(numDims);
	for (unsigned int i=0; i<numDims; i++)
	  point[i] = Sacado::ScalarValue<MeshScalarT>::eval(coordVec(cell,qp,i));
	bulkModulus(cell,qp) = exp_rf_kl->evaluate(point, rv);
      }
    }
  }
  if (isThermoElastic) {
    for (std::size_t cell=0; cell < workset.numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
	bulkModulus(cell,qp) -= dKdT_value * (Temperature(cell,qp) - refTemp);
      }
    }
  }
}

// **********************************************************************
template<typename EvalT,typename Traits>
typename BulkModulus<EvalT,Traits>::ScalarT& 
BulkModulus<EvalT,Traits>::getValue(const std::string &n)
{
  if (n == "Bulk Modulus")
    return constant_value;
  else if (n == "dKdT Value")
    return dKdT_value;
  for (int i=0; i<rv.size(); i++) {
    if (n == Albany::strint("Bulk Modulus KL Random Variable",i))
      return rv[i];
  }
  TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
			     std::endl <<
			     "Error! Logic error in getting paramter " << n
			     << " in BulkModulus::getValue()" << std::endl);
  return constant_value;
}

// **********************************************************************
// **********************************************************************
}

