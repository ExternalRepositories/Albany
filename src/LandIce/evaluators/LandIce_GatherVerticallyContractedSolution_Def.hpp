//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Phalanx_Print.hpp"
#include "Sacado.hpp"

#include "Albany_ThyraUtils.hpp"
#include "Albany_GlobalLocalIndexer.hpp"
#include "Albany_AbstractDiscretization.hpp"

#include "LandIce_GatherVerticallyContractedSolution.hpp"

//uncomment the following line if you want debug output to be printed to screen
//#define OUTPUT_TO_SCREEN

namespace LandIce {

//**********************************************************************

template<typename EvalT, typename Traits>
GatherVerticallyContractedSolutionBase<EvalT, Traits>::
GatherVerticallyContractedSolutionBase(const Teuchos::ParameterList& p,
                  const Teuchos::RCP<Albany::Layouts>& dl)
{
  Teuchos::RCP<Teuchos::FancyOStream> out(Teuchos::VerboseObjectBase::getDefaultOStream());
  cell_topo = p.get<Teuchos::RCP<const CellTopologyData> >("Cell Topology");
  const auto& opType = p.get<std::string>("Contraction Operator");
  if(opType == "Vertical Sum")
    op = VerticalSum;
  else if (opType == "Vertical Average")
    op = VerticalAverage;
  else {
    TEUCHOS_TEST_FOR_EXCEPTION (true, std::runtime_error,
                                  "Error! \"" << opType << "\" is not a valid Contraction Operator. Valid Operators are: \"Vertical Sum\" and \"Vertical Average\"");
  }

  isVector =  p.get<bool>("Is Vector");

  offset = p.get<int>("Solution Offset");

  std::vector<PHX::DataLayout::size_type> dims;

  dl->node_vector->dimensions(dims);
  numNodes = dims[1];
  vecDim = isVector ? dims[2] : 1;

  meshPart = p.get<std::string>("Mesh Part");

  std::string sideSetName  = p.get<std::string> ("Side Set Name");
  TEUCHOS_TEST_FOR_EXCEPTION (dl->side_layouts.find(sideSetName)==dl->side_layouts.end(), std::runtime_error,
                              "Error! Layout for side set " << sideSetName << " not found.\n");
  Teuchos::RCP<Albany::Layouts> dl_side = dl->side_layouts.at(sideSetName);

  this->useCollapsedSidesets = dl_side->useCollapsedSidesets;

  if(isVector)
    contractedSol = decltype(contractedSol)(p.get<std::string>("Contracted Solution Name"), this->useCollapsedSidesets ? dl_side->node_vector_sideset : dl_side->node_vector);
  else
    contractedSol = decltype(contractedSol)(p.get<std::string>("Contracted Solution Name"), this->useCollapsedSidesets ? dl_side->node_scalar_sideset : dl_side->node_scalar);

  this->addEvaluatedField(contractedSol);

  this->setName("GatherVerticallyContractedSolution"+PHX::print<EvalT>());
}

//**********************************************************************

template<typename EvalT, typename Traits>
void GatherVerticallyContractedSolutionBase<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(contractedSol,fm);
  d.fill_field_dependencies(this->dependentFields(),this->evaluatedFields(),false);
}

//**********************************************************************

template<typename Traits>
GatherVerticallyContractedSolution<PHAL::AlbanyTraits::Residual, Traits>::
GatherVerticallyContractedSolution(const Teuchos::ParameterList& p,
                                 const Teuchos::RCP<Albany::Layouts>& dl)
 : GatherVerticallyContractedSolutionBase<PHAL::AlbanyTraits::Residual, Traits>(p,dl)
{
  // Nothing to do here
}

