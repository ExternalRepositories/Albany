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


#ifndef ALBANY_MULTIHEATPROBLEM_HPP
#define ALBANY_MULTIHEATPROBLEM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"

#include "QCAD_MaterialDatabase.hpp"

namespace Albany {

  /*!
   * \brief Abstract interface for representing a 1-D finite element
   * problem.
   */
  class MultiHeatProblem : public AbstractProblem {
  public:
  
    //! Default constructor
    MultiHeatProblem(
                         const Teuchos::RCP<Teuchos::ParameterList>& params,
                         const Teuchos::RCP<ParamLib>& paramLib,
                         const int numDim_, 
                         const Teuchos::RCP<const Epetra_Comm>& comm_);

    //! Destructor
    ~MultiHeatProblem();

    //! Build the PDE instantiations, boundary conditions, and initial solution
    void buildProblem(
       Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
       StateManager& stateMgr,
       std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses);

    //! Each problem must generate it's list of valide parameters
    Teuchos::RCP<const Teuchos::ParameterList> getValidProblemParameters() const;

  private:

    //! Private to prohibit copying
    MultiHeatProblem(const MultiHeatProblem&);
    
    //! Private to prohibit copying
    MultiHeatProblem& operator=(const MultiHeatProblem&);

    void constructEvaluators(const Albany::MeshSpecsStruct& meshSpecs,
                             Albany::StateManager& stateMgr,
                             std::vector< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses,
                             int physSet);

    void constructDirichletEvaluators(const Albany::MeshSpecsStruct& meshSpecs);

  protected:

    //! Boundary conditions on source term
    bool periodic;
    bool haveSource;
    bool haveAbsorption;
	bool haveMatDB;
    int numDim;

    std::string mtrlDbFilename;
    Teuchos::RCP<QCAD::MaterialDatabase> materialDB;
    Teuchos::RCP<const Epetra_Comm> comm;

  };

}

#endif // ALBANY_HEATNONLINEARSOURCEPROBLEM_HPP
