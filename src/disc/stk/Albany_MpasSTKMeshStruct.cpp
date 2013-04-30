//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#define ALBANY_SEACAS
#ifdef ALBANY_SEACAS

#include <iostream>

#include "Albany_MpasSTKMeshStruct.hpp"
#include "Teuchos_VerboseObject.hpp"

#include <Shards_BasicTopologies.hpp>

#include <stk_mesh/base/Entity.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include <stk_mesh/base/GetBuckets.hpp>
#include <stk_mesh/base/FieldData.hpp>
#include <stk_mesh/base/Selector.hpp>
#include <stk_io/IossBridge.hpp>
#include <Ioss_SubSystem.h>

//#include <stk_mesh/fem/FEMHelpers.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "Albany_Utils.hpp"

void Albany::MpasSTKMeshStruct::setFieldAndBulkData(
                  const Teuchos::RCP<const Epetra_Comm>& comm,
                  const Teuchos::RCP<Teuchos::ParameterList>& params,
                  const unsigned int neq_,
                  const Teuchos::RCP<Albany::StateInfoStruct>& sis,
                  const unsigned int worksetSize){};


Albany::MpasSTKMeshStruct::MpasSTKMeshStruct(const Teuchos::RCP<Teuchos::ParameterList>& params,
                                             const Teuchos::RCP<const Epetra_Comm>& comm,
                                             const std::vector<int>& indexToTriangleID, const std::vector<int>& verticesOnTria, int nGlobalTriangles) :
  GenericSTKMeshStruct(params,2),
  out(Teuchos::VerboseObjectBase::getDefaultOStream()),
  periodic(false),
  NumEles(indexToTriangleID.size())
{
  elem_map = Teuchos::rcp(new Epetra_Map(nGlobalTriangles, indexToTriangleID.size(), &indexToTriangleID[0], 0, *comm)); // Distribute the elems equally
  
  params->validateParameters(*getValidDiscretizationParameters(),0);


  std::string ebn="Element Block 0";
  partVec[0] = & metaData->declare_part(ebn, metaData->element_rank() );
  ebNameToIndex[ebn] = 0;

#ifdef ALBANY_SEACAS
  stk::io::put_io_part_attribute(*partVec[0]);
#endif


  std::vector<std::string> nsNames;
  std::string nsn="Lateral";
  nsNames.push_back(nsn);
  nsPartVec[nsn] = & metaData->declare_part(nsn, metaData->node_rank() );
#ifdef ALBANY_SEACAS
    stk::io::put_io_part_attribute(*nsPartVec[nsn]);
#endif
  nsn="Internal";
  nsNames.push_back(nsn);
  nsPartVec[nsn] = & metaData->declare_part(nsn, metaData->node_rank() );
#ifdef ALBANY_SEACAS
    stk::io::put_io_part_attribute(*nsPartVec[nsn]);
#endif


  std::vector<std::string> ssNames;
  std::string ssn="LateralSide";
  ssNames.push_back(ssn);
    ssPartVec[ssn] = & metaData->declare_part(ssn, metaData->side_rank() );
#ifdef ALBANY_SEACAS
    stk::io::put_io_part_attribute(*ssPartVec[ssn]);
#endif

  stk::mesh::fem::set_cell_topology<shards::Triangle<3> >(*partVec[0]);
  stk::mesh::fem::set_cell_topology<shards::Line<2> >(*ssPartVec[ssn]);

  numDim = 2;
  int cub = params->get("Cubature Degree",3);
  int worksetSizeMax = params->get("Workset Size",50);
  int worksetSize = this->computeWorksetSize(worksetSizeMax, elem_map->NumMyElements());

  const CellTopologyData& ctd = *metaData->get_cell_topology(*partVec[0]).getCellTopologyData();

  this->meshSpecs[0] = Teuchos::rcp(new Albany::MeshSpecsStruct(ctd, numDim, cub,
                             nsNames, ssNames, worksetSize, partVec[0]->name(),
                             ebNameToIndex, this->interleavedOrdering));


}