template<typename Traits>
void GatherVerticallyContractedSolution<PHAL::AlbanyTraits::Residual, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::ArrayRCP<const ST> x_constView = Albany::getLocalData(workset.x);

  Kokkos::deep_copy(this->contractedSol.get_view(), ScalarT(0.0));

  TEUCHOS_TEST_FOR_EXCEPTION(workset.sideSetViews.is_null(), std::logic_error,
                             "Side sets defined in input file but not properly specified on the mesh.\n");

  const Albany::LocalSideSetInfoList& ssList = *(workset.sideSetViews);
  Albany::LocalSideSetInfoList::const_iterator it = ssList.find(this->meshPart);

  if (it != ssList.end()) {
    const Albany::LocalSideSetInfo& sideSet = it->second;

    // Loop over the sides that form the boundary condition
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> >& wsElNodeID  = workset.disc->getWsElNodeID()[workset.wsIndex];
    const Albany::LayeredMeshNumbering<GO>& layeredMeshNumbering = *workset.disc->getLayeredMeshNumbering();
    const Albany::NodalDOFManager& solDOFManager = workset.disc->getOverlapDOFManager("ordinary_solution");

    const auto& ov_node_indexer = *workset.disc->getOverlapNodeGlobalLocalIndexer();
    const int numLayers = layeredMeshNumbering.numLayers;

    Teuchos::ArrayRCP<double> quadWeights(numLayers+1); //doing trapezoidal rule
    if(this->op == this->VerticalSum){
      quadWeights.assign(quadWeights.size(),1.0);
    } else  { //Average
      const Teuchos::ArrayRCP<double>& layers_ratio = layeredMeshNumbering.layers_ratio;
      quadWeights[0] = 0.5*layers_ratio[0]; quadWeights[numLayers] = 0.5*layers_ratio[numLayers-1];
      for(int i=1; i<numLayers; ++i)
        quadWeights[i] = 0.5*(layers_ratio[i-1] + layers_ratio[i]);
    }

    for (int sideSet_idx = 0; sideSet_idx < sideSet.size; ++sideSet_idx) { // loop over the sides on this ws and name
      // Get the data that corresponds to the side
      const int elem_LID = sideSet.elem_LID(sideSet_idx);
      const int elem_side = sideSet.side_local_id(sideSet_idx);
      const CellTopologyData_Subcell& side =  this->cell_topo->side[elem_side];
      const int numSideNodes = side.topology->node_count;

      const Teuchos::ArrayRCP<GO>& elNodeID = wsElNodeID[elem_LID];

      //we only consider elements on the top.
      GO baseId;
      for (int i = 0; i < numSideNodes; ++i) {
        const std::size_t node = side.node[i];
        baseId = layeredMeshNumbering.getColumnId(elNodeID[node]);
        std::vector<double> contrSol(this->vecDim,0);
        for(int il=0; il<numLayers+1; ++il) {
          const GO gnode = layeredMeshNumbering.getId(baseId, il);
          const LO inode = ov_node_indexer.getLocalElement(gnode);
          for(int comp=0; comp<this->vecDim; ++comp)
            contrSol[comp] += x_constView[solDOFManager.getLocalDOF(inode, comp+this->offset)]*quadWeights[il];
        }
        if(this->isVector)
        {
          for(int comp=0; comp<this->vecDim; ++comp)
          {
            if (this->useCollapsedSidesets) {
              this->contractedSol(sideSet_idx,i,comp) = contrSol[comp];
            } else {
              this->contractedSol(elem_LID,elem_side,i,comp) = contrSol[comp];
            }
          }
        }
        else
        {
          if (this->useCollapsedSidesets) {
            this->contractedSol(sideSet_idx,i) = contrSol[0];
          } else {
            this->contractedSol(elem_LID,elem_side,i) = contrSol[0];
          }
        }
      }
    }
  }
}

template<typename Traits>
GatherVerticallyContractedSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
GatherVerticallyContractedSolution(const Teuchos::ParameterList& p,
                                 const Teuchos::RCP<Albany::Layouts>& dl)
 : GatherVerticallyContractedSolutionBase<PHAL::AlbanyTraits::Jacobian, Traits>(p,dl)
{
  // Nothing to do here
}

