//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef POISSONSEQUATIONPROBLEM_HPP
#define POISSONSEQUATIONPROBLEM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"
#include "Albany_BCUtils.hpp"
#include "ATO_OptimizationProblem.hpp"
#include "ATO_Mixture.hpp"
#include "ATO_Utils.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"
#include "PHAL_AlbanyTraits.hpp"

#ifdef ATO_USES_COGENT
#include <Cogent_Integrator.hpp>
#include <Cogent_IntegratorFactory.hpp>
#endif

namespace Albany {

  /*!
   * \brief Abstract interface for representing a 2-D finite element
   * problem.
   */
  class PoissonsEquationProblem : 
    public ATO::OptimizationProblem ,
    public virtual Albany::AbstractProblem {
  public:
  
    //! Default constructor
    PoissonsEquationProblem(
		      const Teuchos::RCP<Teuchos::ParameterList>& params_,
		      const Teuchos::RCP<ParamLib>& paramLib_,
		      const int numDim_);

    //! Destructor
    virtual ~PoissonsEquationProblem();

    //! Return number of spatial dimensions
    virtual int spatialDimension() const { return numDim; }

    //! Build the PDE instantiations, boundary conditions, and initial solution
    virtual void buildProblem(
      Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct> >  meshSpecs,
      StateManager& stateMgr);

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

    void getAllocatedStates(
      Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Kokkos::DynRankView<RealType, PHX::Device> > > > oldState_,
      Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Kokkos::DynRankView<RealType, PHX::Device> > > > newState_) const;

  private:

    //! Private to prohibit copying
    PoissonsEquationProblem(const PoissonsEquationProblem&);
    
    //! Private to prohibit copying
    PoissonsEquationProblem& operator=(const PoissonsEquationProblem&);

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
    void constructNeumannEvaluators(const Teuchos::RCP<Albany::MeshSpecsStruct>& meshSpecs);


  protected:

    int numDim;

    Teuchos::RCP<Albany::Layouts> dl;

    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Kokkos::DynRankView<RealType, PHX::Device> > > > oldState;
    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Kokkos::DynRankView<RealType, PHX::Device> > > > newState;

  };

}

#include "Albany_SolutionAverageResponseFunction.hpp"
#include "Albany_SolutionTwoNormResponseFunction.hpp"
#include "Albany_SolutionMaxValueResponseFunction.hpp"
#include "Albany_Utils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_ResponseUtilities.hpp"
#include "Albany_EvaluatorUtils.hpp"
#include "Albany_StateManager.hpp"

//#include "PHAL_Source.hpp"
#include "ATO_ScaleVector.hpp"
#include "ATO_TopologyFieldWeighting.hpp"
#include "ATO_TopologyWeighting.hpp"
#include "ATO_VectorResidual.hpp"
#include "ATO_AddForce.hpp"
#ifdef ATO_USES_COGENT
#include "ATO_ComputeBasisFunctions.hpp"
#endif
#include "PHAL_SaveStateField.hpp"

#include "Time.hpp"

