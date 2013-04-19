//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef PROJECTIONPROBLEM_HPP
#define PROJECTIONPROBLEM_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_ResponseUtilities.hpp"
#include "Albany_EvaluatorUtils.hpp"
#include "PHAL_AlbanyTraits.hpp"
#include "Projection.hpp"

namespace Albany {

  /*!
   * \brief Problem definition for total Lagrangian solid mechanics problem with projection
   */
  class ProjectionProblem : public Albany::AbstractProblem {
  public:
  
    //! Default constructor
    ProjectionProblem(const Teuchos::RCP<Teuchos::ParameterList>& params,
			  const Teuchos::RCP<ParamLib>& paramLib,
			  const int numEq);

    //! Destructor
    virtual ~ProjectionProblem();
    
     //Set problem information for computation of rigid body modes (in src/Albany_SolverFactory.cpp)
    void getRBMInfoForML(
         int& numPDEs, int& numElasticityDim, int& numScalar, int& nullSpaceDim);

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
         Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > oldState_,
	 Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > newState_
	 ) const;

  private:

    //! Private to prohibit copying
    ProjectionProblem(const ProjectionProblem&);
    
    //! Private to prohibit copying
    ProjectionProblem& operator=(const ProjectionProblem&);

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
    int T_offset;  //Position of T unknown in nodal DOFs
    int X_offset;  //Position of X unknown in nodal DOFs, followed by Y,Z
    int numDim;    //Number of spatial dimensions and displacement variable 

    std::string matModel;

    std::string projectionVariable;
    int projectionRank;

    LCM::Projection projection;
    bool isProjectedVarVector;
    bool isProjectedVarTensor;

    std::string insertionCriteria;

    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > oldState;
    Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::RCP<Intrepid::FieldContainer<RealType> > > > newState;
  };
}

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Albany_AbstractProblem.hpp"

#include "Phalanx.hpp"
#include "PHAL_Workset.hpp"
#include "PHAL_Dimension.hpp"
#include "PHAL_AlbanyTraits.hpp"
#include "Albany_Utils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_ResponseUtilities.hpp"
#include "Albany_EvaluatorUtils.hpp"

#include "Time.hpp"
#include "Strain.hpp"
#include "DefGrad.hpp"
#include "PHAL_SaveStateField.hpp"
#include "Porosity.hpp"

#include "ElasticModulus.hpp"
#include "ShearModulus.hpp"
#include "PoissonsRatio.hpp"

#include "PHAL_Source.hpp"
#include "L2ProjectionResidual.hpp"
#include "TLElasResid.hpp"
#include "PHAL_NSMaterialProperty.hpp"

#include "J2Stress.hpp"
//#include "J2Fiber.hpp"
#include "Neohookean.hpp"
#include "PisdWdF.hpp"
#include "HardeningModulus.hpp"
#include "YieldStrength.hpp"
#include "SaturationModulus.hpp"
#include "SaturationExponent.hpp"
#include "DislocationDensity.hpp"
#include "FaceFractureCriteria.hpp"
#include "FaceAverage.hpp"



