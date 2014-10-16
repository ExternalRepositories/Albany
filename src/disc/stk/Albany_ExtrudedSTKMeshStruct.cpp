//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <iostream>

#include "Albany_ExtrudedSTKMeshStruct.hpp"
#include "Albany_STKDiscretization.hpp"
#include "Albany_IossSTKMeshStruct.hpp"
#include "Teuchos_VerboseObject.hpp"

#include <Shards_BasicTopologies.hpp>

#include <stk_mesh/base/Entity.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include <stk_mesh/base/GetBuckets.hpp>
#include <stk_mesh/base/FieldBase.hpp>
#include <stk_mesh/base/Selector.hpp>

#ifdef ALBANY_SEACAS
#include <stk_io/IossBridge.hpp>
#endif

//#include <stk_mesh/fem/FEMHelpers.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "Albany_Utils.hpp"

//TODO: Generalize the importer so that it can extrude quad meshes

const Tpetra::global_size_t INVALID = Teuchos::OrdinalTraits<Tpetra::global_size_t>::invalid (); 

Albany::ExtrudedSTKMeshStruct::ExtrudedSTKMeshStruct(const Teuchos::RCP<Teuchos::ParameterList>& params, const Teuchos::RCP<const Teuchos_Comm>& comm) :
    GenericSTKMeshStruct(params, Teuchos::null, 3), out(Teuchos::VerboseObjectBase::getDefaultOStream()), periodic(false) {
  params->validateParameters(*getValidDiscretizationParameters(), 0);

  std::string ebn = "Element Block 0";
  partVec[0] = &metaData->declare_part(ebn, stk::topology::ELEMENT_RANK);
  ebNameToIndex[ebn] = 0;

#ifdef ALBANY_SEACAS
  stk::io::put_io_part_attribute(*partVec[0]);
#endif

  std::vector<std::string> nsNames;
  std::string nsn = "Lateral";
  nsNames.push_back(nsn);
  nsPartVec[nsn] = &metaData->declare_part(nsn, stk::topology::NODE_RANK);
#ifdef ALBANY_SEACAS
  stk::io::put_io_part_attribute(*nsPartVec[nsn]);
#endif
  nsn = "Internal";
  nsNames.push_back(nsn);
  nsPartVec[nsn] = &metaData->declare_part(nsn, stk::topology::NODE_RANK);
#ifdef ALBANY_SEACAS
  stk::io::put_io_part_attribute(*nsPartVec[nsn]);
#endif
  nsn = "Bottom";
  nsNames.push_back(nsn);
  nsPartVec[nsn] = &metaData->declare_part(nsn, stk::topology::NODE_RANK);
#ifdef ALBANY_SEACAS
  stk::io::put_io_part_attribute(*nsPartVec[nsn]);
#endif

  std::vector<std::string> ssNames;
  std::string ssnLat = "lateralside";
  std::string ssnBottom = "basalside";
  std::string ssnTop = "upperside";

  ssNames.push_back(ssnLat);
  ssNames.push_back(ssnBottom);
  ssNames.push_back(ssnTop);
  ssPartVec[ssnLat] = &metaData->declare_part(ssnLat, metaData->side_rank());
  ssPartVec[ssnBottom] = &metaData->declare_part(ssnBottom, metaData->side_rank());
  ssPartVec[ssnTop] = &metaData->declare_part(ssnTop, metaData->side_rank());
#ifdef ALBANY_SEACAS
  stk::io::put_io_part_attribute(*ssPartVec[ssnLat]);
  stk::io::put_io_part_attribute(*ssPartVec[ssnBottom]);
  stk::io::put_io_part_attribute(*ssPartVec[ssnTop]);
#endif

  Teuchos::RCP<Teuchos::ParameterList> params2D(new Teuchos::ParameterList());
  params2D->set("Use Serial Mesh", params->get("Use Serial Mesh", false));
  params2D->set("Exodus Input File Name", params->get("Exodus Input File Name", "IceSheet.exo"));
  meshStruct2D = Teuchos::rcp(new Albany::IossSTKMeshStruct(params2D, adaptParams, comm));
  Teuchos::RCP<Albany::StateInfoStruct> sis = Teuchos::rcp(new Albany::StateInfoStruct);
  Albany::AbstractFieldContainer::FieldContainerRequirements req;
  meshStruct2D->setFieldAndBulkData(comm, params, 1, req, sis, meshStruct2D->getMeshSpecs()[0]->worksetSize);

  stk::mesh::Selector select_owned_in_part = stk::mesh::Selector(meshStruct2D->metaData->universal_part()) & stk::mesh::Selector(meshStruct2D->metaData->locally_owned_part());
  int numCells = stk::mesh::count_selected_entities(select_owned_in_part, meshStruct2D->bulkData->buckets(stk::topology::ELEMENT_RANK));

  std::string shape = params->get("Element Shape", "Hexahedron");
  std::string basalside_name;
  if(shape == "Tetrahedron")  {
    ElemShape = Tetrahedron;
    basalside_name = shards::getCellTopologyData<shards::Triangle<3> >()->name;
  }
  else if (shape == "Wedge")  {
    ElemShape = Wedge;
    basalside_name = shards::getCellTopologyData<shards::Triangle<3> >()->name;
  }
  else if (shape == "Hexahedron") {
    ElemShape = Hexahedron;
    basalside_name = shards::getCellTopologyData<shards::Quadrilateral<4> >()->name;
  }
  else
    TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameterValue,
              std::endl << "Error in ExtrudedSTKMeshStruct: Element Shape " << shape << " not recognized. Possible values: Tetrahedron, Wedge, Hexahedron");

  std::string elem2d_name(meshStruct2D->getMeshSpecs()[0]->ctd.base->name);
  TEUCHOS_TEST_FOR_EXCEPTION(basalside_name != elem2d_name, Teuchos::Exceptions::InvalidParameterValue,
                std::endl << "Error in ExtrudedSTKMeshStruct: Expecting topology name of elements of 2d mesh to be " <<  basalside_name << " but it is " << elem2d_name);


  switch (ElemShape) {
  case Tetrahedron:
    stk::mesh::set_cell_topology<shards::Tetrahedron<4> >(*partVec[0]);
    stk::mesh::set_cell_topology<shards::Triangle<3> >(*ssPartVec[ssnBottom]);
    stk::mesh::set_cell_topology<shards::Triangle<3> >(*ssPartVec[ssnTop]);
    stk::mesh::set_cell_topology<shards::Triangle<3> >(*ssPartVec[ssnLat]);
    NumBaseElemeNodes = 3;
    break;
  case Wedge:
    stk::mesh::set_cell_topology<shards::Wedge<6> >(*partVec[0]);
    stk::mesh::set_cell_topology<shards::Triangle<3> >(*ssPartVec[ssnBottom]);
    stk::mesh::set_cell_topology<shards::Triangle<3> >(*ssPartVec[ssnTop]);
    stk::mesh::set_cell_topology<shards::Quadrilateral<4> >(*ssPartVec[ssnLat]);
    NumBaseElemeNodes = 3;
    break;
  case Hexahedron:
    stk::mesh::set_cell_topology<shards::Hexahedron<8> >(*partVec[0]);
    stk::mesh::set_cell_topology<shards::Quadrilateral<4> >(*ssPartVec[ssnBottom]);
    stk::mesh::set_cell_topology<shards::Quadrilateral<4> >(*ssPartVec[ssnTop]);
    stk::mesh::set_cell_topology<shards::Quadrilateral<4> >(*ssPartVec[ssnLat]);
    NumBaseElemeNodes = 4;
    break;
  }



  numDim = 3;
  int cub = params->get("Cubature Degree", 3);
  int worksetSizeMax = params->get("Workset Size", 50);
  int worksetSize = this->computeWorksetSize(worksetSizeMax, numCells);

  const CellTopologyData& ctd = *metaData->get_cell_topology(*partVec[0]).getCellTopologyData();

  this->meshSpecs[0] = Teuchos::rcp(new Albany::MeshSpecsStruct(ctd, numDim, cub, nsNames, ssNames, worksetSize, partVec[0]->name(), ebNameToIndex, this->interleavedOrdering));

}