template <typename EvalT>
Teuchos::RCP<const PHX::FieldTag>
Albany::PoissonsEquationProblem::constructEvaluators(
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

  // get the name of the current element block
   std::string elementBlockName = meshSpecs.ebName;

   RCP<shards::CellTopology> cellType = rcp(new shards::CellTopology (&meshSpecs.ctd));
   RCP<Intrepid2::Basis<PHX::Device, RealType, RealType> >
     intrepidBasis = Albany::getIntrepid2Basis(meshSpecs.ctd);

   const int numNodes = intrepidBasis->getCardinality();
   const int worksetSize = meshSpecs.worksetSize;


   int cubatureDegree = meshSpecs.cubatureDegree;

   std::string blockType = "Body";

#ifdef ATO_USES_COGENT
   bool isNonconformal = false;
   Teuchos::ParameterList geomSpec, blockSpec;
   if(params->isSublist("Configuration")){
     if(params->sublist("Configuration").isType<bool>("Nonconformal"))
       isNonconformal = params->sublist("Configuration").get<bool>("Nonconformal");
   }
   RCP<Cogent::Integrator> projector;
   if(isNonconformal){
     // find geom spec
     Teuchos::ParameterList& blocksParams = params->sublist("Configuration").sublist("Element Blocks");
     int nBlocks = blocksParams.get<int>("Number of Element Blocks");
     std::string specName;
     for(int i=0; i<nBlocks; i++){
       std::string specName_i = Albany::strint("Element Block", i);
       blockSpec = blocksParams.sublist(specName_i);
       if( blockSpec.get<std::string>("Name") == elementBlockName ){
         geomSpec = blockSpec.sublist("Geometry Construction");
         if( geomSpec.get<bool>("Uniform Quadrature") ){
           specName = specName_i;
           break;
         } else {
           TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                                    "Nonconformal method requires 'Uniform Quadrature'");
         }
       }
     }
     Cogent::IntegratorFactory integratorFactory;
     projector = integratorFactory.create(cellType, intrepidBasis, geomSpec);

     int projectionOrder = geomSpec.get<int>("Projection Order");
     cubatureDegree = 2*projectionOrder;

     blockType = geomSpec.get<std::string>("Geometry Type");

   }
#endif

   Intrepid2::DefaultCubatureFactory cubFactory;
   RCP <Intrepid2::Cubature<PHX::Device> > cubature = cubFactory.create<PHX::Device, RealType, RealType>(*cellType, cubatureDegree);

   const int numDim = cubature->getDimension();
   const int numQPts = cubature->getNumPoints();
   const int numVertices = cellType->getNodeCount();

   *out << "Field Dimensions: Workset=" << worksetSize 
        << ", Vertices= " << numVertices
        << ", Nodes= " << numNodes
        << ", QuadPts= " << numQPts
        << ", Dim= " << numDim << std::endl;


   // Construct standard FEM evaluators with standard field names                              
   dl = rcp(new Albany::Layouts(worksetSize,numVertices,numNodes,numQPts,numDim));

   Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtils(dl);
   ATO::Utils<EvalT, PHAL::AlbanyTraits> atoUtils(dl,numDim);

   // Temporary variable used numerous times below
   Teuchos::RCP<PHX::Evaluator<AlbanyTraits> > ev;

#ifdef ATO_USES_COGENT
   if( isNonconformal ){
     Teuchos::Array<std::string> topoNames = geomSpec.get<Teuchos::Array<std::string> >("Level Set Names");
     int numNames = topoNames.size();
     for(int i=0; i<numNames; i++){
       RCP<ParameterList> p = rcp(new ParameterList);
       Albany::StateStruct::MeshFieldEntity entity = Albany::StateStruct::NodalDataToElemNode;
       p = stateMgr.registerStateVariable(topoNames[i], dl->node_scalar, "all", true, &entity);
     }
   }
#endif

   std::string boundaryTermName("Boundary Value");

   Teuchos::ArrayRCP<std::string> dof_names(1);
   dof_names[0] = "Phi";
   Teuchos::ArrayRCP<std::string> resid_names(1);
   resid_names[0] = dof_names[0]+" Residual";


   std::string kinVarName("kinVar");
   std::string gradPhiName(dof_names[0]+" Gradient");

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructDOFInterpolationEvaluator(dof_names[0]));

   // computes gradPhi
   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructDOFGradInterpolationEvaluator(dof_names[0]));

   fm0.template registerEvaluator<EvalT>
       (evalUtils.constructGatherSolutionEvaluator_noTransient(/*is_vector_dof=*/ false, dof_names));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructGatherCoordinateVectorEvaluator());

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructMapToPhysicalFrameEvaluator(cellType, cubature));

