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


#ifndef ALBANY_HELMHOLTZN2DPROBLEM_HPP
#define ALBANY_HELMHOLTZN2DPROBLEM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"

namespace Albany {

  /*!
   * \brief Abstract interface for representing a 1-D finite element
   * problem.
   */
  class Helmholtz2DProblem : public Albany::AbstractProblem {
  public:
  
    //! Default constructor
    Helmholtz2DProblem(const Teuchos::RCP<Teuchos::ParameterList>& params,
		       const Teuchos::RCP<ParamLib>& paramLib);

    //! Destructor
    virtual ~Helmholtz2DProblem();

    //! Return number of spatial dimensions
    virtual int spatialDimension() const { return 2; }

     //! Build the PDE instantiations, boundary conditions, and initial solution
    virtual void buildProblem(
      const Teuchos::RCP<Albany::Application>& app,
      Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
      StateManager& stateMgr,
      Teuchos::Array< Teuchos::RCP<Albany::AbstractResponseFunction> >& responses);

    // Build evaluators
    virtual Teuchos::Array< Teuchos::RCP<const PHX::FieldTag> >
    buildEvaluators(
      PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
      const Albany::MeshSpecsStruct& meshSpecs,
      Albany::StateManager& stateMgr,
      Albany::FieldManagerChoice fmchoice,
      const Teuchos::RCP<Teuchos::ParameterList>& responseList);

    //! Each problem must generate it's list of valide parameters
    Teuchos::RCP<const Teuchos::ParameterList> getValidProblemParameters() const;

  private:

    //! Private to prohibit copying
    Helmholtz2DProblem(const Helmholtz2DProblem&);
    
    //! Private to prohibit copying
    Helmholtz2DProblem& operator=(const Helmholtz2DProblem&);

  public:

    //! Main problem setup routine. Not directly called, but indirectly by following functions
    template <typename EvalT> Teuchos::RCP<const PHX::FieldTag>
    constructEvaluators(
      PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
      const Albany::MeshSpecsStruct& meshSpecs,
      Albany::StateManager& stateMgr,
      Albany::FieldManagerChoice fmchoice,
      const Teuchos::RCP<Teuchos::ParameterList>& responseList);

    void constructDirichletEvaluators(const Albany::MeshSpecsStruct& meshSpecs);

  protected:

    //! Boundary conditions, factor on source term
    double ksqr;
    bool haveSource;
  };

}

#include "Albany_Utils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_EvaluatorUtils.hpp"
#include "Albany_ResponseUtilities.hpp"

#include "PHAL_Source.hpp"
#include "PHAL_HelmholtzResid.hpp"