Albany::ExtrudedSTKMeshStruct::~ExtrudedSTKMeshStruct() {
}

void Albany::ExtrudedSTKMeshStruct::setFieldAndBulkData(const Teuchos::RCP<const Teuchos_Comm>& comm, const Teuchos::RCP<Teuchos::ParameterList>& params, const unsigned int neq_, const AbstractFieldContainer::FieldContainerRequirements& req, const Teuchos::RCP<Albany::StateInfoStruct>& sis,
    const unsigned int worksetSize) {

  int numLayers = params->get("NumLayers", 10);
  bool useGlimmerSpacing = params->get("Use Glimmer Spacing", false);
  long long int maxGlobalElements2D = 0;
  long long int maxGlobalVertices2dId = 0;
  long long int numGlobalVertices2D = 0;
  long long int maxGlobalEdges2D = 0;
  bool Ordering = params->get("Columnwise Ordering", false);
  bool isTetra = true;

  const stk::mesh::BulkData& bulkData2D = *meshStruct2D->bulkData;
  const stk::mesh::MetaData& metaData2D = *meshStruct2D->metaData; //bulkData2D.mesh_meta_data();

  std::vector<double> levelsNormalizedThickness(numLayers + 1), temperatureNormalizedZ;

  if(useGlimmerSpacing)
    for (int i = 0; i < numLayers+1; i++)
      levelsNormalizedThickness[numLayers-i] = 1.0- (1.0 - std::pow(double(i) / numLayers + 1.0, -2))/(1.0 - std::pow(2.0, -2));
  else  //uniform layers
    for (int i = 0; i < numLayers+1; i++)
      levelsNormalizedThickness[i] = double(i) / numLayers;

  /*std::cout<< "Levels: ";
  for (int i = 0; i < numLayers+1; i++)
    std::cout<< levelsNormalizedThickness[i] << " ";
  std::cout<< "\n";*/

  stk::mesh::Selector select_owned_in_part = stk::mesh::Selector(metaData2D.universal_part()) & stk::mesh::Selector(metaData2D.locally_owned_part());

  stk::mesh::Selector select_overlap_in_part = stk::mesh::Selector(metaData2D.universal_part()) & (stk::mesh::Selector(metaData2D.locally_owned_part()) | stk::mesh::Selector(metaData2D.globally_shared_part()));

  stk::mesh::Selector select_edges = stk::mesh::Selector(*metaData2D.get_part("LateralSide")) & (stk::mesh::Selector(metaData2D.locally_owned_part()) | stk::mesh::Selector(metaData2D.globally_shared_part()));

  std::vector<stk::mesh::Entity> cells2D;
  stk::mesh::get_selected_entities(select_overlap_in_part, bulkData2D.buckets(stk::topology::ELEMENT_RANK), cells2D);

  std::vector<stk::mesh::Entity> nodes2D;
  stk::mesh::get_selected_entities(select_overlap_in_part, bulkData2D.buckets(stk::topology::NODE_RANK), nodes2D);

  std::vector<stk::mesh::Entity> edges2D;
  stk::mesh::get_selected_entities(select_edges, bulkData2D.buckets(metaData2D.side_rank()), edges2D);

  long long int maxOwnedElements2D(0), maxOwnedNodes2D(0), maxOwnedSides2D(0), numOwnedNodes2D(0);
  for (int i = 0; i < cells2D.size(); i++)
    maxOwnedElements2D = std::max(maxOwnedElements2D, (long long int) bulkData2D.identifier(cells2D[i]));
  for (int i = 0; i < nodes2D.size(); i++)
    maxOwnedNodes2D = std::max(maxOwnedNodes2D, (long long int) bulkData2D.identifier(nodes2D[i]));
  for (int i = 0; i < edges2D.size(); i++)
    maxOwnedSides2D = std::max(maxOwnedSides2D, (long long int) bulkData2D.identifier(edges2D[i]));
  numOwnedNodes2D = stk::mesh::count_selected_entities(select_owned_in_part, bulkData2D.buckets(stk::topology::NODE_RANK));

  //comm->MaxAll(&maxOwnedElements2D, &maxGlobalElements2D, 1);
  //comm->MaxAll(&maxOwnedNodes2D, &maxGlobalVertices2dId, 1);
  //comm->MaxAll(&maxOwnedSides2D, &maxGlobalEdges2D, 1);
  //comm->SumAll(&numOwnedNodes2D, &numGlobalVertices2D, 1);
  //IK, 10/1/14: in order for reduceAll to work with Teuchos::Comm object we have to cast things as ints; long long ints don't work 
  //(because Teuchos::Comm is templated on just int?).  Are long long ints really necessary?  Look into this -- ask Mauro.
  //Are there issues for 64 bit integer build of Albany? 
   LO maxOwnedElements2DInt = maxOwnedElements2D;
  GO maxGlobalElements2DInt = maxGlobalElements2D;
  LO maxOwnedNodes2DInt = maxOwnedNodes2D;
  GO maxGlobalVertices2dIdInt = maxGlobalVertices2dId;
  LO maxOwnedSides2DInt = maxOwnedSides2D;
  GO maxGlobalEdges2DInt = maxGlobalEdges2D;
  int numOwnedNodes2DInt = numOwnedNodes2D;
  int numGlobalVertices2DInt = numGlobalVertices2D;
  //comm->MaxAll(&maxOwnedElements2D, &maxGlobalElements2D, 1);
  Teuchos::reduceAll<LO,GO>(*comm, Teuchos::REDUCE_MAX, maxOwnedElements2DInt, Teuchos::ptr(&maxGlobalElements2DInt));
  //comm->MaxAll(&maxOwnedNodes2D, &maxGlobalVertices2dId, 1);
  Teuchos::reduceAll<LO,GO>(*comm, Teuchos::REDUCE_MAX, maxOwnedNodes2DInt, Teuchos::ptr(&maxGlobalVertices2dIdInt));
  //comm->MaxAll(&maxOwnedSides2D, &maxGlobalEdges2D, 1);
  Teuchos::reduceAll<LO,GO>(*comm, Teuchos::REDUCE_MAX, maxOwnedSides2DInt, Teuchos::ptr(&maxGlobalEdges2DInt));
  //comm->SumAll(&numOwnedNodes2D, &numGlobalVertices2D, 1);
  //The following should not be int int...
  Teuchos::reduceAll<int,int>(*comm, Teuchos::REDUCE_SUM, 1, &numOwnedNodes2DInt, &numGlobalVertices2DInt);
  maxOwnedElements2D = maxOwnedElements2DInt; 
  maxGlobalElements2D = maxGlobalElements2DInt; 
  maxOwnedNodes2D = maxOwnedNodes2DInt; 
  maxGlobalVertices2dId = maxGlobalVertices2dIdInt; 
  maxOwnedSides2D = maxOwnedSides2DInt; 
  maxGlobalEdges2D = maxGlobalEdges2DInt; 
  numOwnedNodes2D = numOwnedNodes2DInt; 
  numGlobalVertices2D = numGlobalVertices2DInt; 

  if (comm->getRank() == 0) std::cout << "Importing ascii files ...";

  //std::cout << "Num Global Elements: " << maxGlobalElements2D<< " " << maxGlobalVertices2dId<< " " << maxGlobalEdges2D << std::endl;

  Teuchos::Array<GO> indices(nodes2D.size());
  std::vector<GO> serialIndices;
  for (int i = 0; i < nodes2D.size(); ++i)
    indices[i] = bulkData2D.identifier(nodes2D[i]) - 1;
  
  Teuchos::RCP<const Tpetra_Map> nodes_map = Tpetra::createNonContigMap<LO, GO> (indices(), comm);
  int numMyElements = (comm->getRank() == 0) ? numGlobalVertices2D : 0;
  //Teuchos::RCP<const Tpetra_Map> serial_nodes_map = Tpetra::createUniformContigMap<LO, GO>(numMyElements, comm); 
  Teuchos::RCP<const Tpetra_Map> serial_nodes_map = Teuchos::rcp(new const Tpetra_Map(INVALID, numMyElements, 0, comm)); 
  Teuchos::RCP<Tpetra_Import> importOperator = Teuchos::rcp(new Tpetra_Import(serial_nodes_map, nodes_map));

  Teuchos::RCP<Tpetra_Vector> temp = Teuchos::rcp(new Tpetra_Vector(serial_nodes_map));
  Teuchos::RCP<Tpetra_Vector> sHeightVec;
  Teuchos::RCP<Tpetra_Vector> thickVec;
  Teuchos::RCP<Tpetra_Vector> bFrictionVec;
  Teuchos::RCP<std::vector<Tpetra_Vector> > temperatureVecInterp;
  Teuchos::RCP<std::vector<Tpetra_Vector> > sVelocityVec;
  Teuchos::RCP<std::vector<Tpetra_Vector> > velocityRMSVec;



  bool hasSurface_height =  std::find(req.begin(), req.end(), "surface_height") != req.end();

  {
    sHeightVec = Teuchos::rcp(new Tpetra_Vector(nodes_map));
    std::string fname = params->get<std::string>("Surface Height File Name", "surface_height.ascii");
    read2DFileSerial(fname, temp, comm);
    sHeightVec->doImport(*temp, *importOperator, Tpetra::INSERT);
  }
  Teuchos::ArrayRCP<const ST> sHeightVec_constView = sHeightVec->get1dView();


  bool hasThickness =  std::find(req.begin(), req.end(), "thickness") != req.end();

  {
    std::string fname = params->get<std::string>("Thickness File Name", "thickness.ascii");
    read2DFileSerial(fname, temp, comm);
    thickVec = Teuchos::rcp(new Tpetra_Vector(nodes_map));
    thickVec->doImport(*temp, *importOperator, Tpetra::INSERT);
  }
  Teuchos::ArrayRCP<const ST> thickVec_constView = thickVec->get1dView();


  bool hasBasal_friction = std::find(req.begin(), req.end(), "basal_friction") != req.end();
  if(hasBasal_friction) {
    std::string fname = params->get<std::string>("Basal Friction File Name", "basal_friction.ascii");
    read2DFileSerial(fname, temp, comm);
    bFrictionVec = Teuchos::rcp(new Tpetra_Vector(nodes_map));
    bFrictionVec->doImport(*temp, *importOperator, Tpetra::INSERT);
  }
  Teuchos::ArrayRCP<const ST> bFrictionVec_constView = bFrictionVec->get1dView();

  bool hasTemperature = std::find(req.begin(), req.end(), "temperature") != req.end();
  if(hasTemperature) {
    std::vector<Tpetra_Vector> temperatureVec;
    temperatureVecInterp = Teuchos::rcp(new std::vector<Tpetra_Vector>(numLayers + 1, Tpetra_Vector(nodes_map)));
    std::string fname = params->get<std::string>("Temperature File Name", "temperature.ascii");
    readFileSerial(fname, serial_nodes_map, nodes_map, importOperator, temperatureVec, temperatureNormalizedZ, comm);


    int il0, il1, verticalTSize(temperatureVec.size());
    double h0(0.0);

    for (int il = 0; il < numLayers + 1; il++) {
      if (levelsNormalizedThickness[il] <= temperatureNormalizedZ[0]) {
        il0 = 0;
        il1 = 0;
        h0 = 1.0;
      }

      else if (levelsNormalizedThickness[il] >= temperatureNormalizedZ[verticalTSize - 1]) {
        il0 = verticalTSize - 1;
        il1 = verticalTSize - 1;
        h0 = 0.0;
      }

      else {
        int k = 0;
        while (levelsNormalizedThickness[il] > temperatureNormalizedZ[++k])
          ;
        il0 = k - 1;
        il1 = k;
        h0 = (temperatureNormalizedZ[il1] - levelsNormalizedThickness[il]) / (temperatureNormalizedZ[il1] - temperatureNormalizedZ[il0]);
      }
      Teuchos::ArrayRCP<ST> temperatureVecInterp_nonConstView = (*temperatureVecInterp)[il].get1dViewNonConst();
      Teuchos::ArrayRCP<const ST> temperatureVec_constView_il0 = temperatureVec[il0].get1dView();
      Teuchos::ArrayRCP<const ST> temperatureVec_constView_il1 = temperatureVec[il1].get1dView();

      for (int i = 0; i < nodes_map->getNodeNumElements(); i++)
        temperatureVecInterp_nonConstView[i] = h0 * temperatureVec_constView_il0[i] + (1.0 - h0) * temperatureVec_constView_il1[i];
    }
  }

  std::vector<Tpetra_Vector> tempSV(neq_, Tpetra_Vector(serial_nodes_map));

  bool hasSurfaceVelocity = std::find(req.begin(), req.end(), "surface_velocity") != req.end();
  if(hasSurfaceVelocity) {
    std::string fname = params->get<std::string>("Surface Velocity File Name", "surface_velocity.ascii");
    readFileSerial(fname, tempSV, comm);
    sVelocityVec = Teuchos::rcp(new std::vector<Tpetra_Vector> (neq_, Tpetra_Vector(nodes_map)));
    for (int i = 0; i < tempSV.size(); i++)
      (*sVelocityVec)[i].doImport(tempSV[i], *importOperator, Tpetra::INSERT);
  }

  bool hasSurfaceVelocityRMS = std::find(req.begin(), req.end(), "surface_velocity_rms") != req.end();
  if(hasSurfaceVelocityRMS) {
    std::string fname = params->get<std::string>("Surface Velocity RMS File Name", "velocity_RMS.ascii");
    readFileSerial(fname, tempSV, comm);
    velocityRMSVec = Teuchos::rcp(new std::vector<Tpetra_Vector> (neq_, Tpetra_Vector(nodes_map)));
    for (int i = 0; i < tempSV.size(); i++)
      (*velocityRMSVec)[i].doImport(tempSV[i], *importOperator, Tpetra::INSERT);
  }

  if (comm->getRank() == 0) std::cout << " done." << std::endl;

  long long int elemColumnShift = (Ordering == 1) ? 1 : maxGlobalElements2D;
  int lElemColumnShift = (Ordering == 1) ? 1 : cells2D.size();
  int elemLayerShift = (Ordering == 0) ? 1 : numLayers;

  long long int vertexColumnShift = (Ordering == 1) ? 1 : maxGlobalVertices2dId;
  int lVertexColumnShift = (Ordering == 1) ? 1 : nodes2D.size();
  int vertexLayerShift = (Ordering == 0) ? 1 : numLayers + 1;

  long long int edgeColumnShift = (Ordering == 1) ? 1 : maxGlobalEdges2D;
  int lEdgeColumnShift = (Ordering == 1) ? 1 : edges2D.size();
  int edgeLayerShift = (Ordering == 0) ? 1 : numLayers;

  this->SetupFieldData(comm, neq_, req, sis, worksetSize);

  metaData->commit();

  bulkData->modification_begin(); // Begin modifying the mesh

  stk::mesh::PartVector nodePartVec;
  stk::mesh::PartVector singlePartVec(1);
  stk::mesh::PartVector emptyPartVec;
  unsigned int ebNo = 0; //element block #???

  singlePartVec[0] = nsPartVec["Bottom"];

  typedef AbstractSTKFieldContainer::ScalarFieldType ScalarFieldType;
  typedef AbstractSTKFieldContainer::VectorFieldType VectorFieldType;
  typedef AbstractSTKFieldContainer::QPScalarFieldType ElemScalarFieldType;

  AbstractSTKFieldContainer::IntScalarFieldType* proc_rank_field = fieldContainer->getProcRankField();
  VectorFieldType* coordinates_field = fieldContainer->getCoordinatesField();
  stk::mesh::FieldBase const* coordinates_field2d = bulkData2D.mesh_meta_data().coordinate_field();
  VectorFieldType* surface_velocity_field = metaData->get_field<VectorFieldType>(stk::topology::NODE_RANK, "surface_velocity");
  VectorFieldType* surface_velocity_RMS_field = metaData->get_field<VectorFieldType>(stk::topology::NODE_RANK, "surface_velocity_rms");
  ScalarFieldType* surface_height_field = metaData->get_field<ScalarFieldType>(stk::topology::NODE_RANK, "surface_height");
  ScalarFieldType* thickness_field = metaData->get_field<ScalarFieldType>(stk::topology::NODE_RANK, "thickness");
  ScalarFieldType* basal_friction_field = metaData->get_field<ScalarFieldType>(stk::topology::NODE_RANK, "basal_friction");
  ElemScalarFieldType* temperature_field = metaData->get_field<ElemScalarFieldType>(stk::topology::ELEMENT_RANK, "temperature");

  std::vector<long long int> prismMpasIds(NumBaseElemeNodes), prismGlobalIds(2 * NumBaseElemeNodes);

  for (int i = 0; i < (numLayers + 1) * nodes2D.size(); i++) {
    int ib = (Ordering == 0) * (i % lVertexColumnShift) + (Ordering == 1) * (i / vertexLayerShift);
    int il = (Ordering == 0) * (i / lVertexColumnShift) + (Ordering == 1) * (i % vertexLayerShift);
    stk::mesh::Entity node;
    stk::mesh::Entity node2d = nodes2D[ib];
    stk::mesh::EntityId node2dId = bulkData2D.identifier(node2d) - 1;
    if (il == 0)
      node = bulkData->declare_entity(stk::topology::NODE_RANK, il * vertexColumnShift + vertexLayerShift * node2dId + 1, singlePartVec);
    else
      node = bulkData->declare_entity(stk::topology::NODE_RANK, il * vertexColumnShift + vertexLayerShift * node2dId + 1, nodePartVec);

    double* coord = stk::mesh::field_data(*coordinates_field, node);
    double const* coord2d = (double const*) stk::mesh::field_data(*coordinates_field2d, node2d);
    coord[0] = coord2d[0];
    coord[1] = coord2d[1];

    int lid = nodes_map->getLocalElement((long long int)(node2dId));
    coord[2] = sHeightVec_constView[lid] - thickVec_constView[lid] * (1. - levelsNormalizedThickness[il]);

    if(hasSurface_height && surface_height_field) {
      double* sHeight = stk::mesh::field_data(*surface_height_field, node);
      sHeight[0] = sHeightVec_constView[lid];
    }

    if(hasThickness && thickness_field) {
      double* thick = stk::mesh::field_data(*thickness_field, node);
      thick[0] = thickVec_constView[lid];
    }

    if(surface_velocity_field) {
      double* sVelocity = stk::mesh::field_data(*surface_velocity_field, node);
      Teuchos::ArrayRCP<const ST> sVelocityVec_constView_0 = (*sVelocityVec)[0].get1dView();
      Teuchos::ArrayRCP<const ST> sVelocityVec_constView_1 = (*sVelocityVec)[1].get1dView();
      sVelocity[0] = sVelocityVec_constView_0[lid];
      sVelocity[1] = sVelocityVec_constView_1[lid];
    }

    if(surface_velocity_RMS_field) {
      double* velocityRMS = stk::mesh::field_data(*surface_velocity_RMS_field, node);
      Teuchos::ArrayRCP<const ST> velocityRMSVec_constView_0 = (*velocityRMSVec)[0].get1dView();
      Teuchos::ArrayRCP<const ST> velocityRMSVec_constView_1 = (*velocityRMSVec)[1].get1dView();
      velocityRMS[0] = velocityRMSVec_constView_0[lid];
      velocityRMS[1] = velocityRMSVec_constView_1[lid];

    }

    if(hasBasal_friction && basal_friction_field) {
      double* bFriction = stk::mesh::field_data(*basal_friction_field, node);
      bFriction[0] = bFrictionVec_constView[lid];
    }
  }

  long long int tetrasLocalIdsOnPrism[3][4];

  for (int i = 0; i < cells2D.size() * numLayers; i++) {

    int ib = (Ordering == 0) * (i % lElemColumnShift) + (Ordering == 1) * (i / elemLayerShift);
    int il = (Ordering == 0) * (i / lElemColumnShift) + (Ordering == 1) * (i % elemLayerShift);

    int shift = il * vertexColumnShift;

    singlePartVec[0] = partVec[ebNo];

    //TODO: this could be done only in the first layer and then copied into the other layers

    stk::mesh::Entity const* rel = bulkData2D.begin_nodes(cells2D[ib]);
    double tempOnPrism = 0; //Set temperature constant on each prism/Hexa
    Teuchos::ArrayRCP<const ST> temperatureVecInterp_constView_il = (*temperatureVecInterp)[il].get1dView();
    Teuchos::ArrayRCP<const ST> temperatureVecInterp_constView_ilplus1 = (*temperatureVecInterp)[il + 1].get1dView();
    for (int j = 0; j < NumBaseElemeNodes; j++) {
      stk::mesh::EntityId node2dId = bulkData2D.identifier(rel[j]) - 1;
      int node2dLId = nodes_map->getLocalElement((long long int)(node2dId));
      stk::mesh::EntityId mpasLowerId = vertexLayerShift * node2dId;
      stk::mesh::EntityId lowerId = shift + vertexLayerShift * node2dId;
      prismMpasIds[j] = mpasLowerId;
      prismGlobalIds[j] = lowerId;
      prismGlobalIds[j + NumBaseElemeNodes] = lowerId + vertexColumnShift;
      if(hasTemperature)
        tempOnPrism += 1. / NumBaseElemeNodes / 2. * temperatureVecInterp_constView_il[node2dLId] + temperatureVecInterp_constView_ilplus1[node2dLId];
    }

    switch (ElemShape) {
    case Tetrahedron: {
      tetrasFromPrismStructured(&prismMpasIds[0], &prismGlobalIds[0], tetrasLocalIdsOnPrism);

      stk::mesh::EntityId prismId = il * elemColumnShift + elemLayerShift * (bulkData2D.identifier(cells2D[ib]) - 1);
      for (int iTetra = 0; iTetra < 3; iTetra++) {
        stk::mesh::Entity elem = bulkData->declare_entity(stk::topology::ELEMENT_RANK, 3 * prismId + iTetra + 1, singlePartVec);
        for (int j = 0; j < 4; j++) {
          stk::mesh::Entity node = bulkData->get_entity(stk::topology::NODE_RANK, tetrasLocalIdsOnPrism[iTetra][j] + 1);
          bulkData->declare_relation(elem, node, j);
        }
        int* p_rank = (int*) stk::mesh::field_data(*proc_rank_field, elem);
        p_rank[0] = comm->getRank();
        if(hasTemperature && temperature_field) {
          double* temperature = stk::mesh::field_data(*temperature_field, elem);
          temperature[0] = tempOnPrism;
        }
      }
    }
      break;
    case Wedge:
    case Hexahedron: {
      stk::mesh::EntityId prismId = il * elemColumnShift + elemLayerShift * (bulkData2D.identifier(cells2D[ib]) - 1);
      stk::mesh::Entity elem = bulkData->declare_entity(stk::topology::ELEMENT_RANK, prismId + 1, singlePartVec);
      for (int j = 0; j < 2 * NumBaseElemeNodes; j++) {
        stk::mesh::Entity node = bulkData->get_entity(stk::topology::NODE_RANK, prismGlobalIds[j] + 1);
        bulkData->declare_relation(elem, node, j);
      }
      int* p_rank = (int*) stk::mesh::field_data(*proc_rank_field, elem);
      p_rank[0] = comm->getRank();
      if(hasTemperature && temperature_field) {
        double* temperature = stk::mesh::field_data(*temperature_field, elem);
        temperature[0] = tempOnPrism;
      }
    }
    }
  }

  int numSubelemOnPrism, numBasalSidePoints;
  int basalSideLID, upperSideLID;

  switch (ElemShape) {
  case Tetrahedron:
    numSubelemOnPrism = 3;
    numBasalSidePoints = 3;
    basalSideLID = 3;  //depends on how the tetrahedron is located in the Prism, see tetraFaceIdOnPrismFaceId below.
    upperSideLID = 1;
    break;
  case Wedge:
    numSubelemOnPrism = 1;
    numBasalSidePoints = 3;
    basalSideLID = 3;  //depends on how the tetrahedron is located in the Prism.
    upperSideLID = 4;
    break;
  case Hexahedron:
    numSubelemOnPrism = 1;
    numBasalSidePoints = 4;
    basalSideLID = 4;  //depends on how the tetrahedron is located in the Prism.
    upperSideLID = 5;
    break;
  }

  singlePartVec[0] = ssPartVec["lateralside"];

  //The following two arrays have being computed offline using the computeMap function in .hpp file.

  //tetraFaceIdOnPrismFaceId[ minIndex ][ PrismFaceID ]
  int tetraFaceIdOnPrismFaceId[6][5] = {{0, 1, 2, 3, 1}, {2, 0, 1, 3, 1}, {1, 2, 0, 3, 1}, {2, 1, 0, 1 ,3}, {0, 2, 1, 1, 3}, {1, 0, 2, 1, 3}};

  //tetraAdjacentToPrismLateralFace[ minIndex ][ prismType ][ PrismFaceID ][ iTetra ]. There are to Terahedra adjacent to a Prism face. iTetra in {0,1}
  int tetraAdjacentToPrismLateralFace[6][2][3][2] = { { { { 1, 2 }, { 0, 1 }, { 0, 2 } }, { { 0, 2 }, { 0, 1 }, { 1, 2 } } },
                                                      { { { 0, 2 }, { 1, 2 }, { 0, 1 } }, { { 1, 2 }, { 0, 2 }, { 0, 1 } } },
                                                      { { { 0, 1 }, { 0, 2 }, { 1, 2 } }, { { 0, 1 }, { 1, 2 }, { 0, 2 } } },
                                                      { { { 0, 2 }, { 0, 1 }, { 1, 2 } }, { { 1, 2 }, { 0, 1 }, { 0, 2 } } },
                                                      { { { 1, 2 }, { 0, 2 }, { 0, 1 } }, { { 0, 2 }, { 1, 2 }, { 0, 1 } } },
                                                      { { { 0, 1 }, { 1, 2 }, { 0, 2 } }, { { 0, 1 }, { 0, 2 }, { 1, 2 } } } };

  for (int i = 0; i < edges2D.size() * numLayers; i++) {
    int ib = (Ordering == 0) * (i % lEdgeColumnShift) + (Ordering == 1) * (i / edgeLayerShift);
    // if(!isBoundaryEdge[ib]) continue; //WARNING: assuming that all the edges stored are boundary edges!!

    stk::mesh::Entity edge2d = edges2D[ib];
    stk::mesh::Entity const* rel = bulkData2D.begin_elements(edge2d);
    stk::mesh::ConnectivityOrdinal const* ordinals = bulkData2D.begin_element_ordinals(edge2d);

    int il = (Ordering == 0) * (i / lEdgeColumnShift) + (Ordering == 1) * (i % edgeLayerShift);
    stk::mesh::Entity elem2d = rel[0];
    stk::mesh::EntityId edgeLID = ordinals[0]; //bulkData2D.identifier(rel[0]);

    stk::mesh::EntityId basalElemId = bulkData2D.identifier(elem2d) - 1;
    stk::mesh::EntityId Edge2dId = bulkData2D.identifier(edge2d) - 1;
    switch (ElemShape) {
    case Tetrahedron: {
      rel = bulkData2D.begin_nodes(elem2d);
      for (int j = 0; j < NumBaseElemeNodes; j++) {
        stk::mesh::EntityId node2dId = bulkData2D.identifier(rel[j]) - 1;
        prismMpasIds[j] = vertexLayerShift * node2dId;
      }
      int minIndex;
      int pType = prismType(&prismMpasIds[0], minIndex);
      stk::mesh::EntityId tetraId = 3 * il * elemColumnShift + 3 * elemLayerShift * basalElemId;

      stk::mesh::Entity elem0 = bulkData->get_entity(stk::topology::ELEMENT_RANK, tetraId + tetraAdjacentToPrismLateralFace[minIndex][pType][edgeLID][0] + 1);
      stk::mesh::Entity elem1 = bulkData->get_entity(stk::topology::ELEMENT_RANK, tetraId + tetraAdjacentToPrismLateralFace[minIndex][pType][edgeLID][1] + 1);

      stk::mesh::Entity side0 = bulkData->declare_entity(metaData->side_rank(), 2 * edgeColumnShift * il +  2 * Edge2dId * edgeLayerShift + 1, singlePartVec);
      stk::mesh::Entity side1 = bulkData->declare_entity(metaData->side_rank(), 2 * edgeColumnShift * il +  2 * Edge2dId * edgeLayerShift + 1 + 1, singlePartVec);

      bulkData->declare_relation(elem0, side0, tetraFaceIdOnPrismFaceId[minIndex][edgeLID]);
      bulkData->declare_relation(elem1, side1, tetraFaceIdOnPrismFaceId[minIndex][edgeLID]);

      stk::mesh::Entity const* rel_elemNodes0 = bulkData->begin_nodes(elem0);
      stk::mesh::Entity const* rel_elemNodes1 = bulkData->begin_nodes(elem1);
      for (int j = 0; j < 3; j++) {
     //   std::cout << j <<", " << edgeLID << ", " << minIndex << ", " << tetraFaceIdOnPrismFaceId[minIndex][edgeLID] << ","  << std::endl;
        stk::mesh::Entity node0 = rel_elemNodes0[this->meshSpecs[0]->ctd.side[tetraFaceIdOnPrismFaceId[minIndex][edgeLID]].node[j]];
        bulkData->declare_relation(side0, node0, j);
        stk::mesh::Entity node1 = rel_elemNodes1[this->meshSpecs[0]->ctd.side[tetraFaceIdOnPrismFaceId[minIndex][edgeLID]].node[j]];
        bulkData->declare_relation(side1, node1, j);
      }
    }

      break;
    case Wedge:
    case Hexahedron: {
      stk::mesh::EntityId prismId = il * elemColumnShift + elemLayerShift * basalElemId;
      stk::mesh::Entity elem = bulkData->get_entity(stk::topology::ELEMENT_RANK, prismId + 1);
      stk::mesh::Entity side = bulkData->declare_entity(metaData->side_rank(), edgeColumnShift * il +Edge2dId * edgeLayerShift + 1, singlePartVec);
      bulkData->declare_relation(elem, side, edgeLID);

      stk::mesh::Entity const* rel_elemNodes = bulkData->begin_nodes(elem);
      for (int j = 0; j < 4; j++) {
        stk::mesh::Entity node = rel_elemNodes[this->meshSpecs[0]->ctd.side[edgeLID].node[j]];
        bulkData->declare_relation(side, node, j);
      }
    }
    break;
    }
  }

  //then we store the lower and upper faces of prisms, which corresponds to triangles of the basal mesh
  edgeLayerShift = (Ordering == 0) ? 1 : numLayers + 1;
  edgeColumnShift = elemColumnShift;

  singlePartVec[0] = ssPartVec["basalside"];


  long long int edgeOffset = maxGlobalEdges2D * numLayers;
  if(ElemShape == Tetrahedron) edgeOffset *= 2;

  for (int i = 0; i < cells2D.size(); i++) {
    stk::mesh::Entity elem2d = cells2D[i];
    stk::mesh::EntityId elem2d_id = bulkData2D.identifier(elem2d) - 1;
    stk::mesh::Entity side = bulkData->declare_entity(metaData->side_rank(), elem2d_id + edgeOffset + 1, singlePartVec);
    stk::mesh::Entity elem = bulkData->get_entity(stk::topology::ELEMENT_RANK, elem2d_id * numSubelemOnPrism * elemLayerShift + 1);
    bulkData->declare_relation(elem, side, basalSideLID);

    stk::mesh::Entity const* rel_elemNodes = bulkData->begin_nodes(elem);
    for (int j = 0; j < numBasalSidePoints; j++) {
      stk::mesh::Entity node = rel_elemNodes[this->meshSpecs[0]->ctd.side[basalSideLID].node[j]];
      bulkData->declare_relation(side, node, j);
    }
  }

  singlePartVec[0] = ssPartVec["upperside"];

  edgeOffset += maxGlobalElements2D;

  for (int i = 0; i < cells2D.size(); i++) {
    stk::mesh::Entity elem2d = cells2D[i];
    stk::mesh::EntityId elem2d_id = bulkData2D.identifier(elem2d) - 1;
    stk::mesh::Entity side = bulkData->declare_entity(metaData->side_rank(), elem2d_id  + edgeOffset + 1, singlePartVec);
    stk::mesh::Entity elem = bulkData->get_entity(stk::topology::ELEMENT_RANK, elem2d_id * numSubelemOnPrism * elemLayerShift + (numLayers - 1) * numSubelemOnPrism * elemColumnShift + 1 + (numSubelemOnPrism - 1));
    bulkData->declare_relation(elem, side, upperSideLID);

    stk::mesh::Entity const* rel_elemNodes = bulkData->begin_nodes(elem);
    for (int j = 0; j < numBasalSidePoints; j++) {
      stk::mesh::Entity node = rel_elemNodes[this->meshSpecs[0]->ctd.side[upperSideLID].node[j]];
      bulkData->declare_relation(side, node, j);
    }
  }

  bulkData->modification_end();

}

