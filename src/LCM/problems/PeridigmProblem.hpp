//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef PERIDIGMPROBLEM_HPP
#define PERIDIGMPROBLEM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"
#include "PHAL_AlbanyTraits.hpp"

namespace Albany {

  /*!
   * \brief Interface to Peridigm peridynamics code.
   */
  class PeridigmProblem : public Albany::AbstractProblem {
  public:
  
    //! Default constructor
    PeridigmProblem(const Teuchos::RCP<Teuchos::ParameterList>& params,
                    const Teuchos::RCP<ParamLib>& paramLib,
                    const int numEqm,
                    const Teuchos::RCP<const Epetra_Comm>& comm);

    //! Destructor
    virtual ~PeridigmProblem();

    //! Return number of spatial dimensions
    virtual int spatialDimension() const { return numDim; }

    //! Build the PDE instantiations, boundary conditions, and initial solution
    virtual void buildProblem(Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs, StateManager& stateMgr);

    // Build evaluators
    virtual Teuchos::Array< Teuchos::RCP<const PHX::FieldTag> >
    buildEvaluators(
      PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
      const Albany::MeshSpecsStruct& meshSpecs,
      Albany::StateManager& stateMgr,
      Albany::FieldManagerChoice fmchoice,
      const Teuchos::RCP<Teuchos::ParameterList>& responseList);

    //! Each problem must generate it's list of valid parameters
    Teuchos::RCP<const Teuchos::ParameterList> getValidProblemParameters() const;

  private:

    //! Private to prohibit copying
    PeridigmProblem(const PeridigmProblem&);
    
    //! Private to prohibit copying
    PeridigmProblem& operator=(const PeridigmProblem&);

  public:

    //! Main problem setup routine. Not directly called, but indirectly by following functions
    template <typename EvalT> 
    Teuchos::RCP<const PHX::FieldTag>
    constructEvaluators(
      PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
      const Albany::MeshSpecsStruct& meshSpecs,
      Albany::StateManager& stateMgr,
      Albany::FieldManagerChoice fmchoice,
      const Teuchos::RCP<Teuchos::ParameterList>& responseList);

    void constructDirichletEvaluators(const Albany::MeshSpecsStruct& meshSpecs);

  protected:

    //! Boundary conditions on source term
    bool haveSource;
    int numDim;
    bool haveMatDB;
    std::string mtrlDbFilename;
    Teuchos::RCP<QCAD::MaterialDatabase> materialDB;

  };

}

#include "Albany_Utils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_ResponseUtilities.hpp"
#include "Albany_EvaluatorUtils.hpp"

#include "PHAL_Source.hpp"
#include "CurrentCoords.hpp"
#include "PeridigmForce.hpp"
#include "PHAL_SaveStateField.hpp"

template <typename EvalT>
Teuchos::RCP<const PHX::FieldTag>
Albany::PeridigmProblem::constructEvaluators(
  PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
  const Albany::MeshSpecsStruct& meshSpecs,
  Albany::StateManager& stateMgr,
  Albany::FieldManagerChoice fieldManagerChoice,
  const Teuchos::RCP<Teuchos::ParameterList>& responseList)
{
   using Teuchos::RCP;
   using Teuchos::rcp;
   using Teuchos::ParameterList;
   using PHX::DataLayout;
   using PHX::MDALayout;
   using std::vector;
   using std::string;
   using PHAL::AlbanyTraits;

  // get the name of the current element block
  string elementBlockName = meshSpecs.ebName;

  // WHAT'S IN meshSpecs Albany::MeshSpecsStruct?

   const int worksetSize = meshSpecs.worksetSize;
   const int numVertices = 1;
   const int numNodes = 1;
   const int numQPts = 1;

   *out << "Field Dimensions: Workset=" << worksetSize 
        << ", Vertices= " << numVertices
        << ", Nodes= " << numNodes
        << ", Dim= " << numDim << std::endl;

   // Construct evaluators

   RCP<Albany::Layouts> dataLayout = rcp(new Albany::Layouts(worksetSize, numVertices, numNodes, numQPts, numDim));
   TEUCHOS_TEST_FOR_EXCEPTION(dataLayout->vectorAndGradientLayoutsAreEquivalent==false, std::logic_error,
                              "Data Layout Usage in Peridigm problems assume vecDim = numDim");
   Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtils(dataLayout);

   Teuchos::ArrayRCP<std::string> dof_name(1), dof_name_dotdot(1), residual_name(1);
   dof_name[0] = "Displacement";
   dof_name_dotdot[0] = "Displacement_dotdot";
   residual_name[0] = "Residual";

   Teuchos::RCP<PHX::Evaluator<AlbanyTraits> > ev;

   bool supportsTransient = false;
   { // Solution
     if(!supportsTransient)
       fm0.template registerEvaluator<EvalT>(evalUtils.constructGatherSolutionEvaluator_noTransient(true, dof_name));
     else
       fm0.template registerEvaluator<EvalT>(evalUtils.constructGatherSolutionEvaluator(true, dof_name, dof_name_dotdot));
   }

   { // Volume
     
   }

   { // Gather Coord Vec
     fm0.template registerEvaluator<EvalT>(evalUtils.constructGatherCoordinateVectorEvaluator());
   }

   if (haveSource) { // Source
     TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
				"Error!  Sources not available for Peridigm!");
   }

   { // Current Coordinates
     RCP<ParameterList> p = rcp(new ParameterList("Current Coordinates"));
     p->set<std::string>("Reference Coordinates Name", "Coord Vec");
     p->set<std::string>("Displacement Name", "Displacement");
     p->set<std::string>("Current Coordinates Name", "Current Coordinates");
     ev = rcp(new LCM::CurrentCoords<EvalT, AlbanyTraits>(*p, dataLayout));
      fm0.template registerEvaluator<EvalT>(ev);
   }

   { // Peridigm Force
     RCP<ParameterList> p = rcp(new ParameterList("Force"));

     // Parameter list to be passed to Peridigm object
     Teuchos::ParameterList& peridigmParametersList = p->sublist("Peridigm Parameters");

     // Required data layouts
     p->set< RCP<DataLayout> >("Node Vector Data Layout", dataLayout->node_vector);    

     // Input
     p->set<string>("Reference Coordinates Name", "Coord Vec");
     p->set<string>("Current Coordinates Name", "Current Coordinates");

     // Output
     p->set<string>("Force Name", "Force");
     p->set<string>("Residual Name", "Residual");

     ev = rcp(new LCM::PeridigmForce<EvalT, AlbanyTraits>(*p, dataLayout));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   { // Scatter Residual
     fm0.template registerEvaluator<EvalT>(evalUtils.constructScatterResidualEvaluator(true, residual_name));
   }

   if (fieldManagerChoice == Albany::BUILD_RESID_FM)  {
     PHX::Tag<typename EvalT::ScalarT> res_tag("Scatter", dataLayout->dummy);
     fm0.requireField<EvalT>(res_tag);
     return res_tag.clone();
   }
   else if (fieldManagerChoice == Albany::BUILD_RESPONSE_FM) {
     Albany::ResponseUtilities<EvalT, PHAL::AlbanyTraits> respUtils(dataLayout);
     return respUtils.constructResponses(fm0, *responseList, stateMgr);
   }

   return Teuchos::null;
}

#endif // PERIDIGMPROBLEM_HPP
