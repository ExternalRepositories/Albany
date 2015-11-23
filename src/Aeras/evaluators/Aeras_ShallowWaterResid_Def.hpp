//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Teuchos_RCP.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Sacado_ParameterRegistration.hpp"
#include "PHAL_Utilities.hpp"

#include "Intrepid_FunctionSpaceTools.hpp"
#include "Aeras_ShallowWaterConstants.hpp"

#include "Shards_CellTopologyData.h"
namespace Aeras {


#define ALBANY_KOKKOS_UNDER_DEVELOPMENT


//**********************************************************************
template<typename EvalT, typename Traits>
ShallowWaterResid<EvalT, Traits>::
ShallowWaterResid(const Teuchos::ParameterList& p,
		const Teuchos::RCP<Albany::Layouts>& dl) :
		wBF      (p.get<std::string> ("Weighted BF Name"), dl->node_qp_scalar),
		wGradBF  (p.get<std::string> ("Weighted Gradient BF Name"),dl->node_qp_gradient),
		U        (p.get<std::string> ("QP Variable Name"), dl->node_vector),
		UNodal   (p.get<std::string> ("Nodal Variable Name"), dl->node_vector),
		UDotDotNodal   (p.get<std::string> ("Time Dependent Nodal Variable Name"), dl->node_vector),
		UDot     (p.get<std::string> ("QP Time Derivative Variable Name"), dl->node_vector),
		UDotDot     (p.get<std::string> ("Time Dependent Variable Name"), dl->node_vector),
		cellType      (p.get<Teuchos::RCP <shards::CellTopology> > ("Cell Type")),
		mountainHeight  (p.get<std::string> ("Aeras Surface Height QP Variable Name"), dl->qp_scalar),
		jacobian_inv  (p.get<std::string>  ("Jacobian Inv Name"), dl->qp_tensor ),
		jacobian_det  (p.get<std::string>  ("Jacobian Det Name"), dl->qp_scalar ),
		weighted_measure (p.get<std::string>  ("Weights Name"),   dl->qp_scalar ),
		jacobian  (p.get<std::string>  ("Jacobian Name"), dl->qp_tensor ),
		source    (p.get<std::string> ("Shallow Water Source QP Variable Name"), dl->qp_vector),
		Residual (p.get<std::string> ("Residual Name"), dl->node_vector),
		intrepidBasis (p.get<Teuchos::RCP<Intrepid::Basis<RealType, Intrepid::FieldContainer<RealType> > > > ("Intrepid Basis") ),
		cubature      (p.get<Teuchos::RCP <Intrepid::Cubature<RealType> > >("Cubature")),
		spatialDim(p.get<std::size_t>("spatialDim")),
		sphere_coord  (p.get<std::string>  ("Spherical Coord Name"), dl->qp_gradient ),
		lambda_nodal  (p.get<std::string>  ("Lambda Coord Nodal Name"), dl->node_scalar),
		theta_nodal   (p.get<std::string>  ("Theta Coord Nodal Name"), dl->node_scalar),
		gravity (Aeras::ShallowWaterConstants::self().gravity),
		hyperviscosity (p.get<std::string> ("Hyperviscosity Name"), dl->qp_vector),
		Omega(2.0*(Aeras::ShallowWaterConstants::self().pi)/(24.*3600.)),
		RRadius(1.0/Aeras::ShallowWaterConstants::self().earthRadius),
		doNotDampRotation(true)
		{

	//OG I noticed that source(...,0) is never used, it means TC4 is broken.

	Teuchos::ParameterList* shallowWaterList = p.get<Teuchos::ParameterList*>("Shallow Water Problem");

	usePrescribedVelocity = shallowWaterList->get<bool>("Use Prescribed Velocity", false); //Default: false

	useImplHyperviscosity = shallowWaterList->get<bool>("Use Implicit Hyperviscosity", false); //Default: false

	useExplHyperviscosity = shallowWaterList->get<bool>("Use Explicit Hyperviscosity", false); //Default: false

	plotVorticity = shallowWaterList->get<bool>("Plot Vorticity", false); //Default: false

	//OG: temporary, because if tau=0, sqrt(hyperviscosity(:)) leads to nans in laplace.
	//maybe, changing order of operation, like multiplying by tau later would help?
	sHvTau = sqrt(shallowWaterList->get<double>("Hyperviscosity Tau", 0.0));

	if( useImplHyperviscosity && useExplHyperviscosity )
		TEUCHOS_TEST_FOR_EXCEPTION(true,
				Teuchos::Exceptions::InvalidParameter,"Aeras::ShallowWaterResid: " <<
				"The namelist sets useImplHyperviscosity = true and useExplHyperviscosity = true. " <<
				"Using both explicit and implicit hyperviscosity is not possible. "<<
				"Set useImplHyperviscosity or useImplHyperviscosity (or both) to false.");

	//#define ALBANY_VERBOSE

	AlphaAngle = shallowWaterList->get<double>("Rotation Angle", 0.0); //Default: 0.0

	const CellTopologyData *ctd = cellType->getCellTopologyData();
	int nNodes = ctd->node_count;
	int nDim   = ctd->dimension;
	std::string name     = ctd->name;
	size_t      len      = name.find("_");
	if (len != std::string::npos) name = name.substr(0,len);

#ifdef ALBANY_VERBOSE
	std::cout << "In Aeras::ShallowWaterResid: CellTopology is " << name << " with nodes " << nNodes
			<< "  dim " << nDim << std::endl;
	std::cout << "FullCellTopology name is " << ctd->name << std::endl;
#endif



	qpToNodeMap.resize(nNodes);
	nodeToQPMap.resize(nNodes);
	//Spectral quads
	if (name == "SpectralQuadrilateral" || name == "SpectralShellQuadrilateral") {
		if (nNodes == 4) {
			qpToNodeMap[0] = 0; qpToNodeMap[1] = 1;
			qpToNodeMap[2] = 3; qpToNodeMap[3] = 2;
			nodeToQPMap[0] = 0; nodeToQPMap[1] = 1;
			nodeToQPMap[2] = 3; nodeToQPMap[3] = 2;
		}
		else {
			for(int i=0; i<nNodes; i++) {
				qpToNodeMap[i] = i;
				nodeToQPMap[i] = i;
			}
		}
	}
	//Isoparametric quads
	else if (name == "Quadrilateral" || name == "ShellQuadrilateral") {
		TEUCHOS_TEST_FOR_EXCEPTION(true,
				Teuchos::Exceptions::InvalidParameter,"Aeras::ShallowWaterResid no longer works with isoparameteric " <<
				"Quads/ShellQuads! Please re-run with spectral elements (IKT, 7/30/2015).");
		if (nNodes == 4) {
			for(int i=0; i<nNodes; i++) {
				qpToNodeMap[i] = i;
				nodeToQPMap[i] = i;
			}
		}
		else if (nNodes == 9) {
			qpToNodeMap[0] = 0; qpToNodeMap[1] = 4; qpToNodeMap[2] = 1;
			qpToNodeMap[3] = 7; qpToNodeMap[4] = 8; qpToNodeMap[5] = 5;
			qpToNodeMap[6] = 3; qpToNodeMap[7] = 6; qpToNodeMap[8] = 2;
			nodeToQPMap[0] = 0; nodeToQPMap[1] = 2; nodeToQPMap[2] = 8;
			nodeToQPMap[3] = 6; nodeToQPMap[4] = 1; nodeToQPMap[5] = 5;
			nodeToQPMap[6] = 7; nodeToQPMap[7] = 3; nodeToQPMap[8] = 4;
		}
		else {
			TEUCHOS_TEST_FOR_EXCEPTION(true,
					Teuchos::Exceptions::InvalidParameter,"Aeras::ShallowWaterResid: qpToNodeMap and nodeToQPMap " <<
					"not implemented for quadrilateral/shellquadrilateral element with " << nNodes << ".");
		}
	}
	else {
		TEUCHOS_TEST_FOR_EXCEPTION(true,
				Teuchos::Exceptions::InvalidParameter,"Aeras::ShallowWaterResid: qpToNodeMap and nodeToQPMap " <<
				"non-quadrilateral/shellquadrilateral elements.");
	}

	this->addDependentField(U);
	this->addDependentField(UNodal);
	this->addDependentField(UDot);
	this->addDependentField(UDotDot);
	this->addDependentField(UDotDotNodal);
	this->addDependentField(wBF);
	this->addDependentField(wGradBF);
	this->addDependentField(mountainHeight);
	this->addDependentField(sphere_coord);
	this->addDependentField(hyperviscosity);
	this->addDependentField(lambda_nodal);
	this->addDependentField(theta_nodal);
	this->addDependentField(source);

	this->addDependentField(weighted_measure);
	this->addDependentField(jacobian);
	this->addDependentField(jacobian_inv);
	this->addDependentField(jacobian_det);

	this->addEvaluatedField(Residual);

	std::vector<PHX::DataLayout::size_type> dims;

	wGradBF.fieldTag().dataLayout().dimensions(dims);
	numNodes = dims[1];
	numQPs   = dims[2];
	numDims  = dims[3];

	if (nNodes != numQPs) {
		TEUCHOS_TEST_FOR_EXCEPTION(true,
				Teuchos::Exceptions::InvalidParameter, "Aeras::ShallowWaterResid must be run such that nNodes == numQPs!  "
				<<  "This does not hold: numNodes = " <<  nNodes << ", numQPs = " << numQPs << ".");
	}

	refWeights        .resize               (numQPs);
	grad_at_cub_points.resize     (numNodes, numQPs, 2);
	refPoints         .resize               (numQPs, 2);
#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
	nodal_jacobian.resize(numNodes, 2, 2);
	nodal_inv_jacobian.resize(numNodes, 2, 2);
	nodal_det_j.resize(numNodes);
	wrk_.resize(numNodes, 2);

#endif

	//can I resize all the time?
	//wrk2_.resize(numQPs);


	cubature->getCubature(refPoints, refWeights);

	intrepidBasis->getValues(grad_at_cub_points, refPoints, Intrepid::OPERATOR_GRAD);

	this->setName("Aeras::ShallowWaterResid"+PHX::typeAsString<EvalT>());


	U.fieldTag().dataLayout().dimensions(dims);
	vecDim  = dims[2];

	std::vector<PHX::DataLayout::size_type> gradDims;
	wGradBF.fieldTag().dataLayout().dimensions(gradDims);


	//  std::cout << " vecDim = " << vecDim << std::endl;
	//  std::cout << " numDims = " << numDims << std::endl;
	//  std::cout << " numQPs = " << numQPs << std::endl;
	//  std::cout << " numNodes = " << numNodes << std::endl;

	// Register Reynolds number as Sacado-ized Parameter
	Teuchos::RCP<ParamLib> paramLib = p.get<Teuchos::RCP<ParamLib> >("Parameter Library");
	this->registerSacadoParameter("Gravity", paramLib);
	this->registerSacadoParameter("Omega", paramLib);
#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
	//Allocationg additional data for Kokkos functors
	nodal_jacobian=Kokkos::View<MeshScalarT***, PHX::Device>("nodal_jacobian",numNodes,2,2);
	nodal_inv_jacobian=Kokkos::View<MeshScalarT***, PHX::Device>("nodal_inv_jacobian",numNodes,2,2);
	nodal_det_j=Kokkos::View<MeshScalarT*, PHX::Device>("nodal_det_j",numNodes);
	refWeights_Kokkos=Kokkos::View<MeshScalarT*, PHX::Device>("refWeights_Kokkos",numQPs);
	grad_at_cub_points_Kokkos=Kokkos::View<MeshScalarT***, PHX::Device>("grad_at_cub_points_Kokkos",numNodes,numQPs,2);
	refPoints_kokkos=Kokkos::View<MeshScalarT**, PHX::Device>("refPoints_Kokkos",numQPs,2);

	for (int i=0; i<numQPs; i++)
	{
		refWeights_Kokkos(i)=refWeights(i);
		for (int j=0; j<2; j++){
			refPoints_kokkos(i,j)=refPoints(i,j);
			for (int k=0; k<numNodes; k++)
				grad_at_cub_points_Kokkos(k,i,j)=grad_at_cub_points(k,i,j);
		}
	}

	std::vector<PHX::index_size_type> ddims_;
#ifdef  ALBANY_FAST_FELIX
	ddims_.push_back(ALBANY_SLFAD_SIZE);
#else
	ddims_.push_back(95);
#endif

	surf=PHX::MDField<ScalarT,Node>("surf",Teuchos::rcp(new PHX::MDALayout<Node>(numNodes)));
	surf.setFieldData(ViewFactory::buildView(surf.fieldTag(),ddims_));
	surftilde=PHX::MDField<ScalarT,Node>("surftilde",Teuchos::rcp(new PHX::MDALayout<Node>(numNodes)));
	surftilde.setFieldData(ViewFactory::buildView(surftilde.fieldTag(),ddims_));
	hgradNodes=PHX::MDField<ScalarT,QuadPoint,Dim>("hgradNodes",Teuchos::rcp(new PHX::MDALayout<QuadPoint,Dim>(numQPs,2)));
	hgradNodes.setFieldData(ViewFactory::buildView(hgradNodes.fieldTag(),ddims_));
	htildegradNodes=PHX::MDField<ScalarT,QuadPoint,Dim>("htildegradNodes",Teuchos::rcp(new PHX::MDALayout<QuadPoint,Dim>(numQPs,2)));
	htildegradNodes.setFieldData(ViewFactory::buildView(htildegradNodes.fieldTag(),ddims_));

	//og synchronize changes with latest code modifications for HV
	uX=PHX::MDField<ScalarT,Node>("uX",Teuchos::rcp(new PHX::MDALayout<Node>(numNodes)));
	uX.setFieldData(ViewFactory::buildView(surf.fieldTag(),ddims_));
	uY=PHX::MDField<ScalarT,Node>("uY",Teuchos::rcp(new PHX::MDALayout<Node>(numNodes)));
	uY.setFieldData(ViewFactory::buildView(surf.fieldTag(),ddims_));
	uZ=PHX::MDField<ScalarT,Node>("uZ",Teuchos::rcp(new PHX::MDALayout<Node>(numNodes)));
	uZ.setFieldData(ViewFactory::buildView(surf.fieldTag(),ddims_));
	utX=PHX::MDField<ScalarT,Node>("utX",Teuchos::rcp(new PHX::MDALayout<Node>(numNodes)));
	utX.setFieldData(ViewFactory::buildView(surf.fieldTag(),ddims_));
	utY=PHX::MDField<ScalarT,Node>("utY",Teuchos::rcp(new PHX::MDALayout<Node>(numNodes)));
	utY.setFieldData(ViewFactory::buildView(surf.fieldTag(),ddims_));
	utZ=PHX::MDField<ScalarT,Node>("utZ",Teuchos::rcp(new PHX::MDALayout<Node>(numNodes)));
	utZ.setFieldData(ViewFactory::buildView(surf.fieldTag(),ddims_));

	uXgradNodes=PHX::MDField<ScalarT,QuadPoint,Dim>("uXgradNodes",Teuchos::rcp(new PHX::MDALayout<QuadPoint,Dim>(numQPs,2)));
	uXgradNodes.setFieldData(ViewFactory::buildView(uXgradNodes.fieldTag(),ddims_));
	uYgradNodes=PHX::MDField<ScalarT,QuadPoint,Dim>("uYgradNodes",Teuchos::rcp(new PHX::MDALayout<QuadPoint,Dim>(numQPs,2)));
	uYgradNodes.setFieldData(ViewFactory::buildView(uYgradNodes.fieldTag(),ddims_));
	uZgradNodes=PHX::MDField<ScalarT,QuadPoint,Dim>("uZgradNodes",Teuchos::rcp(new PHX::MDALayout<QuadPoint,Dim>(numQPs,2)));
	uZgradNodes.setFieldData(ViewFactory::buildView(uZgradNodes.fieldTag(),ddims_));
	utXgradNodes=PHX::MDField<ScalarT,QuadPoint,Dim>("utXgradNodes",Teuchos::rcp(new PHX::MDALayout<QuadPoint,Dim>(numQPs,2)));
	utXgradNodes.setFieldData(ViewFactory::buildView(utXgradNodes.fieldTag(),ddims_));
	utYgradNodes=PHX::MDField<ScalarT,QuadPoint,Dim>("utYgradNodes",Teuchos::rcp(new PHX::MDALayout<QuadPoint,Dim>(numQPs,2)));
	utYgradNodes.setFieldData(ViewFactory::buildView(utYgradNodes.fieldTag(),ddims_));
	utZgradNodes=PHX::MDField<ScalarT,QuadPoint,Dim>("utZgradNodes",Teuchos::rcp(new PHX::MDALayout<QuadPoint,Dim>(numQPs,2)));
	utZgradNodes.setFieldData(ViewFactory::buildView(utZgradNodes.fieldTag(),ddims_));

	wrk1qp_scalar_scope1_ = PHX::MDField<ScalarT,QuadPoint>("wrk1qp_scalar_scope1_",Teuchos::rcp(new PHX::MDALayout<QuadPoint>(numQPs)));
	wrk1qp_scalar_scope1_.setFieldData(ViewFactory::buildView(wrk1qp_scalar_scope1_.fieldTag(),ddims_));
	wrk2qp_scalar_scope1_ = PHX::MDField<ScalarT,QuadPoint>("wrk2qp_scalar_scope1_",Teuchos::rcp(new PHX::MDALayout<QuadPoint>(numQPs)));
	wrk2qp_scalar_scope1_.setFieldData(ViewFactory::buildView(wrk2qp_scalar_scope1_.fieldTag(),ddims_));
	wrk3qp_scalar_scope1_ = PHX::MDField<ScalarT,QuadPoint>("wrk3qp_scalar_scope1_",Teuchos::rcp(new PHX::MDALayout<QuadPoint>(numQPs)));
	wrk3qp_scalar_scope1_.setFieldData(ViewFactory::buildView(wrk3qp_scalar_scope1_.fieldTag(),ddims_));
	wrk4qp_scalar_scope1_ = PHX::MDField<ScalarT,QuadPoint>("wrk4qp_scalar_scope1_",Teuchos::rcp(new PHX::MDALayout<QuadPoint>(numQPs)));
	wrk4qp_scalar_scope1_.setFieldData(ViewFactory::buildView(wrk4qp_scalar_scope1_.fieldTag(),ddims_));

	//OG Note that when the code is clean from QuadPoint, these vars can be deleted (or more precisely, the ones on QP points should be deleted).
	wrk1node_scalar_scope1_ = PHX::MDField<ScalarT,Node>("wrk1node_scalar_scope1_",Teuchos::rcp(new PHX::MDALayout<Node>(numNodes)));
	wrk1node_scalar_scope1_.setFieldData(ViewFactory::buildView(wrk1node_scalar_scope1_.fieldTag(),ddims_));
	wrk2node_scalar_scope1_ = PHX::MDField<ScalarT,Node>("wrk2node_scalar_scope1_",Teuchos::rcp(new PHX::MDALayout<Node>(numNodes)));
	wrk2node_scalar_scope1_.setFieldData(ViewFactory::buildView(wrk2node_scalar_scope1_.fieldTag(),ddims_));
	wrk3node_scalar_scope1_ = PHX::MDField<ScalarT,Node>("wrk3node_scalar_scope1_",Teuchos::rcp(new PHX::MDALayout<Node>(numNodes)));
	wrk3node_scalar_scope1_.setFieldData(ViewFactory::buildView(wrk3node_scalar_scope1_.fieldTag(),ddims_));


	wrk1node_vector_scope1_ = PHX::MDField<ScalarT,Node,Dim>("wrk1node_vector_scope1_",Teuchos::rcp(new PHX::MDALayout<Node,Dim>(numNodes,2)));
	wrk1node_vector_scope1_.setFieldData(ViewFactory::buildView(wrk1node_vector_scope1_.fieldTag(),ddims_));
	wrk2node_vector_scope1_ = PHX::MDField<ScalarT,Node,Dim>("wrk2node_vector_scope1_",Teuchos::rcp(new PHX::MDALayout<Node,Dim>(numNodes,2)));
	wrk2node_vector_scope1_.setFieldData(ViewFactory::buildView(wrk2node_vector_scope1_.fieldTag(),ddims_));

	wrk1qp_vector_scope1_ = PHX::MDField<ScalarT,QuadPoint,Dim>("wrk1qp_vector_scope1_",Teuchos::rcp(new PHX::MDALayout<QuadPoint,Dim>(numQPs,2)));
	wrk1qp_vector_scope1_.setFieldData(ViewFactory::buildView(wrk1qp_vector_scope1_.fieldTag(),ddims_));
	wrk2qp_vector_scope1_ = PHX::MDField<ScalarT,QuadPoint,Dim>("wrk2qp_vector_scope1_",Teuchos::rcp(new PHX::MDALayout<QuadPoint,Dim>(numQPs,2)));
	wrk2qp_vector_scope1_.setFieldData(ViewFactory::buildView(wrk2qp_vector_scope1_.fieldTag(),ddims_));


//	wrk3_vector_scope1_ = PHX::MDField<ScalarT,Node,VecDim>("wrk3_vector_scope1_",Teuchos::rcp(new PHX::MDALayout<Node,VecDim>(numNodes,3)));
//	wrk3_vector_scope1_.setFieldData(ViewFactory::buildView(wrk3_vector_scope1_.fieldTag(),ddims_));

	wrk1node_vector_scope2_ = PHX::MDField<ScalarT,Node,Dim>("wrk1node_vector_scope2_",Teuchos::rcp(new PHX::MDALayout<Node,Dim>(numNodes,2)));
	wrk1node_vector_scope2_.setFieldData(ViewFactory::buildView(wrk1node_vector_scope2_.fieldTag(),ddims_));


	nodeToQPMap_Kokkos=Kokkos::View<int*, PHX::Device> ("nodeToQPMap_Kokkos",nNodes);
	for (int i=0; i<nNodes; i++)
		nodeToQPMap_Kokkos(i)=nodeToQPMap[i];
#endif


		}

//**********************************************************************
template<typename EvalT, typename Traits>
void ShallowWaterResid<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
		PHX::FieldManager<Traits>& fm)
		{
	this->utils.setFieldData(U,fm);
	this->utils.setFieldData(UNodal,fm);
	this->utils.setFieldData(UDot,fm);
	this->utils.setFieldData(UDotDot,fm);
	this->utils.setFieldData(UDotDotNodal,fm);
	this->utils.setFieldData(wBF,fm);
	this->utils.setFieldData(wGradBF,fm);
	this->utils.setFieldData(mountainHeight,fm);

	this->utils.setFieldData(sphere_coord,fm);
	this->utils.setFieldData(hyperviscosity,fm);
	this->utils.setFieldData(lambda_nodal,fm);
	this->utils.setFieldData(theta_nodal,fm);
	this->utils.setFieldData(source,fm);

	this->utils.setFieldData(weighted_measure, fm);
	this->utils.setFieldData(jacobian, fm);
	this->utils.setFieldData(jacobian_inv, fm);
	this->utils.setFieldData(jacobian_det, fm);

	this->utils.setFieldData(Residual,fm);

		}