Albany::MpasSTKMeshStruct::MpasSTKMeshStruct(const Teuchos::RCP<Teuchos::ParameterList>& params,
                                             const Teuchos::RCP<const Epetra_Comm>& comm,
                                             const std::vector<int>& indexToTriangleID, const std::vector<int>& verticesOnTria, int nGlobalTriangles, int numLayers, int Ordering) :
  GenericSTKMeshStruct(params,3),
  out(Teuchos::VerboseObjectBase::getDefaultOStream()),
  periodic(false),
  NumEles(indexToTriangleID.size())
{
  std::vector<int> indexToPrismID(indexToTriangleID.size()*numLayers);

  //Int ElemColumnShift = (ordering == ColumnWise) ? 1 : indexToTriangleID.size();
  int elemColumnShift = (Ordering == 1) ? 1 : nGlobalTriangles;
  int lElemColumnShift = (Ordering == 1) ? 1 : indexToTriangleID.size();
  int elemLayerShift = (Ordering == 0) ? 1 : numLayers;

  for(int il=0; il< numLayers; il++)
  {
	  int shift = il*elemColumnShift;
	  int lShift = il*lElemColumnShift;
	  for(int j=0; j< indexToTriangleID.size(); j++)
	  {
		  int lid = lShift + j*elemLayerShift;
		  indexToPrismID[lid] = shift+elemLayerShift * indexToTriangleID[j];
	  }
  }

  elem_map = Teuchos::rcp(new Epetra_Map(nGlobalTriangles*numLayers, indexToPrismID.size(), &indexToPrismID[0], 0, *comm)); // Distribute the elems equally

  params->validateParameters(*getValidDiscretizationParameters(),0);


  std::string ebn="Element Block 0";
  partVec[0] = & metaData->declare_part(ebn, metaData->element_rank() );
  ebNameToIndex[ebn] = 0;

#ifdef ALBANY_SEACAS
  stk::io::put_io_part_attribute(*partVec[0]);
#endif


  std::vector<std::string> nsNames;
  std::string nsn="Lateral";
  nsNames.push_back(nsn);
  nsPartVec[nsn] = & metaData->declare_part(nsn, metaData->node_rank() );
#ifdef ALBANY_SEACAS
    stk::io::put_io_part_attribute(*nsPartVec[nsn]);
#endif
  nsn="Internal";
  nsNames.push_back(nsn);
  nsPartVec[nsn] = & metaData->declare_part(nsn, metaData->node_rank() );
#ifdef ALBANY_SEACAS
    stk::io::put_io_part_attribute(*nsPartVec[nsn]);
#endif


  std::vector<std::string> ssNames;
  std::string ssnLat="LateralSide";
  std::string ssnBottom="BasalSide";
  std::string ssnTop="UpperSide";
  ssNames.push_back(ssnLat);
  ssNames.push_back(ssnBottom);
  ssNames.push_back(ssnTop);
  ssPartVec[ssnLat] = & metaData->declare_part(ssnLat, metaData->side_rank() );
  ssPartVec[ssnBottom] = & metaData->declare_part(ssnBottom, metaData->side_rank() );
  ssPartVec[ssnTop] = & metaData->declare_part(ssnTop, metaData->side_rank() );
#ifdef ALBANY_SEACAS
    stk::io::put_io_part_attribute(*ssPartVec[ssnLat]);
    stk::io::put_io_part_attribute(*ssPartVec[ssnBottom]);
    stk::io::put_io_part_attribute(*ssPartVec[ssnTop]);
#endif

  stk::mesh::fem::set_cell_topology<shards::Wedge<6> >(*partVec[0]);
  stk::mesh::fem::set_cell_topology<shards::Triangle<3> >(*ssPartVec[ssnBottom]);
  stk::mesh::fem::set_cell_topology<shards::Triangle<3> >(*ssPartVec[ssnTop]);
  stk::mesh::fem::set_cell_topology<shards::Quadrilateral<4> >(*ssPartVec[ssnLat]);

  numDim = 3;
  int cub = params->get("Cubature Degree",3);
  int worksetSizeMax = params->get("Workset Size",50);
  int worksetSize = this->computeWorksetSize(worksetSizeMax, elem_map->NumMyElements());

  const CellTopologyData& ctd = *metaData->get_cell_topology(*partVec[0]).getCellTopologyData();

  this->meshSpecs[0] = Teuchos::rcp(new Albany::MeshSpecsStruct(ctd, numDim, cub,
                             nsNames, ssNames, worksetSize, partVec[0]->name(),
                             ebNameToIndex, this->interleavedOrdering));


}

