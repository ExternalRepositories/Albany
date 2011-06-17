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


#include <vector>
#include <string>

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

namespace PHAL {

template<typename EvalT, typename Traits>
SaveStateField<EvalT, Traits>::
SaveStateField(const Teuchos::ParameterList& p) 
{  
  // States Not Saved for Generic Type, only Specializations
  this->setName("Save State Field"+PHX::TypeString<EvalT>::value);
}

// **********************************************************************
template<typename EvalT, typename Traits> 
void SaveStateField<EvalT, Traits>::postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  // States Not Saved for Generic Type, only Specializations
}

// **********************************************************************
template<typename EvalT, typename Traits>
void SaveStateField<EvalT, Traits>::evaluateFields(typename Traits::EvalData workset)
{ 
  // States Not Saved for Generic Type, only Specializations
}
// **********************************************************************
// **********************************************************************
template<typename Traits>
SaveStateField<PHAL::AlbanyTraits::Residual, Traits>::
SaveStateField(const Teuchos::ParameterList& p)
{  
  fieldName =  p.get<std::string>("Field Name");
  stateName =  p.get<std::string>("State Name");
  PHX::MDField<ScalarT> f(fieldName, p.get<Teuchos::RCP<PHX::DataLayout> >("State Field Layout") );
  state = f;

  savestate_operation = Teuchos::rcp(new PHX::Tag<ScalarT>
    (fieldName, p.get< Teuchos::RCP<PHX::DataLayout> >("Dummy Data Layout")));

  this->addDependentField(state);
  this->addEvaluatedField(*savestate_operation);

  this->setName("Save State " + stateName +" to Field " + fieldName
                + PHX::TypeString<PHAL::AlbanyTraits::Residual>::value);
}

// **********************************************************************
template<typename Traits> 
void SaveStateField<PHAL::AlbanyTraits::Residual, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(state,fm);

  //state.dimensions(dims); //get dimensions
}
// **********************************************************************
template<typename Traits>
void SaveStateField<PHAL::AlbanyTraits::Residual, Traits>::
evaluateFields(typename Traits::EvalData workset)
{ 
  //cout << "SaveStateField copying off  " << fieldName << " to state " 
  //     << stateName << " with size " << state.size() << endl;
  
  // Get state field container of same name
  Albany::StateVariables& newState = *workset.newState;
  Intrepid::FieldContainer<RealType>& savedState  = *newState[stateName];

  double max = 0;
  for (int i=0; i < state.size() ; ++i) {
    savedState[i] = state[i];
    if(fabs(state[i]) > max) max = fabs(state[i]);
  }

  // Get shards Array (from STK) for this state
  // Need to check if we can just copy full size -- can assiume same ordering?
    Albany::MDArray sta = (*workset.stateArrayPtr)[stateName];
    std::vector<int> dims;
    sta.dimensions(dims);
    int size = dims.size();

    switch (size) {
      case 2:     
        for (int cell = 0; cell < dims[0]; ++cell)
          for (int qp = 0; qp < dims[1]; ++qp)
                sta(cell, qp) = state(cell,qp);;
        break;
      case 3:     
        for (int cell = 0; cell < dims[0]; ++cell)
          for (int qp = 0; qp < dims[1]; ++qp)
            for (int i = 0; i < dims[2]; ++i)
                sta(cell, qp, i) = state(cell,qp,i);
        break;
      case 4:     
        for (int cell = 0; cell < dims[0]; ++cell)
          for (int qp = 0; qp < dims[1]; ++qp)
            for (int i = 0; i < dims[2]; ++i)
              for (int j = 0; j < dims[3]; ++j)
                  sta(cell, qp, i, j) = state(cell,qp,i,j);
        break;
      default:
      TEST_FOR_EXCEPTION(size<2||size>4, std::logic_error,
          "Unexpected Array dimensions in SaveStateField: " << size);
    }
}

}