#ifdef ATO_USES_COGENT
   if( isNonconformal ){
     RCP<ParameterList> p = rcp(new ParameterList("Compute Basis Functions"));

     //Input
     p->set< Albany::StateManager* >("State Manager Ptr", &stateMgr );
     p->set<bool>("Static Topology", true);

     p->set< RCP<Cogent::Integrator> >("Cubature",     projector);
     p->set<std::string>("Coordinate Vector Name",   "Coord Vec");
     p->set<std::string>("Weights Name",               "Weights");
     p->set<std::string>("Jacobian Det Name",     "Jacobian Det");
     p->set<std::string>("Jacobian Name",             "Jacobian");
     p->set<std::string>("Jacobian Inv Name",     "Jacobian Inv");
     p->set<std::string>("BF Name",                         "BF");
     p->set<std::string>("Weighted BF Name",               "wBF");
     p->set<std::string>("Gradient BF Name",           "Grad BF");
     p->set<std::string>("Weighted Gradient BF Name", "wGrad BF");
 
     ev = rcp(new ATO::ComputeBasisFunctions<EvalT,AlbanyTraits>(*p,dl,&meshSpecs));
     fm0.template registerEvaluator<EvalT>(ev);

     atoUtils.SaveCellStateField(fm0, stateMgr, "Weights", elementBlockName, dl->qp_scalar);
 
   } else