template<typename Traits>
void GatherVerticallyContractedSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::ArrayRCP<const ST> x_constView = Albany::getLocalData(workset.x);
  
  TEUCHOS_TEST_FOR_EXCEPTION(workset.sideSetViews.is_null(), std::logic_error,
                             "Side sets defined in input file but not properly specified on the mesh.\n");

  Kokkos::deep_copy(this->contractedSol.get_view(), ScalarT(0.0));

  const Albany::LocalSideSetInfoList& ssList = *(workset.sideSetViews);
  Albany::LocalSideSetInfoList::const_iterator it = ssList.find(this->meshPart);

  if (it != ssList.end()) {
    const int neq = workset.wsElNodeEqID.extent(2);

    const Albany::LocalSideSetInfo& sideSet = it->second;

    // Loop over the sides that form the boundary condition
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> >& wsElNodeID  = workset.disc->getWsElNodeID()[workset.wsIndex];
    const Albany::LayeredMeshNumbering<GO>& layeredMeshNumbering = *workset.disc->getLayeredMeshNumbering();
    const int numLayers = layeredMeshNumbering.numLayers;
    const Albany::NodalDOFManager& solDOFManager = workset.disc->getOverlapDOFManager("ordinary_solution");
    const auto& ov_node_indexer = *workset.disc->getOverlapNodeGlobalLocalIndexer();

    Teuchos::ArrayRCP<double> quadWeights(numLayers+1);
    if(this->op == this->VerticalSum){
      quadWeights.assign(quadWeights.size(),1.0);
    } else  { //Average, doing trapezoidal rule
      const Teuchos::ArrayRCP<double>& layers_ratio = layeredMeshNumbering.layers_ratio;
      quadWeights[0] = 0.5*layers_ratio[0]; quadWeights[numLayers] = 0.5*layers_ratio[numLayers-1];
      for(int i=1; i<numLayers; ++i)
        quadWeights[i] = 0.5*(layers_ratio[i-1] + layers_ratio[i]);
    }

    for (int sideSet_idx = 0; sideSet_idx < sideSet.size; ++sideSet_idx) { // loop over the sides on this ws and name
      // Get the data that corresponds to the side
      const int elem_LID = sideSet.elem_LID(sideSet_idx);
      const int elem_side = sideSet.side_local_id(sideSet_idx);
      const CellTopologyData_Subcell& side =  this->cell_topo->side[elem_side];
      int numSideNodes = side.topology->node_count;

      const Teuchos::ArrayRCP<GO>& elNodeID = wsElNodeID[elem_LID];
      std::vector<double> velx(this->numNodes,0), vely(this->numNodes,0);

      GO baseId;
      for (int i = 0; i < numSideNodes; ++i) {
        const std::size_t node = side.node[i];
        baseId = layeredMeshNumbering.getColumnId(elNodeID[node]);
        std::vector<double> contrSol(this->vecDim,0);
        for(int il=0; il<numLayers+1; ++il) {
          const GO gnode = layeredMeshNumbering.getId(baseId, il);
          const LO inode = ov_node_indexer.getLocalElement(gnode);
          for(int comp=0; comp<this->vecDim; ++comp)
            contrSol[comp] += x_constView[solDOFManager.getLocalDOF(inode, comp+this->offset)]*quadWeights[il];
        }

        if (this->useCollapsedSidesets) {
          if(this->isVector) {
            for(int comp=0; comp<this->vecDim; ++comp) {
              this->contractedSol(sideSet_idx,i,comp) = FadType(this->contractedSol(sideSet_idx,i,comp).size(), contrSol[comp]);
              for(int il=0; il<numLayers+1; ++il)
                this->contractedSol(sideSet_idx,i,comp).fastAccessDx(neq*(this->numNodes+numSideNodes*il+i)+comp+this->offset) = quadWeights[il]*workset.j_coeff;
            }
          } else {
            this->contractedSol(sideSet_idx,i) = FadType(this->contractedSol(sideSet_idx,i).size(), contrSol[0]);
            for(int il=0; il<numLayers+1; ++il)
              this->contractedSol(sideSet_idx,i).fastAccessDx(neq*(this->numNodes+numSideNodes*il+i)+this->offset) = quadWeights[il]*workset.j_coeff;
          } 
        } else {
          if(this->isVector) {
            for(int comp=0; comp<this->vecDim; ++comp) {
              this->contractedSol(elem_LID,elem_side,i,comp) = FadType(this->contractedSol(elem_LID,elem_side,i,comp).size(), contrSol[comp]);
              for(int il=0; il<numLayers+1; ++il)
                this->contractedSol(elem_LID,elem_side,i,comp).fastAccessDx(neq*(this->numNodes+numSideNodes*il+i)+comp+this->offset) = quadWeights[il]*workset.j_coeff;
            }
          } else {
            this->contractedSol(elem_LID,elem_side,i) = FadType(this->contractedSol(elem_LID,elem_side,i).size(), contrSol[0]);
            for(int il=0; il<numLayers+1; ++il)
              this->contractedSol(elem_LID,elem_side,i).fastAccessDx(neq*(this->numNodes+numSideNodes*il+i)+this->offset) = quadWeights[il]*workset.j_coeff;
          } 
        }
      }
    }
  }
}

