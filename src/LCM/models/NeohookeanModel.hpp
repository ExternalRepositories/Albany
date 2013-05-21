//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#if !defined(LCM_NeohookeanModel_hpp)
#define LCM_NeohookeanModel_hpp

#include "Phalanx_ConfigDefs.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"
#include "Albany_Layouts.hpp"
#include "LCM/models/ConstitutiveModel.hpp"

namespace LCM {

  //! \brief Constitutive Model Base Class
  template<typename EvalT, typename Traits>
  class NeohookeanModel : public LCM::ConstitutiveModel<EvalT, Traits>
  {
  public:

    typedef typename EvalT::ScalarT ScalarT;
    typedef typename EvalT::MeshScalarT MeshScalarT;

    using ConstitutiveModel<EvalT,Traits>::num_dims_;
    using ConstitutiveModel<EvalT,Traits>::num_pts_;
    using ConstitutiveModel<EvalT,Traits>::field_name_map_;

    ///
    /// Constructor
    ///
    NeohookeanModel(Teuchos::ParameterList* p,
                    const Teuchos::RCP<Albany::Layouts>& dl);

    ///
    /// Method to compute the state (e.g. energy, stress, tangent)
    ///
    virtual 
    void 
    computeState(typename Traits::EvalData workset,
                 std::map<std::string, Teuchos::RCP<PHX::MDField<ScalarT> > > dep_fields,
                 std::map<std::string, Teuchos::RCP<PHX::MDField<ScalarT> > > eval_fields);

  private:

    ///
    /// Private to prohibit copying
    ///
    NeohookeanModel(const NeohookeanModel&);

    ///
    /// Private to prohibit copying
    ///
    NeohookeanModel& operator=(const NeohookeanModel&);

  };
}

#endif
