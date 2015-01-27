//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
//
// IK, 1/8/15: This is a new class where we will implement spectral
// elements for Aeras, by reading in a STK mesh from an Exodus file
// containing a bilinear quad/hex mesh and enriching it with
// additional nodes to create a higher order mesh.
//
// I have begun filling in to-dos here towards getting the spectral
// elements, but have not completed it.  Basically all the stk::mesh
// calls need to be examined and many of them rewritten.  The
// coordinates array needs to be sized to have the new enriched nodes.
// All the maps need to be defined to have the enriched mesh.
// wsElNode* objects need to be populated with enriched mesh.

// Standard includes
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

// Trilinos includes
#include <Teuchos_TwoDArray.hpp>
#include <Shards_BasicTopologies.hpp>
#include <Intrepid_CellTools.hpp>
#include <Intrepid_Basis.hpp>
#include <Intrepid_HGRAD_QUAD_Cn_FEM.hpp>
#include <Intrepid_CubaturePolylib.hpp>
#include <stk_util/parallel/Parallel.hpp>
#include <stk_mesh/base/FEMHelpers.hpp>
#include <stk_mesh/base/Entity.hpp>
#include <stk_mesh/base/CreateEdges.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include <stk_mesh/base/GetBuckets.hpp>
#include <stk_mesh/base/Selector.hpp>
#include <PHAL_Dimension.hpp>
#ifdef ALBANY_SEACAS
#include <Ionit_Initializer.h>
#include <netcdf.h>
#ifdef ALBANY_PAR_NETCDF
extern "C" 
{
#include <netcdf_par.h>
}
#endif
#endif
#ifdef ALBANY_EPETRA
#include "Epetra_Export.h"
#include "EpetraExt_MultiVectorOut.h"
#include "Petra_Converters.hpp"
#endif

// Albany includes
#include "Albany_Utils.hpp"
#include "Albany_NodalGraphUtils.hpp"
#include "Albany_STKNodeFieldContainer.hpp"
#include "Albany_BucketArray.hpp"
#include "Aeras_SpectralDiscretization.hpp"

// Constants
const double pi = 3.1415926535897932385;

const Tpetra::global_size_t INVALID =
  Teuchos::OrdinalTraits<Tpetra::global_size_t>::invalid ();

// Uncomment the following line if you want debug output to be printed to screen
// #define OUTPUT_TO_SCREEN

Aeras::SpectralDiscretization::
SpectralDiscretization(Teuchos::RCP<Albany::AbstractSTKMeshStruct> stkMeshStruct_,
                  const Teuchos::RCP<const Teuchos_Comm>& commT_,
                  const Teuchos::RCP<Albany::RigidBodyModes>& rigidBodyModes_) :
  out(Teuchos::VerboseObjectBase::getDefaultOStream()),
  previous_time_label(-1.0e32),
  metaData(*stkMeshStruct_->metaData),
  bulkData(*stkMeshStruct_->bulkData),
  commT(commT_),
  rigidBodyModes(rigidBodyModes_),
  neq(stkMeshStruct_->neq),
  stkMeshStruct(stkMeshStruct_),
  interleavedOrdering(stkMeshStruct_->interleavedOrdering)
{
#ifdef OUTPUT_TO_SCREEN
  std::cout <<"In Aeras::SpectralDiscretization constructor!" << std::endl; 
#endif

#ifdef ALBANY_EPETRA
  comm = Albany::createEpetraCommFromTeuchosComm(commT_);
#endif

  // Get from parameter list how many points per edge we have (default = 2: no enrichment)
  points_per_edge = stkMeshStruct->points_per_edge; 
  Aeras::SpectralDiscretization::updateMesh();
}

Aeras::SpectralDiscretization::~SpectralDiscretization()
{
#ifdef ALBANY_SEACAS
  if (stkMeshStruct->cdfOutput)
      if (netCDFp)
    if (const int ierr = nc_close (netCDFp))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "close returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
#endif

  for (int i=0; i< toDelete.size(); i++) delete [] toDelete[i];
}


#ifdef ALBANY_EPETRA
Teuchos::RCP<const Epetra_Map>
Aeras::SpectralDiscretization::getMap() const
{
  // AMB: The new nodalDOFsStructContainer means that when Epetra is enabled, the
  // Epetra maps have precedence, and the Tpetra maps must remain consistent
  // with them.
  //dp-remove Teuchos::RCP<const Epetra_Map> map = Petra::TpetraMap_To_EpetraMap(mapT, comm);
  return map;
}
#endif

Teuchos::RCP<const Tpetra_Map>
Aeras::SpectralDiscretization::getMapT() const
{
  return mapT;
}


#ifdef ALBANY_EPETRA
Teuchos::RCP<const Epetra_Map>
Aeras::SpectralDiscretization::getOverlapMap() const
{
  //dp-remove Teuchos::RCP<const Epetra_Map> overlap_map = Petra::TpetraMap_To_EpetraMap(overlap_mapT, comm);
  return overlap_map;
}
#endif

Teuchos::RCP<const Tpetra_Map>
Aeras::SpectralDiscretization::getOverlapMapT() const
{
  return overlap_mapT;
}

#ifdef ALBANY_EPETRA
Teuchos::RCP<const Epetra_Map>
Aeras::SpectralDiscretization::getMap(const std::string& field_name) const 
{
  return nodalDOFsStructContainer.getDOFsStruct(field_name).map;
}

Teuchos::RCP<const Epetra_Map>
Aeras::SpectralDiscretization::getOverlapMap(const std::string& field_name) const 
{
  return nodalDOFsStructContainer.getDOFsStruct(field_name).overlap_map;
}

Teuchos::RCP<const Epetra_CrsGraph>
Aeras::SpectralDiscretization::getJacobianGraph() const
{
  Teuchos::RCP<const Epetra_CrsGraph> graph= Petra::TpetraCrsGraph_To_EpetraCrsGraph(graphT, comm);
  return graph;
}
#endif

Teuchos::RCP<const Tpetra_CrsGraph>
Aeras::SpectralDiscretization::getJacobianGraphT() const
{
  return graphT;
}

#ifdef ALBANY_EPETRA
Teuchos::RCP<const Epetra_CrsGraph>
Aeras::SpectralDiscretization::getOverlapJacobianGraph() const
{
  Teuchos::RCP<const Epetra_CrsGraph> overlap_graph= Petra::TpetraCrsGraph_To_EpetraCrsGraph(overlap_graphT, comm);
  return overlap_graph;
}
#endif

Teuchos::RCP<const Tpetra_CrsGraph>
Aeras::SpectralDiscretization::getOverlapJacobianGraphT() const
{
  return overlap_graphT;
}


#ifdef ALBANY_EPETRA
Teuchos::RCP<const Epetra_Map>
Aeras::SpectralDiscretization::getNodeMap() const
{
  //dp-remove Teuchos::RCP<const Epetra_Map> node_map = Petra::TpetraMap_To_EpetraMap(node_mapT, comm);
  return node_map;
}

Teuchos::RCP<const Epetra_Map>
Aeras::SpectralDiscretization::getOverlapNodeMap() const
{
  return overlap_node_map;
}
#endif

Teuchos::RCP<const Tpetra_Map>
Aeras::SpectralDiscretization::getNodeMapT() const
{
  return node_mapT;
}

