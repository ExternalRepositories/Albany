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


#ifndef QCAD_SCHRODINGERPROBLEM_HPP
#define QCAD_SCHRODINGERPROBLEM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"

namespace QCAD {

  /*!
   * \brief Abstract interface for representing a 1-D finite element
   * problem.
   */
  class SchrodingerProblem : public Albany::AbstractProblem {
  public:
  
    //! Default constructor
    SchrodingerProblem(
                         const Teuchos::RCP<Teuchos::ParameterList>& params,
                         const Teuchos::RCP<ParamLib>& paramLib,
                         const int numDim_,
			 const Teuchos::RCP<const Epetra_Comm>& comm_);

    //! Destructor
    ~SchrodingerProblem();

    //! Build the PDE instantiations, boundary conditions, and initial solution
    void buildProblem(
       const Albany::MeshSpecsStruct& meshSpecs,
       Albany::StateManager& stateMgr,
       std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses);

    //! Each problem must generate it's list of valide parameters
    Teuchos::RCP<const Teuchos::ParameterList> getValidProblemParameters() const;

  private:

    //! Private to prohibit copying
    SchrodingerProblem(const SchrodingerProblem&);
    
    //! Private to prohibit copying
    SchrodingerProblem& operator=(const SchrodingerProblem&);

    void constructEvaluators(const Albany::MeshSpecsStruct& meshSpecs,
                             Albany::StateManager& stateMgr,
			     std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses);

    void constructResponses(std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses,
			    Teuchos::ParameterList& responseList, 
			    std::map<std::string, Teuchos::RCP<Teuchos::ParameterList> >& evaluators_to_build, 
			    Albany::StateManager& stateMgr,
			    Teuchos::RCP<PHX::DataLayout> qp_scalar, Teuchos::RCP<PHX::DataLayout> qp_vector, 
			    Teuchos::RCP<PHX::DataLayout> cell_scalar, Teuchos::RCP<PHX::DataLayout> dummy);

    // Andy: next three functions or ones similar should probably go in base class

    void createResponseFieldManager(std::map<std::string, Teuchos::RCP<Teuchos::ParameterList> >& response_evaluators_to_build,
				    std::map<std::string, Teuchos::RCP<Teuchos::ParameterList> >& evaluators_to_build,
				    const std::vector<std::string>& responseIDs_to_require, Teuchos::RCP<PHX::DataLayout> dummy);

    // - Returns true if responseName was recognized and response function constructed.
    // - If p is non-Teuchos::null upon exit, then an evaluator should be build using
    //   p as the parameter list. 
    bool getStdResponseFn(std::string responseName, int responseIndex,
			  Teuchos::ParameterList& responseList,
			  std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses,
			  Albany::StateManager& stateMgr,
			  Teuchos::RCP<PHX::DataLayout> qp_scalar, Teuchos::RCP<PHX::DataLayout> qp_vector, 
			  Teuchos::RCP<PHX::DataLayout> cell_scalar,  Teuchos::RCP<PHX::DataLayout> dummy,
			  Teuchos::RCP<Teuchos::ParameterList>& p);

    // Helper function for constructResponses and getStdResponseFn
    Teuchos::RCP<Teuchos::ParameterList> setupResponseFnForEvaluator(  
		  Teuchos::ParameterList& responseList, int responseNumber,
		  std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses,
		  Teuchos::RCP<PHX::DataLayout> dummy);


  protected:
    Teuchos::RCP<const Epetra_Comm> comm;
    bool havePotential;
    bool haveMaterial;
    double energy_unit_in_eV, length_unit_in_m;
    std::string potentialStateName;
    std::string mtrlDbFilename;

    int numDim;
    int nEigenvectorsToOuputAsStates;
    bool bOnlySolveInQuantumBlocks;
  };

}

#endif