// *********************************************************************
//Kokkos functors
#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
template< typename ScalarT, typename ArrayT1,typename ArrayT2, typename ArrayJac, typename ArrayGrad>
KOKKOS_INLINE_FUNCTION
void gradient(const ArrayT1  & fieldAtNodes,
		const int &cell, ArrayT2  & gradField, ArrayJac &jacobian_inv, ArrayGrad &grad_at_cub_points_Kokkos) {

	for (int qp=0; qp < grad_at_cub_points_Kokkos.dimension(1); ++qp) {

		ScalarT gx = 0;
		ScalarT gy = 0;
		for (int node=0; node < grad_at_cub_points_Kokkos.dimension(0); ++node) {

			const typename PHAL::Ref<const ScalarT>::type
			field = fieldAtNodes(node);

			gx +=   field*grad_at_cub_points_Kokkos(node, qp,0);
			gy +=   field*grad_at_cub_points_Kokkos(node, qp,1);
		}

		gradField(qp, 0) = jacobian_inv(cell, qp, 0, 0)*gx + jacobian_inv(cell, qp, 1, 0)*gy;
		gradField(qp, 1) = jacobian_inv(cell, qp, 0, 1)*gx + jacobian_inv(cell, qp, 1, 1)*gy;
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT,typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT,Traits>::divergence3(const PHX::MDField<ScalarT,Node, Dim>  & fieldAtNodes,
		const PHX::MDField<ScalarT,QuadPoint>  & div_,
		const int & cell) const  {

	const PHX::MDField<ScalarT,Node, Dim>  &  vcontra_ = wrk1node_vector_scope2_;
	//og is it necessary?
	//vcontra.deep_copy(0);

	for (std::size_t node=0; node < numNodes; ++node) {

		const MeshScalarT & jinv00 = nodal_inv_jacobian(node, 0, 0);
		const MeshScalarT & jinv01 = nodal_inv_jacobian(node, 0, 1);
		const MeshScalarT & jinv10 = nodal_inv_jacobian(node, 1, 0);
		const MeshScalarT & jinv11 = nodal_inv_jacobian(node, 1, 1);

		vcontra_(node, 0 ) = nodal_det_j(node)*(
				jinv00*fieldAtNodes(node, 0) + jinv01*fieldAtNodes(node, 1) );
		vcontra_(node, 1 ) = nodal_det_j(node)*(
				jinv10*fieldAtNodes(node, 0)+ jinv11*fieldAtNodes(node, 1) );
	}


	for (int qp=0; qp < numQPs; ++qp) {
		for (int node=0; node < numNodes; ++node) {
			//      ScalarT tempAdd =vcontra(node, 0)*grad_at_cub_points_Kokkos(node, qp,0)
			//                  + vcontra(node, 1)*grad_at_cub_points_Kokkos(node, qp,1);
			//     Kokkos::atomic_fetch_add(&div_hU(qp), tempAdd);
			div_(qp) +=   vcontra_(node, 0)*grad_at_cub_points_Kokkos(node, qp,0)
                				  + vcontra_(node, 1)*grad_at_cub_points_Kokkos(node, qp,1);
		}

	}

	for (int qp=0; qp < numQPs; ++qp) {
		div_(qp) = div_(qp)/jacobian_det(cell,qp);
	}

}


///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT,typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT,Traits>::
gradient3(const PHX::MDField<ScalarT, Node>  & field,
		const PHX::MDField<ScalarT, QuadPoint, Dim>  & gradient_,
		const int & cell) const {

	for (std::size_t qp=0; qp < numQPs; ++qp) {

		ScalarT gx = 0;
		ScalarT gy = 0;
		for (std::size_t node=0; node < numNodes; ++node) {

			const typename PHAL::Ref<const ScalarT>::type
			field_ = field(node);
			gx += field_*grad_at_cub_points_Kokkos(node, qp,0);
			gy += field_*grad_at_cub_points_Kokkos(node, qp,1);
		}

		gradient_(qp, 0) = nodal_inv_jacobian(qp, 0, 0)*gx + nodal_inv_jacobian(qp, 1, 0)*gy;
		gradient_(qp, 1) = nodal_inv_jacobian(qp, 0, 1)*gx + nodal_inv_jacobian(qp, 1, 1)*gy;
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT,typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT,Traits>::curl3(
		const PHX::MDField<ScalarT, Node, Dim>  & field,
		const PHX::MDField<ScalarT, QuadPoint>  & curl_,
		const int &cell) const {

	const PHX::MDField<ScalarT,Node, Dim>  &  vcovar_ = wrk1node_vector_scope2_;

	for (int node=0; node < numNodes; ++node) {

		const MeshScalarT & j00 = nodal_jacobian(node, 0, 0);
		const MeshScalarT & j01 = nodal_jacobian(node, 0, 1);
		const MeshScalarT & j10 = nodal_jacobian(node, 1, 0);
		const MeshScalarT & j11 = nodal_jacobian(node, 1, 1);

		vcovar_(node, 0 ) = j00*field(node, 0) + j10*field(node, 1);
		vcovar_(node, 1 ) = j01*field(node, 0) + j11*field(node, 1);
	}

	for (int qp=0; qp < numQPs; ++qp) {
		for (int node=0; node < numNodes; ++node) {

			curl_(qp) +=  vcovar_(node, 1)*grad_at_cub_points_Kokkos(node, qp,0)
                				  - vcovar_(node, 0)*grad_at_cub_points_Kokkos(node, qp,1);
		}
		curl_(qp) = curl_(qp)/jacobian_det(cell,qp);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
product_h_uv(const PHX::MDField<ScalarT,Node, Dim>  & huv_, const int& cell) const{

	for (int node=0; node < numNodes; ++node) {
		const typename PHAL::Ref<const ScalarT>::type
		unodal0 = UNodal(cell,node,0);
		huv_(node,0)= unodal0*UNodal(cell,node,1);
		huv_(node,1)= unodal0*UNodal(cell,node,2);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
compute_Residual0(const int& cell) const
{
	const PHX::MDField<ScalarT,Node, Dim>  &  huv_ = wrk1node_vector_scope1_;
	product_h_uv(huv_, cell);

	const PHX::MDField<ScalarT,QuadPoint>  &  div_ = wrk1qp_scalar_scope1_;
	divergence3(huv_, div_, cell);

	for (int qp=0; qp < numQPs; ++qp) {
		int node = qp;
		Residual(cell,node,0) += (UDot(cell,qp,0) + div_(qp))*wBF(cell, node, qp);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
compute_h_ImplHV(const int& cell) const
{

	///impl hv
	for (std::size_t node=0; node < numNodes; ++node)
		surftilde(node) = UNodal(cell,node,3);

	gradient<ScalarT>(surftilde, cell, htildegradNodes, jacobian_inv, grad_at_cub_points_Kokkos);
	///

	for (int qp=0; qp < numQPs; ++qp) {
		for (int node=0; node < numNodes; ++node) {
			Residual(cell,node,0) -= hyperviscosity(cell,qp,0)*htildegradNodes(qp,0)*wGradBF(cell,node,qp,0)
	                        				 + hyperviscosity(cell,qp,0)*htildegradNodes(qp,1)*wGradBF(cell,node,qp,1);
		}
	}

	for (std::size_t node=0; node < numNodes; ++node)
		surf(node) = UNodal(cell,node,0);

	gradient<ScalarT>(surf, cell, hgradNodes, jacobian_inv, grad_at_cub_points_Kokkos);

	for (std::size_t qp=0; qp < numQPs; ++qp) {
		size_t node = qp;
		Residual(cell,node,3) += U(cell,qp,3)*wBF(cell,node,qp);
	}
	for (std::size_t qp=0; qp < numQPs; ++qp) {
		for (std::size_t node=0; node < numNodes; ++node) {
			Residual(cell,node,3) += hgradNodes(qp,0)*wGradBF(cell,node,qp,0)
                            				+ hgradNodes(qp,1)*wGradBF(cell,node,qp,1);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
operator() (const ShallowWaterResid_VecDim3_usePrescribedVelocity_Tag& tag, const int& cell) const
{
	fill_nodal_metrics(cell);

	compute_Residual0(cell);
	compute_Residuals12_prescribed(cell);
}


///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
operator() (const ShallowWaterResid_VecDim4_Tag& tag, const int& cell) const
{

	fill_nodal_metrics(cell);

	compute_Residual0(cell);
	compute_h_ImplHV(cell);
	compute_Residuals12_prescribed(cell);
}


///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
compute_Residuals12_prescribed(const int& cell) const
{
	for (int qp=0; qp < numQPs; ++qp) {
		// before this loop used int node = qp;
		//  ... wBF(cell, node, qp), ...Residual(cell,node,1)
		const typename PHAL::Ref<const ScalarT>::type
		wbf = wBF(cell, qp, qp);

		//OG Something should be done about this source,
		//it is there for TC4 and is hardly in use (not to mention TC4 is broken).
		Residual(cell,qp,1) += (UDot(cell,qp,1) + source(cell,qp,1))*wbf;
		Residual(cell,qp,2) += (UDot(cell,qp,2) + source(cell,qp,2))*wbf;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
compute_Residuals12_notprescribed (const int& cell) const
{

	const PHX::MDField<ScalarT, QuadPoint>  &  cor_ = wrk1qp_scalar_scope1_;
	//probably this call can be removed
	get_coriolis3(cor_, cell);
	const PHX::MDField<ScalarT, Node> & kineticenergy_ = wrk2node_scalar_scope1_;
	const PHX::MDField<ScalarT, Node> & potentialenergy_ = wrk3node_scalar_scope1_;
	const PHX::MDField<ScalarT,Node, Dim> & vectoru_ = wrk1node_vector_scope1_;

	for (int node=0; node < numNodes; ++node) {
		const typename PHAL::Ref<const ScalarT>::type
		depth = UNodal(cell,node,0) + mountainHeight(cell, nodeToQPMap_Kokkos[node]),
		ulambda = UNodal(cell, node,1),
		utheta  = UNodal(cell, node,2);

		kineticenergy_(node) = 0.5*(ulambda*ulambda + utheta*utheta);
		potentialenergy_(node) = gravity*depth;

		vectoru_(node, 0) = ulambda;
		vectoru_(node, 1) = utheta;
	}

	const PHX::MDField<ScalarT, QuadPoint> & curlU_ = wrk4qp_scalar_scope1_;
	curl3(vectoru_, curlU_, cell);

	const PHX::MDField<ScalarT, QuadPoint, Dim> & gradKineticEnergy_ = wrk1qp_vector_scope1_;
	const PHX::MDField<ScalarT, QuadPoint, Dim> & gradPotentialEnergy_ = wrk2qp_vector_scope1_;

	gradient3(kineticenergy_, gradKineticEnergy_, cell);
	gradient3(potentialenergy_, gradPotentialEnergy_, cell);

	for (int qp=0; qp < numQPs; ++qp) {
		//int node = qp;
		const typename PHAL::Ref<const ScalarT>::type
		coriolis_ = cor_(qp),
		curl_ = curlU_(qp),//  old code curl_ = curlU(qp)
		wbf_ = wBF(cell, qp, qp);

		Residual(cell,qp,1) += (   UDot(cell,qp,1) + gradKineticEnergy_(qp,0)
				+ gradPotentialEnergy_(qp,0)
				- ( coriolis_ + curl_ )*U(cell, qp, 2)
		)*wbf_;
		Residual(cell,qp,2) += (   UDot(cell,qp,2) + gradKineticEnergy_(qp,1)
				+ gradPotentialEnergy_(qp,1)
				+ ( coriolis_ + curl_ )*U(cell, qp, 1)
		)*wbf_;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
operator() (const ShallowWaterResid_VecDim3_no_usePrescribedVelocity_Tag& tag, const int& cell) const
{

	fill_nodal_metrics(cell);

	compute_Residual0(cell);
	compute_Residuals12_notprescribed(cell);
}

///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
operator() (const ShallowWaterResid_BuildLaplace_for_huv_Tag& tag, const int& cell) const
{

	/*if((j_coeff == 0)&&(m_coeff == 1)&&(workset.current_time == 0)&&(plotVorticity)){
		for (std::size_t qp=0; qp < numQPs; ++qp) {
		  for (std::size_t node=0; node < numNodes; ++node) {
		     Residual(cell,node,3) += UDot(cell,qp,3);
		  }
		}*/

	fill_nodal_metrics(cell);

	BuildLaplace_for_h(cell);
	BuildLaplace_for_uv(cell);

}

/*template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
compute_coefficients_K(const typename PHAL::Ref<const MeshScalarT>::type lam,
		                     const typename PHAL::Ref<const MeshScalarT>::type th)*/
/*
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
compute_coefficients_K(const MeshScalarT lam,
		                     const MeshScalarT th)
{
    k11 = -sin(lam);
    k12 = -sin(th)*cos(lam);
    k21 =  cos(lam);
    k22 = -sin(th)*sin(lam);
    k32 =  cos(th);
}*/

/*
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
compute_3Dvelocity(std::size_t node, const ScalarT lam, const ScalarT th, const ScalarT ulambda, const ScalarT utheta,
		const PHX::MDField<ScalarT, Node, VecDim>  & uxyz ) const
{

	const ScalarT
	k11 = -sin(lam),
	k12 = -sin(th)*cos(lam),
	k21 =  cos(lam),
	k22 = -sin(th)*sin(lam),
	k32 =  cos(th);

	uxyz(node,0) = k11*ulambda + k12*utheta;
	uxyz(node,1) = k21*ulambda + k22*utheta;
	uxyz(node,2) = k32*utheta;
}*/


template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
compute_3Dvelocity(std::size_t node, const ScalarT lam, const ScalarT th, const ScalarT ulambda, const ScalarT utheta,
		const PHX::MDField<ScalarT, Node>  & ux, const PHX::MDField<ScalarT, Node>  & uy, const PHX::MDField<ScalarT, Node>  & uz) const
{
	const ScalarT
	k11 = -sin(lam),
	k12 = -sin(th)*cos(lam),
	k21 =  cos(lam),
	k22 = -sin(th)*sin(lam),
	k32 =  cos(th);

	ux(node) = k11*ulambda + k12*utheta;
	uy(node) = k21*ulambda + k22*utheta;
	uz(node) = k32*utheta;
}

///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
BuildLaplace_for_uv (const int& cell) const
{

	/*if((j_coeff == 0)&&(m_coeff == 1)&&(workset.current_time == 0)&&(plotVorticity)){
		for (std::size_t qp=0; qp < numQPs; ++qp) {
		  for (std::size_t node=0; node < numNodes; ++node) {
		     Residual(cell,node,3) += UDot(cell,qp,3);
		  }
		}*/

//	const PHX::MDField<ScalarT, Node, VecDim>  &  uxyz_ = wrk3_vector_scope1_;
    const PHX::MDField<ScalarT, Node>  &  ux_ = wrk1node_scalar_scope1_;
    const PHX::MDField<ScalarT, Node>  &  uy_ = wrk2node_scalar_scope1_;
    const PHX::MDField<ScalarT, Node>  &  uz_ = wrk3node_scalar_scope1_;

	for (std::size_t node=0; node < numNodes; ++node) {

		const typename PHAL::Ref<const ScalarT>::type
		utlambda = UDotDotNodal(cell, node,1),
		uttheta  = UDotDotNodal(cell, node,2);

		const typename PHAL::Ref<const ScalarT>::type
		lam = lambda_nodal(cell, node),
		th = theta_nodal(cell, node);

		const ScalarT
		k11 = -sin(lam),
		k12 = -sin(th)*cos(lam),
		k21 =  cos(lam),
		k22 = -sin(th)*sin(lam),
		k32 =  cos(th);

		// compute_coefficients_K(lam,th);

		utX(node) = k11*utlambda + k12*uttheta;
		utY(node) = k21*utlambda + k22*uttheta;
		utZ(node) = k32*uttheta;

		compute_3Dvelocity(node, lam, th, utlambda, uttheta, ux_, uy_, uz_ );

	}

	gradient<ScalarT>(utX, cell, utXgradNodes, jacobian_inv, grad_at_cub_points_Kokkos);
	gradient<ScalarT>(utY, cell, utYgradNodes, jacobian_inv, grad_at_cub_points_Kokkos);
	gradient<ScalarT>(utZ, cell, utZgradNodes, jacobian_inv, grad_at_cub_points_Kokkos);

	for (std::size_t qp=0; qp < numQPs; ++qp) {
		for (std::size_t node=0; node < numNodes; ++node) {

			//OG Changing MeshScalarT to ScalarT type.
			//As a wild guess, this can be contributing to a bug in Laplace (Laplace is not symmetric?),
			//since trig. functions of lam/th are differentiated.
			const typename PHAL::Ref<const ScalarT>::type
			lam = sphere_coord(cell, qp, 0),
			th = sphere_coord(cell, qp, 1);

			//K = -sin L    -sin T cos L
			//     cos L    -sin T sin L
			//     0         cos T
			//K^{-1} = K^T
			const ScalarT
			k11 = -sin(lam),
			k12 = -sin(th)*cos(lam),
			k21 =  cos(lam),
			k22 = -sin(th)*sin(lam),
			k32 =  cos(th);

			//compute_coefficients_K(lam,th);

			Residual(cell,node,1) +=
					sHvTau*(
							k11*( utXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
							+ k21*( utYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utYgradNodes(qp,1)*wGradBF(cell,node,qp,1))
							//k31 = 0
					);


			Residual(cell,node,2) +=
					sHvTau*(
							k12*( utXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
							+ k22*( utYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utYgradNodes(qp,1)*wGradBF(cell,node,qp,1))
							+ k32*( utZgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utZgradNodes(qp,1)*wGradBF(cell,node,qp,1))
					);


			/*
              if(doNotDampRotation){
                 //adding back the first mode (in sph. harmonic basis) which corresponds to -2/R/R eigenvalue of laplace

                 Residual(cell,node,1) +=
                    -hyperviscosity(cell,qp,0)*2.0*U(cell,qp,4)*RRadius*RRadius*wBF(cell,node,qp);

                 Residual(cell,node,2) +=
                    -hyperviscosity(cell,qp,0)*2.0*U(cell,qp,5)*RRadius*RRadius*wBF(cell,node,qp);

                 Residual(cell,node,4) += -2.0*U(cell,qp,1)*wBF(cell,node,qp)*RRadius*RRadius;

                 Residual(cell,node,5) += -2.0*U(cell,qp,2)*wBF(cell,node,qp)*RRadius*RRadius;
              } */
		}
	}

}


///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
operator() (const ShallowWaterResid_BuildLaplace_for_h_Tag& tag, const int& cell) const
{
	fill_nodal_metrics(cell);
	BuildLaplace_for_h(cell);
}


///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
BuildLaplace_for_h (const int& cell) const
{

	//laplace forming for h field

	for (std::size_t node=0; node < numNodes; ++node)
		surftilde(node) = UDotDotNodal(cell,node,0);

	gradient<ScalarT>(surftilde, cell, htildegradNodes, jacobian_inv, grad_at_cub_points_Kokkos);

	for (std::size_t qp=0; qp < numQPs; ++qp) {
		for (std::size_t node=0; node < numNodes; ++node) {

			Residual(cell,node,0) += sHvTau*htildegradNodes(qp,0)*wGradBF(cell,node,qp,0)
                                				  +  sHvTau*htildegradNodes(qp,1)*wGradBF(cell,node,qp,1);

		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
operator() (const ShallowWaterResid_VecDim6_Tag& tag, const int& cell) const
{
	fill_nodal_metrics(cell);

	compute_Residual0(cell);
	compute_h_ImplHV(cell);
	compute_Residuals12_notprescribed(cell);
	compute_uv_ImplHV(cell);
}


///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT, typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT, Traits>::
compute_uv_ImplHV (const int& cell) const
{
	for (int node=0; node < numNodes; ++node) {

		const typename PHAL::Ref<const ScalarT>::type
		ulambda = UNodal(cell, node,1),
		utheta  = UNodal(cell, node,2),
		utlambda = UNodal(cell, node,4),
		uttheta = UNodal(cell, node,5);

		const typename PHAL::Ref<const ScalarT>::type
		lam = lambda_nodal(cell, node),
		th = theta_nodal(cell, node);


		const ScalarT
		k11 = -sin(lam),
		k12 = -sin(th)*cos(lam),
		k21 =  cos(lam),
		k22 = -sin(th)*sin(lam),
		k32 =  cos(th);
		//compute_coefficients_K(lam,th);

		uX(node) = k11*ulambda + k12*utheta;
		uY(node) = k21*ulambda + k22*utheta;
		uZ(node) = k32*utheta;

		utX(node) = k11*utlambda + k12*uttheta;
		utY(node) = k21*utlambda + k22*uttheta;
		utZ(node) = k32*uttheta;
	}

	gradient<ScalarT>(uX, cell, uXgradNodes, jacobian_inv, grad_at_cub_points_Kokkos);
	gradient<ScalarT>(uY, cell, uYgradNodes, jacobian_inv, grad_at_cub_points_Kokkos);
	gradient<ScalarT>(uZ, cell, uZgradNodes, jacobian_inv, grad_at_cub_points_Kokkos);

	gradient<ScalarT>(utX, cell, utXgradNodes, jacobian_inv, grad_at_cub_points_Kokkos);
	gradient<ScalarT>(utY, cell, utYgradNodes, jacobian_inv, grad_at_cub_points_Kokkos);
	gradient<ScalarT>(utZ, cell, utZgradNodes, jacobian_inv, grad_at_cub_points_Kokkos);

	for (int qp=0; qp < numQPs; ++qp) {
		for (int node=0; node < numNodes; ++node) {

			const typename PHAL::Ref<const ScalarT>::type
			lam = sphere_coord(cell, qp, 0),
			th = sphere_coord(cell, qp, 1);

			//K = -sin L    -sin T cos L
			//     cos L    -sin T sin L
			//     0         cos T
			//K^{-1} = K^T


			const ScalarT
			k11 = -sin(lam),
			k12 = -sin(th)*cos(lam),
			k21 =  cos(lam),
			k22 = -sin(th)*sin(lam),
			k32 =  cos(th);

			//compute_coefficients_K(lam,th);

			//Do not delete:
			//Consider
			//V - tensor in tensor HV formulation, not hyperviscosity coefficient,
			//assume V = [v11 v12; v21 v22] then expressions below, for Residual(cell,node,1)
			//would take form
			//     k11*( (v11*utXgradNodes(qp,0) + v12*utXgradNodes(qp,1))*wGradBF(cell,node,qp,0) +
			//             (v21*utXgradNodes(qp,0) + v22*utXgradNodes(qp,1))*wGradBF(cell,node,qp,1)
			//           )
			//     + k21*( (v11*utYgradNodes(qp,0) + v12*utYgradNodes(qp,1))*wGradBF(cell,node,qp,0) +
			//             (v21*utYgradNodes(qp,0) + v22*utYgradNodes(qp,1))*wGradBF(cell,node,qp,1)
			//           )
			//instead of
			//Residual(cell,node,1) -=
			//		hyperviscosity(cell,qp,0)*(
			//				k11*( utXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
			//				+ k21*( utYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utYgradNodes(qp,1)*wGradBF(cell,node,qp,1))
			//				//k31 = 0
			//		);

			Residual(cell,node,1) -=
					hyperviscosity(cell,qp,0)*(
							k11*( utXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
							+ k21*( utYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utYgradNodes(qp,1)*wGradBF(cell,node,qp,1))
							//k31 = 0
					);


			Residual(cell,node,2) -=
					hyperviscosity(cell,qp,0)*(
							k12*( utXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
							+ k22*( utYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utYgradNodes(qp,1)*wGradBF(cell,node,qp,1))
							+ k32*( utZgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utZgradNodes(qp,1)*wGradBF(cell,node,qp,1))
					);


			Residual(cell,node,4) += U(cell,qp,4)*wBF(cell,node,qp)
                				  + k11*( uXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + uXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
								  + k21*( uYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + uYgradNodes(qp,1)*wGradBF(cell,node,qp,1));
			//k31 = 0

			Residual(cell,node,5) += U(cell,qp,5)*wBF(cell,node,qp)
							+ k12*( uXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + uXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
							+ k22*( uYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + uYgradNodes(qp,1)*wGradBF(cell,node,qp,1))
							+ k32*( uZgradNodes(qp,0)*wGradBF(cell,node,qp,0) + uZgradNodes(qp,1)*wGradBF(cell,node,qp,1));

			if(doNotDampRotation){
				//adding back the first mode (in sph. harmonic basis) which corresponds to -2/R/R eigenvalue of laplace

				Residual(cell,node,1) +=
						-hyperviscosity(cell,qp,0)*2.0*U(cell,qp,4)*RRadius*RRadius*wBF(cell,node,qp);

				Residual(cell,node,2) +=
						-hyperviscosity(cell,qp,0)*2.0*U(cell,qp,5)*RRadius*RRadius*wBF(cell,node,qp);

				Residual(cell,node,4) += -2.0*U(cell,qp,1)*wBF(cell,node,qp)*RRadius*RRadius;

				Residual(cell,node,5) += -2.0*U(cell,qp,2)*wBF(cell,node,qp)*RRadius*RRadius;
			}

		}
	}
}


#endif
//**********************************************************************
template<typename EvalT, typename Traits>
void ShallowWaterResid<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{

	double j_coeff = workset.j_coeff;
	double m_coeff = workset.m_coeff;
	double n_coeff = workset.n_coeff;

    obtainLaplaceOp = (n_coeff == 1) ? true : false;

	//  MeshScalarT k11, k12, k21, k22, k32;




#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
	PHAL::set(Residual, 0.0);

#ifdef ALBANY_VERBOSE
	std::cout << "In SW_resid: j_coeff, m_coeff, n_coeff: " << j_coeff << ", " << m_coeff << ", " << n_coeff << std::endl;
#endif 

	//Note that vars huAtNodes, div_hU, ... below are redefined locally here.
	//Global vars with such names exist too (see constructor).
	Intrepid::FieldContainer<ScalarT>  huAtNodes(numNodes,2);
	Intrepid::FieldContainer<ScalarT>  div_hU(numQPs);
	Intrepid::FieldContainer<ScalarT>  kineticEnergyAtNodes(numNodes);
	Intrepid::FieldContainer<ScalarT>  gradKineticEnergy(numQPs,2);
	Intrepid::FieldContainer<ScalarT>  potentialEnergyAtNodes(numNodes);
	Intrepid::FieldContainer<ScalarT>  gradPotentialEnergy(numQPs,2);
	Intrepid::FieldContainer<ScalarT>  uAtNodes(numNodes, 2);
	Intrepid::FieldContainer<ScalarT>  curlU(numQPs);
	Intrepid::FieldContainer<ScalarT>  coriolis(numQPs);

	//container for surface height for viscosty
	Intrepid::FieldContainer<ScalarT> surf(numNodes);
	Intrepid::FieldContainer<ScalarT> surftilde(numNodes);
	//conteiner for surface height gradient for viscosity
	Intrepid::FieldContainer<ScalarT> hgradNodes(numQPs,2);
	Intrepid::FieldContainer<ScalarT> htildegradNodes(numQPs,2);

	//auxiliary vars, (u,v) in lon-lat is transformed to (ux,uy,uz) in XYZ
	Intrepid::FieldContainer<ScalarT> uX(numNodes);
	Intrepid::FieldContainer<ScalarT> uY(numNodes);
	Intrepid::FieldContainer<ScalarT> uZ(numNodes);

	//auxiliary vars, (utilde,vtilde) in lon-lat is transformed to (utx,uty,utz) in XYZ
	Intrepid::FieldContainer<ScalarT> utX(numNodes);
	Intrepid::FieldContainer<ScalarT> utY(numNodes);
	Intrepid::FieldContainer<ScalarT> utZ(numNodes);

	Intrepid::FieldContainer<ScalarT> uXgradNodes(numQPs,2);
	Intrepid::FieldContainer<ScalarT> uYgradNodes(numQPs,2);
	Intrepid::FieldContainer<ScalarT> uZgradNodes(numQPs,2);
	Intrepid::FieldContainer<ScalarT> utXgradNodes(numQPs,2);
	Intrepid::FieldContainer<ScalarT> utYgradNodes(numQPs,2);
	Intrepid::FieldContainer<ScalarT> utZgradNodes(numQPs,2);

	for (std::size_t cell=0; cell < workset.numCells; ++cell) {

		// Depth Equation (Eq# 0)
		huAtNodes.initialize();
		div_hU.initialize();

		surf.initialize();
		surftilde.initialize();
		hgradNodes.initialize();
		htildegradNodes.initialize();

		for (std::size_t node=0; node < numNodes; ++node) {
			ScalarT surfaceHeight = UNodal(cell,node,0);
			ScalarT ulambda = UNodal(cell, node,1);
			ScalarT utheta  = UNodal(cell, node,2);
			huAtNodes(node,0) = surfaceHeight*ulambda;
			huAtNodes(node,1) = surfaceHeight*utheta;
		}

		//OG this is a bug
		//for (std::size_t node=0; node < numNodes; ++node)
		//  surf(node) = UNodal(cell,node,0);
		//gradient(surf, cell, hgradNodes);


		//In case of Explicit Hyperviscosity we form Laplace operator if omega=n=1 .
		//This code should not be executed if hv coefficient is zero, the check
		//is in Albany_SolverFactory.

		if (useExplHyperviscosity) {
			//OG: maybe, int(n_coeff) == 1 ?
			if(n_coeff == 1){

				for (std::size_t node=0; node < numNodes; ++node)
					surftilde(node) = UDotDotNodal(cell,node,0);
				gradient(surftilde, cell, htildegradNodes);

				for (std::size_t qp=0; qp < numQPs; ++qp) {
					for (std::size_t node=0; node < numNodes; ++node) {

						Residual(cell,node,0) += sHvTau*htildegradNodes(qp,0)*wGradBF(cell,node,qp,0)
                                		  +  sHvTau*htildegradNodes(qp,1)*wGradBF(cell,node,qp,1);

						//OG: This doesn't quite work when hvTau=0=hyperviscosity(:,:,:) .
						//In case of hvTau = 0, sqrt(hyperviscosity(:))=[0 | nan nan ...] and laplace op. below contains nans as well.
						//My best guess is that this is due to automatic differentiation.
						//Residual(cell,node,0) += sqrt(hyperviscosity(cell,qp,0))*htildegradNodes(qp,0)*wGradBF(cell,node,qp,0)
						//                      +  sqrt(hyperviscosity(cell,qp,0))*htildegradNodes(qp,1)*wGradBF(cell,node,qp,1);

					}
				}
			}//end if ncoeff==1

			//OG: this is a patch to fix vorticity field Residual(..,..,3)
			//for backward Euler. This adds a nontrivial block to the mass matrix that is stored to compute
			//a hyperviscosity  update for residual, LM^{-1}L. Since L contains zero block for vorticity
			//variable, the residual does not change with this modification. But nans in M^{-1} are avoided.
			//This if-statement may not be the best to detect the stage of computing M.
			if((j_coeff == 0)&&(m_coeff == 1)&&(workset.current_time == 0)&&(plotVorticity)&&(!usePrescribedVelocity)){
				for (std::size_t qp=0; qp < numQPs; ++qp) {
					for (std::size_t node=0; node < numNodes; ++node) {
						Residual(cell,node,3) += UDot(cell,qp,3);
					}
				}
			}


		}//end of Laplace forming for h field

		if (useImplHyperviscosity) {
			for (std::size_t node=0; node < numNodes; ++node)
				surftilde(node) = UNodal(cell,node,3);
			gradient(surftilde, cell, htildegradNodes);
		}


		divergence(huAtNodes, cell, div_hU);

		for (std::size_t qp=0; qp < numQPs; ++qp) {
			std::size_t node = qp;
			Residual(cell,node,0) += UDot(cell,qp,0)*wBF(cell, node, qp)
                            		+  div_hU(qp)*wBF(cell, node, qp);
		}

		if (useImplHyperviscosity) { //hyperviscosity residual(0) = residual(0) - tau*grad(htilde)*grad(phi)
			//for tensor HV, hyperViscosity is (cell, qp, 2,2)
			//so the code below is temporary
			for (std::size_t qp=0; qp < numQPs; ++qp) {
				for (std::size_t node=0; node < numNodes; ++node) {
					Residual(cell,node,0) -= hyperviscosity(cell,qp,0)*htildegradNodes(qp,0)*wGradBF(cell,node,qp,0)
                                		+  hyperviscosity(cell,qp,0)*htildegradNodes(qp,1)*wGradBF(cell,node,qp,1);
				}
			}
		}

	}

	if (useImplHyperviscosity) { //hyperviscosity residual(3) = htilde*phi + grad(h)*grad(phi)

		for (std::size_t cell=0; cell < workset.numCells; ++cell) {
			surf.initialize();
			hgradNodes.initialize();
			for (std::size_t node=0; node < numNodes; ++node)
				surf(node) = UNodal(cell,node,0);
			gradient(surf, cell, hgradNodes);

			for (std::size_t qp=0; qp < numQPs; ++qp) {
				std::size_t node = qp;
				Residual(cell,node,3) += U(cell,qp,3)*wBF(cell,node,qp);
			}
			for (std::size_t qp=0; qp < numQPs; ++qp) {
				for (std::size_t node=0; node < numNodes; ++node) {
					Residual(cell,node,3) += hgradNodes(qp,0)*wGradBF(cell,node,qp,0)
                                		  + hgradNodes(qp,1)*wGradBF(cell,node,qp,1);
				}
			}
		}
	}// endif use hyperviscosity
	// Velocity Equations
	if (usePrescribedVelocity) {
		for (std::size_t cell=0; cell < workset.numCells; ++cell) {
			for (std::size_t qp=0; qp < numQPs; ++qp) {
				std::size_t node = qp;
				Residual(cell,node,1) += UDot(cell,qp,1)*wBF(cell,node,qp) + source(cell,qp,1)*wBF(cell, node, qp);
				Residual(cell,node,2) += UDot(cell,qp,2)*wBF(cell,node,qp) + source(cell,qp,2)*wBF(cell, node, qp);
			}
		}
	}
	else { // Solve for velocity

		// Velocity Equations (Eq# 1,2)
		for (std::size_t cell=0; cell < workset.numCells; ++cell) {

			potentialEnergyAtNodes.initialize();
			gradPotentialEnergy.initialize();

			kineticEnergyAtNodes.initialize();
			gradKineticEnergy.initialize();
			uAtNodes.initialize();

			get_coriolis(cell, coriolis);

			if((useExplHyperviscosity)&&(n_coeff == 1)) {
				//uX.initialize();
				//uY.initialize();
				//uZ.initialize();
				utX.initialize();
				utY.initialize();
				utZ.initialize();
				//uXgradNodes.initialize();
				//uYgradNodes.initialize();
				//uZgradNodes.initialize();
				utXgradNodes.initialize();
				utYgradNodes.initialize();
				utZgradNodes.initialize();
			}
			if(useImplHyperviscosity) {
				uX.initialize();
				uY.initialize();
				uZ.initialize();
				utX.initialize();
				utY.initialize();
				utZ.initialize();
				uXgradNodes.initialize();
				uYgradNodes.initialize();
				uZgradNodes.initialize();
				utXgradNodes.initialize();
				utYgradNodes.initialize();
				utZgradNodes.initialize();
			}


			for (std::size_t node=0; node < numNodes; ++node) {
				ScalarT depth = UNodal(cell,node,0) + mountainHeight(cell, nodeToQPMap[node]);
				ScalarT ulambda = UNodal(cell, node,1);
				ScalarT utheta  = UNodal(cell, node,2);
				kineticEnergyAtNodes(node) = 0.5*(ulambda*ulambda + utheta*utheta);

				potentialEnergyAtNodes(node) = gravity*depth;

				uAtNodes(node, 0) = ulambda;
				uAtNodes(node, 1) = utheta;

				ScalarT utlambda;
				ScalarT uttheta;

				if((useExplHyperviscosity)&&(n_coeff == 1)) {
					utlambda = UDotDotNodal(cell, node,1);
					uttheta  = UDotDotNodal(cell, node,2);
				}

				if(useImplHyperviscosity) {
					utlambda = UNodal(cell, node,4);
					uttheta  = UNodal(cell, node,5);
				}

				if((useExplHyperviscosity)&&(n_coeff == 1)) {
					const typename PHAL::Ref<const MeshScalarT>::type
					lam = lambda_nodal(cell, node),
					th = theta_nodal(cell, node);

					const MeshScalarT
					k11 = -sin(lam),
					k12 = -sin(th)*cos(lam),
					k21 =  cos(lam),
					k22 = -sin(th)*sin(lam),
					k32 =  cos(th);

					//uX(node) = k11*ulambda + k12*utheta;
					//uY(node) = k21*ulambda + k22*utheta;
					//uZ(node) = k32*utheta;

					utX(node) = k11*utlambda + k12*uttheta;
					utY(node) = k21*utlambda + k22*uttheta;
					utZ(node) = k32*uttheta;

				}

				if(useImplHyperviscosity) {
					const typename PHAL::Ref<const MeshScalarT>::type
					lam = lambda_nodal(cell, node),
					th = theta_nodal(cell, node);

					const MeshScalarT
					k11 = -sin(lam),
					k12 = -sin(th)*cos(lam),
					k21 =  cos(lam),
					k22 = -sin(th)*sin(lam),
					k32 =  cos(th);

					uX(node) = k11*ulambda + k12*utheta;
					uY(node) = k21*ulambda + k22*utheta;
					uZ(node) = k32*utheta;

					utX(node) = k11*utlambda + k12*uttheta;
					utY(node) = k21*utlambda + k22*uttheta;
					utZ(node) = k32*uttheta;

				}

			}

			if ((useExplHyperviscosity)&&(n_coeff == 1)) {
				//gradient(uX, cell, uXgradNodes);
				//gradient(uY, cell, uYgradNodes);
				//gradient(uZ, cell, uZgradNodes);

				gradient(utX, cell, utXgradNodes);
				gradient(utY, cell, utYgradNodes);
				gradient(utZ, cell, utZgradNodes);
			}

			if (useImplHyperviscosity){
				gradient(uX, cell, uXgradNodes);
				gradient(uY, cell, uYgradNodes);
				gradient(uZ, cell, uZgradNodes);

				gradient(utX, cell, utXgradNodes);
				gradient(utY, cell, utYgradNodes);
				gradient(utZ, cell, utZgradNodes);
			}

			gradient(potentialEnergyAtNodes, cell, gradPotentialEnergy);

			gradient(kineticEnergyAtNodes, cell, gradKineticEnergy);
			curl(uAtNodes, cell, curlU);

			if(plotVorticity){
				if(useImplHyperviscosity){
					for (std::size_t qp=0; qp < numQPs; ++qp)
						for (std::size_t node=0; node < numNodes; ++node)
							Residual(cell,node,6) += (U(cell,qp,6) - curlU(qp))*wBF(cell,node,qp);
				}else{
					for (std::size_t qp=0; qp < numQPs; ++qp)
						for (std::size_t node=0; node < numNodes; ++node)
							Residual(cell,node,3) += (U(cell,qp,3) - curlU(qp))*wBF(cell,node,qp);
				}
			}

			for (std::size_t qp=0; qp < numQPs; ++qp) {
				size_t node = qp;
				Residual(cell,node,1) += (   UDot(cell,qp,1) + gradKineticEnergy(qp,0)
						+ gradPotentialEnergy(qp,0)
						- ( coriolis(qp) + curlU(qp) )*U(cell, qp, 2)
				)*wBF(cell,node,qp);
				Residual(cell,node,2) += (   UDot(cell,qp,2) + gradKineticEnergy(qp,1)
						+ gradPotentialEnergy(qp,1)
						+ ( coriolis(qp) + curlU(qp) )*U(cell, qp, 1)
				)*wBF(cell,node,qp);
			}
			if (useImplHyperviscosity) {
				for (std::size_t qp=0; qp < numQPs; ++qp) {
					for (std::size_t node=0; node < numNodes; ++node) {
						const typename PHAL::Ref<const MeshScalarT>::type
						lam = sphere_coord(cell, qp, 0),
						th = sphere_coord(cell, qp, 1);

						//K = -sin L    -sin T cos L
								//     cos L    -sin T sin L
								//     0         cos T
								//K^{-1} = K^T
								const MeshScalarT
								k11 = -sin(lam),
								k12 = -sin(th)*cos(lam),
								k21 =  cos(lam),
								k22 = -sin(th)*sin(lam),
								k32 =  cos(th);

								//Do not delete:
								//Consider
								//V - tensor in tensor HV formulation, not hyperviscosity coefficient,
								//assume V = [v11 v12; v21 v22] then expressions below, for Residual(cell,node,1)
								//would take form
								/*     k11*( (v11*utXgradNodes(qp,0) + v12*utXgradNodes(qp,1))*wGradBF(cell,node,qp,0) +
             (v21*utXgradNodes(qp,0) + v22*utXgradNodes(qp,1))*wGradBF(cell,node,qp,1)
           )
     + k21*( (v11*utYgradNodes(qp,0) + v12*utYgradNodes(qp,1))*wGradBF(cell,node,qp,0) + 
             (v21*utYgradNodes(qp,0) + v22*utYgradNodes(qp,1))*wGradBF(cell,node,qp,1)
           )
								 */


								Residual(cell,node,1) -=
										hyperviscosity(cell,qp,0)*(
												k11*( utXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
												+ k21*( utYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utYgradNodes(qp,1)*wGradBF(cell,node,qp,1))
												//k31 = 0
										);


								Residual(cell,node,2) -=
										hyperviscosity(cell,qp,0)*(
												k12*( utXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
												+ k22*( utYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utYgradNodes(qp,1)*wGradBF(cell,node,qp,1))
												+ k32*( utZgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utZgradNodes(qp,1)*wGradBF(cell,node,qp,1))
										);


								Residual(cell,node,4) += U(cell,qp,4)*wBF(cell,node,qp)
                		  + k11*( uXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + uXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
						  + k21*( uYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + uYgradNodes(qp,1)*wGradBF(cell,node,qp,1));
								//k31 = 0

										Residual(cell,node,5) += U(cell,qp,5)*wBF(cell,node,qp)
                		  + k12*( uXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + uXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
						  + k22*( uYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + uYgradNodes(qp,1)*wGradBF(cell,node,qp,1))
						  + k32*( uZgradNodes(qp,0)*wGradBF(cell,node,qp,0) + uZgradNodes(qp,1)*wGradBF(cell,node,qp,1));

										if(doNotDampRotation){
											//adding back the first mode (in sph. harmonic basis) which corresponds to -2/R/R eigenvalue of laplace

											Residual(cell,node,1) +=
													-hyperviscosity(cell,qp,0)*2.0*U(cell,qp,4)*RRadius*RRadius*wBF(cell,node,qp);

											Residual(cell,node,2) +=
													-hyperviscosity(cell,qp,0)*2.0*U(cell,qp,5)*RRadius*RRadius*wBF(cell,node,qp);

											Residual(cell,node,4) += -2.0*U(cell,qp,1)*wBF(cell,node,qp)*RRadius*RRadius;

											Residual(cell,node,5) += -2.0*U(cell,qp,2)*wBF(cell,node,qp)*RRadius*RRadius;
										}
					}
				}
			}//end if ImplHV



			if ((useExplHyperviscosity)&&(n_coeff == 1)) {
				for (std::size_t qp=0; qp < numQPs; ++qp) {
					for (std::size_t node=0; node < numNodes; ++node) {

						const typename PHAL::Ref<const MeshScalarT>::type
						lam = sphere_coord(cell, qp, 0),
						th = sphere_coord(cell, qp, 1);

						//K = -sin L    -sin T cos L
								//     cos L    -sin T sin L
						//     0         cos T
						//K^{-1} = K^T
						const MeshScalarT
						k11 = -sin(lam),
						k12 = -sin(th)*cos(lam),
						k21 =  cos(lam),
						k22 = -sin(th)*sin(lam),
						k32 =  cos(th);

						//Do not delete:
						//Consider
						//V - tensor in tensor HV formulation, not hyperviscosity coefficient,
						//assume V = [v11 v12; v21 v22] then expressions below, for Residual(cell,node,1)
						//would take form
						/*     k11*( (v11*utXgradNodes(qp,0) + v12*utXgradNodes(qp,1))*wGradBF(cell,node,qp,0) +
               (v21*utXgradNodes(qp,0) + v22*utXgradNodes(qp,1))*wGradBF(cell,node,qp,1)
             )
       + k21*( (v11*utYgradNodes(qp,0) + v12*utYgradNodes(qp,1))*wGradBF(cell,node,qp,0) +
               (v21*utYgradNodes(qp,0) + v22*utYgradNodes(qp,1))*wGradBF(cell,node,qp,1)
             )
						 */


						Residual(cell,node,1) +=
								sHvTau*(
										k11*( utXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
										+ k21*( utYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utYgradNodes(qp,1)*wGradBF(cell,node,qp,1))
										//k31 = 0
								);


						Residual(cell,node,2) +=
								sHvTau*(
										k12*( utXgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utXgradNodes(qp,1)*wGradBF(cell,node,qp,1))
										+ k22*( utYgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utYgradNodes(qp,1)*wGradBF(cell,node,qp,1))
										+ k32*( utZgradNodes(qp,0)*wGradBF(cell,node,qp,0) + utZgradNodes(qp,1)*wGradBF(cell,node,qp,1))
								);

						if(plotVorticity) Residual(cell,node,3) += 0.0;
						/*
              if(doNotDampRotation){
                 //adding back the first mode (in sph. harmonic basis) which corresponds to -2/R/R eigenvalue of laplace

                 Residual(cell,node,1) +=
                    -hyperviscosity(cell,qp,0)*2.0*U(cell,qp,4)*RRadius*RRadius*wBF(cell,node,qp);

                 Residual(cell,node,2) +=
                    -hyperviscosity(cell,qp,0)*2.0*U(cell,qp,5)*RRadius*RRadius*wBF(cell,node,qp);

                 Residual(cell,node,4) += -2.0*U(cell,qp,1)*wBF(cell,node,qp)*RRadius*RRadius;

                 Residual(cell,node,5) += -2.0*U(cell,qp,2)*wBF(cell,node,qp)*RRadius*RRadius;
              } */
					}
				}
			}//end if ExplHV



		} // end workset cell loop
	} //end if !prescribedVelocities
#else
	a = Aeras::ShallowWaterConstants::self().earthRadius;
	myPi = Aeras::ShallowWaterConstants::self().pi;

	Kokkos::deep_copy(Residual.get_kokkos_view(), ScalarT(0.0));
	//every policy should start with fill_nodal_metrics
	if (usePrescribedVelocity) {
		if (useImplHyperviscosity)
			Kokkos::parallel_for(ShallowWaterResid_VecDim4_Policy(0,workset.numCells),*this);
		else if (useExplHyperviscosity)

			if( obtainLaplaceOp ){
				Kokkos::parallel_for(ShallowWaterResid_BuildLaplace_for_h_Policy(0,workset.numCells),*this);
			}else
				Kokkos::parallel_for(ShallowWaterResid_VecDim3_usePrescribedVelocity_Policy(0,workset.numCells),*this);

		else
			Kokkos::parallel_for(ShallowWaterResid_VecDim3_usePrescribedVelocity_Policy(0,workset.numCells),*this);

	}
	else {
		if (useImplHyperviscosity)
			Kokkos::parallel_for(ShallowWaterResid_VecDim6_Policy(0,workset.numCells),*this);
		else if (useExplHyperviscosity)

			if( obtainLaplaceOp ){
				Kokkos::parallel_for(ShallowWaterResid_BuildLaplace_for_huv_Policy(0,workset.numCells),*this);
			}else{
				Kokkos::parallel_for(ShallowWaterResid_VecDim3_no_usePrescribedVelocity_Policy(0,workset.numCells),*this);
			}

		else
			Kokkos::parallel_for(ShallowWaterResid_VecDim3_no_usePrescribedVelocity_Policy(0,workset.numCells),*this);
	}

#endif
}

//**********************************************************************
// Provide Access to Parameter for sensitivity/optimization/UQ
template<typename EvalT,typename Traits>
typename ShallowWaterResid<EvalT,Traits>::ScalarT&
ShallowWaterResid<EvalT,Traits>::getValue(const std::string &n)
{
	if (n=="Gravity") return gravity;
	else if (n=="Omega") return Omega;
}

#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
template<typename EvalT,typename Traits>
void
ShallowWaterResid<EvalT,Traits>::divergence(const Intrepid::FieldContainer<ScalarT>  & fieldAtNodes,
		std::size_t cell, Intrepid::FieldContainer<ScalarT>  & div) {

	Intrepid::FieldContainer<ScalarT>& vcontra = wrk_;
	vcontra.initialize();

	fill_nodal_metrics(cell);

	vcontra.initialize();
	div.initialize();

	for (std::size_t node=0; node < numNodes; ++node) {

		const MeshScalarT jinv00 = nodal_inv_jacobian(node, 0, 0);
		const MeshScalarT jinv01 = nodal_inv_jacobian(node, 0, 1);
		const MeshScalarT jinv10 = nodal_inv_jacobian(node, 1, 0);
		const MeshScalarT jinv11 = nodal_inv_jacobian(node, 1, 1);

		vcontra(node, 0 ) = nodal_det_j(node)*(
				jinv00*fieldAtNodes(node, 0) + jinv01*fieldAtNodes(node, 1) );
		vcontra(node, 1 ) = nodal_det_j(node)*(
				jinv10*fieldAtNodes(node, 0)+ jinv11*fieldAtNodes(node, 1) );
	}


	for (std::size_t qp=0; qp < numQPs; ++qp) {
		for (std::size_t node=0; node < numNodes; ++node) {

			div(qp) +=   vcontra(node, 0)*grad_at_cub_points(node, qp,0)
                		  + vcontra(node, 1)*grad_at_cub_points(node, qp,1);
		}

	}

	for (std::size_t qp=0; qp < numQPs; ++qp) {
		div(qp) = div(qp)/jacobian_det(cell,qp);
	}

	//  for(size_t v = 0; v < numNodes; ++v) {
	//    for(size_t q = 0; q < numQPs; ++q) {
	//      div(q) += jacobian_inv(cell,q,0,0)*grad_at_cub_points(v, q, 0)*fieldAtNodes(v, 0) +
	//          jacobian_inv(cell,q,0,1)*grad_at_cub_points(v, q, 0)*fieldAtNodes(v, 1) +
	//          jacobian_inv(cell,q,1,0)*grad_at_cub_points(v, q, 1)*fieldAtNodes(v, 0) +
	//          jacobian_inv(cell,q,1,1)*grad_at_cub_points(v, q, 1)*fieldAtNodes(v, 1);
	//
	//    }
	//  }
}
#endif

//**********************************************************************
#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
template<typename EvalT,typename Traits>
void
ShallowWaterResid<EvalT,Traits>::gradient(const Intrepid::FieldContainer<ScalarT>  & fieldAtNodes,
		std::size_t cell, Intrepid::FieldContainer<ScalarT>  & gradField) {

	gradField.initialize();

	for (std::size_t qp=0; qp < numQPs; ++qp) {

		ScalarT gx = 0;
		ScalarT gy = 0;
		for (std::size_t node=0; node < numNodes; ++node) {

			gx +=   fieldAtNodes(node)*grad_at_cub_points(node, qp,0);
			gy +=   fieldAtNodes(node)*grad_at_cub_points(node, qp,1);
		}

		gradField(qp, 0) = jacobian_inv(cell, qp, 0, 0)*gx + jacobian_inv(cell, qp, 1, 0)*gy;
		gradField(qp, 1) = jacobian_inv(cell, qp, 0, 1)*gx + jacobian_inv(cell, qp, 1, 1)*gy;
	}

}
#endif

// *********************************************************************
#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
template<typename EvalT,typename Traits>
void
ShallowWaterResid<EvalT,Traits>::fill_nodal_metrics(std::size_t cell) {

	nodal_jacobian.initialize();
	nodal_det_j.initialize();
	nodal_inv_jacobian.initialize();

	for (size_t v = 0; v < numNodes; ++v) {
		int qp = nodeToQPMap[v];

		for (size_t b1 = 0; b1 < 2; ++b1) {
			for (size_t b2 = 0; b2 < 2; ++b2) {

				nodal_jacobian(v, b1, b2) = jacobian(cell, qp,b1, b2);
				nodal_inv_jacobian(v, b1, b2) = jacobian_inv(cell, qp,b1, b2);
			}
		}
		nodal_det_j(v) = jacobian_det(cell, qp);
	}
	return;

}
#endif
// *********************************************************************
//Kokkos functors
#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
template<typename EvalT,typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT,Traits>::fill_nodal_metrics(const int &cell) const {

	for (size_t v = 0; v < numNodes; ++v) {
		int qp = nodeToQPMap_Kokkos[v];

		for (size_t b1 = 0; b1 < 2; ++b1) {
			for (size_t b2 = 0; b2 < 2; ++b2) {

				nodal_jacobian(v, b1, b2) = jacobian(cell, qp,b1, b2);
				nodal_inv_jacobian(v, b1, b2) = jacobian_inv(cell, qp,b1, b2);
			}
		}
		nodal_det_j(v) = jacobian_det(cell, qp);
	}
	return;

}
#endif
// *********************************************************************

#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
//og: rename this to vorticity
template<typename EvalT,typename Traits>
void
ShallowWaterResid<EvalT,Traits>::curl(const Intrepid::FieldContainer<ScalarT>  & nodalVector,
		std::size_t cell, Intrepid::FieldContainer<ScalarT>  & curl) {

	Intrepid::FieldContainer<ScalarT>& covariantVector = wrk_;
	covariantVector.initialize();

	fill_nodal_metrics(cell);

	covariantVector.initialize();
	curl.initialize();

	for (std::size_t node=0; node < numNodes; ++node) {

		const MeshScalarT j00 = nodal_jacobian(node, 0, 0);
		const MeshScalarT j01 = nodal_jacobian(node, 0, 1);
		const MeshScalarT j10 = nodal_jacobian(node, 1, 0);
		const MeshScalarT j11 = nodal_jacobian(node, 1, 1);

		covariantVector(node, 0 ) = j00*nodalVector(node, 0) + j10*nodalVector(node, 1);
		covariantVector(node, 1 ) = j01*nodalVector(node, 0) + j11*nodalVector(node, 1);
	}


	for (std::size_t qp=0; qp < numQPs; ++qp) {
		for (std::size_t node=0; node < numNodes; ++node) {

			curl(qp) +=   covariantVector(node, 1)*grad_at_cub_points(node, qp,0)
                		  - covariantVector(node, 0)*grad_at_cub_points(node, qp,1);
		}
		curl(qp) = curl(qp)/jacobian_det(cell,qp);
	}
}
#endif

// *********************************************************************

#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
template<typename EvalT,typename Traits>
void
ShallowWaterResid<EvalT,Traits>::get_coriolis(std::size_t cell, Intrepid::FieldContainer<ScalarT>  & coriolis) {

	coriolis.initialize();
	double alpha = AlphaAngle; /*must match what is in initial condition for TC2 and TC5.
                      //see AAdatpt::AerasZonal analytic function. */

	for (std::size_t qp=0; qp < numQPs; ++qp) {
		const MeshScalarT lambda = sphere_coord(cell, qp, 0);
		const MeshScalarT theta = sphere_coord(cell, qp, 1);
		coriolis(qp) = 2*Omega*( -cos(lambda)*cos(theta)*sin(alpha) + sin(theta)*cos(alpha));
	}

}
#endif

// *********************************************************************
//Kokkos functors
#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT

///////////////////////////////////////////////////////////////////////////////////////////////
template<typename EvalT,typename Traits>
KOKKOS_INLINE_FUNCTION
void ShallowWaterResid<EvalT,Traits>::
get_coriolis3(const PHX::MDField<ScalarT,QuadPoint>  & cor_,
		const int &cell) const {

	//double alpha = AlphaAngle; //must match what is in initial condition for TC2 and TC5.
	//see AAdatpt::AerasZonal analytic function.
	//
	for (int qp=0; qp < numQPs; ++qp) {
		const MeshScalarT lambda = sphere_coord(cell, qp, 0);
		const MeshScalarT theta = sphere_coord(cell, qp, 1);
		cor_(qp) = 2*Omega*( -cos(lambda)*cos(theta)*sin(AlphaAngle) + sin(theta)*cos(AlphaAngle));
	}
}


#endif
//**********************************************************************

}
