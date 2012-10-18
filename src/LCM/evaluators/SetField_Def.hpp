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
#include "Tensor.h"
#include "LCM/evaluators/SetField.hpp"

namespace LCM {

template<typename EvalT, typename Traits>
SetField<EvalT, Traits>::
SetField(const Teuchos::ParameterList& p) :
  evaluatedFieldName( p.get<std::string>("Evaluated Field Name") ),
  evaluatedField( p.get<std::string>("Evaluated Field Name"), p.get<Teuchos::RCP<PHX::DataLayout> >("Evaluated Field Data Layout") ),
  fieldValues( p.get<Teuchos::ArrayRCP<ScalarT> >("Field Values"))
{
  // Get the dimensions of the data layout for the field that is to be set
  p.get<Teuchos::RCP<PHX::DataLayout> >("Evaluated Field Data Layout")->dimensions(evaluatedFieldDimensions);

  // Register the field to be set as an evaluated field
  this->addEvaluatedField(evaluatedField);
  this->setName("SetField" + PHX::TypeString<EvalT>::value);
}

template<typename EvalT, typename Traits>
void SetField<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(evaluatedField, fm);
}

template<typename EvalT, typename Traits>
void SetField<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  unsigned int numDimensions = evaluatedFieldDimensions.size();

  TEUCHOS_TEST_FOR_EXCEPT_MSG(numDimensions < 2, "SetField::evaluateFields(), unsupported field type.");  
  int dim1 = evaluatedFieldDimensions[0];
  int dim2 = evaluatedFieldDimensions[1];

  if(numDimensions == 2){
    TEUCHOS_TEST_FOR_EXCEPT_MSG(fieldValues.size() != dim1*dim2, "SetField::evaluateFields(), inconsistent data sizes.");
    for(int i=0 ; i<dim1 ; ++i){
      for(int j=0 ; j<dim2 ; ++j){
        evaluatedField(i,j) = fieldValues[i*dim2 + j];
      }
    }
  }
  else if(numDimensions == 3){
    int dim3 = evaluatedFieldDimensions[2];
    TEUCHOS_TEST_FOR_EXCEPT_MSG(fieldValues.size() != dim1*dim2*dim3, "SetField::evaluateFields(), inconsistent data sizes.");
    for(int i=0 ; i<dim1 ; ++i){
      for(int j=0 ; j<dim2 ; ++j){
        for(int m=0 ; m<dim3 ; ++m){
          evaluatedField(i,j,m) = fieldValues[i*dim2*dim3 + j*dim3 + m];
        }
      }
    }
  }
  else if(numDimensions == 4){
    int dim3 = evaluatedFieldDimensions[2];
    int dim4 = evaluatedFieldDimensions[3];
    std::cout << std::endl;
    std::cout << "in SetField" << std::endl;
    std::cout << " dim1: " << dim1 << ", dim2: " << dim2 << ", dim3: " << dim3 << ", dim4: " << dim4 << std::endl;
    std::cout << "fieldValues.size(): " << fieldValues.size() << std::endl;
    std::cout << "dim1*dim2*dim3*dim4: " << dim1*dim2*dim3*dim4 << std::endl;
    TEUCHOS_TEST_FOR_EXCEPT_MSG(fieldValues.size() != dim1*dim2*dim3*dim4, "SetField::evaluateFields(), inconsistent data sizes.");
    for(int i=0 ; i<dim1 ; ++i){
      for(int j=0 ; j<dim2 ; ++j){
        for(int m=0 ; m<dim3 ; ++m){
          for(int n=0 ; n<dim4 ; ++n){
            evaluatedField(i,j,m,n) = fieldValues[i*dim2*dim3*dim4 + j*dim3*dim4 + m*dim4 + n];
          }
        }
      }
    }
  }
  else{
    TEUCHOS_TEST_FOR_EXCEPT_MSG(numDimensions > 4, "SetField::evaluateFields(), unsupported data type.");
  }
}

}