template <typename EvalT>
Teuchos::RCP<const PHX::FieldTag>
Albany::Helmholtz2DProblem::constructEvaluators(
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
   using PHAL::AlbanyTraits;

   RCP<shards::CellTopology> cellType = rcp(new shards::CellTopology(&meshSpecs.ctd)); 
   RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > >
     intrepidBasis = Albany::getIntrepidBasis(meshSpecs.ctd);

   const int numNodes = intrepidBasis->getCardinality();
   const int worksetSize = meshSpecs.worksetSize;

   Intrepid::DefaultCubatureFactory<RealType> cubFactory;

   RCP <Intrepid::Cubature<RealType> > cubature = cubFactory.create(*cellType, meshSpecs.cubatureDegree);

   const int numDim = cubature->getDimension();
   const int numQPts = cubature->getNumPoints();
   const int numVertices = cellType->getNodeCount();

   *out << "Field Dimensions: Workset=" << worksetSize
        << ", Vertices= " << numVertices
        << ", Nodes= " << numNodes
        << ", QuadPts= " << numQPts
        << ", Dim= " << numDim << endl;

   RCP<Albany::Layouts> dl = rcp(new Albany::Layouts(worksetSize,numVertices,numNodes,numQPts,numDim));
   Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtils(dl);
   bool supportsTransient=false;

  // Temporary variable used numerous times below
  Teuchos::RCP<PHX::Evaluator<AlbanyTraits> > ev;

   // Define Field Names

   Teuchos::ArrayRCP<string> dof_names(neq);
     dof_names[0] = "U";
     dof_names[1] = "V";

   Teuchos::ArrayRCP<string> dof_names_dot(neq);
   if (supportsTransient) {
     for (int i=0; i<neq; i++) dof_names_dot[i] = dof_names[i]+"_dot";
   }

   Teuchos::ArrayRCP<string> resid_names(neq);
     for (int i=0; i<neq; i++) resid_names[i] = dof_names[i]+" Residual";

   if (supportsTransient) fm0.template registerEvaluator<EvalT>
       (evalUtils.constructGatherSolutionEvaluator(false, dof_names, dof_names_dot));
   else fm0.template registerEvaluator<EvalT>
       (evalUtils.constructGatherSolutionEvaluator_noTransient(false, dof_names));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructScatterResidualEvaluator(false, resid_names));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructGatherCoordinateVectorEvaluator());

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructMapToPhysicalFrameEvaluator(cellType, cubature));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructComputeBasisFunctionsEvaluator(cellType, intrepidBasis, cubature));

   for (int i=0; i<neq; i++) {
     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFInterpolationEvaluator(dof_names[i]));

     if (supportsTransient)
     fm0.template registerEvaluator<EvalT>
         (evalUtils.constructDOFInterpolationEvaluator(dof_names_dot[i]));

     fm0.template registerEvaluator<EvalT>
       (evalUtils.constructDOFGradInterpolationEvaluator(dof_names[i]));
  }

  if (haveSource) { // Source on U (Real) equation
    RCP<ParameterList> p = rcp(new ParameterList("Helmholtz Source U"));

    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<string>("Pressure Source Name", "U GaussMonotone");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");


    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Source Functions");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::Source<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (haveSource) { // Source on V (Imag) equation
    RCP<ParameterList> p = rcp(new ParameterList("Helmholtz Source V"));

    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<string>("Pressure Source Name", "V GaussMonotone");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");


    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Source Functions");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::Source<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  { // Helmholtz Resid
    RCP<ParameterList> p = rcp(new ParameterList("Helmholtz Resid"));

    //Input
    p->set<string>("Weighted BF Name", "wBF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl->node_qp_scalar);

    p->set<string>("U Variable Name", "U");
    p->set<string>("V Variable Name", "V");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

    p->set<string>("U Gradient Variable Name", "U Gradient");
    p->set<string>("V Gradient Variable Name", "V Gradient");
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<bool>("Have Source", haveSource);
    p->set<string>("U Pressure Source Name", "U GaussMonotone");
    p->set<string>("V Pressure Source Name", "V GaussMonotone");

    p->set<double>("Ksqr", ksqr);
    p->set<RCP<ParamLib> >("Parameter Library", paramLib);

    //Output
    p->set<string>("U Residual Name", "U Residual");
    p->set<string>("V Residual Name", "V Residual");
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", dl->node_scalar);

    ev = rcp(new PHAL::HelmholtzResid<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }


  if (fieldManagerChoice == Albany::BUILD_RESID_FM)  {
    PHX::Tag<typename EvalT::ScalarT> res_tag("Scatter", dl->dummy);
    fm0.requireField<EvalT>(res_tag);
    return res_tag.clone();
  }

  else if (fieldManagerChoice == Albany::BUILD_RESPONSE_FM){ 
    Albany::ResponseUtilities<EvalT, PHAL::AlbanyTraits> respUtils(dl);
    return respUtils.constructResponses(fm0, *responseList, stateMgr);
  }

  return Teuchos::null;
}
#endif // ALBANY_HEATNONLINEARSOURCE2DPROBLEM_HPP