Albany::MpasSTKMeshStruct::~MpasSTKMeshStruct()
{
}

void
Albany::MpasSTKMeshStruct::setFieldAndBulkData(
                                               const Teuchos::RCP<const Epetra_Comm>& comm,
                                               const Teuchos::RCP<Teuchos::ParameterList>& params,
                                               const Teuchos::RCP<Albany::StateInfoStruct>& sis,
                                               const std::vector<int>& indexToVertexID, const std::vector<double>& verticesCoords, const std::vector<bool>& isVertexBoundary, int nGlobalVertices,
                                               const std::vector<int>& verticesOnTria,
                                               const std::vector<bool>& isBoundaryEdge, const std::vector<int>& trianglesOnEdge, const std::vector<int>& trianglesPositionsOnEdge,
                                               const std::vector<int>& verticesOnEdge,
                                               const std::vector<int>& indexToEdgeID, int nGlobalEdges,
                                               const std::vector<int>& indexToTriangleID,
                                               const unsigned int worksetSize,
                                               int numLayers, int Ordering)
{
	this->SetupFieldData(comm, 2, sis, worksetSize);

    int elemColumnShift = (Ordering == 1) ? 1 : elem_map->NumGlobalElements()/numLayers;
    int lElemColumnShift = (Ordering == 1) ? 1 : indexToTriangleID.size();
    int elemLayerShift = (Ordering == 0) ? 1 : numLayers;

    int vertexColumnShift = (Ordering == 1) ? 1 : nGlobalVertices;
    int lVertexColumnShift = (Ordering == 1) ? 1 : indexToVertexID.size();
    int vertexLayerShift = (Ordering == 0) ? 1 : numLayers+1;

    int edgeColumnShift = (Ordering == 1) ? 1 : nGlobalEdges;
    int lEdgeColumnShift = (Ordering == 1) ? 1 : indexToEdgeID.size();
    int edgeLayerShift = (Ordering == 0) ? 1 : numLayers;


  metaData->commit();

  bulkData->modification_begin(); // Begin modifying the mesh

  stk::mesh::PartVector nodePartVec;
  stk::mesh::PartVector singlePartVec(1);
  stk::mesh::PartVector emptyPartVec;
  cout << "elem_map # elments: " << elem_map->NumMyElements() << endl;
  unsigned int ebNo = 0; //element block #???

  for(int i=0; i< (numLayers+1)*indexToVertexID.size(); i++)
  {
	  int ib = (Ordering == 0)*(i%lVertexColumnShift) + (Ordering == 1)*(i/vertexLayerShift);
	  int il = (Ordering == 0)*(i/lVertexColumnShift) + (Ordering == 1)*(i%vertexLayerShift);

	  stk::mesh::Entity& node = bulkData->declare_entity(metaData->node_rank(), il*vertexColumnShift+vertexLayerShift * indexToVertexID[ib]+1, nodePartVec);

      double* coord = stk::mesh::field_data(*coordinates_field, node);
	  coord[0] = verticesCoords[3*ib];   coord[1] = verticesCoords[3*ib+1]; coord[2] = double(il)/numLayers;
  }

  for (int i=0; i<elem_map->NumMyElements(); i++) {

	 int ib = (Ordering == 0)*(i%lElemColumnShift) + (Ordering == 1)*(i/elemLayerShift);
	 int il = (Ordering == 0)*(i/lElemColumnShift) + (Ordering == 1)*(i%elemLayerShift);

	 int shift = il*vertexColumnShift;

	 singlePartVec[0] = partVec[ebNo];
     stk::mesh::Entity& elem  = bulkData->declare_entity(metaData->element_rank(), elem_map->GID(i)+1, singlePartVec);

     for(int j=0; j<3; j++)
     {
    	 stk::mesh::Entity& node = *bulkData->get_entity(metaData->node_rank(), shift+vertexLayerShift * indexToVertexID[verticesOnTria[3*ib+j]]+1);
    	 bulkData->declare_relation(elem, node, j);

    	 stk::mesh::Entity& node_top = *bulkData->get_entity(metaData->node_rank(), shift+vertexColumnShift+vertexLayerShift * indexToVertexID[verticesOnTria[3*ib+j]]+1);
    	 bulkData->declare_relation(elem, node_top, j+3);
     }

     int* p_rank = stk::mesh::field_data(*proc_rank_field, elem);
     p_rank[0] = comm->MyPID();
  }


  int numBdEdges(0);
  for (int i=0; i<indexToEdgeID.size(); i++)
	  numBdEdges += isBoundaryEdge[i];

  singlePartVec[0] = ssPartVec["LateralSide"];

  //first we store the lateral faces of prisms, which corresponds to edges of the basal mesh

  for (int i=0; i<indexToEdgeID.size()*numLayers; i++) {
	 int ib = (Ordering == 0)*(i%lEdgeColumnShift) + (Ordering == 1)*(i/edgeLayerShift);
	 if(isBoundaryEdge[ib])
	 {
		 int il = (Ordering == 0)*(i/lEdgeColumnShift) + (Ordering == 1)*(i%edgeLayerShift);
		 int basalEdgeId = indexToEdgeID[ib]*edgeLayerShift;
		 int basalElemId = indexToTriangleID[trianglesOnEdge[2*ib]]*elemLayerShift;
		 int basalVertexId[2] = {indexToVertexID[verticesOnEdge[2*ib]]*vertexLayerShift, indexToVertexID[verticesOnEdge[2*ib+1]]*vertexLayerShift};
		 stk::mesh::Entity& side = bulkData->declare_entity(metaData->side_rank(), edgeColumnShift*il+basalEdgeId+1, singlePartVec);
		 stk::mesh::Entity& elem  = *bulkData->get_entity(metaData->element_rank(),  basalElemId+elemColumnShift*il+1);
		 bulkData->declare_relation(elem, side,  trianglesPositionsOnEdge[2*ib] );
		 for(int j=0; j<2; j++)
		 {
			 stk::mesh::Entity& nodeBottom = *bulkData->get_entity(metaData->node_rank(), basalVertexId[j]+vertexColumnShift*il+1);
			 bulkData->declare_relation(side, nodeBottom, j);
			 stk::mesh::Entity& nodeTop = *bulkData->get_entity(metaData->node_rank(), basalVertexId[j]+vertexColumnShift*(il+1)+1);
			 bulkData->declare_relation(side, nodeTop, j+2);
		 }
	 }
  }

  //then we store the lower and upper faces of prisms, which corresponds to triangles of the basal mesh

  edgeLayerShift = (Ordering == 0) ? 1 : numLayers+1;
  edgeColumnShift = elemColumnShift;

  singlePartVec[0] = ssPartVec["BasalSide"];

  int edgeOffset = nGlobalEdges*numLayers;
  for (int i=0; i<indexToTriangleID.size(); i++)
  {
	  stk::mesh::Entity& side = bulkData->declare_entity(metaData->side_rank(), indexToTriangleID[i]*edgeLayerShift+edgeOffset+1, singlePartVec);
	  stk::mesh::Entity& elem  = *bulkData->get_entity(metaData->element_rank(),  indexToTriangleID[i]*elemLayerShift+1);
	  bulkData->declare_relation(elem, side,  3);
	  for(int j=0; j<3; j++)
	  {
		 stk::mesh::Entity& node = *bulkData->get_entity(metaData->node_rank(), vertexLayerShift*indexToVertexID[verticesOnTria[3*i+j]]+1);
		 bulkData->declare_relation(side, node, j);
	  }
  }

  singlePartVec[0] = ssPartVec["UpperSide"];

  for (int i=0; i<indexToTriangleID.size(); i++)
  {
  	  stk::mesh::Entity& side = bulkData->declare_entity(metaData->side_rank(), indexToTriangleID[i]*edgeLayerShift+numLayers*edgeColumnShift+edgeOffset+1, singlePartVec);
  	  stk::mesh::Entity& elem  = *bulkData->get_entity(metaData->element_rank(),  indexToTriangleID[i]*elemLayerShift+(numLayers-1)*elemColumnShift+1);
  	  bulkData->declare_relation(elem, side,  4);
  	  for(int j=0; j<3; j++)
  	  {
  		 stk::mesh::Entity& node = *bulkData->get_entity(metaData->node_rank(), vertexLayerShift*indexToVertexID[verticesOnTria[3*i+j]]+numLayers*vertexColumnShift+1);
  		 bulkData->declare_relation(side, node, j);
  	  }
  }

  bulkData->modification_end();
}