#endif

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructComputeBasisFunctionsEvaluator(cellType, intrepidBasis, cubature));





  { // Time
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<std::string>("Time Name", "Time");
    p->set<std::string>("Delta Time Name", "Delta Time");
    p->set< RCP<DataLayout> >("Workset Scalar Data Layout", dl->workset_scalar);
    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    p->set<bool>("Disable Transient", true);

    ev = rcp(new LCM::Time<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("Time",dl->workset_scalar, dl->dummy, 
                                       elementBlockName, "scalar", 0.0, true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if( blockType == "Body" )
  { // Linear isotropic material response
    RCP<ParameterList> p = rcp(new ParameterList(kinVarName));

    //Input
    p->set<std::string>("Input Vector Name", gradPhiName);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);


    // check for multiple element block specs
    Teuchos::ParameterList& configParams = params->sublist("Configuration");

    if( configParams.isSublist("Element Blocks") ){
      Teuchos::ParameterList& blocksParams = configParams.sublist("Element Blocks");
      int nblocks = blocksParams.get<int>("Number of Element Blocks");
      bool blockFound = false;
      for(int ib=0; ib<nblocks; ib++){
        Teuchos::ParameterList& blockParams = blocksParams.sublist(Albany::strint("Element Block", ib));
        std::string blockName = blockParams.get<std::string>("Name");
        if( blockName != elementBlockName ) continue;
        blockFound = true;

        // user can specify a material or a mixture
        if( blockParams.isSublist("Material") ){
          // parse material
          Teuchos::ParameterList& materialParams = blockParams.sublist("Material",false);
          if( materialParams.isSublist("Homogenized Constants") ){
            Teuchos::ParameterList& homogParams = p->sublist("Homogenized Constants",false);
            homogParams.setParameters(materialParams.sublist("Homogenized Constants",true));
            p->set<Albany::StateManager*>("State Manager", &stateMgr);
            p->set<Teuchos::RCP<Albany::Layouts> >("Data Layout", dl);
          } else {
            p->set<double>("Coefficient", materialParams.get<double>("Isotropic Modulus"));
          }
          //Output
          p->set<std::string>("Output Vector Name", kinVarName);
  
          ev = rcp(new ATO::ScaleVector<EvalT,AlbanyTraits>(*p));
          fm0.template registerEvaluator<EvalT>(ev);
      
          // state the strain in the state manager so for ATO
          p = stateMgr.registerStateVariable(kinVarName,dl->qp_vector, dl->dummy, 
                                             elementBlockName, "scalar", 0.0, false, false);
          ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
          fm0.template registerEvaluator<EvalT>(ev);
      
          //if(some input stuff)
          atoUtils.SaveCellStateField(fm0, stateMgr, kinVarName, elementBlockName, dl->qp_vector);

        } else
        if( blockParams.isSublist("Mixture") ){
          // parse mixture
          Teuchos::ParameterList& mixtureParams = blockParams.sublist("Mixture",false);
          int nmats = mixtureParams.get<int>("Number of Materials");

          //-- create individual materials --//
          for(int imat=0; imat<nmats; imat++){
            RCP<ParameterList> pmat = rcp(new ParameterList(*p));
            Teuchos::ParameterList& materialParams = mixtureParams.sublist(Albany::strint("Material", imat));
            if( materialParams.isSublist("Homogenized Constants") ){
              Teuchos::ParameterList& homogParams = pmat->sublist("Homogenized Constants",false);
              homogParams.setParameters(materialParams.sublist("Homogenized Constants",true));
              pmat->set<Albany::StateManager*>("State Manager", &stateMgr);
              pmat->set<Teuchos::RCP<Albany::Layouts> >("Data Layout", dl);
            } else {
              pmat->set<double>("Coefficient", materialParams.get<double>("Isotropic Modulus"));
            }
            //Output
            std::string outName = Albany::strint(kinVarName, imat);
            pmat->set<std::string>("Output Vector Name", outName);
  
            ev = rcp(new ATO::ScaleVector<EvalT,AlbanyTraits>(*pmat));
            fm0.template registerEvaluator<EvalT>(ev);
        
            // state the strain in the state manager so for ATO
            pmat = stateMgr.registerStateVariable(outName,dl->qp_vector, dl->dummy, 
                                                  elementBlockName, "scalar", 0.0, false, false);
            ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*pmat));
            fm0.template registerEvaluator<EvalT>(ev);
        
            //if(some input stuff)
            atoUtils.SaveCellStateField(fm0, stateMgr, outName, elementBlockName, dl->qp_vector);
          }

          //-- create mixture --//
          TEUCHOS_TEST_FOR_EXCEPTION( !mixtureParams.isSublist("Mixed Fields"), std::logic_error,
                                  "'Mixture' requested but no 'Fields' defined"  << std::endl <<
                                  "Add 'Fields' list");
          {
            Teuchos::ParameterList& fieldsParams = mixtureParams.sublist("Mixed Fields",false);
            int nfields = fieldsParams.get<int>("Number of Mixed Fields");


            //-- create individual mixture field evaluators --//
            for(int ifield=0; ifield<nfields; ifield++){
              Teuchos::ParameterList& fieldParams = fieldsParams.sublist(Albany::strint("Mixed Field", ifield));
              std::string fieldName = fieldParams.get<std::string>("Field Name");

              RCP<ParameterList> p = rcp(new ParameterList(fieldName + " Mixed Field"));

              std::string fieldLayout = fieldParams.get<std::string>("Field Layout");
              p->set<std::string>("Field Layout", fieldLayout);

              std::string mixtureRule = fieldParams.get<std::string>("Rule Type");

              // currently only SIMP-type mixture is implemented
              Teuchos::ParameterList& simpParams = fieldParams.sublist(mixtureRule);
 
              Teuchos::RCP<ATO::TopologyArray> 
                topologyArray = params->get<Teuchos::RCP<ATO::TopologyArray> >("Topologies");
              p->set<Teuchos::RCP<ATO::TopologyArray> > ("Topologies", topologyArray);

              // topology and function indices
              p->set<Teuchos::Array<int> >("Topology Indices", 
                                           simpParams.get<Teuchos::Array<int> >("Topology Indices"));
              p->set<Teuchos::Array<int> >("Function Indices", 
                                           simpParams.get<Teuchos::Array<int> >("Function Indices"));

              // constituent var names
              Teuchos::Array<int> matIndices = simpParams.get<Teuchos::Array<int> >("Material Indices");
              int nMats = matIndices.size();
              Teuchos::Array<int> topoIndices = simpParams.get<Teuchos::Array<int> >("Topology Indices");
              int nTopos = topoIndices.size();
              TEUCHOS_TEST_FOR_EXCEPTION(nMats != nTopos+1, std::logic_error, std::endl <<
                                        "For SIMP Mixture, 'Materials' list must be 1 longer than 'Topologies' list"
                                        << std::endl);
              Teuchos::Array<std::string> constituentNames(nMats);
              for(int imat=0; imat<nmats; imat++){
                std::string constituentName = Albany::strint(fieldName, matIndices[imat]);
                constituentNames[imat] = constituentName;
              }
              p->set<Teuchos::Array<std::string> >("Constituent Variable Names", constituentNames);
              
              // mixture var name
              p->set<std::string>("Mixture Variable Name",fieldName);

              // basis functions
              p->set<std::string>("BF Name", "BF");
    
              ev = rcp(new ATO::Mixture<EvalT,AlbanyTraits>(*p,dl));
              fm0.template registerEvaluator<EvalT>(ev);
            }
          }
        } else
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                                  "'Material' or 'Mixture' not specified for '" 
                                  << elementBlockName << "'");
      }
      TEUCHOS_TEST_FOR_EXCEPTION(!blockFound, std::logic_error,
                                 "Material definition for block named '" << elementBlockName << "' not found");
    } else {





   
      if( params->isType<int>("Add Cell Problem Forcing") )
        p->set<int>("Cell Forcing Column", params->get<int>("Add Cell Problem Forcing") );
  
      if( params->isSublist("Homogenized Constants") ){
        Teuchos::ParameterList& homogParams = p->sublist("Homogenized Constants",false);
        homogParams.setParameters(params->sublist("Homogenized Constants",true));
        p->set<Albany::StateManager*>("State Manager", &stateMgr);
        p->set<Teuchos::RCP<Albany::Layouts> >("Data Layout", dl);
      } else {
        p->set<double>("Coefficient", params->get<double>("Isotropic Modulus"));
      }
  
      //Output
      p->set<std::string>("Output Vector Name", kinVarName);
  
      ev = rcp(new ATO::ScaleVector<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
  
      // state the kinetic variable in the state manager for ATO
      p = stateMgr.registerStateVariable(kinVarName,dl->qp_vector, dl->dummy, 
                                         elementBlockName, "scalar", 0.0, false, false);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
    
      //if(some input stuff)
      atoUtils.SaveCellStateField(fm0, stateMgr, kinVarName, elementBlockName, dl->qp_vector);
    }
  } else
  if( blockType == "Boundary" )