template <typename EvalT>
Teuchos::RCP<const PHX::FieldTag>
Albany::ProjectionProblem::constructEvaluators(
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
  string elementBlockName = meshSpecs.ebName;

   RCP<shards::CellTopology> cellType = rcp(new shards::CellTopology (&meshSpecs.ctd));
   RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > >
     intrepidBasis = Albany::getIntrepidBasis(meshSpecs.ctd);

   const int numNodes = intrepidBasis->getCardinality();
   const int worksetSize = meshSpecs.worksetSize;

   Intrepid::DefaultCubatureFactory<RealType> cubFactory;
   RCP <Intrepid::Cubature<RealType> > cubature = cubFactory.create(*cellType, meshSpecs.cubatureDegree);

   // Create intrepid basis and cubature for the face averaging
   // this isn't the best way of defining the basis functions - requires you to know
   //   the face type at compile time
   RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > faceAveIntrepidBasis;
      faceAveIntrepidBasis = Teuchos::rcp(
        new Intrepid::Basis_HGRAD_QUAD_C1_FEM<RealType,
            Intrepid::FieldContainer<RealType> >());
   // the quadrature is general to the topology of the faces of the volume elements
   RCP<Intrepid::Cubature<RealType> > faceAveCubature =
     cubFactory.create(cellType->getCellTopologyData()->side->topology,meshSpecs.cubatureDegree);


   const int numQPts = cubature->getNumPoints();
   const int numVertices = cellType->getNodeCount();
   const int numFaces = cellType->getFaceCount();

   *out << "Field Dimensions: Workset=" << worksetSize 
        << ", Vertices= " << numVertices
        << ", Nodes= " << numNodes
        << ", QuadPts= " << numQPts
        << ", Faces= " << numFaces
        << ", Dim= " << numDim << endl;

   // Construct standard FEM evaluators with standard field names                              
   RCP<Albany::Layouts> dl = rcp(new Albany::Layouts(worksetSize,numVertices,numNodes,numQPts,numDim));
   TEUCHOS_TEST_FOR_EXCEPTION(dl->vectorAndGradientLayoutsAreEquivalent==false, std::logic_error,
                              "Data Layout Usage in Mechanics problems assume vecDim = numDim");
   Albany::EvaluatorUtils<EvalT, PHAL::AlbanyTraits> evalUtils(dl);

   // Create a separate set of evaluators with their own data layout for use by the projection
   RCP<Albany::Layouts> dl_proj = rcp(new Albany::Layouts(worksetSize,numVertices,numNodes,numQPts,numDim,numDim*numDim,numFaces));
   Albany::EvaluatorUtils<EvalT,PHAL::AlbanyTraits> evalUtils_proj(dl_proj);
   string scatterName="Scatter Projection";


   // ----------------------setup the solution field ---------------//

   // Displacement Variable
   Teuchos::ArrayRCP<string> dof_names(1);
     dof_names[0] = "Displacement";
   Teuchos::ArrayRCP<string> resid_names(1);
     resid_names[0] = dof_names[0]+" Residual";

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructDOFVecInterpolationEvaluator(dof_names[0]));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructDOFVecGradInterpolationEvaluator(dof_names[0]));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructGatherSolutionEvaluator_noTransient(true, dof_names, X_offset));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructScatterResidualEvaluator(true, resid_names, X_offset));

  // Projected Field Variable
   Teuchos::ArrayRCP<string> tdof_names(1);
   tdof_names[0] = "Projected Field";
     //Teuchos::ArrayRCP<string> tdof_names_dot(1);
     //tdof_names_dot[0] = tdof_names[0]+"_dot";
   Teuchos::ArrayRCP<string> tresid_names(1);
     tresid_names[0] = tdof_names[0]+" Residual";

   fm0.template registerEvaluator<EvalT>
     (evalUtils_proj.constructDOFVecInterpolationEvaluator(tdof_names[0]));

   //fm0.template registerEvaluator<EvalT>
   //  (evalUtils_proj.constructDOFVecInterpolationEvaluator(tdof_names_dot[0]));

   fm0.template registerEvaluator<EvalT>
     (evalUtils_proj.constructDOFVecGradInterpolationEvaluator(tdof_names[0]));

   // Need to use different arguments depending on the rank of the projected variables
   //   see the Albany_EvaluatorUtil class for specifics
   fm0.template registerEvaluator<EvalT>
     (evalUtils_proj.constructGatherSolutionEvaluator_noTransient(
    		 isProjectedVarVector, tdof_names, T_offset));

   fm0.template registerEvaluator<EvalT>
     (evalUtils_proj.constructScatterResidualEvaluator(
    		 isProjectedVarVector, tresid_names, T_offset, scatterName));

   // ----------------------setup the solution field ---------------//

   // General FEM stuff
   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructGatherCoordinateVectorEvaluator());

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructMapToPhysicalFrameEvaluator(cellType, cubature));

   fm0.template registerEvaluator<EvalT>
     (evalUtils.constructComputeBasisFunctionsEvaluator(cellType, intrepidBasis, cubature));

   // Temporary variable used numerous times below
   Teuchos::RCP<PHX::Evaluator<AlbanyTraits> > ev;

   { // Time
     RCP<ParameterList> p = rcp(new ParameterList);

     p->set<string>("Time Name", "Time");
     p->set<string>("Delta Time Name", " Delta Time");
     p->set< RCP<DataLayout> >("Workset Scalar Data Layout", dl->workset_scalar);
     p->set<RCP<ParamLib> >("Parameter Library", paramLib);
     p->set<bool>("Disable Transient", true);

     ev = rcp(new LCM::Time<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Time",dl->workset_scalar, dl->dummy, elementBlockName, "scalar", 0.0, true);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }

   { // Strain
     RCP<ParameterList> p = rcp(new ParameterList("Strain"));

     //Input
     p->set<string>("Gradient QP Variable Name", "Displacement Gradient");

     //Output
     p->set<string>("Strain Name", "Strain"); //dl->qp_tensor also

     ev = rcp(new LCM::Strain<EvalT,AlbanyTraits>(*p,dl));
     fm0.template registerEvaluator<EvalT>(ev);
     p = stateMgr.registerStateVariable("Strain",dl->qp_tensor, dl->dummy, elementBlockName, "scalar", 0.0,true);
     ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
     fm0.template registerEvaluator<EvalT>(ev);
   }


  { // Elastic Modulus
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("QP Variable Name", "Elastic Modulus");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Elastic Modulus");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new LCM::ElasticModulus<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  { // Shear Modulus
      RCP<ParameterList> p = rcp(new ParameterList);

      p->set<string>("QP Variable Name", "Shear Modulus");
      p->set<string>("QP Coordinate Vector Name", "Coord Vec");
      p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
      p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
      p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

      p->set<RCP<ParamLib> >("Parameter Library", paramLib);
      Teuchos::ParameterList& paramList = params->sublist("Shear Modulus");
      p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

      ev = rcp(new LCM::ShearModulus<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
    }

  { // Poissons Ratio 
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("QP Variable Name", "Poissons Ratio");
    p->set<string>("QP Coordinate Vector Name", "Coord Vec");
    p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Poissons Ratio");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    // Setting this turns on linear dependence of nu on T, nu = nu_ + dnudT*T)
    //p->set<string>("QP Projected Field Name", "Projected Field");

    ev = rcp(new LCM::PoissonsRatio<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  if (matModel == "Neohookean")
    {
      { // Stress
        RCP<ParameterList> p = rcp(new ParameterList("Stress"));

        //Input
        p->set<string>("DefGrad Name", "Deformation Gradient");
        p->set<string>("Elastic Modulus Name", "Elastic Modulus");
        p->set<string>("Poissons Ratio Name", "Poissons Ratio");
        p->set<string>("DetDefGrad Name", "Jacobian");

        //Output
        p->set<string>("Stress Name", matModel);

        ev = rcp(new LCM::Neohookean<EvalT,AlbanyTraits>(*p,dl));
        fm0.template registerEvaluator<EvalT>(ev);
        p = stateMgr.registerStateVariable(matModel,dl->qp_tensor, dl->dummy, elementBlockName, "scalar", 0.0);
        ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);
      }
    }
    else if (matModel == "Neohookean AD")
    {
      RCP<ParameterList> p = rcp(new ParameterList("Stress"));

      //Input
      p->set<string>("Elastic Modulus Name", "Elastic Modulus");
      p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
      p->set<string>("Poissons Ratio Name", "Poissons Ratio");  // dl->qp_scalar also

      p->set<string>("DefGrad Name", "Deformation Gradient");
      p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

      //Output
      p->set<string>("Stress Name", matModel); //dl->qp_tensor also

      ev = rcp(new LCM::PisdWdF<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
      p = stateMgr.registerStateVariable(matModel,dl->qp_tensor, dl->dummy, elementBlockName, "scalar", 0.0);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
    }
    else if (matModel == "J2" || matModel== "J2Fiber")
    {
      { // Hardening Modulus
        RCP<ParameterList> p = rcp(new ParameterList);

        p->set<string>("QP Variable Name", "Hardening Modulus");
        p->set<string>("QP Coordinate Vector Name", "Coord Vec");
        p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
        p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
        p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

        p->set<RCP<ParamLib> >("Parameter Library", paramLib);
        Teuchos::ParameterList& paramList = params->sublist("Hardening Modulus");
        p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

        ev = rcp(new LCM::HardeningModulus<EvalT,AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);
      }

      { // Yield Strength
        RCP<ParameterList> p = rcp(new ParameterList);

        p->set<string>("QP Variable Name", "Yield Strength");
        p->set<string>("QP Coordinate Vector Name", "Coord Vec");
        p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
        p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
        p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

        p->set<RCP<ParamLib> >("Parameter Library", paramLib);
        Teuchos::ParameterList& paramList = params->sublist("Yield Strength");
        p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

        ev = rcp(new LCM::YieldStrength<EvalT,AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);
      }

      { // Saturation Modulus
        RCP<ParameterList> p = rcp(new ParameterList);

        p->set<string>("Saturation Modulus Name", "Saturation Modulus");
        p->set<string>("QP Coordinate Vector Name", "Coord Vec");
        p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
        p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
        p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

        p->set<RCP<ParamLib> >("Parameter Library", paramLib);
        Teuchos::ParameterList& paramList = params->sublist("Saturation Modulus");
        p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

        ev = rcp(new LCM::SaturationModulus<EvalT,AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);
      }

      { // Saturation Exponent
        RCP<ParameterList> p = rcp(new ParameterList);

        p->set<string>("Saturation Exponent Name", "Saturation Exponent");
        p->set<string>("QP Coordinate Vector Name", "Coord Vec");
        p->set< RCP<DataLayout> >("Node Data Layout", dl->node_scalar);
        p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
        p->set< RCP<DataLayout> >("QP Vector Data Layout", dl->qp_vector);

        p->set<RCP<ParamLib> >("Parameter Library", paramLib);
        Teuchos::ParameterList& paramList = params->sublist("Saturation Exponent");
        p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

        ev = rcp(new LCM::SaturationExponent<EvalT,AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);
      }

      if ( numDim == 3 && params->get("Compute Dislocation Density Tensor", false) )
      { // Dislocation Density Tensor
        RCP<ParameterList> p = rcp(new ParameterList("Dislocation Density"));

        //Input
        p->set<string>("Fp Name", "Fp");
        p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);
        p->set<string>("BF Name", "BF");
        p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl->node_qp_scalar);
        p->set<string>("Gradient BF Name", "Grad BF");
        p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

        //Output
        p->set<string>("Dislocation Density Name", "G"); //dl->qp_tensor also

        //Declare what state data will need to be saved (name, layout, init_type)
        ev = rcp(new LCM::DislocationDensity<EvalT,AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);
        p = stateMgr.registerStateVariable("G",dl->qp_tensor, dl->dummy, elementBlockName, "scalar", 0.0);
        ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);
      }

      if(matModel == "J2")
      {// Stress
        RCP<ParameterList> p = rcp(new ParameterList("Stress"));

        //Input
        p->set<string>("DefGrad Name", "Deformation Gradient");
        p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

        p->set<string>("Elastic Modulus Name", "Elastic Modulus");
        p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

        p->set<string>("Poissons Ratio Name", "Poissons Ratio");  // dl->qp_scalar also
        p->set<string>("Hardening Modulus Name", "Hardening Modulus"); // dl->qp_scalar also
        p->set<string>("Saturation Modulus Name", "Saturation Modulus"); // dl->qp_scalar also
        p->set<string>("Saturation Exponent Name", "Saturation Exponent"); // dl->qp_scalar also
        p->set<string>("Yield Strength Name", "Yield Strength"); // dl->qp_scalar also
        p->set<string>("DetDefGrad Name", "Jacobian");  // dl->qp_scalar also

        //Output
        p->set<string>("Stress Name", matModel); //dl->qp_tensor also
        p->set<string>("Fp Name", "Fp");  // dl->qp_tensor also
        p->set<string>("Eqps Name", "eqps");  // dl->qp_scalar also

        //Declare what state data will need to be saved (name, layout, init_type)

        ev = rcp(new LCM::J2Stress<EvalT,AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);
        p = stateMgr.registerStateVariable(matModel,dl->qp_tensor, dl->dummy, elementBlockName, "scalar", 0.0);
        ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);
        p = stateMgr.registerStateVariable("Fp",dl->qp_tensor, dl->dummy, elementBlockName, "identity", 1.0, true);
        ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);
        p = stateMgr.registerStateVariable("eqps",dl->qp_scalar, dl->dummy, elementBlockName, "scalar", 0.0, true);
        ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
        fm0.template registerEvaluator<EvalT>(ev);
      }

    }
 //   else
 //     TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
 // 			       "Unrecognized Material Name: " << matModel
 // 			       << "  Recognized names are : Neohookean and J2");




  if (haveSource) { // Source
    RCP<ParameterList> p = rcp(new ParameterList);

    p->set<string>("Source Name", "Source");
    p->set<string>("QP Variable Name", "Projected Field");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<RCP<ParamLib> >("Parameter Library", paramLib);
    Teuchos::ParameterList& paramList = params->sublist("Source Functions");
    p->set<Teuchos::ParameterList*>("Parameter List", &paramList);

    ev = rcp(new PHAL::Source<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  { // Deformation Gradient
    RCP<ParameterList> p = rcp(new ParameterList("Deformation Gradient"));

    //Inputs: flags, weights, GradU
    const bool avgJ = params->get("avgJ", false);
    p->set<bool>("avgJ Name", avgJ);
    const bool volavgJ = params->get("volavgJ", false);
    p->set<bool>("volavgJ Name", volavgJ);
    const bool weighted_Volume_Averaged_J = params->get("weighted_Volume_Averaged_J", false);
    p->set<bool>("weighted_Volume_Averaged_J Name", weighted_Volume_Averaged_J);
    p->set<string>("Weights Name","Weights");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);
    p->set<string>("Gradient QP Variable Name", "Displacement Gradient");
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    //Outputs: F, J
    p->set<string>("DefGrad Name", "Deformation Gradient"); //dl->qp_tensor also
    p->set<string>("DetDefGrad Name", "Jacobian");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    ev = rcp(new LCM::DefGrad<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("Displacement Gradient",dl->qp_tensor,
    		                            dl->dummy, elementBlockName, "identity",true);
    ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
    p = stateMgr.registerStateVariable("Jacobian",
    		                           dl->qp_scalar, dl->dummy, elementBlockName, "scalar", 1.0,true);
	ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }

  { // Displacement Resid
    RCP<ParameterList> p = rcp(new ParameterList("Displacement Resid"));

    //Input
    p->set<string>("Stress Name", matModel);
    p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl->qp_tensor);

    p->set<string>("DefGrad Name", "Deformation Gradient"); //dl->qp_tensor also

    p->set<string>("DetDefGrad Name", "Jacobian");
    p->set< RCP<DataLayout> >("QP Scalar Data Layout", dl->qp_scalar);

    p->set<string>("Weighted Gradient BF Name", "wGrad BF");
    p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl->node_qp_vector);

    p->set<string>("Weighted BF Name", "wBF");
    p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl->node_qp_scalar);
    p->set<RCP<ParamLib> >("Parameter Library", paramLib);

    p->set<bool>("Disable Transient", true);

    //Output
    p->set<string>("Residual Name", "Displacement Residual");
    p->set< RCP<DataLayout> >("Node Vector Data Layout", dl->node_vector);

    ev = rcp(new LCM::TLElasResid<EvalT,AlbanyTraits>(*p));
    fm0.template registerEvaluator<EvalT>(ev);
  }


  { // L2 projection
      RCP<ParameterList> p = rcp(new ParameterList("Projected Field Resid"));

      //Input
      p->set<string>("Weighted BF Name", "wBF");
      p->set< RCP<DataLayout> >("Node QP Scalar Data Layout", dl_proj->node_qp_scalar);

      p->set<string>("Weighted Gradient BF Name", "wGrad BF");
      p->set< RCP<DataLayout> >("Node QP Vector Data Layout", dl_proj->node_qp_vector);

      p->set<bool>("Have Source", false);
      p->set<string>("Source Name", "Source");

      p->set<string>("Projected Field Name", "Projected Field");
      p->set< RCP<DataLayout> >("QP Vector Data Layout", dl_proj->qp_vector);

      p->set<string>("Projection Field Name", projectionVariable);
      p->set< RCP<DataLayout> >("QP Tensor Data Layout", dl_proj->qp_tensor);

      //Output
      p->set<string>("Residual Name", "Projected Field Residual");
      p->set< RCP<DataLayout> >("Node Vector Data Layout", dl_proj->node_vector);

      ev = rcp(new LCM::L2ProjectionResidual<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
      p = stateMgr.registerStateVariable("Projected Field",dl_proj->qp_vector, dl_proj->dummy, elementBlockName, "scalar", 0.0, true);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
  }

  { // Fracture Criterion
	  RCP<ParameterList> p = rcp(new ParameterList("Face Fracture Criteria"));

	  // Input
      // Nodal coordinates in the reference configuration
      p->set<string>("Coordinate Vector Name","Coord Vec");
      p->set< RCP<DataLayout> >("Vertex Vector Data Layout",dl->vertices_vector);

      p->set<string>("Face Average Name","Face Average");
	  p->set< RCP<DataLayout> >("Face Vector Data Layout", dl_proj->face_vector);

	  p->set<Teuchos::RCP<shards::CellTopology> >("Cell Type",cellType);

	  RealType yield = params->sublist("Yield Strength").get("Value",0.0);
	  p->set<RealType>("Yield Name",yield);

	  RealType fractureLimit = params->sublist("Insertion Criteria").get("Fracture Limit",0.0);
	  p->set<RealType>("Fracture Limit Name",fractureLimit);

	  p->set<std::string>("Insertion Criteria Name",params->sublist("Insertion Criteria").get("Insertion Criteria",""));

	  // Output
	  p->set<string>("Criteria Met Name","Criteria Met");
	  p->set<RCP<DataLayout> >("Face Scalar Data Layout", dl_proj->face_scalar);

	  // This is in here to trick the code to run the evaluator - does absolutely nothing
	  p->set<string>("Temp2 Name","Temp2");
	  p->set< RCP<DataLayout> >("Cell Scalar Data Layout", dl_proj->cell_scalar);

	  ev = rcp(new LCM::FaceFractureCriteria<EvalT,AlbanyTraits>(*p));
	  fm0.template registerEvaluator<EvalT>(ev);
	  p = stateMgr.registerStateVariable("Temp2",dl_proj->cell_scalar,dl_proj->dummy, elementBlockName, "scalar",0.0,true);
	  ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
	  fm0.template registerEvaluator<EvalT>(ev);
  }

  { // Face Average
      RCP<ParameterList> p = rcp(new ParameterList("Face Average"));

      // Input
      // Nodal coordinates in the reference configuration
      p->set<string>("Coordinate Vector Name","Coord Vec");
      p->set< RCP<DataLayout> >("Vertex Vector Data Layout",dl->vertices_vector);

      // The solution of the projection at the nodes
      p->set<string>("Projected Field Name","Projected Field");
      p->set< RCP<DataLayout> >("Node Vector Data Layout",dl_proj->node_vector);

      // the cubature and basis function information
      p->set<Teuchos::RCP<Intrepid::Cubature<RealType> > >("Face Cubature",faceAveCubature);
      p->set<Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > >(
         "Face Intrepid Basis",faceAveIntrepidBasis);

      p->set<Teuchos::RCP<shards::CellTopology> >("Cell Type",cellType);

      // Output
      p->set<string>("Face Average Name","Face Average");
      p->set< RCP<DataLayout> >("Face Vector Data Layout", dl_proj->face_vector);

      // This is in here to trick the code to run the evaluator - does absolutely nothing
      p->set<string>("Temp Name","Temp");
      p->set< RCP<DataLayout> >("Cell Scalar Data Layout", dl_proj->cell_scalar);

      ev = rcp(new LCM::FaceAverage<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
      p = stateMgr.registerStateVariable("Temp",dl_proj->cell_scalar,dl_proj->dummy, elementBlockName, "scalar",0.0,true);
      ev = rcp(new PHAL::SaveStateField<EvalT,AlbanyTraits>(*p));
      fm0.template registerEvaluator<EvalT>(ev);
  }

  if (fieldManagerChoice == Albany::BUILD_RESID_FM)  {
    PHX::Tag<typename EvalT::ScalarT> res_tag("Scatter", dl->dummy);
    fm0.requireField<EvalT>(res_tag);

    PHX::Tag<typename EvalT::ScalarT> res_tag2(scatterName, dl->dummy);
    fm0.requireField<EvalT>(res_tag2);

    return res_tag.clone();
  }
  else if (fieldManagerChoice == Albany::BUILD_RESPONSE_FM) {
    Albany::ResponseUtilities<EvalT, PHAL::AlbanyTraits> respUtils(dl);
    return respUtils.constructResponses(fm0, *responseList, stateMgr);
  }

  return Teuchos::null;
}

#endif // PROJECTIONPROBLEM_HPP