void
Albany::MpasSTKMeshStruct::setFieldAndBulkData(
                                               const Teuchos::RCP<const Epetra_Comm>& comm,
                                               const Teuchos::RCP<Teuchos::ParameterList>& params,
                                               const Teuchos::RCP<Albany::StateInfoStruct>& sis,
                                               const std::vector<int>& indexToVertexID, const std::vector<double>& verticesCoords, const std::vector<bool>& isVertexBoundary, int nGlobalVertices,
                                               const std::vector<int>& verticesOnTria,
                                               const std::vector<bool>& isBoundaryEdge, const std::vector<int>& trianglesOnEdge, const std::vector<int>& trianglesPositionsOnEdge,
                                               const std::vector<int>& verticesOnEdge,
                                               const std::vector<int>& indexToEdgeID, int nGlobalEdges,
                                               const unsigned int worksetSize)
{
  this->SetupFieldData(comm, 2, sis, worksetSize);

  metaData->commit();

  bulkData->modification_begin(); // Begin modifying the mesh

  stk::mesh::PartVector nodePartVec;
  stk::mesh::PartVector singlePartVec(1);
  stk::mesh::PartVector emptyPartVec;
  cout << "elem_map # elments: " << elem_map->NumMyElements() << endl; 
  unsigned int ebNo = 0; //element block #??? 
  int sideID = 0;

  for (int i=0; i<indexToVertexID.size(); i++)
  {
	  stk::mesh::Entity& node = bulkData->declare_entity(metaData->node_rank(), indexToVertexID[i]+1, nodePartVec);

	  double* coord;
	  coord = stk::mesh::field_data(*coordinates_field, node);
	  coord[0] = verticesCoords[3*i];   coord[1] = verticesCoords[3*i+1]; coord[2] = verticesCoords[3*i+2];
  }

  for (int i=0; i<elem_map->NumMyElements(); i++)
  {

     singlePartVec[0] = partVec[ebNo];
     stk::mesh::Entity& elem  = bulkData->declare_entity(metaData->element_rank(), elem_map->GID(i)+1, singlePartVec);

     for(int j=0; j<3; j++)
     {
    	 stk::mesh::Entity& node = *bulkData->get_entity(metaData->node_rank(), indexToVertexID[verticesOnTria[3*i+j]]+1);
    	 bulkData->declare_relation(elem, node, j);
     }
    
     int* p_rank = stk::mesh::field_data(*proc_rank_field, elem);
     p_rank[0] = comm->MyPID();
  }

  for (int i=0; i<indexToEdgeID.size(); i++) {

	 if(isBoundaryEdge[i])
	 {

		 singlePartVec[0] = ssPartVec["LateralSide"];
		 stk::mesh::Entity& side = bulkData->declare_entity(metaData->side_rank(), indexToEdgeID[i]+1, singlePartVec);
		 stk::mesh::Entity& elem  = *bulkData->get_entity(metaData->element_rank(),  elem_map->GID(trianglesOnEdge[2*i])+1);
		 bulkData->declare_relation(elem, side,  trianglesPositionsOnEdge[2*i] /*local side id*/);
		 for(int j=0; j<2; j++)
		 {
			 stk::mesh::Entity& node = *bulkData->get_entity(metaData->node_rank(), indexToVertexID[verticesOnEdge[2*i+j]]+1);
			 bulkData->declare_relation(side, node, j);
		 }
	 }
  }

  bulkData->modification_end();
}

Teuchos::RCP<const Teuchos::ParameterList>
Albany::MpasSTKMeshStruct::getValidDiscretizationParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
    this->getValidGenericSTKParameters("Valid ASCII_DiscParams");

  return validPL;
}
#endif