Teuchos::RCP<const Teuchos::ParameterList> Albany::ExtrudedSTKMeshStruct::getValidDiscretizationParameters() const {
  Teuchos::RCP<Teuchos::ParameterList> validPL = this->getValidGenericSTKParameters("Valid Extruded_DiscParams");
  validPL->set<std::string>("Exodus Input File Name", "", "File Name For Exodus Mesh Input");
  validPL->set<std::string>("Surface Height File Name", "surface_height.ascii", "Name of the file containing the surface height data");
  validPL->set<std::string>("Thickness File Name", "thickness.ascii", "Name of the file containing the thickness data");
  validPL->set<std::string>("Surface Velocity File Name", "surface_velocity.ascii", "Name of the file containing the surface velocity data");
  validPL->set<std::string>("Surface Velocity RMS File Name", "velocity_RMS.ascii", "Name of the file containing the surface velocity RMS data");
  validPL->set<std::string>("Basal Friction File Name", "basal_friction.ascii", "Name of the file containing the basal friction data");
  validPL->set<std::string>("Temperature File Name", "temperature.ascii", "Name of the file containing the temperature data");
  validPL->set<std::string>("Element Shape", "Hexahedron", "Shape of the Element: Tetrahedron, Wedge, Hexahedron");
  validPL->set<int>("NumLayers", 10, "Number of vertical Layers of the extruded mesh. In a vertical column, the mesh will have numLayers+1 nodes");
  validPL->set<bool>("Use Glimmer Spacing", false, "When true, the layer spacing is computed according to Glimmer formula (layers are denser close to the bedrock)");
  validPL->set<bool>("Columnwise Ordering", false, "True for Columnwise ordering, false for Layerwise ordering");
  return validPL;
}