Teuchos::RCP<const Tpetra_Map>
Aeras::SpectralDiscretization::getOverlapNodeMapT() const
{
  return overlap_node_mapT;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<LO> > > >::type&
Aeras::SpectralDiscretization::getWsElNodeEqID() const
{
  return wsElNodeEqID;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> > >::type&
Aeras::SpectralDiscretization::getWsElNodeID() const
{
  return wsElNodeID;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type&
Aeras::SpectralDiscretization::getCoords() const
{
  return coords;
}

const Albany::WorksetArray<Teuchos::ArrayRCP<double> >::type&
Aeras::SpectralDiscretization::getSphereVolume() const
{
  return sphereVolume;
}

void
Aeras::SpectralDiscretization::printCoords() const
{
  std::cout << "Processor " << bulkData.parallel_rank() << " has "
            << coords.size() << " worksets." << std::endl;
  for (int ws = 0; ws < coords.size(); ws++)             // workset
  {
    for (int e = 0; e < coords[ws].size(); e++)          // cell
    {
      for (int j = 0; j < coords[ws][e].size(); j++)     // node
      {
        for (int d = 0; d < stkMeshStruct->numDim; d++)  // node
        {
          std::cout << "Coord for workset: " << ws << " element: " << e
                    << " node: " << j << " DOF: " << d << " is: "
                    << coords[ws][e][j][d] << std::endl;
        }
      }
    }
  }
}


// IK, 1/8/15, FIXME: getCoordinates() needs to be rewritten to
// include the enriched nodes.
const Teuchos::ArrayRCP<double>&
Aeras::SpectralDiscretization::getCoordinates() const
{
  // Coordinates are computed here, and not precomputed,
  // since the mesh can move in shape opt problems

  Albany::AbstractSTKFieldContainer::VectorFieldType* coordinates_field = stkMeshStruct->getCoordinatesField();

  for (int i=0; i < numOverlapNodes; i++)
  {
    GO node_gid = gid(overlapnodes[i]);
    int node_lid = overlap_node_mapT->getLocalElement(node_gid);

    double* x = stk::mesh::field_data(*coordinates_field, overlapnodes[i]);
    for (int dim=0; dim<stkMeshStruct->numDim; dim++)
      coordinates[3*node_lid + dim] = x[dim];

  }

  return coordinates;
}

// These methods were added to support mesh adaptation, which is currently
// limited to FMDBDiscretization.
void Aeras::SpectralDiscretization::
setCoordinates(const Teuchos::ArrayRCP<const double>& c)
{
  TEUCHOS_TEST_FOR_EXCEPTION(
    true, std::logic_error,
    "Aeras::SpectralDiscretization::setCoordinates is not implemented.");
}
void Aeras::SpectralDiscretization::
setReferenceConfigurationManager(const Teuchos::RCP<AAdapt::rc::Manager>& rcm)
{
  TEUCHOS_TEST_FOR_EXCEPTION(
    true, std::logic_error,
    "Aeras::SpectralDiscretization::setReferenceConfigurationManager is not implemented.");
}

// The function transformMesh() maps a unit cube domain by applying a transformation

// IK, 1/8/15, FIXME: I've removed all the FELIX stuff from
// transformMesh() as this is for now an Aeras-only class.  The
// setting of the schar mountain transformation needs to be fixed to
// use the new (enriched) nodes rather than the nodes pulled from STK.
void
Aeras::SpectralDiscretization::transformMesh()
{
  using std::cout; using std::endl;
  Albany::AbstractSTKFieldContainer::VectorFieldType* coordinates_field = stkMeshStruct->getCoordinatesField();
  std::string transformType = stkMeshStruct->transformType;

  if (transformType == "None") {}
  else if (transformType == "Aeras Schar Mountain")
  {
    *out << "Aeras Schar Mountain transformation!" << endl;
    double rhoOcean = 1028.0; // ocean density, in kg/m^3
    for (int i=0; i < numOverlapNodes; i++)
    {
      double* x = stk::mesh::field_data(*coordinates_field, overlapnodes[i]);
      x[0] = x[0];
      double hstar = 0.0, h;
      if (std::abs(x[0]-150.0) <= 25.0)
        hstar = 3.0* std::pow(cos(M_PI*(x[0]-150.0) / 50.0),2);
      h = hstar * std::pow(cos(M_PI*(x[0]-150.0) / 8.0),2);
      x[1] = x[1] + h*(25.0 - x[1])/25.0;
    }
  }
  else
  {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
      "Aeras::SpectralDiscretization::transformMesh() Unknown transform type :" << transformType << std::endl);
  }
}

//IK, 1/23/15: ultimately we want to implement setupMLCoords() 
//for the enriched mesh.  This could only be needed with ML/MueLu preconditioners.
void Aeras::SpectralDiscretization::setupMLCoords()
{
  *out << "Warning: setupMLCoords() not yet implemented in Aeras::SpectralDiscretization!" << 
          "ML and MueLu will not receive coordinates for repartitioning if used." << std::endl; 
/*  if (rigidBodyModes.is_null()) return;
  if (!rigidBodyModes->isMLUsed() && !rigidBodyModes->isMueLuUsed()) return;

  const int numDim = stkMeshStruct->numDim;
  Albany::AbstractSTKFieldContainer::VectorFieldType*
    coordinates_field = stkMeshStruct->getCoordinatesField();
  rigidBodyModes->resize(numDim, numOwnedNodes);
  double* const coords = rigidBodyModes->getCoordArray();

  for (int i = 0; i < numOwnedNodes; i++)
  {
    GO node_gid = gid(ownednodes[i]);
    int node_lid = node_mapT->getLocalElement(node_gid);
    double* X = stk::mesh::field_data(*coordinates_field, ownednodes[i]);
    for (int j = 0; j < numDim; j++)
      coords[j*numOwnedNodes + node_lid] = X[j];
  }

  rigidBodyModes->setCoordinatesAndNullspace(node_mapT, mapT);

  // Some optional matrix-market output was tagged on here; keep that
  // functionality.
  writeCoordsToMatrixMarket();
  */
}

void Aeras::SpectralDiscretization::writeCoordsToMatrixMarket() const
{
  // if user wants to write the coordinates to matrix market file,
  // write them to matrix market file
  if (rigidBodyModes->isMLUsed() && stkMeshStruct->writeCoordsToMMFile)
  {
    double *xx, *yy, *zz;
    rigidBodyModes->getCoordArrays(xx, yy, zz);
    if (node_mapT->getComm()->getRank()==0) {std::cout << "Writing mesh coordinates to Matrix Market file." << std::endl;}
    int numMyElements = (node_mapT->getComm()->getRank() == 0) ? node_mapT->getGlobalNumElements() : 0;
    Teuchos::RCP<Tpetra_Import> importOperatorT;
    Teuchos::RCP<Tpetra_Map> serial_mapT;
    Teuchos::ArrayView<ST> xxAV = Teuchos::arrayView(xx, numOwnedNodes);
    Teuchos::RCP<Tpetra_Vector> xCoordsT = Teuchos::rcp(new Tpetra_Vector(node_mapT, xxAV));
    // Writing of coordinates to MatrixMarket file for Ray
    if (node_mapT->getComm()->getSize() > 1)
    {
      serial_mapT = Teuchos::rcp(new Tpetra_Map(INVALID, numMyElements, 0, node_mapT->getComm()));
      // create importer from parallel map to serial map and populate serial solution xfinal_serial
      importOperatorT = Teuchos::rcp(new Tpetra_Import(node_mapT, serial_mapT));
      // Writing of coordinates to MatrixMarket file for Ray
      Teuchos::RCP<Tpetra_Vector> xCoords_serialT = Teuchos::rcp(new Tpetra_Vector(serial_mapT));
      xCoords_serialT->doImport(*xCoordsT, *importOperatorT, Tpetra::INSERT);
      Tpetra_MatrixMarket_Writer::writeDenseFile("xCoords.mm", xCoords_serialT);
    }
    else
      Tpetra_MatrixMarket_Writer::writeDenseFile("xCoords.mm", xCoordsT);
    if (yy != NULL)
    {
      Teuchos::ArrayView<ST> yyAV = Teuchos::arrayView(yy, numOwnedNodes);
      Teuchos::RCP<Tpetra_Vector> yCoordsT = Teuchos::rcp(new Tpetra_Vector(node_mapT, yyAV));  
      if (node_mapT->getComm()->getSize() > 1)
      {
        Teuchos::RCP<Tpetra_Vector> yCoords_serialT = Teuchos::rcp(new Tpetra_Vector(serial_mapT));
        yCoords_serialT->doImport(*yCoordsT, *importOperatorT, Tpetra::INSERT);
        Tpetra_MatrixMarket_Writer::writeDenseFile("yCoords.mm", yCoords_serialT);
      }
      else 
        Tpetra_MatrixMarket_Writer::writeDenseFile("yCoords.mm", yCoordsT);
    }
    if (zz != NULL)
    {
      Teuchos::ArrayView<ST> zzAV = Teuchos::arrayView(zz, numOwnedNodes);
      Teuchos::RCP<Tpetra_Vector> zCoordsT = Teuchos::rcp(new Tpetra_Vector(node_mapT, zzAV));
      if (node_mapT->getComm()->getSize() > 1)
      {
        Teuchos::RCP<Tpetra_Vector> zCoords_serialT = Teuchos::rcp(new Tpetra_Vector(serial_mapT));
        zCoords_serialT->doImport(*zCoordsT, *importOperatorT, Tpetra::INSERT);
        Tpetra_MatrixMarket_Writer::writeDenseFile("zCoords.mm", zCoords_serialT);
      }
      else 
        Tpetra_MatrixMarket_Writer::writeDenseFile("zCoords.mm", zCoordsT);
    }
  }
}

const Albany::WorksetArray<std::string>::type&
Aeras::SpectralDiscretization::getWsEBNames() const
{
  return wsEBNames;
}

const Albany::WorksetArray<int>::type&
Aeras::SpectralDiscretization::getWsPhysIndex() const
{
  return wsPhysIndex;
}

#ifdef ALBANY_EPETRA
void
Aeras::SpectralDiscretization::writeSolution(const Epetra_Vector& soln,
                                             const double time, const bool overlapped)
{
  // Put solution as Epetra_Vector into STK Mesh
  if(!overlapped)
    setSolutionField(soln);

  // soln coming in is overlapped
  else
    setOvlpSolutionField(soln);

#ifdef ALBANY_SEACAS

  if (stkMeshStruct->exoOutput && stkMeshStruct->transferSolutionToCoords)
  {
    Teuchos::RCP<Albany::AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

    container->transferSolutionToCoords();

    if (!mesh_data.is_null())
    {
      // Mesh coordinates have changed. Rewrite output file by
      // deleting the mesh data object and recreate it
      setupExodusOutput();
    }
  }


   // Skip this write unless the proper interval has been reached
  if (stkMeshStruct->exoOutput && !(outputInterval % stkMeshStruct->exoOutputInterval)) {

     double time_label = monotonicTimeLabel(time);

     int out_step = mesh_data->process_output_request(outputFileIdx, time_label);

     if (mapT->getComm()->getRank()==0) {
       *out << "Aeras::SpectralDiscretization::writeSolution: writing time " << time;
       if (time_label != time) *out << " with label " << time_label;
       *out << " to index " <<out_step<<" in file "<<stkMeshStruct->exoOutFile<< std::endl;
     }
  }
  outputInterval++;
#endif
}
#endif

void
Aeras::SpectralDiscretization::writeSolutionT(const Tpetra_Vector& solnT,
                                              const double time,
                                              const bool overlapped)
{
  writeSolutionToMeshDatabaseT(solnT, time, overlapped);
  writeSolutionToFileT(solnT, time, overlapped);
}

void
Aeras::SpectralDiscretization::writeSolutionToMeshDatabaseT(const Tpetra_Vector& solnT,
                                                            const double time,
                                                            const bool overlapped)
{
  // Put solution as Epetra_Vector into STK Mesh
  if (!overlapped)
    setSolutionFieldT(solnT);
  // soln coming in is overlapped
  else
    setOvlpSolutionFieldT(solnT);
}

void
Aeras::SpectralDiscretization::writeSolutionToFileT(const Tpetra_Vector& solnT,
                                                    const double time,
                                                    const bool overlapped)
{
#ifdef ALBANY_SEACAS

  if (stkMeshStruct->exoOutput && stkMeshStruct->transferSolutionToCoords)
  {
    Teuchos::RCP<Albany::AbstractSTKFieldContainer> container =
      stkMeshStruct->getFieldContainer();
    container->transferSolutionToCoords();
    if (!mesh_data.is_null())
    {
      // Mesh coordinates have changed. Rewrite output file by
      // deleting the mesh data object and recreate it
      setupExodusOutput();
    }
  }

  // Skip this write unless the proper interval has been reached
  if (stkMeshStruct->exoOutput && !(outputInterval % stkMeshStruct->exoOutputInterval))
  {
    double time_label = monotonicTimeLabel(time);
    int out_step = mesh_data->process_output_request(outputFileIdx, time_label);
    if (mapT->getComm()->getRank() == 0)
    {
      *out << "Aeras::SpectralDiscretization::writeSolution: writing time " << time;
      if (time_label != time) *out << " with label " << time_label;
      *out << " to index " <<out_step<<" in file "<<stkMeshStruct->exoOutFile<< std::endl;
    }
  }
  if (stkMeshStruct->cdfOutput && !(outputInterval % stkMeshStruct->cdfOutputInterval))
  {
    double time_label = monotonicTimeLabel(time);
    const int out_step = processNetCDFOutputRequestT(solnT);
    if (mapT->getComm()->getRank() == 0)
    {
      *out << "Aeras::SpectralDiscretization::writeSolution: writing time " << time;
      if (time_label != time) *out << " with label " << time_label;
      *out << " to index " <<out_step<<" in file "<<stkMeshStruct->cdfOutFile<< std::endl;
    }
  }
  outputInterval++;
#endif

}

double
Aeras::SpectralDiscretization::monotonicTimeLabel(const double time)
{
  // If increasing, then all is good
  if (time > previous_time_label)
  {
    previous_time_label = time;
    return time;
  }

  // Try absolute value
  double time_label = fabs(time);
  if (time_label > previous_time_label)
  {
    previous_time_label = time_label;
    return time_label;
  }

  // Try adding 1.0 to time
  if (time_label+1.0 > previous_time_label)
  {
    previous_time_label = time_label+1.0;
    return time_label+1.0;
  }

  // Otherwise, just add 1.0 to previous
  previous_time_label += 1.0;
  return previous_time_label;
}

#ifdef ALBANY_EPETRA
void
Aeras::SpectralDiscretization::setResidualField(const Epetra_Vector& residual)
{
  // Nothing to do for Aeras -- LCM-only function
}
#endif

void
Aeras::SpectralDiscretization::setResidualFieldT(const Tpetra_Vector& residualT)
{
  // Nothing to do for Aeras -- LCM-only function
}


#ifdef ALBANY_EPETRA
Teuchos::RCP<Epetra_Vector>
Aeras::SpectralDiscretization::getSolutionField(bool overlapped) const
{
  // Copy soln vector into solution field, one node at a time
  Teuchos::ArrayView<const GO> indicesAV = mapT->getNodeElementList();
  int numElements = mapT->getNodeNumElements();
#ifdef ALBANY_64BIT_INT
  Teuchos::Array<int> i_indices(numElements);
  for(std::size_t k = 0; k < numElements; k++)
	i_indices[k] = Teuchos::as<int>(indicesAV[k]);
  Teuchos::RCP<Epetra_Map> map = Teuchos::rcp(new Epetra_Map(-1, numElements, i_indices.getRawPtr(), 0, *comm));
#else
  Teuchos::RCP<Epetra_Map> map = Teuchos::rcp(new Epetra_Map(-1, numElements, indicesAV.getRawPtr(), 0, *comm));
#endif
  Teuchos::RCP<Epetra_Vector> soln = Teuchos::rcp(new Epetra_Vector(*map));
  this->getSolutionField(*soln, overlapped);
  return soln;
}
#endif

Teuchos::RCP<Tpetra_Vector>
Aeras::SpectralDiscretization::getSolutionFieldT(bool overlapped) const
{
  // Copy soln vector into solution field, one node at a time
  Teuchos::RCP<Tpetra_Vector> solnT = Teuchos::rcp(new Tpetra_Vector(mapT));
  this->getSolutionFieldT(*solnT, overlapped);
  return solnT;
}


int
Aeras::SpectralDiscretization::getSolutionFieldHistoryDepth() const
{
  return stkMeshStruct->getSolutionFieldHistoryDepth();
}

#ifdef ALBANY_EPETRA
Teuchos::RCP<Epetra_MultiVector>
Aeras::SpectralDiscretization::getSolutionFieldHistory() const
{
  const int stepCount = this->getSolutionFieldHistoryDepth();
  return this->getSolutionFieldHistoryImpl(stepCount);
}

Teuchos::RCP<Epetra_MultiVector>
Aeras::SpectralDiscretization::getSolutionFieldHistory(int maxStepCount) const
{
  const int stepCount = std::min(this->getSolutionFieldHistoryDepth(), maxStepCount);
  return this->getSolutionFieldHistoryImpl(stepCount);
}

// IK, 10/28/13: this function should be converted to Tpetra...
void
Aeras::SpectralDiscretization::getSolutionFieldHistory(Epetra_MultiVector &result) const
{
  Teuchos::RCP<Epetra_Map> map = Petra::TpetraMap_To_EpetraMap(mapT, comm);
  TEUCHOS_TEST_FOR_EXCEPT(!map->SameAs(result.Map()));
  const int stepCount = std::min(this->getSolutionFieldHistoryDepth(), result.NumVectors());
  Epetra_MultiVector head(View, result, 0, stepCount);
  this->getSolutionFieldHistoryImpl(head);
}

Teuchos::RCP<Epetra_MultiVector>
Aeras::SpectralDiscretization::getSolutionFieldHistoryImpl(int stepCount) const
{
  const int vectorCount = stepCount > 0 ? stepCount : 1; // A valid MultiVector has at least one vector
  Teuchos::ArrayView<const GO> indicesAV = mapT->getNodeElementList();
  LO numElements = mapT->getNodeNumElements();
#ifdef ALBANY_64BIT_INT
  Teuchos::Array<int> i_indices(numElements);
  for(std::size_t k = 0; k < numElements; k++)
	i_indices[k] = Teuchos::as<int>(indicesAV[k]);
  Teuchos::RCP<Epetra_Map> map = Teuchos::rcp(new Epetra_Map(-1, numElements, i_indices.getRawPtr(), 0, *comm));
#else
  Teuchos::RCP<Epetra_Map> map = Teuchos::rcp(new Epetra_Map(-1, numElements, indicesAV.getRawPtr(), 0, *comm));
#endif
  const Teuchos::RCP<Epetra_MultiVector> result = Teuchos::rcp(new Epetra_MultiVector(*map, vectorCount));
  if (stepCount > 0)
  {
    this->getSolutionFieldHistoryImpl(*result);
  }
  return result;
}

void
Aeras::SpectralDiscretization::getSolutionFieldHistoryImpl(Epetra_MultiVector &result) const
{
  const int stepCount = result.NumVectors();
  for (int i = 0; i < stepCount; ++i)
  {
    stkMeshStruct->loadSolutionFieldHistory(i);
    Epetra_Vector v(View, result, i);
    this->getSolutionField(v);
  }
}

// IK, 1/8/15, FIXME: getSolutionField*, getField*, setField*,
// setSolutionField* need to work with enriched nodes rather than STK
// nodes.
void
Aeras::SpectralDiscretization::getSolutionField(Epetra_Vector &result, const bool overlapped) const
{
  TEUCHOS_TEST_FOR_EXCEPTION(overlapped, std::logic_error, "Not implemented.");

  Teuchos::RCP<Albany::AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::Selector locally_owned = metaData.locally_owned_part();

  Teuchos::RCP<Epetra_Map> node_map = Petra::TpetraMap_To_EpetraMap(node_mapT, comm);
  container->fillSolnVector(result, locally_owned, node_map);
}

void
Aeras::SpectralDiscretization::getField(Epetra_Vector &result, const std::string& name) const
{
  Teuchos::RCP<Albany::AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  const std::string& part = nodalDOFsStructContainer.fieldToMap.find(name)->second->first.first;
  stk::mesh::Selector selector = metaData.locally_owned_part();
  if(part.size())
  {
    std::map<std::string, stk::mesh::Part*>::const_iterator it = stkMeshStruct->nsPartVec.find(part);
    if(it != stkMeshStruct->nsPartVec.end())
      selector &= stk::mesh::Selector( *(it->second) );
  }


  const DOFsStruct& dofsStruct = nodalDOFsStructContainer.getDOFsStruct(name);

  container->fillVector(result, name, selector, dofsStruct.node_map,dofsStruct.dofManager);
}
#endif // ALBANY_EPETRA

void
Aeras::SpectralDiscretization::getSolutionFieldT(Tpetra_Vector &resultT, const bool overlapped) const
{
  TEUCHOS_TEST_FOR_EXCEPTION(overlapped, std::logic_error, "Not implemented.");

  Teuchos::RCP<Albany::AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::Selector locally_owned = metaData.locally_owned_part();

  container->fillSolnVectorT(resultT, locally_owned, node_mapT);
}

/*****************************************************************/
/*** Private functions follow. These are just used in above code */
/*****************************************************************/

#ifdef ALBANY_EPETRA
void
Aeras::SpectralDiscretization::setField(const Epetra_Vector &result, const std::string& name, bool overlapped)
{
  Teuchos::RCP<Albany::AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  const std::string& part = nodalDOFsStructContainer.fieldToMap.find(name)->second->first.first;

  stk::mesh::Selector selector = overlapped ?
      metaData.locally_owned_part() | metaData.globally_shared_part() :
      metaData.locally_owned_part();

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  if(part.size())
  {
    std::map<std::string, stk::mesh::Part*>::const_iterator it = stkMeshStruct->nsPartVec.find(part);
    if(it != stkMeshStruct->nsPartVec.end())
      selector &= stk::mesh::Selector( *(it->second) );
  }

  const DOFsStruct& dofsStruct = nodalDOFsStructContainer.getDOFsStruct(name);

  if(overlapped)
    container->saveVector(result, name, selector, dofsStruct.overlap_node_map, dofsStruct.overlap_dofManager);
  else
    container->saveVector(result, name, selector, dofsStruct.node_map, dofsStruct.dofManager);
}

void
Aeras::SpectralDiscretization::setSolutionField(const Epetra_Vector& soln)
{
  // Copy soln vector into solution field, one node at a time
  // Note that soln coming in is the local (non overlapped) soln

  Teuchos::RCP<Albany::AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the on-processor nodes
  stk::mesh::Selector locally_owned = metaData.locally_owned_part();

  Teuchos::RCP<Epetra_Map> node_map = Petra::TpetraMap_To_EpetraMap(node_mapT, comm);
  container->saveSolnVector(soln, locally_owned, node_map);

}
#endif // ALBANY_EPETRA

// Tpetra version of above
void
Aeras::SpectralDiscretization::setSolutionFieldT(const Tpetra_Vector& solnT)
{

  // Copy soln vector into solution field, one node at a time
  // Note that soln coming in is the local (non overlapped) soln

  Teuchos::RCP<Albany::AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the on-processor nodes
  stk::mesh::Selector locally_owned = metaData.locally_owned_part();

  container->saveSolnVectorT(solnT, locally_owned, node_mapT);

}

#ifdef ALBANY_EPETRA
void
Aeras::SpectralDiscretization::setOvlpSolutionField(const Epetra_Vector& soln)
{
  // Copy soln vector into solution field, one node at a time
  // Note that soln coming in is the local+ghost (overlapped) soln

  Teuchos::RCP<Albany::AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the processor-visible nodes
  stk::mesh::Selector select_owned_or_shared = metaData.locally_owned_part() | metaData.globally_shared_part();

  Teuchos::RCP<Epetra_Map> overlap_node_map = Petra::TpetraMap_To_EpetraMap(overlap_node_mapT, comm);
  container->saveSolnVector(soln, select_owned_or_shared, overlap_node_map);

}
#endif // ALBANY_EPETRA

void
Aeras::SpectralDiscretization::setOvlpSolutionFieldT(const Tpetra_Vector& solnT)
{
  // Copy soln vector into solution field, one node at a time
  // Note that soln coming in is the local+ghost (overlapped) soln

  Teuchos::RCP<Albany::AbstractSTKFieldContainer> container = stkMeshStruct->getFieldContainer();

  // Iterate over the processor-visible nodes
  stk::mesh::Selector select_owned_or_shared = metaData.locally_owned_part() | metaData.globally_shared_part();

  container->saveSolnVectorT(solnT, select_owned_or_shared, overlap_node_mapT);

}

inline GO Aeras::SpectralDiscretization::gid(const stk::mesh::Entity node) const
{ return bulkData.identifier(node)-1; }

int Aeras::SpectralDiscretization::getOwnedDOF(const int inode, const int eq) const
{
  if (interleavedOrdering) return inode*neq + eq;
  else  return inode + numOwnedNodes*eq;
}

int Aeras::SpectralDiscretization::getOverlapDOF(const int inode, const int eq) const
{
  if (interleavedOrdering) return inode*neq + eq;
  else  return inode + numOverlapNodes*eq;
}

GO Aeras::SpectralDiscretization::getGlobalDOF(const GO inode, const int eq) const
{
  if (interleavedOrdering) return inode*neq + eq;
  else  return inode + numGlobalNodes*eq;
}

int Aeras::SpectralDiscretization::nonzeroesPerRow(const int neq) const
{
  int numDim = stkMeshStruct->numDim;
  int estNonzeroesPerRow;
  switch (numDim)
  {
  case 0: estNonzeroesPerRow=1*neq; break;
  case 1: estNonzeroesPerRow=3*neq; break;
  case 2: estNonzeroesPerRow=9*neq; break;
  case 3: estNonzeroesPerRow=27*neq; break;
  default: TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
			      "SpectralDiscretization:  Bad numDim"<< numDim);
  }
  return estNonzeroesPerRow;
}

stk::mesh::EntityId
Aeras::SpectralDiscretization::getMaximumID(const stk::mesh::EntityRank rank) const
{
  // Get the local maximum ID
  stk::mesh::EntityId last_entity =
    (--bulkData.end_entities(rank))->first.id();

  // Use a parallel MAX reduction to obtain the global maximum ID
  //
  // FIXME: WFS: I added what appear to be unnecessary casts to (int*)
  // in order to avoid compilation errors complaining that we do not
  // have Teuchos::Serializations for unsigned long long.  This
  // appears to be because HAVE_TEUCHOS_LONG_LONG_INT=OFF when
  // building Trilinos.  That might be easily changed, but it might
  // indicate some larger issue that needs to be dealt with.  I will
  // leave it like this until I have figured it out.
  stk::mesh::EntityId result;
  Teuchos::reduceAll(*commT,
                     Teuchos::REDUCE_MAX,
                     1,
                     (int*)(&last_entity),
                     (int*)(&result));
  return result;
}

void Aeras::SpectralDiscretization::enrichMesh()
{
  *out << "In Aeras::SpectralDiscretization::enrichMesh()" << std::endl; 
  // Initialization
  size_t np  = points_per_edge;
  *out << "Points per edge: " << np << std::endl; 
  size_t np2 = np * np;
  GO maxGID    = getMaximumID(stk::topology::NODE_RANK);
  GO maxEdgeID = getMaximumID(stk::topology::EDGE_RANK);

  // Edges are not created by default, so we create them here
  stk::mesh::create_edges(bulkData);

  // Fill in the enriched edge array
  const stk::mesh::BucketVector & edgeBuckets =
    bulkData.buckets(stk::topology::EDGE_RANK);
  enrichedEdges.resize(edgeBuckets.size());
  for (size_t ibuck = 0; ibuck < edgeBuckets.size(); ++ibuck)
  {
    stk::mesh::Bucket & edgeBucket = *edgeBuckets[ibuck];
    enrichedEdges[ibuck].resize(edgeBucket.size());
    for (size_t iedge = 0; iedge < edgeBucket.size(); ++iedge)
    {
      stk::mesh::Entity edge = edgeBucket[iedge];
      unsigned numNodes = bulkData.num_nodes(edge);
      TEUCHOS_TEST_FOR_EXCEPTION(
        numNodes != 2,
        std::logic_error,
        "Starting edges for enriched elements must be linear.  Edge " << edge
        << " has " << numNodes << " nodes.");
      const stk::mesh::Entity * nodes = bulkData.begin_nodes(edge);
      enrichedEdges[ibuck][iedge].resize(np);
      enrichedEdges[ibuck][iedge][0] = gid(nodes[0]);
      for (GO inode = 1; inode < np-1; ++inode)
      {
        enrichedEdges[ibuck][iedge][inode] =
          maxGID + gid(edge)*(np-2) + inode;
      }
      enrichedEdges[ibuck][iedge][np-1] = gid(nodes[1]);
    }
  }

  // Fill in the enriched element array
  const stk::mesh::BucketVector & elementBuckets =
    bulkData.buckets(stk::topology::ELEMENT_RANK);
  wsElNodeID.resize(elementBuckets.size());
  for (size_t ibuck = 0; ibuck < elementBuckets.size(); ++ibuck)
  {
    stk::mesh::Bucket & elementBucket = *elementBuckets[ibuck];
    wsElNodeID[ibuck].resize(elementBucket.size());
    for (size_t ielem = 0; ielem < elementBucket.size(); ++ielem)
    {
      stk::mesh::Entity element = elementBucket[ielem];
      unsigned numNodes = bulkData.num_nodes(element);
      TEUCHOS_TEST_FOR_EXCEPTION(
        numNodes != 4,
        std::logic_error,
        "Starting elements for enriched elements must be linear quadrilaterals."
        "  Element " << element << " has " << numNodes << " nodes.");
      const stk::mesh::Entity * nodes = bulkData.begin_nodes(element);
      // Here we allocate a new Teuchos::TwoDArray (named buffer at
      // address bufferPtr) for storing and accessing a 2D array of
      // element nodes.  We will then construct a Teuchos::ArrayRCP
      // pointing to the same buffer that assumes ownership of the
      // buffer, so that it can be stored in the wsElNodeID data
      // structure.
      Teuchos::TwoDArray<GO> * bufferPtr = new Teuchos::TwoDArray<GO>(np,np);
      Teuchos::TwoDArray<GO> & buffer = *bufferPtr;
      wsElNodeID[ibuck][ielem] =
        Teuchos::ArrayRCP<GO>(&buffer[0][0],0,np2,true);

      // Copy the linear corner node IDs to the enriched element
      buffer[0   ][0   ] = gid(nodes[0]);
      buffer[0   ][np-1] = gid(nodes[1]);
      buffer[np-1][np-1] = gid(nodes[2]);
      buffer[np-1][0   ] = gid(nodes[3]);

      // Copy the enriched edge nodes to the enriched element.  Note
      // that the enriched edge may or may not be aligned with the
      // tensor grid edge.  So we check the first node ID and copy
      // in the appropriate direction.
      const stk::mesh::Entity * edges = bulkData.begin_edges(element);
      // Edge 0
      const stk::mesh::Entity * edgeNodes = bulkData.begin_nodes(edges[0]);
      for (unsigned inode = 1; inode < np-1; ++inode)
        if (edgeNodes[0] == nodes[0])
          buffer[0][inode] = gid(edgeNodes[inode]);
        else
          buffer[0][inode] = gid(edgeNodes[np-inode-1]);
      // Edge 1
      edgeNodes = bulkData.begin_nodes(edges[1]);
      for (unsigned inode = 1; inode < np-1; ++inode)
        if (edgeNodes[1] == nodes[1])
          buffer[inode][np-1] = gid(edgeNodes[inode]);
        else
          buffer[inode][np-1] = gid(edgeNodes[np-inode-1]);
      // Edge 2
      edgeNodes = bulkData.begin_nodes(edges[2]);
      for (unsigned inode = 1; inode < np-1; ++inode)
        if (edgeNodes[2] == nodes[2])
          buffer[np-1][inode] = gid(edgeNodes[np-inode-1]);
        else
          buffer[np-1][inode] = gid(edgeNodes[inode]);
      // Edge 3
      edgeNodes = bulkData.begin_nodes(edges[3]);
      for (unsigned inode = 1; inode < np-1; ++inode)
        if (edgeNodes[3] == nodes[3])
          buffer[inode][0] = gid(edgeNodes[np-inode-1]);
        else
          buffer[inode][0] = gid(edgeNodes[inode]);

      // Create new interior nodes for the enriched element
      GO offset = maxGID + (maxEdgeID+1) * (np-2) +
        gid(element) * (np-2) * (np-2) + 1;
      for (unsigned ii = 0; ii < np-2; ++ii)
        for (unsigned jj = 0; jj < np-2; ++jj)
          buffer[ii+1][jj+1] = offset + ii * (np-2) + jj;
    }
  }

  // Mark locally owned edges as owned
  edgeIsOwned.clear();
  stk::mesh::Selector locally_owned  = metaData.locally_owned_part();
  const stk::mesh::BucketVector & ownedEdgeBuckets =
    bulkData.get_buckets(stk::topology::EDGE_RANK, locally_owned);
  for (size_t ibuck = 0; ibuck < ownedEdgeBuckets.size(); ++ibuck)
  {
    stk::mesh::Bucket & edgeBucket = *ownedEdgeBuckets[ibuck];
    for (size_t iedge = 0; iedge < edgeBucket.size(); ++iedge)
      edgeIsOwned[gid(edgeBucket[iedge])] = true;
  }

  // Marked locally shared edges as unowned
  stk::mesh::Selector locally_shared = !locally_owned;
  const stk::mesh::BucketVector & sharedEdgeBuckets =
    bulkData.get_buckets(stk::topology::EDGE_RANK, locally_shared);
  for (size_t ibuck = 0; ibuck < sharedEdgeBuckets.size(); ++ibuck)
  {
    stk::mesh::Bucket & edgeBucket = *sharedEdgeBuckets[ibuck];
    for (size_t iedge = 0; iedge < edgeBucket.size(); ++iedge)
      edgeIsOwned[gid(edgeBucket[iedge])] = false;
  }
}

#ifdef ALBANY_EPETRA
void Aeras::SpectralDiscretization::computeNodalEpetraMaps (bool overlapped)
{
  // Loads member data:  ownednodes, numOwnedNodes, node_map, numGlobalNodes, map
  // maps for owned nodes and unknowns

  stk::mesh::Selector map_type_selector = overlapped ?
           (metaData.locally_owned_part() | metaData.globally_shared_part()) :
           metaData.locally_owned_part();

  NodalDOFsStructContainer::MapOfDOFsStructs::iterator it, it2;
  NodalDOFsStructContainer::MapOfDOFsStructs& mapOfDOFsStructs = nodalDOFsStructContainer.mapOfDOFsStructs;
  std::vector< stk::mesh::Entity> nodes;
  std::vector<int> indices;
  int numNodes(0);

  // compute NumGlobalNodes
  stk::mesh::get_selected_entities( map_type_selector ,
                  bulkData.buckets( stk::topology::NODE_RANK ) ,
                  nodes );

  long long int maxID(0), maxGID(0);
  for (int i=0; i < nodes.size(); i++)
    maxID = std::max(maxID, static_cast<long long int>(gid(nodes[i])));
  comm->MaxAll(&maxID, &maxGID, 1);
  numGlobalNodes = maxGID+1; // maxGID is the same for overlapped and unique maps

  // build maps
  for(it = mapOfDOFsStructs.begin(); it != mapOfDOFsStructs.end(); ++it )
  {
    stk::mesh::Selector selector(map_type_selector);
    const std::string& part = it->first.first;
    int nComp = it->first.second;
    if(part.size())
    {
      std::map<std::string, stk::mesh::Part*>::const_iterator it3 = stkMeshStruct->nsPartVec.find(part);
      if(it3 != stkMeshStruct->nsPartVec.end())
        selector &= *(it3->second);
      else
      { // throw error
        std::ostringstream msg;
        msg << "Aeras::SpectralDiscretization::computeNodalEpetraMaps(overlapped==" << overlapped <<
            "):\n    Part " << part << " is not in  stkMeshStruct->nsPartVec.\n";
        throw std::runtime_error(msg.str());
      }
    }

    stk::mesh::get_selected_entities( selector ,
                bulkData.buckets( stk::topology::NODE_RANK ) ,
                nodes );

    numNodes = nodes.size();
    std::vector<int> indices(numNodes*nComp);
    Albany::NodalDOFManager* dofManager = (overlapped) ? &it->second.overlap_dofManager : &it->second.dofManager;
    dofManager->setup(&bulkData, nComp, numNodes, numGlobalNodes, interleavedOrdering);

    for (int i=0; i < numNodes; i++)
      for (int j=0; j < nComp; j++)
        indices[dofManager->getLocalDOF(i,j)] = dofManager->getGlobalDOF(nodes[i],j);

    Teuchos::RCP<Epetra_Map>& map = (overlapped) ? it->second.overlap_map : it->second.map;
    map = Teuchos::null;
    map = Teuchos::rcp(new Epetra_Map(-1, indices.size(), indices.data(), 0, *comm));

    Teuchos::RCP<Epetra_Map>& node_map = (overlapped) ? it->second.overlap_node_map : it->second.node_map;
    node_map = Teuchos::null;

    it2=it;
    if((nComp==1) || ((it2=mapOfDOFsStructs.find(make_pair(part,1)))!=mapOfDOFsStructs.end()))
    {
      node_map = (overlapped) ? it2->second.overlap_map : it2->second.map;
    }
  }
}
#endif // ALBANY_EPETRA

void Aeras::SpectralDiscretization::computeOwnedNodesAndUnknowns()
{
  *out << "In Aeras::SpectralDiscretization::computeOwnedNodesAndUnknowns()" << std::endl; 
  // Initialization
  int np = points_per_edge;

  // Compute the STK Mesh selector
  stk::mesh::Selector select_owned_in_part =
    stk::mesh::Selector(metaData.universal_part()) &
    stk::mesh::Selector(metaData.locally_owned_part());

  // The owned nodes will be the owned corner nodes from the original
  // linear STK mesh, the non-endpoint nodes from the owned edges, plus
  // all of the enriched interior nodes.  Start with the corner nodes.
  stk::mesh::get_selected_entities(select_owned_in_part,
				   bulkData.buckets(stk::topology::NODE_RANK),
				   ownednodes);
  numOwnedNodes = ownednodes.size();

  // Now add the number of nodes from the owned edges
  const stk::mesh::BucketVector & ownedEdgeBuckets =
    bulkData.get_buckets(stk::topology::EDGE_RANK, select_owned_in_part);
  size_t numNewEdgeNodes = 0;
  for (size_t ibuck = 0; ibuck < ownedEdgeBuckets.size(); ++ibuck)
  {
    stk::mesh::Bucket & edgeBucket = *ownedEdgeBuckets[ibuck];
    numNewEdgeNodes += edgeBucket.size() * (np-2);
  }
  numOwnedNodes += numNewEdgeNodes;

  // Now add the number of nodes from the enriched element interiors
  const stk::mesh::BucketVector & elementBuckets =
    bulkData.get_buckets(stk::topology::ELEMENT_RANK, select_owned_in_part);
  size_t numNewElementNodes = 0;
  for (size_t ibuck = 0; ibuck < elementBuckets.size(); ++ibuck)
  {
    stk::mesh::Bucket & elementBucket = *elementBuckets[ibuck];
    numNewElementNodes += elementBucket.size() * (np-2) * (np-2);
  }
  numOwnedNodes += numNewElementNodes;

#ifdef ALBANY_EPETRA
  // FIXME: WFS: not updated yet for enriched elements
  node_map = nodalDOFsStructContainer.getDOFsStruct("mesh_nodes").map;
  map = nodalDOFsStructContainer.getDOFsStruct("ordinary_solution").map;

  node_mapT = Petra::EpetraMap_To_TpetraMap(node_map, commT);
  mapT = Petra::EpetraMap_To_TpetraMap(map, commT);

  if (Teuchos::nonnull(stkMeshStruct->nodal_data_base))
    stkMeshStruct->nodal_data_base->resizeLocalMap(
      node_mapT->getNodeElementList(), commT);
#else // ALBANY_EPETRA

  //////////////////////////////////////////////////////////////////////
  // N.B.: Filling the indicesT array is inherently serial
  Teuchos::Array<GO> indicesT(numOwnedNodes);
  size_t inode = 0;

  // Add the ownednodes to indicesT
  for (size_t i = 0; i < ownednodes.size(); ++i)
    indicesT[inode++] = gid(ownednodes[i]);

  // Get a bucket of all the edges so that the local indexes match the
  // enrichedEdges indexes.  Loop over these edges to add their nodes
  // to indicesT, when the edges are owned
  const stk::mesh::BucketVector edgeBuckets =
    bulkData.buckets(stk::topology::EDGE_RANK);
  for (size_t ibuck = 0; ibuck < edgeBuckets.size(); ++ibuck)
  {
    stk::mesh::Bucket & edgeBucket = *edgeBuckets[ibuck];
    for (size_t iedge = 0; iedge < edgeBucket.size(); ++iedge)
    {
      stk::mesh::Entity edge = edgeBucket[iedge];
      if (edgeIsOwned[gid(edge)])
      {
        // Note that local edge nodes 0 and 3 have already been
        // handled correctly by the previous loop over ownednodes
        indicesT[inode++] = enrichedEdges[ibuck][iedge][1];
        indicesT[inode++] = enrichedEdges[ibuck][iedge][2];
      }
    }
  }

  // Add all of the interior nodes of the enriched elements to indicesT
  for (size_t ibuck = 0; ibuck < wsElNodeID.size(); ++ibuck)
    for (size_t ielem = 0; ielem < wsElNodeID[ibuck].size(); ++ielem)
      for (size_t ii = 1; ii < np-1; ++ii)
        for (size_t jj = 1; jj < np-1; ++jj)
          indicesT[inode++] = wsElNodeID[ibuck][ielem][ii*np+jj];

  assert (inode == numOwnedNodes);
  // End fill indicesT
  //////////////////////////////////////////////////////////////////////

  node_mapT = Teuchos::null; // delete existing map happens here on remesh
  node_mapT = Tpetra::createNonContigMap<LO, GO>(indicesT(), commT);

  numGlobalNodes = node_mapT->getMaxAllGlobalIndex() + 1;

  Teuchos::Array<GO> dofIndicesT(numOwnedNodes * neq);
  for (size_t i = 0; i < numOwnedNodes; ++i)
    for (size_t j = 0; j < neq; ++j)
      dofIndicesT[getOwnedDOF(i,j)] = getGlobalDOF(indicesT[i],j);

  mapT = Teuchos::null; // delete existing map happens here on remesh
  mapT = Tpetra::createNonContigMap<LO, GO>(dofIndicesT(), commT);

  TEUCHOS_TEST_FOR_EXCEPTION(
    Teuchos::nonnull(stkMeshStruct->nodal_data_base),
    std::logic_error,
    "Nodal database not implemented for Aeras::SpectralDiscretization");
#endif
}

void Aeras::SpectralDiscretization::computeOverlapNodesAndUnknowns()
{
  // Initialization
  int np = points_per_edge;

  // Compute the STK Mesh selector
  stk::mesh::Selector select_overlap_in_part =
    stk::mesh::Selector( metaData.universal_part() ) &
    ( stk::mesh::Selector( metaData.locally_owned_part() )
      | stk::mesh::Selector( metaData.globally_shared_part() ) );

  // Count the number of overlap nodes from the original linear STK mesh
  stk::mesh::get_selected_entities(select_overlap_in_part,
				   bulkData.buckets(stk::topology::NODE_RANK),
				   overlapnodes);
  numOverlapNodes = overlapnodes.size();

  // Now add the number of nodes from the edges
  const stk::mesh::BucketVector & overlapEdgeBuckets =
    bulkData.get_buckets(stk::topology::EDGE_RANK, select_overlap_in_part);
  size_t numNewEdgeNodes = 0;
  for (size_t ibuck = 0; ibuck < overlapEdgeBuckets.size(); ++ibuck)
  {
    stk::mesh::Bucket & edgeBucket = *overlapEdgeBuckets[ibuck];
    numNewEdgeNodes += edgeBucket.size() * (np-2);
  }
  numOverlapNodes += numNewEdgeNodes;

  // Now add the number of nodes from the enriched element interiors
  const stk::mesh::BucketVector & elementBuckets =
    bulkData.get_buckets(stk::topology::ELEMENT_RANK, select_overlap_in_part);
  size_t numNewElementNodes = 0;
  for (size_t ibuck = 0; ibuck < elementBuckets.size(); ++ibuck)
  {
    stk::mesh::Bucket & elementBucket = *elementBuckets[ibuck];
    numNewElementNodes += elementBucket.size() * (np-2) * (np-2);
  }
  numOverlapNodes += numNewElementNodes;

#ifdef ALBANY_EPETRA
  // FIXME: WFS: not updated yet for enriched elements
  numOverlapNodes = overlapnodes.size();

  overlap_map = nodalDOFsStructContainer.getDOFsStruct("ordinary_solution").overlap_map;
  overlap_node_map = nodalDOFsStructContainer.getDOFsStruct("mesh_nodes").overlap_map;

  overlap_node_mapT = Petra::EpetraMap_To_TpetraMap(overlap_node_map, commT);
  overlap_mapT = Petra::EpetraMap_To_TpetraMap(overlap_map, commT);  

  if(Teuchos::nonnull(stkMeshStruct->nodal_data_base))
    stkMeshStruct->nodal_data_base->resizeOverlapMap(
      overlap_node_mapT->getNodeElementList(), commT);
#else // ALBANY_EPETRA

  //////////////////////////////////////////////////////////////////////
  // N.B.: Filling the overlapIndicesT array is inherently serial

  // Copy owned indices to overlap indices
  Teuchos::ArrayView<const GO> ownedIndicesT = node_mapT->getNodeElementList();
  Teuchos::Array<GO> overlapIndicesT(numOverlapNodes);
  for (size_t i = 0; i < ownedIndicesT.size(); ++i)
    overlapIndicesT[i] = ownedIndicesT[i];

  // Copy shared nodes from original STK mesh to overlap indices
  size_t inode = ownedIndicesT.size();
  std::vector< stk::mesh::Entity > shared_nodes;
  stk::mesh::get_selected_entities(metaData.globally_shared_part(),
				   bulkData.buckets(stk::topology::NODE_RANK),
				   shared_nodes);
  for (size_t i = 0; i < shared_nodes.size(); ++i)
    overlapIndicesT[inode++] = gid(shared_nodes[i]);

  // Copy non-vertex nodes from shared edges to overlap indices
  std::vector< stk::mesh::Entity > shared_edges;
  stk::mesh::get_selected_entities(metaData.globally_shared_part(),
				   bulkData.buckets(stk::topology::EDGE_RANK),
				   shared_edges);
  for (size_t i = 0; i < shared_edges.size(); ++i)
  {
    const stk::mesh::Entity * edgeNodes = bulkData.begin_nodes(shared_edges[i]);
    // Note that local edge nodes 0 and 3 have already been
    // handled correctly by the previous loop over shared_nodes
    overlapIndicesT[inode++] = gid(edgeNodes[1]);
    overlapIndicesT[inode++] = gid(edgeNodes[2]);
  }

  assert (inode == numOverlapNodes);
  // End fill overlapIndicesT
  //////////////////////////////////////////////////////////////////////

  overlap_node_mapT = Teuchos::null; // delete existing map happens here on remesh
  overlap_node_mapT = Tpetra::createNonContigMap<LO, GO>(overlapIndicesT(), commT);

  // Compute the overlap DOF indices.  Since these might be strided by
  // the number of overlap nodes, we compute them from scratch.
  Teuchos::Array<GO> overlapDofIndicesT(numOverlapNodes * neq);
  for (size_t i = 0; i < numOverlapNodes; ++i)
    for (size_t j = 0; j < neq; ++j)
      overlapDofIndicesT[getOverlapDOF(i,j)] = getGlobalDOF(overlapIndicesT[i],j);

  overlap_mapT = Teuchos::null; // delete existing map happens here on remesh
  overlap_mapT = Tpetra::createNonContigMap<LO, GO>(overlapDofIndicesT(), commT);

#endif // ALBANY_EPETRA
  coordinates.resize(3*numOverlapNodes);
}

void Aeras::SpectralDiscretization::computeCoordinates()
{
  // Initialization
  typedef Intrepid::FieldContainer< double > Field_t;
  typedef Albany::AbstractSTKFieldContainer::VectorFieldType VectorFieldType;
  int np  = points_per_edge;
  int np2 = np * np;
  int deg = np - 1;

  // Compute the 1D Gauss-Lobatto quadrature
  Teuchos::RCP< Intrepid::Cubature< double, Field_t, Field_t > > gl1D =
    Teuchos::rcp(
      new Intrepid::CubaturePolylib< double, Field_t, Field_t >(
        2*deg-1, Intrepid::PL_GAUSS_LOBATTO));

  // Compute the 2D Gauss-Lobatto cubature.  These will be the nodal
  // points of the reference spectral element
  std::vector<
    Teuchos::RCP< Intrepid::Cubature< double, Field_t, Field_t > > > axes;
  axes.push_back(gl1D);
  axes.push_back(gl1D);
  Intrepid::CubatureTensor< double, Field_t, Field_t > gl2D(axes);
  Field_t refCoords(np2, 2);
  Field_t refWeights(np2);
  gl2D.getCubature(refCoords, refWeights);

  // Get the appropriate STK element buckets to extract the element
  // corner nodes
  stk::mesh::Selector select_owned_in_part =
    stk::mesh::Selector(metaData.universal_part())    &
    stk::mesh::Selector(metaData.locally_owned_part());
  stk::mesh::BucketVector const& buckets =
    bulkData.get_buckets(stk::topology::ELEMENT_RANK, select_owned_in_part);

  // Allocate and populate the coordinates
  VectorFieldType * coordinates_field = stkMeshStruct->getCoordinatesField();
  double c[4];
  size_t numWorksets = wsElNodeID.size();
  coords.resize(numWorksets);
  for (size_t iws = 0; iws < numWorksets; ++iws)
  {
    stk::mesh::Bucket & bucket = *buckets[iws];
    size_t numElements = wsElNodeID[iws].size();
    coords[iws].resize(numElements);
    for (size_t ielem = 0; ielem < numElements; ++ielem)
    {
      stk::mesh::Entity element = bucket[ielem];
      const stk::mesh::Entity * stkNodes = bulkData.begin_nodes(element);
      coords[iws][ielem].resize(np2);
      for (size_t inode = 0; inode < np2; ++inode)
      {
        double * coordVals = new double[3];
        coords[iws][ielem][inode] = coordVals;
        toDelete.push_back(coordVals);
      }

      // Phase I: project the reference element coordinates onto the
      // "twisted plane" defined by the four corners of the linear STK
      // shell element, using bilinear interpolation
      for (size_t idim = 0; idim < 3; ++idim)
      {
        // Get the coordinates value along this axis of the corner
        // nodes from the STK mesh
        for (size_t ii = 0; ii < 4; ++ii)
        {
          const GO nodeGid = gid(stkNodes[ii]);
          const LO nodeLid = overlap_node_mapT->getLocalElement(nodeGid);
          c[ii] = stk::mesh::field_data(*coordinates_field, nodeLid)[idim];
        }
        for (size_t inode = 0; inode < np2; ++inode)
        {
          double x = refCoords(inode,0);
          double y = refCoords(inode,1);
          coords[iws][ielem][inode][idim] = (c[0] * (x+1.0) * (y+1.0) -
                                             c[1] * (x-1.0) * (y+1.0) +
                                             c[2] * (x-1.0) * (y-1.0) -
                                             c[3] * (x+1.0) * (y-1.0)) * 0.25;
        }
      }

      // Phase II: project the coordinate values computed in Phase I
      // from the "twisted plane" onto the unit sphere
      for (size_t inode = 0; inode < np2; ++inode)
      {
        double distance = 0.0;
        for (size_t idim = 0; idim < 3; ++idim)
          distance += coords[iws][ielem][inode][idim] *
                      coords[iws][ielem][inode][idim];
        distance = sqrt(distance);
        for (size_t idim = 0; idim < 3; ++idim)
          coords[iws][ielem][inode][idim] /= distance;
      }
    }
  }
}

void Aeras::SpectralDiscretization::computeGraphs()
{
  std::map<int, stk::mesh::Part*>::iterator pv = stkMeshStruct->partVec.begin();
  int nodes_per_element =  metaData.get_cell_topology(*(pv->second)).getNodeCount();

  // int nodes_per_element_est =  metaData.get_cell_topology(*(stkMeshStruct->partVec[0])).getNodeCount();
  // Loads member data:  overlap_graph, numOverlapodes, overlap_node_map, coordinates, graphs

  overlap_graphT = Teuchos::null; // delete existing graph happens here on remesh

  overlap_graphT = Teuchos::rcp(new Tpetra_CrsGraph(overlap_mapT, neq*nodes_per_element));

  stk::mesh::Selector select_owned_in_part =
    stk::mesh::Selector( metaData.universal_part() ) &
    stk::mesh::Selector( metaData.locally_owned_part() );

  stk::mesh::get_selected_entities( select_owned_in_part ,
				    bulkData.buckets( stk::topology::ELEMENT_RANK ) ,
				    cells );

  if (commT->getRank()==0)
    *out << "STKDisc: " << cells.size() << " elements on Proc 0 " << std::endl;

  GO row, col;
  Teuchos::ArrayView<GO> colAV;

  for (std::size_t i=0; i < cells.size(); i++)
  {
    stk::mesh::Entity e = cells[i];
    stk::mesh::Entity const* node_rels = bulkData.begin_nodes(e);
    const size_t num_nodes = bulkData.num_nodes(e);

    // loop over local nodes
    for (std::size_t j=0; j < num_nodes; j++)
    {
      stk::mesh::Entity rowNode = node_rels[j];

      // loop over eqs
      for (std::size_t k=0; k < neq; k++)
      {
        row = getGlobalDOF(gid(rowNode), k);
        for (std::size_t l=0; l < num_nodes; l++)
        {
          stk::mesh::Entity colNode = node_rels[l];
          for (std::size_t m=0; m < neq; m++)
          {
            col = getGlobalDOF(gid(colNode), m);
            colAV = Teuchos::arrayView(&col, 1);
            overlap_graphT->insertGlobalIndices(row, colAV);
          }
        }
      }
    }
  }
  overlap_graphT->fillComplete();

  // Create Owned graph by exporting overlap with known row map
  graphT = Teuchos::null; // delete existing graph happens here on remesh

  graphT = Teuchos::rcp(new Tpetra_CrsGraph(mapT, nonzeroesPerRow(neq)));

  // Create non-overlapped matrix using two maps and export object
  Teuchos::RCP<Tpetra_Export> exporterT = Teuchos::rcp(new Tpetra_Export(overlap_mapT, mapT));
  graphT->doExport(*overlap_graphT, *exporterT, Tpetra::INSERT);
  graphT->fillComplete();
}

void Aeras::SpectralDiscretization::computeWorksetInfo()
{
  stk::mesh::Selector select_owned_in_part =
    stk::mesh::Selector( metaData.universal_part() ) &
    stk::mesh::Selector( metaData.locally_owned_part() );

  stk::mesh::BucketVector const& buckets = bulkData.get_buckets( stk::topology::ELEMENT_RANK, select_owned_in_part );

  const int numBuckets =  buckets.size();

  typedef Albany::AbstractSTKFieldContainer::ScalarFieldType ScalarFieldType;
  typedef Albany::AbstractSTKFieldContainer::VectorFieldType VectorFieldType;
  typedef Albany::AbstractSTKFieldContainer::TensorFieldType TensorFieldType;

  VectorFieldType* coordinates_field = stkMeshStruct->getCoordinatesField();
  //IK, 1/22/15: changing type of sphereVolume_field to propagate David Littlewood's change 
  //yesterday, so code will compile.  Need to look into whether sphereVolume_field is needed for Aeras.
  //ScalarFieldType* sphereVolume_field;
  stk::mesh::Field<double,stk::mesh::Cartesian3d>* sphereVolume_field; 

  if(stkMeshStruct->getFieldContainer()->hasSphereVolumeField())
    sphereVolume_field = stkMeshStruct->getFieldContainer()->getSphereVolumeField();

  wsEBNames.resize(numBuckets);
  for (int i=0; i<numBuckets; i++)
  {
    stk::mesh::PartVector const& bpv = buckets[i]->supersets();

    for (std::size_t j=0; j<bpv.size(); j++)
    {
      if (bpv[j]->primary_entity_rank() == stk::topology::ELEMENT_RANK &&
          !stk::mesh::is_auto_declared_part(*bpv[j]))
      {
        // *out << "Bucket " << i << " is in Element Block:  " << bpv[j]->name()
        //      << "  and has " << buckets[i]->size() << " elements." << std::endl;
        wsEBNames[i]=bpv[j]->name();
      }
    }
  }

  wsPhysIndex.resize(numBuckets);
  if (stkMeshStruct->allElementBlocksHaveSamePhysics)
    for (int i=0; i<numBuckets; i++) wsPhysIndex[i]=0;
  else
    for (int i=0; i<numBuckets; i++) wsPhysIndex[i]=stkMeshStruct->ebNameToIndex[wsEBNames[i]];

  // Fill  wsElNodeEqID(workset, el_LID, local node, Eq) => unk_LID

  wsElNodeEqID.resize(numBuckets);
  //wsElNodeID.resize(numBuckets);
  //coords.resize(numBuckets);
  sphereVolume.resize(numBuckets);

  nodesOnElemStateVec.resize(numBuckets);
  stateArrays.elemStateArrays.resize(numBuckets);
  const Albany::StateInfoStruct& nodal_states = stkMeshStruct->getFieldContainer()->getNodalSIS();

  // Clear map if remeshing
  if(!elemGIDws.empty()) elemGIDws.clear();

  typedef stk::mesh::Cartesian NodeTag;
  typedef stk::mesh::Cartesian ElemTag;
  typedef stk::mesh::Cartesian CompTag;

#ifdef ALBANY_EPETRA
  // FIXME: WFS: not yet updated for enriched elements
  NodalDOFsStructContainer::MapOfDOFsStructs::iterator it;
  NodalDOFsStructContainer::MapOfDOFsStructs& mapOfDOFsStructs = nodalDOFsStructContainer.mapOfDOFsStructs;
  for(it = mapOfDOFsStructs.begin(); it != mapOfDOFsStructs.end(); ++it)
  {
    it->second.wsElNodeEqID.resize(numBuckets);
    it->second.wsElNodeEqID_rawVec.resize(numBuckets);
    it->second.wsElNodeID.resize(numBuckets);
    it->second.wsElNodeID_rawVec.resize(numBuckets);
  }
#endif // ALBANY_EPETRA
  
  for (int b=0; b < numBuckets; b++)
  {

    stk::mesh::Bucket& buck = *buckets[b];
    wsElNodeEqID[b].resize(buck.size());
    //wsElNodeID[b].resize(buck.size());
    //coords[b].resize(buck.size());


    {  // nodalDataToElemNode.

      nodesOnElemStateVec[b].resize(nodal_states.size());

      for (int is=0; is< nodal_states.size(); ++is)
      {
        const std::string& name = nodal_states[is]->name;
        const Albany::StateStruct::FieldDims& dim = nodal_states[is]->dim;
        Albany::MDArray& array = stateArrays.elemStateArrays[b][name];
        std::vector<double>& stateVec = nodesOnElemStateVec[b][is];
        int dim0 = buck.size(); // may be different from dim[0];
        switch (dim.size())
        {
        case 2:     // scalar
        {
          const ScalarFieldType& field = *metaData.get_field<ScalarFieldType>(stk::topology::NODE_RANK, name);
          stateVec.resize(dim0*dim[1]);
          array.assign<ElemTag, NodeTag>(stateVec.data(),dim0,dim[1]);
          for (int i=0; i < dim0; i++)
          {
            stk::mesh::Entity element = buck[i];
            stk::mesh::Entity const* rel = bulkData.begin_nodes(element);
            for (int j=0; j < dim[1]; j++)
            {
              stk::mesh::Entity rowNode = rel[j];
              array(i,j) = *stk::mesh::field_data(field, rowNode);
            }
          }
          break;
        }
        case 3:  // vector
        {
          const VectorFieldType& field = *metaData.get_field<VectorFieldType>(stk::topology::NODE_RANK, name);
          stateVec.resize(dim0*dim[1]*dim[2]);
          array.assign<ElemTag, NodeTag,CompTag>(stateVec.data(),dim0,dim[1],dim[2]);
          for (int i=0; i < dim0; i++)
          {
            stk::mesh::Entity element = buck[i];
            stk::mesh::Entity const* rel = bulkData.begin_nodes(element);
            for (int j=0; j < dim[1]; j++)
            {
              stk::mesh::Entity rowNode = rel[j];
              double* entry = stk::mesh::field_data(field, rowNode);
              for(int k=0; k<dim[2]; k++)
                array(i,j,k) = entry[k];
            }
          }
          break;
        }
        case 4: // tensor
        {
          const TensorFieldType& field = *metaData.get_field<TensorFieldType>(stk::topology::NODE_RANK, name);
          stateVec.resize(dim0*dim[1]*dim[2]*dim[3]);
          array.assign<ElemTag, NodeTag, CompTag, CompTag>(stateVec.data(),dim0,dim[1],dim[2],dim[3]);
          for (int i=0; i < dim0; i++)
          {
            stk::mesh::Entity element = buck[i];
            stk::mesh::Entity const* rel = bulkData.begin_nodes(element);
            for (int j=0; j < dim[1]; j++)
            {
              stk::mesh::Entity rowNode = rel[j];
              double* entry = stk::mesh::field_data(field, rowNode);
              for(int k=0; k<dim[2]; k++)
                for(int l=0; l<dim[3]; l++)
                  array(i,j,k,l) = entry[k*dim[3]+l]; // check this, is stride Correct?
            }
          }
          break;
        }
        }
      }
    }

#ifdef ALBANY_EPETRA
    // FIXME: WFS: not yet updated for enriched elements
    stk::mesh::Entity element = buck[0];
    int nodes_per_element = bulkData.num_nodes(element);
    for(it = mapOfDOFsStructs.begin(); it != mapOfDOFsStructs.end(); ++it)
    {
      int nComp = it->first.second;
      it->second.wsElNodeEqID_rawVec[b].resize(buck.size()*nodes_per_element*nComp);
      it->second.wsElNodeEqID[b].assign<ElemTag, NodeTag, CompTag>(
        it->second.wsElNodeEqID_rawVec[b].data(),(int)buck.size(),nodes_per_element,nComp);
      it->second.wsElNodeID_rawVec[b].resize(buck.size()*nodes_per_element);
      it->second.wsElNodeID[b].assign<ElemTag, NodeTag>(
        it->second.wsElNodeID_rawVec[b].data(),(int)buck.size(),nodes_per_element);
    }
#endif // ALBANY_EPETRA

    // i is the element index within bucket b
    for (std::size_t i=0; i < buck.size(); i++)
    {

      // Traverse all the elements in this bucket
      stk::mesh::Entity element = buck[i];

      // Now, save a map from element GID to workset on this PE
      elemGIDws[gid(element)].ws = b;

      // Now, save a map from element GID to local id on this workset on this PE
      elemGIDws[gid(element)].LID = i;

      stk::mesh::Entity const* node_rels = bulkData.begin_nodes(element);
      const int nodes_per_element = bulkData.num_nodes(element);

      wsElNodeEqID[b][i].resize(nodes_per_element);
      //wsElNodeID[b][i].resize(nodes_per_element);
      //coords[b][i].resize(nodes_per_element);
 
#ifdef ALBANY_EPETRA
      for(it = mapOfDOFsStructs.begin(); it != mapOfDOFsStructs.end(); ++it)
      {
        Albany::IDArray& wsElNodeEqID_array = it->second.wsElNodeEqID[b];
        GIDArray& wsElNodeID_array = it->second.wsElNodeID[b];
        int nComp = it->first.second;
        for (int j=0; j < nodes_per_element; j++)
        {
          stk::mesh::Entity node = node_rels[j];
          wsElNodeID_array((int)i,j) = gid(node);
          for (int k=0; k < nComp; k++)
          {
            const GO node_gid = it->second.overlap_dofManager.getGlobalDOF(node,k);
            const int node_lid = it->second.overlap_map->LID(
#ifdef ALBANY_64BIT_INT
              static_cast<long long int>(node_gid)
#else
              node_gid
#endif
              );
            wsElNodeEqID_array((int)i,j,k) = node_lid;
          }
        }
      }
#endif

      // loop over local nodes
#ifdef ALBANY_EPETRA
      // FIXME: WFS: not yet updated for enriched elements
      DOFsStruct& dofs_struct = mapOfDOFsStructs[make_pair(std::string(""),neq)];
      GIDArray& node_array = dofs_struct.wsElNodeID[b];
      Albany::IDArray& node_eq_array = dofs_struct.wsElNodeEqID[b];
      for (int j=0; j < nodes_per_element; j++)
      {
        const stk::mesh::Entity rowNode = node_rels[j];
        const GO node_gid = gid(rowNode);
        const LO node_lid = overlap_node_mapT->getLocalElement(node_gid);

        TEUCHOS_TEST_FOR_EXCEPTION(node_lid<0, std::logic_error,
			   "STK1D_Disc: node_lid out of range " << node_lid << std::endl);
        //coords[b][i][j] = stk::mesh::field_data(*coordinates_field, rowNode);

        wsElNodeID[b][i][j] = node_array((int)i,j);

        wsElNodeEqID[b][i][j].resize(neq);
        for (int eq=0; eq < neq; eq++)
          wsElNodeEqID[b][i][j][eq] = node_eq_array((int)i,j,eq);
      }
#else
      for (int j=0; j < nodes_per_element; j++)
      {
        const stk::mesh::Entity rowNode = node_rels[j];
        const GO node_gid = gid(rowNode);
        const LO node_lid = overlap_node_mapT->getLocalElement(node_gid);

        TEUCHOS_TEST_FOR_EXCEPTION(node_lid<0, std::logic_error,
			   "STK1D_Disc: node_lid out of range " << node_lid << std::endl);
        //coords[b][i][j] = stk::mesh::field_data(*coordinates_field, rowNode);

        //wsElNodeID[b][i][j] = node_gid;

        wsElNodeEqID[b][i][j].resize(neq);
        for (std::size_t eq=0; eq < neq; eq++)
          wsElNodeEqID[b][i][j][eq] = getOverlapDOF(node_lid,eq);
      }
#endif
    }
  }

  for (int d=0; d<stkMeshStruct->numDim; d++)
  {
  if (stkMeshStruct->PBCStruct.periodic[d])
  {
    for (int b=0; b < numBuckets; b++)
    {
      for (std::size_t i=0; i < buckets[b]->size(); i++)
      {
        int nodes_per_element = buckets[b]->num_nodes(i);
        bool anyXeqZero=false;
        for (int j=0; j < nodes_per_element; j++)
          if (coords[b][i][j][d]==0.0)
            anyXeqZero=true;
        if (anyXeqZero)
        {
          bool flipZeroToScale=false;
          for (int j=0; j < nodes_per_element; j++)
            if (coords[b][i][j][d] > stkMeshStruct->PBCStruct.scale[d]/1.9)
              flipZeroToScale=true;
          if (flipZeroToScale)
          {
            for (int j=0; j < nodes_per_element; j++)
            {
              if (coords[b][i][j][d] == 0.0)
              {
                double* xleak = new double [stkMeshStruct->numDim];
                for (int k=0; k < stkMeshStruct->numDim; k++)
                  if (k==d)
                    xleak[d]=stkMeshStruct->PBCStruct.scale[d];
                  else
                    xleak[k] = coords[b][i][j][k];
                std::string transformType = stkMeshStruct->transformType;
                double alpha = stkMeshStruct->felixAlpha;
                // convert alpha, read in from ParameterList, to radians
                alpha *= pi/180.;
                if ((transformType == "ISMIP-HOM Test A" ||
                     transformType == "ISMIP-HOM Test B" ||
                     transformType == "ISMIP-HOM Test C" ||
                     transformType == "ISMIP-HOM Test D"   ) && d==0)
                {
                  xleak[2] -= stkMeshStruct->PBCStruct.scale[d]*tan(alpha);
                  Albany::StateArray::iterator sHeight =
                    stateArrays.elemStateArrays[b].find("surface_height");
                  if(sHeight != stateArrays.elemStateArrays[b].end())
                    sHeight->second(int(i),j) -=
                      stkMeshStruct->PBCStruct.scale[d]*tan(alpha);
                }
                coords[b][i][j] = xleak; // replace ptr to coords
                toDelete.push_back(xleak);
              }
            }
          }
        }
      }
    }
  }
  }

  typedef Albany::AbstractSTKFieldContainer::ScalarValueState ScalarValueState;
  typedef Albany::AbstractSTKFieldContainer::QPScalarState    QPScalarState;
  typedef Albany::AbstractSTKFieldContainer::QPVectorState    QPVectorState;
  typedef Albany::AbstractSTKFieldContainer::QPTensorState    QPTensorState;
  typedef Albany::AbstractSTKFieldContainer::QPTensor3State   QPTensor3State;
  typedef Albany::AbstractSTKFieldContainer::ScalarState      ScalarState;
  typedef Albany::AbstractSTKFieldContainer::VectorState      VectorState;
  typedef Albany::AbstractSTKFieldContainer::TensorState      TensorState;

  // Pull out pointers to shards::Arrays for every bucket, for every state
  // Code is data-type dependent

  ScalarValueState scalarValue_states = stkMeshStruct->getFieldContainer()->getScalarValueStates();
  QPScalarState qpscalar_states = stkMeshStruct->getFieldContainer()->getQPScalarStates();
  QPVectorState qpvector_states = stkMeshStruct->getFieldContainer()->getQPVectorStates();
  QPTensorState qptensor_states = stkMeshStruct->getFieldContainer()->getQPTensorStates();
  QPTensor3State qptensor3_states = stkMeshStruct->getFieldContainer()->getQPTensor3States();
  std::map<std::string, double>& time = stkMeshStruct->getFieldContainer()->getTime();

  for (std::size_t b=0; b < buckets.size(); b++)
  {
    stk::mesh::Bucket& buck = *buckets[b];
    for (QPScalarState::iterator qpss = qpscalar_states.begin();
              qpss != qpscalar_states.end(); ++qpss)
    {
      Albany::BucketArray<Albany::AbstractSTKFieldContainer::QPScalarFieldType> array(**qpss, buck);
      // Debug
      // std::cout << "Buck.size(): " << buck.size() << " QPSFT dim[1]: " << array.dimension(1) << std::endl;
      Albany::MDArray ar = array;
      stateArrays.elemStateArrays[b][(*qpss)->name()] = ar;
    }
    for (QPVectorState::iterator qpvs = qpvector_states.begin();
              qpvs != qpvector_states.end(); ++qpvs)
    {
      Albany::BucketArray<Albany::AbstractSTKFieldContainer::QPVectorFieldType> array(**qpvs, buck);
      // Debug
      // std::cout << "Buck.size(): " << buck.size() << " QPVFT dim[2]: " << array.dimension(2) << std::endl;
      Albany::MDArray ar = array;
      stateArrays.elemStateArrays[b][(*qpvs)->name()] = ar;
    }
    for (QPTensorState::iterator qpts = qptensor_states.begin();
              qpts != qptensor_states.end(); ++qpts)
    {
      Albany::BucketArray<Albany::AbstractSTKFieldContainer::QPTensorFieldType> array(**qpts, buck);
      // Debug
      // std::cout << "Buck.size(): " << buck.size() << " QPTFT dim[3]: " << array.dimension(3) << std::endl;
      Albany::MDArray ar = array;
      stateArrays.elemStateArrays[b][(*qpts)->name()] = ar;
    }
    for (QPTensor3State::iterator qpts = qptensor3_states.begin();
              qpts != qptensor3_states.end(); ++qpts)
    {
      Albany::BucketArray<Albany::AbstractSTKFieldContainer::QPTensor3FieldType> array(**qpts, buck);
      // Debug
      // std::cout << "Buck.size(): " << buck.size() << " QPT3FT dim[4]: " << array.dimension(4) << std::endl;
      Albany::MDArray ar = array;
      stateArrays.elemStateArrays[b][(*qpts)->name()] = ar;
    }
    for (ScalarValueState::iterator svs = scalarValue_states.begin();
         svs != scalarValue_states.end(); ++svs)
    {
      const int size = 1;
      shards::Array<double, shards::NaturalOrder, Cell> array(&time[*svs], size);
      Albany::MDArray ar = array;
      // Debug
      // std::cout << "Buck.size(): " << buck.size() << " SVState dim[0]: " << array.dimension(0) << std::endl;
      // std::cout << "SV Name: " << *svs << " address : " << &array << std::endl;
      stateArrays.elemStateArrays[b][*svs] = ar;
    }
  }

// Process node data sets if present

  if (Teuchos::nonnull(stkMeshStruct->nodal_data_base) &&
      stkMeshStruct->nodal_data_base->isNodeDataPresent())
  {
    Teuchos::RCP<Albany::NodeFieldContainer> node_states = stkMeshStruct->nodal_data_base->getNodeContainer();

    stk::mesh::BucketVector const& node_buckets = bulkData.get_buckets( stk::topology::NODE_RANK, select_owned_in_part );

    const size_t numNodeBuckets = node_buckets.size();

    stateArrays.nodeStateArrays.resize(numNodeBuckets);
    for (std::size_t b=0; b < numNodeBuckets; b++)
    {
      stk::mesh::Bucket& buck = *node_buckets[b];
      for (Albany::NodeFieldContainer::iterator nfs = node_states->begin();
                nfs != node_states->end(); ++nfs)
      {
        stateArrays.nodeStateArrays[b][(*nfs).first] =
             Teuchos::rcp_dynamic_cast<Albany::AbstractSTKNodeFieldContainer>((*nfs).second)->getMDA(buck);
      }
    }
  }
}

void Aeras::SpectralDiscretization::computeSideSets()
{
  *out << "In Aeras::SpectralDiscretization::computeSideSets(): nothing to do!" << std::endl;
  /* 
  // Clean up existing sideset structure if remeshing
  for(int i = 0; i < sideSets.size(); i++)
    sideSets[i].clear(); // empty the ith map

  const stk::mesh::EntityRank element_rank = stk::topology::ELEMENT_RANK;

  // iterator over all side_rank parts found in the mesh
  std::map<std::string, stk::mesh::Part*>::iterator ss = stkMeshStruct->ssPartVec.begin();

  int numBuckets = wsEBNames.size();

  sideSets.resize(numBuckets); // Need a sideset list per workset

  while ( ss != stkMeshStruct->ssPartVec.end() )
  {
    // Get all owned sides in this side set
    stk::mesh::Selector select_owned_in_sspart =

      // get only entities in the ss part (ss->second is the current sideset part)
      stk::mesh::Selector( *(ss->second) ) &
      // and only if the part is local
      stk::mesh::Selector( metaData.locally_owned_part() );

    std::vector< stk::mesh::Entity > sides ;
    stk::mesh::get_selected_entities( select_owned_in_sspart , // sides local to this processor
				      bulkData.buckets( metaData.side_rank() ) ,
				      sides ); // store the result in "sides"

    *out << "STKDisc: sideset "<< ss->first <<" has size " << sides.size() << "  on Proc 0." << std::endl;

    // loop over the sides to see what they are, then fill in the data holder
    // for side set options, look at $TRILINOS_DIR/packages/stk/stk_usecases/mesh/UseCase_13.cpp

    for (std::size_t localSideID=0; localSideID < sides.size(); localSideID++)
    {

      stk::mesh::Entity sidee = sides[localSideID];

      TEUCHOS_TEST_FOR_EXCEPTION(bulkData.num_elements(sidee) != 1, std::logic_error,
                                 "STKDisc: cannot figure out side set topology for side set " << ss->first << std::endl);

      stk::mesh::Entity elem = bulkData.begin_elements(sidee)[0];

      // containing the side. Note that if the side is internal, it will show up twice in the
      // element list, once for each element that contains it.

      Albany::SideStruct sStruct;

      // Save elem id. This is the global element id
      sStruct.elem_GID = gid(elem);

      int workset = elemGIDws[sStruct.elem_GID].ws; // Get the ws that this element lives in

      // Save elem id. This is the local element id within the workset
      sStruct.elem_LID = elemGIDws[sStruct.elem_GID].LID;

      // Save the side identifier inside of the element. This starts at zero here.
      sStruct.side_local_id = determine_local_side_id(elem, sidee);

      // Save the index of the element block that this elem lives in
      sStruct.elem_ebIndex = stkMeshStruct->ebNameToIndex[wsEBNames[workset]];

      Albany::SideSetList& ssList = sideSets[workset];   // Get a ref to the side set map for this ws
      Albany::SideSetList::iterator it = ssList.find(ss->first); // Get an iterator to the correct sideset (if
                                                                // it exists)

      if(it != ssList.end()) // The sideset has already been created

        it->second.push_back(sStruct); // Save this side to the vector that belongs to the name ss->first

      else { // Add the key ss->first to the map, and the side vector to that map

        std::vector<Albany::SideStruct> tmpSSVec;
        tmpSSVec.push_back(sStruct);

        ssList.insert(Albany::SideSetList::value_type(ss->first, tmpSSVec));

      }

    }

    ss++;
  }*/
}

unsigned
Aeras::SpectralDiscretization::determine_local_side_id(const stk::mesh::Entity elem,
                                                       stk::mesh::Entity side)
{
  using namespace stk;

  stk::topology elem_top = bulkData.bucket(elem).topology();

  const unsigned num_elem_nodes = bulkData.num_nodes(elem);
  const unsigned num_side_nodes = bulkData.num_nodes(side);

  stk::mesh::Entity const* elem_nodes = bulkData.begin_nodes(elem);
  stk::mesh::Entity const* side_nodes = bulkData.begin_nodes(side);

  const stk::topology::rank_t side_rank = metaData.side_rank();

  int side_id = -1 ;

  if(num_elem_nodes == 0 || num_side_nodes == 0){ // Node relations are not present, look at elem->face

    const unsigned num_sides = bulkData.num_connectivity(elem, side_rank);
    stk::mesh::Entity const* elem_sides = bulkData.begin(elem, side_rank);

    for ( unsigned i = 0 ; i < num_sides ; ++i )
    {
      const stk::mesh::Entity elem_side = elem_sides[i];

      if (bulkData.identifier(elem_side) == bulkData.identifier(side)){ // Found the local side in the element

         side_id = static_cast<int>(i);

         return side_id;
      }

    }

    if ( side_id < 0 )
    {
      std::ostringstream msg;
      msg << "determine_local_side_id( " ;
      msg << elem_top.name() ;
      msg << " , Element[ " ;
      msg << bulkData.identifier(elem);
      msg << " ]{" ;
      for ( unsigned i = 0 ; i < num_sides ; ++i )
      {
        msg << " " << bulkData.identifier(elem_sides[i]);
      }
      msg << " } , Side[ " ;
      msg << bulkData.identifier(side);
      msg << " ] ) FAILED" ;
      throw std::runtime_error( msg.str() );
    }

  }
  else { // Conventional elem->node - side->node connectivity present

    std::vector<unsigned> side_map;
    for ( unsigned i = 0 ; side_id == -1 && i < elem_top.num_sides() ; ++i )
    {
      stk::topology side_top    = elem_top.side_topology(i);
      side_map.clear();
      elem_top.side_node_ordinals(i, std::back_inserter(side_map));

      if ( num_side_nodes == side_top.num_nodes() )
      {

        side_id = i ;

        for ( unsigned j = 0 ;
              side_id == static_cast<int>(i) && j < side_top.num_nodes() ; ++j )
        {

          stk::mesh::Entity elem_node = elem_nodes[ side_map[j] ];

          bool found = false ;

          for ( unsigned k = 0 ; ! found && k < side_top.num_nodes() ; ++k )
          {
            found = elem_node == side_nodes[k];
          }

          if ( ! found ) { side_id = -1 ; }
        }
      }
    }

    if ( side_id < 0 )
    {
      std::ostringstream msg ;
      msg << "determine_local_side_id( " ;
      msg << elem_top.name() ;
      msg << " , Element[ " ;
      msg << bulkData.identifier(elem);
      msg << " ]{" ;
      for ( unsigned i = 0 ; i < num_elem_nodes ; ++i )
      {
        msg << " " << bulkData.identifier(elem_nodes[i]);
      }
      msg << " } , Side[ " ;
      msg << bulkData.identifier(side);
      msg << " ]{" ;
      for ( unsigned i = 0 ; i < num_side_nodes ; ++i )
      {
        msg << " " << bulkData.identifier(side_nodes[i]);
      }
      msg << " } ) FAILED" ;
      throw std::runtime_error( msg.str() );
    }
  }

  return static_cast<unsigned>(side_id) ;
}

void Aeras::SpectralDiscretization::computeNodeSets()
{

  *out << "In Aeras::SpectralDiscretization::computeNodeSets(): nothing to do!" << std::endl; 
  /*
  std::map<std::string, stk::mesh::Part*>::iterator ns = stkMeshStruct->nsPartVec.begin();
  Albany::AbstractSTKFieldContainer::VectorFieldType* coordinates_field = stkMeshStruct->getCoordinatesField();

  while ( ns != stkMeshStruct->nsPartVec.end() ) { // Iterate over Node Sets
    // Get all owned nodes in this node set
    stk::mesh::Selector select_owned_in_nspart =
      stk::mesh::Selector( *(ns->second) ) &
      stk::mesh::Selector( metaData.locally_owned_part() );

    std::vector< stk::mesh::Entity > nodes ;
    stk::mesh::get_selected_entities( select_owned_in_nspart ,
				      bulkData.buckets( stk::topology::NODE_RANK ) ,
				      nodes );

    nodeSets[ns->first].resize(nodes.size());
    nodeSetCoords[ns->first].resize(nodes.size());
//    nodeSetIDs.push_back(ns->first); // Grab string ID
    *out << "STKDisc: nodeset "<< ns->first <<" has size " << nodes.size() << "  on Proc 0." << std::endl;
    for (std::size_t i=0; i < nodes.size(); i++)
    {
      GO node_gid = gid(nodes[i]);
      int node_lid = node_mapT->getLocalElement(node_gid);
      nodeSets[ns->first][i].resize(neq);
      for (std::size_t eq=0; eq < neq; eq++)  nodeSets[ns->first][i][eq] = getOwnedDOF(node_lid,eq);
      nodeSetCoords[ns->first][i] = stk::mesh::field_data(*coordinates_field, nodes[i]);
    }
    ns++;
  }*/
}

void Aeras::SpectralDiscretization::setupExodusOutput()
{
#ifdef ALBANY_SEACAS
  if (stkMeshStruct->exoOutput)
  {

    outputInterval = 0;

    std::string str = stkMeshStruct->exoOutFile;

    Ioss::Init::Initializer io;
    
    mesh_data = Teuchos::rcp(new stk::io::StkMeshIoBroker(Albany::getMpiCommFromTeuchosComm(commT)));
    mesh_data->set_bulk_data(bulkData);
    outputFileIdx = mesh_data->create_output_mesh(str, stk::io::WRITE_RESULTS);

    const stk::mesh::FieldVector &fields = mesh_data->meta_data().get_fields();
    for (size_t i=0; i < fields.size(); i++)
    {
      // Hacky, but doesn't appear to be a way to query if a field is already
      // going to be output.
      try
      {
        mesh_data->add_field(outputFileIdx, *fields[i]);
      }
      catch (std::runtime_error const&) { }
    }
  }
#else
  if (stkMeshStruct->exoOutput)
    *out << "\nWARNING: exodus output requested but SEACAS not compiled in:"
         << " disabling exodus output \n" << std::endl;

#endif
}

namespace
{
const std::vector<double> spherical_to_cart(const std::pair<double, double> & sphere)
{
  const double radius_of_earth = 1;
  std::vector<double> cart(3);
  
  cart[0] = radius_of_earth*std::cos(sphere.first)*std::cos(sphere.second);
  cart[1] = radius_of_earth*std::cos(sphere.first)*std::sin(sphere.second);
  cart[2] = radius_of_earth*std::sin(sphere.first);
  
  return cart;
}

double distance (const double* x, const double* y)
{
  const double d = std::sqrt((x[0]-y[0])*(x[0]-y[0]) +
                             (x[1]-y[1])*(x[1]-y[1]) +
                             (x[2]-y[2])*(x[2]-y[2]));
  return d;
}

double distance (const std::vector<double> &x, const std::vector<double> &y) {
  const double d = std::sqrt((x[0]-y[0])*(x[0]-y[0]) +
                             (x[1]-y[1])*(x[1]-y[1]) +
                             (x[2]-y[2])*(x[2]-y[2]));
  return d;
}

bool point_inside(const Teuchos::ArrayRCP<double*> &coords,
                  const std::vector<double>        &sphere_xyz)
{
  // first check if point is near the element:
  const double  tol_inside = 1e-12;
  const double elem_diam = std::max(::distance(coords[0],coords[2]), ::distance(coords[1],coords[3]));
  std::vector<double> center(3,0);
  for (unsigned i=0; i<4; ++i)
    for (unsigned j=0; j<3; ++j) center[j] += coords[i][j];
  for (unsigned j=0; j<3; ++j) center[j] /= 4;
  bool inside = true;

  if ( ::distance(&center[0],&sphere_xyz[0]) > 1.0*elem_diam ) inside = false;

  unsigned j=3;
    for (unsigned i=0; i<4 && inside; ++i)
    {
      std::vector<double> cross(3);
      // outward normal to plane containing j->i edge:  corner(i) x corner(j)
      // sphere dot (corner(i) x corner(j) ) = negative if inside
      cross[0]=  coords[i][1]*coords[j][2] - coords[i][2]*coords[j][1];
      cross[1]=-(coords[i][0]*coords[j][2] - coords[i][2]*coords[j][0]);
      cross[2]=  coords[i][0]*coords[j][1] - coords[i][1]*coords[j][0];
      j = i;
      const double dotprod = cross[0]*sphere_xyz[0] +
                             cross[1]*sphere_xyz[1] +
                             cross[2]*sphere_xyz[2];

      // dot product is proportional to elem_diam. positive means outside,
      // but allow machine precision tolorence:
      if (tol_inside*elem_diam < dotprod) inside = false;
    }
    return inside;
  }


  const Teuchos::RCP<Intrepid::Basis<double, Intrepid::FieldContainer<double> > >
  Basis(const int C)
  {
    // Static types
    typedef Intrepid::FieldContainer< double > Field_t;
    typedef Intrepid::Basis< double, Field_t > Basis_t;
    static const Teuchos::RCP< Basis_t > HGRAD_Basis_4 =
      Teuchos::rcp( new Intrepid::Basis_HGRAD_QUAD_C1_FEM< double, Field_t >() );
    static const Teuchos::RCP< Basis_t > HGRAD_Basis_9 =
      Teuchos::rcp( new Intrepid::Basis_HGRAD_QUAD_C2_FEM< double, Field_t >() );

    // Check for valid value of C
    int deg = (int) std::sqrt((double)C);
    TEUCHOS_TEST_FOR_EXCEPTION(
      deg*deg != C || deg < 2,
      std::logic_error,
      " Aeras::SpectralDiscretization Error Basis not perfect "
      "square > 1" << std::endl);

    // Quick return for linear or quad
    if (C == 4) return HGRAD_Basis_4;
    if (C == 9) return HGRAD_Basis_9;

    // Spectral bases
    return Teuchos::rcp(
      new Intrepid::Basis_HGRAD_QUAD_Cn_FEM< double, Field_t >(
        deg, Intrepid::POINTTYPE_SPECTRAL) );
  }

  double value(const std::vector<double> &soln,
               const std::pair<double, double> &ref)
  {

    const int C = soln.size();
    const Teuchos::RCP<Intrepid::Basis<double, Intrepid::FieldContainer<double> > > HGRAD_Basis = Basis(C);

    const int numPoints        = 1;
    Intrepid::FieldContainer<double> basisVals (C, numPoints);
    Intrepid::FieldContainer<double> tempPoints(numPoints, 2);
    tempPoints(0,0) = ref.first;
    tempPoints(0,1) = ref.second;

    HGRAD_Basis->getValues(basisVals, tempPoints, Intrepid::OPERATOR_VALUE);

    double x = 0;
    for (unsigned j=0; j<C; ++j) x += soln[j] * basisVals(j,0);
    return x;
  }

  void value(double x[3],
             const Teuchos::ArrayRCP<double*> &coords,
             const std::pair<double, double> &ref)
  {

    const int C = coords.size();
    const Teuchos::RCP<Intrepid::Basis<double, Intrepid::FieldContainer<double> > > HGRAD_Basis = Basis(C);

    const int numPoints        = 1;
    Intrepid::FieldContainer<double> basisVals (C, numPoints);
    Intrepid::FieldContainer<double> tempPoints(numPoints, 2);
    tempPoints(0,0) = ref.first;
    tempPoints(0,1) = ref.second;

    HGRAD_Basis->getValues(basisVals, tempPoints, Intrepid::OPERATOR_VALUE);

    for (unsigned i=0; i<3; ++i) x[i] = 0;
    for (unsigned i=0; i<3; ++i)
      for (unsigned j=0; j<C; ++j)
        x[i] += coords[j][i] * basisVals(j,0);
  }

  void grad(double x[3][2],
             const Teuchos::ArrayRCP<double*> &coords,
             const std::pair<double, double> &ref)
  {

    const int C = coords.size();
    const Teuchos::RCP<Intrepid::Basis<double, Intrepid::FieldContainer<double> > > HGRAD_Basis = Basis(C);

    const int numPoints        = 1;
    Intrepid::FieldContainer<double> basisGrad (C, numPoints, 2);
    Intrepid::FieldContainer<double> tempPoints(numPoints, 2);
    tempPoints(0,0) = ref.first;
    tempPoints(0,1) = ref.second;

    HGRAD_Basis->getValues(basisGrad, tempPoints, Intrepid::OPERATOR_GRAD);

    for (unsigned i = 0; i < 3; ++i)
      x[i][0] = x[i][1] = 0;
    for (unsigned i = 0; i < 3; ++i)
      for (unsigned j = 0; j < C; ++j)
      {
        x[i][0] += coords[j][i] * basisGrad(j,0,0);
        x[i][1] += coords[j][i] * basisGrad(j,0,1);
      }
  }

  std::pair<double, double>  ref2sphere(const Teuchos::ArrayRCP<double*> &coords,
                                        const std::pair<double, double> &ref)
  {

    static const double DIST_THRESHOLD= 1.0e-9;

    double x[3];
    value(x,coords,ref);

    const double r = std::sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]);

    for (unsigned i=0; i<3; ++i) x[i] /= r;

    std::pair<double, double> sphere(std::asin(x[2]), std::atan2(x[1],x[0]));

    // ==========================================================
    // enforce three facts:
    //
    // 1) lon at poles is defined to be zero
    //
    // 2) Grid points must be separated by about .01 Meter (on earth)
    //   from pole to be considered "not the pole".
    //
    // 3) range of lon is { 0<= lon < 2*PI }
    //
    // ==========================================================

    if (std::abs(std::abs(sphere.first)-pi/2) < DIST_THRESHOLD) sphere.second = 0;
    else if (sphere.second < 0) sphere.second += 2*pi;

    return sphere;
  }

  void Dmap(const Teuchos::ArrayRCP<double*> &coords,
            const std::pair<double, double>  &sphere,
            const std::pair<double, double>  &ref,
            double D[][2])
  {

    const double th     = sphere.first;
    const double lam    = sphere.second;
    const double sinlam = std::sin(lam);
    const double sinth  = std::sin(th);
    const double coslam = std::cos(lam);
    const double costh  = std::cos(th);

    const double D1[2][3] = {{-sinlam, coslam, 0},
                             {      0,      0, 1}};

    const double D2[3][3] = {{ sinlam*sinlam*costh*costh+sinth*sinth, -sinlam*coslam*costh*costh,             -coslam*sinth*costh},
                             {-sinlam*coslam*costh*costh,              coslam*coslam*costh*costh+sinth*sinth, -sinlam*sinth*costh},
                             {-coslam*sinth,                          -sinlam*sinth,                                        costh}};

    double D3[3][2] = {0};
    grad(D3,coords,ref);

    double D4[3][2] = {0};
    for (unsigned i=0; i<3; ++i)
      for (unsigned j=0; j<2; ++j)
        for (unsigned k=0; k<3; ++k)
           D4[i][j] += D2[i][k] * D3[k][j];

    for (unsigned i=0; i<2; ++i)
      for (unsigned j=0; j<2; ++j) D[i][j] = 0;

    for (unsigned i=0; i<2; ++i)
      for (unsigned j=0; j<2; ++j)
        for (unsigned k=0; k<3; ++k)
          D[i][j] += D1[i][k] * D4[k][j];

  }

  std::pair<double, double>
  parametric_coordinates(const Teuchos::ArrayRCP<double*> &coords,
                         const std::pair<double, double>  &sphere)
  {
    static const double tol_sq = 1e-26;
    static const unsigned MAX_NR_ITER = 10;
    double costh = std::cos(sphere.first);
    double D[2][2], Dinv[2][2];
    double resa = 1;
    double resb = 1;
    std::pair<double, double> ref(0,0); // initial guess is center of element.

    for (unsigned i=0; i<MAX_NR_ITER && tol_sq < (costh*resb*resb + resa*resa) ; ++i)
    {
      const std::pair<double, double> sph = ref2sphere(coords,ref);
      resa = sph.first  - sphere.first;
      resb = sph.second - sphere.second;

      if (resb >  pi) resb -= 2*pi;
      if (resb < -pi) resb += 2*pi;

      Dmap(coords, sph, ref, D);
      const double detD = D[0][0]*D[1][1] - D[0][1]*D[1][0];
      Dinv[0][0] =  D[1][1]/detD;
      Dinv[0][1] = -D[0][1]/detD;
      Dinv[1][0] = -D[1][0]/detD;
      Dinv[1][1] =  D[0][0]/detD;

      const std::pair<double, double> del( Dinv[0][0]*costh*resb + Dinv[0][1]*resa,
                                           Dinv[1][0]*costh*resb + Dinv[1][1]*resa);
      ref.first  -= del.first;
      ref.second -= del.second;
    }
    return ref;
  }

  const std::pair<bool,std::pair<unsigned, unsigned> >point_in_element(const std::pair<double, double> &sphere,
      const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type& coords,
      std::pair<double, double> &parametric)
  {
    const std::vector<double> sphere_xyz = spherical_to_cart(sphere);
    std::pair<bool,std::pair<unsigned, unsigned> > element(false,std::pair<unsigned, unsigned>(0,0));
    for (unsigned i=0; i<coords.size() && !element.first; ++i)
    {
      for (unsigned j=0; j<coords[i].size() && !element.first; ++j)
      {
        const bool found =  point_inside(coords[i][j], sphere_xyz);
        if (found)
        {
          parametric = parametric_coordinates(coords[i][j], sphere);
          if (parametric.first  < -1) parametric.first  = -1;
          if (parametric.second < -1) parametric.second = -1;
          if (1 < parametric.first  ) parametric.first  =  1;
          if (1 < parametric.second ) parametric.second =  1;
          element.first         = true;
          element.second.first  = i;
          element.second.second = j;
        }
      }
    }
    return element;
  }

  void setup_latlon_interp(
    const unsigned nlat,
    const double nlon,
    const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type& coords,
    Albany::WorksetArray<Teuchos::ArrayRCP<std::vector<Aeras::SpectralDiscretization::interp> > >::type& interpdata,
    const Teuchos::RCP<const Teuchos_Comm> commT)
  {
    double err=0;
    const long long unsigned rank = commT->getRank();
    std::vector<double> lat(nlat);
    std::vector<double> lon(nlon);

    unsigned count=0;
    for (unsigned i=0; i<nlat; ++i)
      lat[i] = -pi/2 + i*pi/(nlat-1);
    for (unsigned j=0; j<nlon; ++j)
      lon[j] =       2*j*pi/nlon;
    for (unsigned i=0; i<nlat; ++i)
    {
      for (unsigned j=0; j<nlon; ++j)
      {
        const std::pair<double, double> sphere(lat[i],lon[j]);
        std::pair<double, double> paramtric;
        const std::pair<bool,std::pair<unsigned, unsigned> >element = point_in_element(sphere, coords, paramtric);
        if (element.first)
        {
          // compute error: map 'cart' back to sphere and compare with original
          // interpolation point:
          const unsigned b = element.second.first ;
          const unsigned e = element.second.second;
          const std::vector<double> sphere2_xyz = spherical_to_cart(ref2sphere(coords[b][e], paramtric));
          const std::vector<double> sphere_xyz  = spherical_to_cart(sphere);
          err = std::max(err, ::distance(&sphere2_xyz[0],&sphere_xyz[0]));
          Aeras::SpectralDiscretization::interp interp;
          interp.parametric_coords = paramtric;
          interp.latitude_longitude = std::pair<unsigned,unsigned>(i,j);
          interpdata[b][e].push_back(interp);
          ++count;
        }
      }
      if (!rank && (!(i%64) || i==nlat-1)) std::cout<< "Finished Latitude "<<i<<" of "<<nlat<<std::endl;
    }
    if (!rank) std::cout<<"Max interpolation point search error: "<<err<<std::endl;
  }
}