template<typename Traits>
GatherVerticallyContractedSolution<PHAL::AlbanyTraits::Tangent, Traits>::
GatherVerticallyContractedSolution(const Teuchos::ParameterList& p,
                                 const Teuchos::RCP<Albany::Layouts>& dl)
 : GatherVerticallyContractedSolutionBase<PHAL::AlbanyTraits::Tangent, Traits>(p,dl)
{
  // Nothing to do here
}

template<typename Traits>
void GatherVerticallyContractedSolution<PHAL::AlbanyTraits::Tangent, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::ArrayRCP<const ST> x_constView = Albany::getLocalData(workset.x);

  Kokkos::deep_copy(this->contractedSol.get_view(), ScalarT(0.0));

  TEUCHOS_TEST_FOR_EXCEPTION(workset.sideSetViews.is_null(), std::logic_error,
                             "Side sets defined in input file but not properly specified on the mesh.\n");

  const Albany::LocalSideSetInfoList& ssList = *(workset.sideSetViews);
  Albany::LocalSideSetInfoList::const_iterator it = ssList.find(this->meshPart);

  if (it != ssList.end()) {
    const Albany::LocalSideSetInfo& sideSet = it->second;

    // Loop over the sides that form the boundary condition
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> >& wsElNodeID  = workset.disc->getWsElNodeID()[workset.wsIndex];
    const Albany::LayeredMeshNumbering<GO>& layeredMeshNumbering = *workset.disc->getLayeredMeshNumbering();
    const Albany::NodalDOFManager& solDOFManager = workset.disc->getOverlapDOFManager("ordinary_solution");
    const auto& ov_node_indexer = *workset.disc->getOverlapNodeGlobalLocalIndexer();

    const int numLayers = layeredMeshNumbering.numLayers;

    Teuchos::ArrayRCP<double> quadWeights(numLayers+1);
    if(this->op == this->VerticalSum){
      quadWeights.assign(quadWeights.size(),1.0);
    } else  { //Average, doing trapezoidal rule
      const Teuchos::ArrayRCP<double>& layers_ratio = layeredMeshNumbering.layers_ratio;
      quadWeights[0] = 0.5*layers_ratio[0]; quadWeights[numLayers] = 0.5*layers_ratio[numLayers-1];
      for(int i=1; i<numLayers; ++i)
        quadWeights[i] = 0.5*(layers_ratio[i-1] + layers_ratio[i]);
    }

    for (int sideSet_idx = 0; sideSet_idx < sideSet.size; ++sideSet_idx) { // loop over the sides on this ws and name
      // Get the data that corresponds to the side
      const int elem_LID = sideSet.elem_LID(sideSet_idx);
      const int elem_side = sideSet.side_local_id(sideSet_idx);
      const CellTopologyData_Subcell& side =  this->cell_topo->side[elem_side];
      int numSideNodes = side.topology->node_count;

      const Teuchos::ArrayRCP<GO>& elNodeID = wsElNodeID[elem_LID];

      //we only consider elements on the top.
      GO baseId;
      for (int i = 0; i < numSideNodes; ++i) {
        const std::size_t node = side.node[i];
        baseId = layeredMeshNumbering.getColumnId(elNodeID[node]);
        std::vector<double> contrSol(this->vecDim,0);
        for(int il=0; il<numLayers+1; ++il) {
          const GO gnode = layeredMeshNumbering.getId(baseId, il);
          const LO inode = ov_node_indexer.getLocalElement(gnode);
          for(int comp=0; comp<this->vecDim; ++comp)
            contrSol[comp] += x_constView[solDOFManager.getLocalDOF(inode, comp+this->offset)]*quadWeights[il];
        }
        if (this->useCollapsedSidesets) {
          if(this->isVector) {
            for(int comp=0; comp<this->vecDim; ++comp)
              this->contractedSol(sideSet_idx,i,comp) = contrSol[comp];
          } else {
            this->contractedSol(sideSet_idx,i) = contrSol[0];
          }
        } else {
          if(this->isVector) {
            for(int comp=0; comp<this->vecDim; ++comp)
              this->contractedSol(elem_LID,elem_side,i,comp) = contrSol[comp];
          } else {
            this->contractedSol(elem_LID,elem_side,i) = contrSol[0];
          }
        }
        if (workset.Vx != Teuchos::null && workset.j_coeff != 0.0) {
          TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "Not Implemented yet" << std::endl);
        }
      }
    }
  }
}