void Albany::ExtrudedSTKMeshStruct::read2DFileSerial(std::string &fname, Teuchos::RCP<Tpetra_Vector> content, const Teuchos::RCP<const Teuchos_Comm>& comm) {
  long long int numNodes;
  Teuchos::ArrayRCP<ST> content_nonConstView = content->get1dViewNonConst();
  if (comm->getRank() == 0) {
    std::ifstream ifile;
    ifile.open(fname.c_str());
    if (ifile.is_open()) {
      ifile >> numNodes;
      TEUCHOS_TEST_FOR_EXCEPTION(numNodes != content->getLocalLength(), Teuchos::Exceptions::InvalidParameterValue, std::endl << "Error in ExtrudedSTKMeshStruct: Number of nodes in file " << fname << " (" << numNodes << ") is different from the number expected (" << content->getLocalLength() << ")" << std::endl);

      for (long long int i = 0; i < numNodes; i++)
        ifile >> content_nonConstView[i];
      ifile.close();
    } else {
      std::cout << "Warning in ExtrudedSTKMeshStruct: Unable to open the file " << fname << std::endl;
    }
  }
}

void Albany::ExtrudedSTKMeshStruct::readFileSerial(std::string &fname, std::vector<Tpetra_Vector>& contentVec, const Teuchos::RCP<const Teuchos_Comm>& comm) {
  long long int numNodes, numComponents;
  if (comm->getRank() == 0) {
    std::ifstream ifile;
    ifile.open(fname.c_str());
    if (ifile.is_open()) {
      ifile >> numNodes >> numComponents;
      TEUCHOS_TEST_FOR_EXCEPTION(numNodes != contentVec[0].getLocalLength(), Teuchos::Exceptions::InvalidParameterValue,
          std::endl << "Error in ExtrudedSTKMeshStruct: Number of nodes in file " << fname << " (" << numNodes << ") is different from the number expected (" << contentVec[0].getLocalLength() << ")" << std::endl);
      TEUCHOS_TEST_FOR_EXCEPTION(numComponents != contentVec.size(), Teuchos::Exceptions::InvalidParameterValue,
          std::endl << "Error in ExtrudedSTKMeshStruct: Number of components in file " << fname << " (" << numComponents << ") is different from the number expected (" << contentVec.size() << ")" << std::endl);
      for (int il = 0; il < numComponents; ++il) {
        Teuchos::ArrayRCP<ST> contentVec_nonConstView = contentVec[il].get1dViewNonConst();
        for (long long int i = 0; i < numNodes; i++)
          ifile >> contentVec_nonConstView[i];
      }
      ifile.close();
    } else {
      std::cout << "Warning in ExtrudedSTKMeshStruct: Unable to open input file " << fname << std::endl;
      //	TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
      //			std::endl << "Error in ExtrudedSTKMeshStruct: Unable to open input file " << fname << std::endl);
    }
  }
}