int
Aeras::SpectralDiscretization::processNetCDFOutputRequestT(const Tpetra_Vector& solution_fieldT)
{
#ifdef ALBANY_SEACAS
  // IK, 10/13/14: need to implement!
#endif
  return 0;
}
#ifdef ALBANY_EPETRA
int
Aeras::SpectralDiscretization::processNetCDFOutputRequest(const Epetra_Vector& solution_field)
{
#ifdef ALBANY_SEACAS
  const long long unsigned rank = commT->getRank();
  const unsigned nlat = stkMeshStruct->nLat;
  const unsigned nlon = stkMeshStruct->nLon;

  std::vector<double> local(nlat*nlon*neq, -std::numeric_limits<double>::max());


  for (unsigned n=0; n<neq; ++n)
  {
    for (unsigned b=0; b<interpolateData.size(); ++b)
    {
      Teuchos::ArrayRCP<std::vector<interp> >        Interpb = interpolateData[b];
      Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > Coordsb = coords[b];
      Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> > > ElNodeEqID = wsElNodeEqID[b];

      for (unsigned e=0; e<Interpb.size(); ++e)
      {
        const std::vector<interp>                    &interp = Interpb[e];
        Teuchos::ArrayRCP<double*>                    coordp = Coordsb[e];
        Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >    elnode = ElNodeEqID[e];

        const int C = coordp.size();
        std::vector<double> soln(C);
        for (unsigned i=0; i<C; ++i)
        {
          const int overlap_dof = elnode[i][n];
          soln[i] = solution_field[overlap_dof];
        }
        for (unsigned p=0; p<interp.size(); ++p)
        {
          Aeras::SpectralDiscretization::interp par    = interp[p];
          const double y = value(soln, par.parametric_coords);
          std::pair<unsigned,unsigned> latlon =   par.latitude_longitude;
          local[nlon*latlon.first + latlon.second + n*nlat*nlon] = y;
        }
      }
    }
  }

  std::vector<double> global(neq*nlat*nlon);
  comm->MaxAll(&local[0], &global[0], neq*nlat*nlon);

#ifdef ALBANY_PAR_NETCDF
  const long long unsigned np   = commT->getSize();
  const size_t start            = static_cast<size_t>((rank*nlat)/np);
  const size_t end              = static_cast<size_t>(((rank+1)*nlat)/np);
  const size_t len              = end-start;

  for (unsigned n=0; n<neq; ++n)
  {
    const size_t  startp[] = {netCDFOutputRequest,    0, start, 0};
    const size_t  countp[] = {1, 1, len, nlon};
    if (const int ierr = nc_put_vara_double (netCDFp, varSolns[n], startp, countp, &global[n*nlat*nlon]))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_vara_double returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
  }
#else
  if (netCDFp)
  {
    for (unsigned n=0; n<neq; ++n)
    {
      const size_t  startp[] = {netCDFOutputRequest,    0, 0, 0};
      const size_t  countp[] = {1, 1, nlat, nlon};
      if (const int ierr = nc_put_vara_double (netCDFp, varSolns[n], startp, countp, &global[n*nlat*nlon]))
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
          "nc_put_vara returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
    }
  }
#endif
#endif
  return netCDFOutputRequest++;
}
#endif

