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


#ifndef SETFIELD_HPP
#define SETFIELD_HPP

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "PHAL_Dimension.hpp"

namespace LCM {
/** \brief Sets values in a field, indended for testing.
*/

template<typename EvalT, typename Traits>
class SetField : public PHX::EvaluatorWithBaseImpl<Traits>,
		    public PHX::EvaluatorDerived<EvalT, Traits>  {

public:

  SetField(const Teuchos::ParameterList& p);

  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::ScalarT ScalarT;

  //! The name of the field to be set.
  std::string evaluatedFieldName;

  //! The field that will be set.
  PHX::MDField<ScalarT, Cell, QuadPoint, Dim, Dim> evaluatedField;

  //! The dimensions of the field to be set.
  std::vector<PHX::DataLayout::size_type> evaluatedFieldDimensions;

  //! The values that will be assigned to the field
  Teuchos::ArrayRCP<ScalarT> fieldValues;
};
}

#endif
