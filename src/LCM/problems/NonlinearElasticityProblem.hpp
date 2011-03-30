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


#ifndef NONLINEARELASTICITYPROBLEM_HPP
#define NONLINEARELASTICITYPROBLEM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"

namespace Albany {

  /*!
   * \brief Abstract interface for representing a 2-D finite element
   * problem.
   */
  class NonlinearElasticityProblem : public Albany::AbstractProblem {
  public:
  
    //! Default constructor
    NonlinearElasticityProblem(
                         const Teuchos::RCP<Teuchos::ParameterList>& params,
                         const Teuchos::RCP<ParamLib>& paramLib,
                         const int numEq);

    //! Destructor
    virtual ~NonlinearElasticityProblem();

    //! Build the PDE instantiations, boundary conditions, and initial solution
    virtual void 
    buildProblem( const int worksetSize,
       StateManager& stateMgr,
       const Albany::AbstractDiscretization& disc,
       std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses);

    //! Each problem must generate it's list of valid parameters
    Teuchos::RCP<const Teuchos::ParameterList> getValidProblemParameters() const;

    void getAllocatedStates(
         Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > oldState_,
         Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > newState_
         ) const;

  private:

    //! Private to prohibit copying
    NonlinearElasticityProblem(const NonlinearElasticityProblem&);
    
    //! Private to prohibit copying
    NonlinearElasticityProblem& operator=(const NonlinearElasticityProblem&);

    void constructEvaluators(const int worksetSize, const int cubDegree,
      const CellTopologyData& ctd, StateManager& stateMgr);
  protected:

    //! Boundary conditions on source term
    bool haveSource;
    int numDim;
    int numQPts;
    int numNodes;
    int numVertices;

    std::string matModel;

    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > oldState;
    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > newState;
  };

}

#endif // ALBANY_NONLINEARELASTICITYPROBLEM_HPP