void Aeras::SpectralDiscretization::setupNetCDFOutput()
{
  const long long unsigned rank = commT->getRank();
#ifdef ALBANY_SEACAS
  if (stkMeshStruct->cdfOutput)
  {
    outputInterval = 0;
    const unsigned nlat = stkMeshStruct->nLat;
    const unsigned nlon = stkMeshStruct->nLon;


    std::string str = stkMeshStruct->cdfOutFile;

    interpolateData.resize(coords.size());
    for (int b=0; b < coords.size(); b++) interpolateData[b].resize(coords[b].size());

    setup_latlon_interp(nlat, nlon, coords, interpolateData, commT);

    const std::string name = stkMeshStruct->cdfOutFile;
    netCDFp=0;
    netCDFOutputRequest=0;

#ifdef ALBANY_PAR_NETCDF
    MPI_Comm theMPIComm = Albany::getMpiCommFromTeuchosComm(commT);
    MPI_Info info;
    MPI_Info_create(&info);
    if (const int ierr = nc_create_par (name.c_str(), NC_NETCDF4 | NC_MPIIO | NC_CLOBBER | NC_64BIT_OFFSET, theMPIComm, info, &netCDFp))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_create_par returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
    MPI_Info_free(&info);
#else
    if (!rank)
    if (const int ierr = nc_create (name.c_str(), NC_CLOBBER | NC_SHARE | NC_64BIT_OFFSET | NC_CLASSIC_MODEL, &netCDFp))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_create returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
#endif

    const size_t nlev = 1;
    const char *dimnames[] = {"time","lev","lat","lon"};
    const size_t  dimlen[] = {NC_UNLIMITED, nlev, nlat, nlon};
    int dimID[4]={0,0,0,0};

    for (unsigned i=0; i<4; ++i)
    {
      if (netCDFp)
      if (const int ierr = nc_def_dim (netCDFp,  dimnames[i], dimlen[i], &dimID[i]))
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
          "nc_def_dim returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
    }
    varSolns.resize(neq,0);

    for (unsigned n=0; n<neq; ++n)
    {
      std::ostringstream var;
      var <<"variable_"<<n;
      const char *field_name = var.str().c_str();
      if (netCDFp)
      if (const int ierr = nc_def_var (netCDFp,  field_name, NC_DOUBLE, 4, dimID, &varSolns[n]))
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
          "nc_def_var "<<field_name<<" returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

      const double fillVal = -9999.0;
      if (netCDFp)
      if (const int ierr = nc_put_att (netCDFp,  varSolns[n], "FillValue", NC_DOUBLE, 1, &fillVal))
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
          "nc_put_att FillValue returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
    }

    const char lat_name[] = "latitude";
    const char lat_unit[] = "degrees_north";
    const char lon_name[] = "longitude";
    const char lon_unit[] = "degrees_east";
    int latVarID=0;
      if (netCDFp)
    if (const int ierr = nc_def_var (netCDFp,  "lat", NC_DOUBLE, 1, &dimID[2], &latVarID))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_def_var lat returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
      if (netCDFp)
    if (const int ierr = nc_put_att_text (netCDFp,  latVarID, "long_name", sizeof(lat_name), lat_name))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_att_text "<<lat_name<<" returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
      if (netCDFp)
    if (const int ierr = nc_put_att_text (netCDFp,  latVarID, "units", sizeof(lat_unit), lat_unit))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_att_text "<<lat_unit<<" returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

    int lonVarID=0;
      if (netCDFp)
    if (const int ierr = nc_def_var (netCDFp,  "lon", NC_DOUBLE, 1, &dimID[3], &lonVarID))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_def_var lon returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
      if (netCDFp)
    if (const int ierr = nc_put_att_text (netCDFp,  lonVarID, "long_name", sizeof(lon_name), lon_name))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_att_text "<<lon_name<<" returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
      if (netCDFp)
    if (const int ierr = nc_put_att_text (netCDFp,  lonVarID, "units", sizeof(lon_unit), lon_unit))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_att_text "<<lon_unit<<" returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

    const char history[]="Created by Albany";
      if (netCDFp)
    if (const int ierr = nc_put_att_text (netCDFp,  NC_GLOBAL, "history", sizeof(history), history))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_att_text "<<history<<" returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

      if (netCDFp)
    if (const int ierr = nc_enddef (netCDFp))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_enddef returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

    std::vector<double> deglon(nlon);
    std::vector<double> deglat(nlat);
    for (unsigned i=0; i<nlon; ++i) deglon[i] =((      2*i*pi/nlon) *   (180/pi)) - 180;
    for (unsigned i=0; i<nlat; ++i) deglat[i] = (-pi/2 + i*pi/(nlat-1))*(180/pi);


      if (netCDFp)
    if (const int ierr = nc_put_var (netCDFp, lonVarID, &deglon[0]))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_var lon returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);
      if (netCDFp)
    if (const int ierr = nc_put_var (netCDFp, latVarID, &deglat[0]))
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
        "nc_put_var lat returned error code "<<ierr<<" - "<<nc_strerror(ierr)<<std::endl);

  }