template<typename Traits>
GatherVerticallyContractedSolution<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
GatherVerticallyContractedSolution(const Teuchos::ParameterList& p,
                                 const Teuchos::RCP<Albany::Layouts>& dl)
 : GatherVerticallyContractedSolutionBase<PHAL::AlbanyTraits::DistParamDeriv, Traits>(p,dl)
{
  // Nothing to do here
}

template<typename Traits>
void GatherVerticallyContractedSolution<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::ArrayRCP<const ST> x_constView = Albany::getLocalData(workset.x);

  Kokkos::deep_copy(this->contractedSol.get_view(), ScalarT(0.0));

  TEUCHOS_TEST_FOR_EXCEPTION(workset.sideSetViews.is_null(), std::logic_error,
                             "Side sets defined in input file but not properly specified on the mesh.\n");

  const Albany::LocalSideSetInfoList& ssList = *(workset.sideSetViews);
  Albany::LocalSideSetInfoList::const_iterator it = ssList.find(this->meshPart);

  if (it != ssList.end()) {
    const Albany::LocalSideSetInfo& sideSet = it->second;

    // Loop over the sides that form the boundary condition
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> >& wsElNodeID  = workset.disc->getWsElNodeID()[workset.wsIndex];
    const Albany::LayeredMeshNumbering<GO>& layeredMeshNumbering = *workset.disc->getLayeredMeshNumbering();
    const Albany::NodalDOFManager& solDOFManager = workset.disc->getOverlapDOFManager("ordinary_solution");
    const auto& ov_node_indexer = *workset.disc->getOverlapNodeGlobalLocalIndexer();

    const int numLayers = layeredMeshNumbering.numLayers;
    Teuchos::ArrayRCP<double> quadWeights(numLayers+1); //doing trapezoidal rule

    if(this->op == this->VerticalSum){
      quadWeights.assign(quadWeights.size(),1.0);
    } else  { //Average, doing trapezoidal rule
      const Teuchos::ArrayRCP<double>& layers_ratio = layeredMeshNumbering.layers_ratio;
      quadWeights[0] = 0.5*layers_ratio[0]; quadWeights[numLayers] = 0.5*layers_ratio[numLayers-1];
      for(int i=1; i<numLayers; ++i)
        quadWeights[i] = 0.5*(layers_ratio[i-1] + layers_ratio[i]);
    }

    for (int sideSet_idx = 0; sideSet_idx < sideSet.size; ++sideSet_idx) { // loop over the sides on this ws and name
      // Get the data that corresponds to the side
      const int elem_LID = sideSet.elem_LID(sideSet_idx);
      const int elem_side = sideSet.side_local_id(sideSet_idx);
      const CellTopologyData_Subcell& side =  this->cell_topo->side[elem_side];
      int numSideNodes = side.topology->node_count;

      const Teuchos::ArrayRCP<GO>& elNodeID = wsElNodeID[elem_LID];

      //we only consider elements on the top.
      GO baseId;
      for (int i = 0; i < numSideNodes; ++i) {
        const std::size_t node = side.node[i];
        baseId = layeredMeshNumbering.getColumnId(elNodeID[node]);
        std::vector<double> contrSol(this->vecDim,0);
        for(int il=0; il<numLayers+1; ++il) {
          const GO gnode = layeredMeshNumbering.getId(baseId, il);
          const LO inode = ov_node_indexer.getLocalElement(gnode);
          for(int comp=0; comp<this->vecDim; ++comp)
            contrSol[comp] += x_constView[solDOFManager.getLocalDOF(inode, comp+this->offset)]*quadWeights[il];
        }

        if (this->useCollapsedSidesets) {
          if(this->isVector) {
            for(int comp=0; comp<this->vecDim; ++comp)
              this->contractedSol(sideSet_idx,i,comp) = contrSol[comp];
          } else {
            this->contractedSol(sideSet_idx,i) = contrSol[0];
          }
        } else {
          if(this->isVector) {
            for(int comp=0; comp<this->vecDim; ++comp)
              this->contractedSol(elem_LID,elem_side,i,comp) = contrSol[comp];
          } else {
            this->contractedSol(elem_LID,elem_side,i) = contrSol[0];
          }
        }
      }
    }
  }
}