#ifdef ATO_USES_COGENT
  {
    // Boundary forces
    //
    if(params->isSublist("Implicit Boundary Conditions")){
      Teuchos::ParameterList& bcSpec = params->sublist("Implicit Boundary Conditions");
      atoUtils.constructBoundaryConditionEvaluators( bcSpec, fm0, stateMgr, elementBlockName, boundaryTermName );
    }
  }
#else
  {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                               "Cogent not enabled. 'Boundary' block type not available.");
  }
#endif

  /*******************************************************************************/
  /** Begin topology weighting ***************************************************/
  /*******************************************************************************/
  if(params->isType<Teuchos::RCP<ATO::TopologyArray> >("Topologies"))
  {
    Teuchos::RCP<ATO::TopologyArray>
      topologyArray = params->get<Teuchos::RCP<ATO::TopologyArray> >("Topologies");

    Teuchos::ParameterList& wfParams = params->sublist("Apply Topology Weight Functions");
    int nfields = wfParams.get<int>("Number of Fields");
    for(int ifield=0; ifield<nfields; ifield++){
      Teuchos::ParameterList& fieldParams = wfParams.sublist(Albany::strint("Field", ifield));

      int topoIndex  = fieldParams.get<int>("Topology Index");
      std::string fieldName  = fieldParams.get<std::string>("Name");
      std::string layoutName = fieldParams.get<std::string>("Layout");
      int functionIndex      = fieldParams.get<int>("Function Index");

      std::string reqBlockType;
      if( fieldParams.isType<std::string>("Type") )
        reqBlockType = fieldParams.get<std::string>("Type");
      else
        reqBlockType = "Body";

      if( reqBlockType != blockType ) continue;

      Teuchos::RCP<PHX::DataLayout> layout;
      if( layoutName == "QP Scalar" ) layout = dl->qp_scalar;
      else
      if( layoutName == "QP Vector" ) layout = dl->qp_vector;
      else
      if( layoutName == "QP Tensor" ) layout = dl->qp_tensor;

      Teuchos::RCP<ATO::Topology> topology = (*topologyArray)[topoIndex];

      // Get distributed parameter
      if( topology->getEntityType() == "Distributed Parameter" ){
        RCP<ParameterList> p = rcp(new ParameterList("Distributed Parameter"));
        p->set<std::string>("Parameter Name", topology->getName());
        ev = rcp(new PHAL::GatherScalarNodalParameter<EvalT,AlbanyTraits>(*p, dl) );
        fm0.template registerEvaluator<EvalT>(ev);
      }
  
      RCP<ParameterList> p = rcp(new ParameterList("TopologyWeighting"));

      p->set<Teuchos::RCP<ATO::Topology> >("Topology",topology);

      p->set<std::string>("BF Name", "BF");
      p->set<std::string>("Unweighted Variable Name", fieldName);
      p->set<std::string>("Weighted Variable Name", fieldName+"_Weighted");
      p->set<std::string>("Variable Layout", layoutName);
      p->set<int>("Function Index", functionIndex);

      if( topology->getEntityType() == "Distributed Parameter" )
        ev = rcp(new ATO::TopologyFieldWeighting<EvalT,AlbanyTraits>(*p,dl));
      else
        ev = rcp(new ATO::TopologyWeighting<EvalT,AlbanyTraits>(*p,dl));
  
      fm0.template registerEvaluator<EvalT>(ev);

      //if(some input stuff)
      atoUtils.SaveCellStateField(fm0, stateMgr, fieldName+"_Weighted", 
                                  elementBlockName, layout);
    }
  }
  /*******************************************************************************/
  /** End topology weighting *****************************************************/
  /*******************************************************************************/

  if( blockType == "Body" )
  { // Residual
    RCP<ParameterList> p = rcp(new ParameterList("Residual"));

    p->set<bool>("Disable Transient", true);

    //Input
    if( params->isType<Teuchos::RCP<ATO::TopologyArray> >("Topologies") )
      p->set<std::string>("Vector Name", kinVarName+"_Weighted");
    else
      p->set<std::string>("Vector Name", kinVarName);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<std::string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

    //Output
    p->set<std::string>("Residual Name", resid_names[0]);
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", dl->node_scalar);

    ev = rcp(new ATO::VectorResidual<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  } else
  if( blockType == "Boundary" )
#ifdef ATO_USES_COGENT
  {
    RCP<ParameterList> p = rcp(new ParameterList("Boundary Values"));
    if( params->isType<Teuchos::RCP<ATO::TopologyArray> > ("Topologies") )
      p->set<std::string>("Scalar Name", boundaryTermName+"_Weighted");
    else 
    p->set<std::string>("Scalar Name", boundaryTermName);
    p->set<std::string>("Weighted BF Name", "wBF");
    resid_names[0] = boundaryTermName;
    p->set<std::string>("Out Residual Name", resid_names[0]);
    p->set< RCP<DataLayout> >("Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("Weighted BF Data Layout", dl->node_qp_scalar);
    p->set< RCP<DataLayout> >("Node Scalar Data Layout", dl->node_scalar);
    p->set<bool>("Negative",true);
    ev = rcp(new ATO::AddScalar<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  
  }
#else
  {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                               "Cogent not enabled. 'Boundary' block type not available.");
  }
#endif

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructScatterResidualEvaluator(/*is_vector_dof=*/ false, resid_names));

  if (fieldManagerChoice == Albany::BUILD_RESID_FM)  {
    PHX::Tag<typename EvalT::ScalarT> res_tag("Scatter", dl->dummy);
    fm0.requireField<EvalT>(res_tag);

    return res_tag.clone();
  }
  else if (fieldManagerChoice == Albany::BUILD_RESPONSE_FM) {
    Albany::ResponseUtilities<EvalT, PHAL::AlbanyTraits> respUtils(dl);
    return respUtils.constructResponses(fm0, *responseList, params, stateMgr, &meshSpecs);
  }

  return Teuchos::null;
}

#endif