#else
  if (stkMeshStruct->cdfOutput)
    *out << "\nWARNING: NetCDF output requested but SEACAS not compiled in:"
         << " disabling NetCDF output \n" << std::endl;
  stkMeshStruct->cdfOutput = false;

#endif
}

void Aeras::SpectralDiscretization::reNameExodusOutput(std::string& filename)
{
#ifdef ALBANY_SEACAS
  if (stkMeshStruct->exoOutput && !mesh_data.is_null())
  {
    // Delete the mesh data object and recreate it
    mesh_data = Teuchos::null;

    stkMeshStruct->exoOutFile = filename;

    // reset reference value for monotonic time function call as we are writing to a new file
    previous_time_label = -1.0e32;
  }
#else
  if (stkMeshStruct->exoOutput)
    *out << "\nWARNING: exodus output requested but SEACAS not compiled in:"
         << " disabling exodus output \n" << std::endl;

#endif
}

// Convert the stk mesh on this processor to a nodal graph.
// todo Dev/tested on linear elements only.
void Aeras::SpectralDiscretization::meshToGraph()
{
  if (Teuchos::is_null(stkMeshStruct->nodal_data_base)) return;
  if (!stkMeshStruct->nodal_data_base->isNodeDataPresent()) return;

  // Set up the CRS graph used for solution transfer and projection mass
  // matrices. Assume the Crs row size is 27, which is the maximum number
  // required for first-order hexahedral elements.
  nodalGraph = Teuchos::rcp(new Tpetra_CrsGraph(overlap_node_mapT, 27));

  // Elements that surround a given node, in the form of Entity's.
  std::vector<std::vector<stk::mesh::Entity> > sur_elem;
  // numOverlapNodes are the total # of nodes seen by this pe
  // numOwnedNodes are the total # of nodes owned by this pe
  sur_elem.resize(numOverlapNodes);

  // Get the elements owned by the current processor
  const stk::mesh::Selector select_owned_in_part =
    stk::mesh::Selector( metaData.universal_part() ) &
    stk::mesh::Selector( metaData.locally_owned_part() );

  const stk::mesh::BucketVector& buckets = bulkData.get_buckets(
    stk::topology::ELEMENT_RANK, select_owned_in_part);

  for (int b = 0; b < buckets.size(); ++b)
  {
    const stk::mesh::Bucket& cells = *buckets[b];
    // Find the surrounding elements for each node owned by this processor.
    for (std::size_t ecnt = 0; ecnt < cells.size(); ecnt++)
    {
      const stk::mesh::Entity e = cells[ecnt];
      const stk::mesh::Entity* node_rels = bulkData.begin_nodes(e);
      const size_t num_node_rels = bulkData.num_nodes(e);

      // Loop over nodes within the element.
      for (std::size_t ncnt = 0; ncnt < num_node_rels; ++ncnt)
      {
        const stk::mesh::Entity rowNode = node_rels[ncnt];
        GO nodeGID = gid(rowNode);
        int nodeLID = overlap_node_mapT->getLocalElement(nodeGID);
        // In the case of degenerate elements, where a node can be entered into
        // the connect table twice, need to check to make sure that this element
        // is not already listed as surrounding this node.
        if (sur_elem[nodeLID].empty() || entity_in_list(e, sur_elem[nodeLID]) < 0)
          sur_elem[nodeLID].push_back(e);
      }
    }
  }

  std::size_t max_nsur = 0;
  for (std::size_t ncnt = 0; ncnt < numOverlapNodes; ncnt++)
  {
    if (sur_elem[ncnt].empty())
    {
      TEUCHOS_TEST_FOR_EXCEPTION(
        true, std::logic_error,
        "Node = " << ncnt+1 << " has no elements" << std::endl);
    }
    else
    {
      std::size_t nsur = sur_elem[ncnt].size();
      if (nsur > max_nsur) max_nsur = nsur;
    }
  }

  // end find_surrnd_elems

  // find_adjacency

  // Note that the center node of a subgraph must be owned by this pe, but we
  // want all nodes in the overlap graph to be covered in the nodal graph.

  // loop over all the nodes owned by this PE
  for(std::size_t ncnt = 0; ncnt < numOverlapNodes; ncnt++)
  {
    Teuchos::Array<GO> adjacency;
    GO globalrow = overlap_node_mapT->getGlobalElement(ncnt);
    // loop over the elements surrounding node ncnt
    for(std::size_t ecnt = 0; ecnt < sur_elem[ncnt].size(); ecnt++)
    {
      const stk::mesh::Entity elem  = sur_elem[ncnt][ecnt];
      const stk::mesh::Entity* node_rels = bulkData.begin_nodes(elem);
      const size_t num_node_rels = bulkData.num_nodes(elem);
      std::size_t ws = elemGIDws[gid(elem)].ws;
      // loop over the nodes in the surrounding element elem
      for (std::size_t lnode = 0; lnode < num_node_rels; ++lnode)
      {
        const stk::mesh::Entity node_a = node_rels[lnode];
        // entry is the GID of each node
        GO entry = gid(node_a);
        // Every node in an element adjacent to node 'globalrow' is in this
        // graph.
        if (in_list(entry, adjacency) < 0) adjacency.push_back(entry);
      }
    }
    nodalGraph->insertGlobalIndices(globalrow, adjacency());
  }

  // end find_adjacency

  nodalGraph->fillComplete();
  // Pass the graph RCP to the nodal data block
  stkMeshStruct->nodal_data_base->updateNodalGraph(nodalGraph);
}

