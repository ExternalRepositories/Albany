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


#ifndef QCAD_POISSONPROBLEM_HPP
#define QCAD_POISSONPROBLEM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"
#include "Albany_ProblemUtils.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"

//! Code Base for Quantum Device Simulation Tools LDRD
namespace QCAD {

  /*!
   * \brief Abstract interface for representing a 1-D finite element
   * problem.
   */
  class PoissonProblem : public Albany::AbstractProblem 
  {
  public:
  
    //! Default constructor
    PoissonProblem(
                         const Teuchos::RCP<Teuchos::ParameterList>& params,
                         const Teuchos::RCP<ParamLib>& paramLib,
                         const int numDim_,
			 const Teuchos::RCP<const Epetra_Comm>& comm_);

    //! Destructor
    ~PoissonProblem();

    //! Build the PDE instantiations, boundary conditions, and initial solution
    void buildProblem(
       const Albany::MeshSpecsStruct& meshSpecs,
       Albany::StateManager& stateMgr,
       std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses);

    //! Each problem must generate it's list of valide parameters
    Teuchos::RCP<const Teuchos::ParameterList> getValidProblemParameters() const;

  private:

    //! Private to prohibit copying
    PoissonProblem(const PoissonProblem&);
    
    //! Private to prohibit copying
    PoissonProblem& operator=(const PoissonProblem&);

    void constructEvaluators(const Albany::MeshSpecsStruct& meshSpecs,
                             Albany::StateManager& stateMgr,
			     std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses);

    void constructDirichletEvaluators(const std::vector<std::string>& nodeSetIDs,
                                      const std::vector<std::string>& dirichletNames,
                                      const Albany::ProblemUtils& probUtils);

    void constructResponses(std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses,
			    Teuchos::ParameterList& responseList, 
			    std::map<std::string, Teuchos::RCP<Teuchos::ParameterList> >& evaluators_to_build, 
			    Albany::StateManager& stateMgr,
                            Albany::Layouts& dl);


    // Andy: next three functions or ones similar should probably go in base class

    void createResponseFieldManager(std::map<std::string, Teuchos::RCP<Teuchos::ParameterList> >& response_evaluators_to_build,
				    std::map<std::string, Teuchos::RCP<Teuchos::ParameterList> >& evaluators_to_build,
				    const std::vector<std::string>& responseIDs_to_require, Albany::Layouts& dl);

    // - Returns true if responseName was recognized and response function constructed.
    // - If p is non-Teuchos::null upon exit, then an evaluator should be build using
    //   p as the parameter list. 
    bool getStdResponseFn(std::string responseName, int responseIndex,
			  Teuchos::ParameterList& responseList,
			  std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses,
			  Albany::StateManager& stateMgr,
                          Albany::Layouts& dl,
			  Teuchos::RCP<Teuchos::ParameterList>& p);

    // Helper function for constructResponses and getStdResponseFn
    Teuchos::RCP<Teuchos::ParameterList> setupResponseFnForEvaluator(  
		  Teuchos::ParameterList& responseList, int responseNumber,
		  std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses, 
                  Albany::Layouts& dl);

  protected:

    //! Boundary conditions on source term
    bool periodic;

    //! Parameters to use when constructing evaluators
    Teuchos::RCP<const Epetra_Comm> comm;
    bool haveSource;
    int numDim;
    double length_unit_in_m;
    double temperature;
    std::string mtrlDbFilename;

    //! Parameters for coupling to Schrodinger
    bool bUseSchrodingerSource;
    int nEigenvectors;
    bool bUsePredictorCorrector;     
  };

}

#endif