void Albany::ExtrudedSTKMeshStruct::readFileSerial(std::string &fname, Teuchos::RCP<const Tpetra_Map> map_serial, Teuchos::RCP<const Tpetra_Map> map, Teuchos::RCP<Tpetra_Import> importOperator, std::vector<Tpetra_Vector>& temperatureVec, std::vector<double>& zCoords, const Teuchos::RCP<const Teuchos_Comm>& comm) {
  long long int numNodes;
  int numComponents;
  std::ifstream ifile;
  if (comm->getRank() == 0) {
    ifile.open(fname.c_str());
    if (ifile.is_open()) {
      ifile >> numNodes >> numComponents;

    //  std::cout << "numNodes >> numComponents: " << numNodes << " " << numComponents << std::endl;

      TEUCHOS_TEST_FOR_EXCEPTION(numNodes != map_serial->getNodeNumElements(), Teuchos::Exceptions::InvalidParameterValue,
          std::endl << "Error in ExtrudedSTKMeshStruct: Number of nodes in file " << fname << " (" << numNodes << ") is different from the number expected (" << map_serial->getNodeNumElements() << ")" << std::endl);
    } else {
      std::cout << "Warning in ExtrudedSTKMeshStruct: Unable to open input file " << fname << std::endl;
      //	TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
      //			std::endl << "Error in ExtrudedSTKMeshStruct: Unable to open input file " << fname << std::endl);
    }
  }
  //comm->Broadcast(&numComponents, 1, 0);
  //Cast numComponents as double to do broadcast
  //IK, 10/1/14: Need to look into...
  double numComponentsD = numComponents;
  Teuchos::broadcast<LO, double>(*comm, 0, &numComponentsD);
  zCoords.resize(numComponents);
  Teuchos::RCP<Tpetra_Vector> tempT = Teuchos::rcp(new Tpetra_Vector(map_serial));

  if (comm->getRank() == 0) {
    for (int i = 0; i < numComponents; ++i)
      ifile >> zCoords[i];
  }
  //comm->Broadcast(&zCoords[0], numComponents, 0);
  //IK, 10/1/14: double should be ST? 
  Teuchos::broadcast<LO, double>(*comm, 0, &zCoords[0]);
  
  temperatureVec.resize(numComponents, Tpetra_Vector(map));

  Teuchos::ArrayRCP<ST> tempT_nonConstView = tempT->get1dViewNonConst();
  for (int il = 0; il < numComponents; ++il) {
    if (comm->getRank() == 0)
      for (long long int i = 0; i < numNodes; i++)
        ifile >> tempT_nonConstView[i];
    temperatureVec[il].doImport(*tempT, *importOperator, Tpetra::INSERT);
  }

  if (comm->getRank() == 0)
    ifile.close();

}