void
Aeras::SpectralDiscretization::printVertexConnectivity()
{

  if(Teuchos::is_null(nodalGraph)) return;

  for(std::size_t i = 0; i < numOverlapNodes; i++)
  {

    GO globalvert = overlap_node_mapT->getGlobalElement(i);

    std::cout << "Center vert is : " << globalvert + 1 << std::endl;

    Teuchos::ArrayView<const GO> adj;

    nodalGraph->getGlobalRowView(globalvert, adj);

    for(std::size_t j = 0; j < adj.size(); j++)

      std::cout << "                  " << adj[j] + 1 << std::endl;

   }
}

void
Aeras::SpectralDiscretization::updateMesh(bool /*shouldTransferIPData*/)
{

  enrichMesh();

#ifdef ALBANY_EPETRA
  const Albany::StateInfoStruct& nodal_param_states = stkMeshStruct->getFieldContainer()->getNodalParameterSIS();
  nodalDOFsStructContainer.addEmptyDOFsStruct("ordinary_solution", "", neq);
  nodalDOFsStructContainer.addEmptyDOFsStruct("mesh_nodes", "", 1);
  for(int is=0; is<nodal_param_states.size(); is++)
  {
    const Albany::StateStruct& param_state = *nodal_param_states[is];
    const Albany::StateStruct::FieldDims& dim = param_state.dim;
    int numComps = 1;
    if (dim.size()==3)      // vector
      numComps = dim[2];
    else if (dim.size()==4) // tensor
      numComps = dim[2]*dim[3];

    nodalDOFsStructContainer.addEmptyDOFsStruct(param_state.name, param_state.meshPart,numComps);
  }

  computeNodalEpetraMaps(false);
#endif // ALBANY_EPETRA

  computeOwnedNodesAndUnknowns(); 

  //write owned maps to matrix market file for debug
  Tpetra_MatrixMarket_Writer::writeMapFile("mapT.mm", *mapT);
  Tpetra_MatrixMarket_Writer::writeMapFile("node_mapT.mm", *node_mapT);

  //IK, 1/23/15: I've commented out the guts of this function.
  //It is only needed for ML/MueLu and is not critical right now to get spectral 
  //elements to work.
  setupMLCoords();

#ifdef ALBANY_EPETRA
  computeNodalEpetraMaps(true);
#endif // ALBANY_EPETRA

  computeOverlapNodesAndUnknowns();
  
  //write overlap maps to matrix market file for debug
  Tpetra_MatrixMarket_Writer::writeMapFile("overlap_mapT.mm", *overlap_mapT);
  Tpetra_MatrixMarket_Writer::writeMapFile("overlap_node_mapT.mm", *overlap_node_mapT);

  //IK, 1/23/15, FIXME: to implement
  transformMesh();

  //IK, 1/23/15, FIXME: to implement
  computeGraphs();

  //IK, 1/26/15: This will need to be uncommented at some point
  //computeCoordinates(); 
  //Note that coordinates are set in getCoordinates() and getCoords() as well.  I think the former isn't called anywhere however.

  //IK, 1/23/15, FIXME: to implement
  computeWorksetInfo();
 
  //IK, 1/23/15: I have changed it so nothing happens in the following functions b/c 
  //we have no Dirichlet/Neumann BCs for spherical mesh.
  //Ultimately we probably want to remove these.
  computeNodeSets();
  computeSideSets();

  //IK, 1/26/15 -- commenting out for now
  //setupExodusOutput();

  // Build the node graph needed for the mass matrix for solution transfer and projection operations
  // FIXME this only needs to be called if we are using the L2 Projection response
  // IK, 1/23/15: I don't think we'll need meshToGraph for Aeras.
  //meshToGraph();
  // printVertexConnectivity();
  // IK, 1/26/15 -- commenting out for now
  //setupNetCDFOutput();

}