template<typename Traits>
GatherVerticallyContractedSolution<PHAL::AlbanyTraits::HessianVec, Traits>::
GatherVerticallyContractedSolution(const Teuchos::ParameterList& p,
                                 const Teuchos::RCP<Albany::Layouts>& dl)
 : GatherVerticallyContractedSolutionBase<PHAL::AlbanyTraits::HessianVec, Traits>(p,dl)
{
  // Nothing to do here
}

template<typename Traits>
void GatherVerticallyContractedSolution<PHAL::AlbanyTraits::HessianVec, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::ArrayRCP<const ST> x_constView = Albany::getLocalData(workset.x);
  Teuchos::RCP<const Thyra_MultiVector> direction_x = workset.hessianWorkset.direction_x;
  Teuchos::ArrayRCP<const ST> direction_x_constView;

  int neq = workset.wsElNodeEqID.extent(2);

  bool g_xx_is_active = !workset.hessianWorkset.hess_vec_prod_g_xx.is_null();
  bool g_xp_is_active = !workset.hessianWorkset.hess_vec_prod_g_xp.is_null();
  bool g_px_is_active = !workset.hessianWorkset.hess_vec_prod_g_px.is_null();

  if(g_xx_is_active||g_px_is_active) {
    TEUCHOS_TEST_FOR_EXCEPTION(
        direction_x.is_null(),
        Teuchos::Exceptions::InvalidParameter,
        "\nError in GatherSolution<HessianVec, Traits>: "
        "direction_x is not set and hess_vec_prod_g_xx or"
        "hess_vec_prod_g_px is set.\n");
    direction_x_constView = Albany::getLocalData(direction_x->col(0));
  }

  TEUCHOS_TEST_FOR_EXCEPTION(workset.sideSetViews.is_null(), std::logic_error,
                             "Side sets defined in input file but not properly specified on the mesh.\n");

  const Albany::LayeredMeshNumbering<GO>& layeredMeshNumbering = *workset.disc->getLayeredMeshNumbering();
  int numLayers = layeredMeshNumbering.numLayers;

  Kokkos::deep_copy(this->contractedSol.get_view(), ScalarT(0.0));

  const Albany::LocalSideSetInfoList& ssList = *(workset.sideSetViews);
  Albany::LocalSideSetInfoList::const_iterator it = ssList.find(this->meshPart);

  if (it != ssList.end()) {
    const Albany::LocalSideSetInfo& sideSet = it->second;

    // Loop over the sides that form the boundary condition
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> >& wsElNodeID  = workset.disc->getWsElNodeID()[workset.wsIndex];
    const Albany::NodalDOFManager& solDOFManager = workset.disc->getOverlapDOFManager("ordinary_solution");
    const auto& ov_node_indexer = *workset.disc->getOverlapNodeGlobalLocalIndexer();


    Teuchos::ArrayRCP<double> quadWeights(numLayers+1);
    if(this->op == this->VerticalSum){
      quadWeights.assign(quadWeights.size(),1.0);
    } else  { //Average, doing trapezoidal rule
      const Teuchos::ArrayRCP<double>& layers_ratio = layeredMeshNumbering.layers_ratio;
      quadWeights[0] = 0.5*layers_ratio[0]; quadWeights[numLayers] = 0.5*layers_ratio[numLayers-1];
      for(int i=1; i<numLayers; ++i)
        quadWeights[i] = 0.5*(layers_ratio[i-1] + layers_ratio[i]);
    }

    for (int sideSet_idx = 0; sideSet_idx < sideSet.size; ++sideSet_idx) { // loop over the sides on this ws and name
      // Get the data that corresponds to the side
      const int elem_LID = sideSet.elem_LID(sideSet_idx);
      const int elem_side = sideSet.side_local_id(sideSet_idx);
      const CellTopologyData_Subcell& side =  this->cell_topo->side[elem_side];
      int numSideNodes = side.topology->node_count;

      const Teuchos::ArrayRCP<GO>& elNodeID = wsElNodeID[elem_LID];

      //we only consider elements on the top.
      GO baseId;
      for (int i = 0; i < numSideNodes; ++i) {
        std::size_t node = side.node[i];
        baseId = layeredMeshNumbering.getColumnId(elNodeID[node]);
        std::vector<double> contrSol(this->vecDim,0);
        for(int il=0; il<numLayers+1; ++il) {
          const GO gnode = layeredMeshNumbering.getId(baseId, il);
          const LO inode = ov_node_indexer.getLocalElement(gnode);
          for(int comp=0; comp<this->vecDim; ++comp)
            contrSol[comp] += x_constView[solDOFManager.getLocalDOF(inode, comp+this->offset)]*quadWeights[il];
        }
        std::vector<double> contrDirection(this->vecDim,0);

        if (g_xx_is_active||g_px_is_active)
          for(int il=0; il<numLayers+1; ++il) {
            const GO gnode = layeredMeshNumbering.getId(baseId, il);
            const LO inode = ov_node_indexer.getLocalElement(gnode);
            for(int comp=0; comp<this->vecDim; ++comp)
              contrDirection[comp] += direction_x_constView[solDOFManager.getLocalDOF(inode, comp+this->offset)]*quadWeights[il];
          }

        if (this->useCollapsedSidesets) {
          if(this->isVector) {
            for(int comp=0; comp<this->vecDim; ++comp) {
              this->contractedSol(sideSet_idx,i,comp) = HessianVecFad(this->contractedSol(sideSet_idx,i,comp).size(), contrSol[comp]);
              if (g_xx_is_active||g_px_is_active)
                this->contractedSol(sideSet_idx,i,comp).val().fastAccessDx(0) = contrDirection[comp];
              if (g_xx_is_active||g_xp_is_active)
                for(int il=0; il<numLayers+1; ++il)
                  this->contractedSol(sideSet_idx,i,comp).fastAccessDx(neq*(this->numNodes+numSideNodes*il+i)+comp+this->offset).val() = quadWeights[il] * workset.j_coeff;
            }
          } else {
            this->contractedSol(sideSet_idx,i) = HessianVecFad(this->contractedSol(sideSet_idx,i).size(), contrSol[0]);
            if (g_xx_is_active||g_px_is_active)
              this->contractedSol(sideSet_idx,i).val().fastAccessDx(0) = contrDirection[0];
            if (g_xx_is_active||g_xp_is_active)
              for(int il=0; il<numLayers+1; ++il)
                this->contractedSol(sideSet_idx,i).fastAccessDx(neq*(this->numNodes+numSideNodes*il+i)+this->offset).val() = quadWeights[il] * workset.j_coeff;
          }
        } else {
          if(this->isVector) {
            for(int comp=0; comp<this->vecDim; ++comp) {
              this->contractedSol(elem_LID,elem_side,i,comp) = HessianVecFad(this->contractedSol(elem_LID,elem_side,i,comp).size(), contrSol[comp]);
              if (g_xx_is_active||g_px_is_active)
                this->contractedSol(elem_LID,elem_side,i,comp).val().fastAccessDx(0) = contrDirection[comp];
              if (g_xx_is_active||g_xp_is_active)
                for(int il=0; il<numLayers+1; ++il)
                  this->contractedSol(elem_LID,elem_side,i,comp).fastAccessDx(neq*(this->numNodes+numSideNodes*il+i)+comp+this->offset).val() = quadWeights[il] * workset.j_coeff;
            }
          } else {
            this->contractedSol(elem_LID,elem_side,i) = HessianVecFad(this->contractedSol(elem_LID,elem_side,i).size(), contrSol[0]);
            if (g_xx_is_active||g_px_is_active)
              this->contractedSol(elem_LID,elem_side,i).val().fastAccessDx(0) = contrDirection[0];
            if (g_xx_is_active||g_xp_is_active)
              for(int il=0; il<numLayers+1; ++il)
                this->contractedSol(elem_LID,elem_side,i).fastAccessDx(neq*(this->numNodes+numSideNodes*il+i)+this->offset).val() = quadWeights[il] * workset.j_coeff;
          }
        }
      }
    }
  }
}

} // namespace LandIce
