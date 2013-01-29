//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <limits>
#include "Epetra_Export.h"

#include "Albany_Utils.hpp"
#include "Albany_FMDBDiscretization.hpp"
#include <string>
#include <iostream>
#include <fstream>

#include <Shards_BasicTopologies.hpp>
#include "Shards_CellTopology.hpp"
#include "Shards_CellTopologyData.h"

//#include <Intrepid_FieldContainer.hpp>
#include <PHAL_Dimension.hpp>

const double pi = 3.1415926535897932385;

Albany::FMDBDiscretization::FMDBDiscretization(Teuchos::RCP<Albany::FMDBMeshStruct> fmdbMeshStruct_,
					     const Teuchos::RCP<const Epetra_Comm>& comm_) :
  out(Teuchos::VerboseObjectBase::getDefaultOStream()),
  previous_time_label(-1.0e32),
  comm(comm_),
  neq(fmdbMeshStruct_->neq),
  fmdbMeshStruct(fmdbMeshStruct_),
  interleavedOrdering(fmdbMeshStruct_->interleavedOrdering),
  allocated_xyz(false)
{
  Albany::FMDBDiscretization::updateMesh(fmdbMeshStruct,comm);
  
}

Albany::FMDBDiscretization::~FMDBDiscretization()
{
  if (allocated_xyz) { delete [] xx; delete [] yy; delete [] zz; delete [] rr; allocated_xyz=false;} 

  for (int i=0; i< toDelete.size(); i++) delete [] toDelete[i];
}

	    
Teuchos::RCP<const Epetra_Map>
Albany::FMDBDiscretization::getMap() const
{
  return map;
}

Teuchos::RCP<const Epetra_Map>
Albany::FMDBDiscretization::getOverlapMap() const
{
  return overlap_map;
}

Teuchos::RCP<const Epetra_CrsGraph>
Albany::FMDBDiscretization::getJacobianGraph() const
{
  return graph;
}

Teuchos::RCP<const Epetra_CrsGraph>
Albany::FMDBDiscretization::getOverlapJacobianGraph() const
{
  return overlap_graph;
}

Teuchos::RCP<const Epetra_Map>
Albany::FMDBDiscretization::getNodeMap() const
{
  return node_map;
}

const Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> > > >&
Albany::FMDBDiscretization::getWsElNodeEqID() const
{
  return wsElNodeEqID;
}

const Teuchos::ArrayRCP<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >&
Albany::FMDBDiscretization::getCoords() const
{
  return coords;
}

Teuchos::ArrayRCP<double>& 
Albany::FMDBDiscretization::getCoordinates() const
{
  // get mesh dimension and part handle
  int mesh_dim, counter=0;
  FMDB_Mesh_GetDim(fmdbMeshStruct->getMesh(), &mesh_dim);
  pPart part;
  FMDB_Mesh_GetPart(fmdbMeshStruct->getMesh(), 0, part);
  
  // iterate over all vertices (nodes)
  pMeshEnt node;
  double * node_coords=new double[3];
  pPartEntIter node_it;

  int iterEnd = FMDB_PartEntIter_Init(part, FMDB_VERTEX, FMDB_ALLTOPO, node_it);
  while (!iterEnd)
  {
    iterEnd = FMDB_PartEntIter_GetNext(node_it, node);
    if (iterEnd) break; 
    // get vertex(node) coords
    FMDB_Vtx_GetCoord (node, &node_coords);
    for (int dim=0; dim<mesh_dim; ++dim)
      coordinates[mesh_dim*counter + dim] = node_coords[dim];
    ++counter;
  }
  FMDB_PartEntIter_Del (node_it);
  delete []  node_coords;
  return coordinates;
}

//The function transformMesh() maps a unit cube domain by applying the transformation 
//x = L*x
//y = L*y
//z = s(x,y)*z + b(x,y)*(1-z)
//where b(x,y) and s(x,y) are curves specifying the bedrock and top surface 
//geometries respectively.   
//Currently this function is only needed for some FELIX problems.


void
Albany::FMDBDiscretization::getOwned_xyz(double** x, double** y, double** z,
                                        double **rbm, int& nNodes, int numPDEs, int numScalar,  int nullSpaceDim)
{
  // Function to return x,y,z at owned nodes as double*, specifically for ML

  // get mesh dimension and part handle
  int mesh_dim, counter=0;
  FMDB_Mesh_GetDim(fmdbMeshStruct->getMesh(), &mesh_dim);
  pPart part;
  FMDB_Mesh_GetPart(fmdbMeshStruct->getMesh(), 0, part);

  nNodes = numOwnedNodes;

  if (allocated_xyz) { delete [] xx; delete [] yy; delete [] zz;} 
  xx = new double[numOwnedNodes];
  yy = new double[numOwnedNodes];
  zz = new double[numOwnedNodes];
  if (nullSpaceDim>0) 
    rr = new double[(nullSpaceDim + numScalar)*numPDEs*nNodes];
  else                
    rr = new double[1]; // Just so there is something to delete in destructor
  allocated_xyz = true;

  double* node_coords=new double[3];
  pPartEntIter node_it;
  pMeshEnt node;
  int owner_partid, iterEnd = FMDB_PartEntIter_Init(part, FMDB_VERTEX, FMDB_ALLTOPO, node_it);
  while (!iterEnd)
  {
    iterEnd = FMDB_PartEntIter_GetNext(node_it, node);
    if (iterEnd) break; 

    FMDB_Ent_GetOwnPartID(node, part, &owner_partid);
    if (owner_partid!=FMDB_Part_ID(part)) continue; // skip un-owned entity

    FMDB_Vtx_GetCoord (node, &node_coords);
    xx[counter]=node_coords[0];
    yy[counter]=node_coords[1];
    if (mesh_dim>2) zz[counter]=node_coords[2];
    ++counter;
  }
  FMDB_PartEntIter_Del (node_it);
  delete [] node_coords;

  // Leave unused dim as null pointers.
  if (mesh_dim > 0) *x = xx;
  if (mesh_dim > 1) *y = yy;
  if (mesh_dim > 2) *z = zz;
  *rbm = rr;
}

const Teuchos::ArrayRCP<std::string>& 
Albany::FMDBDiscretization::getWsEBNames() const
{
  return wsEBNames;
}

const Teuchos::ArrayRCP<int>& 
Albany::FMDBDiscretization::getWsPhysIndex() const
{
  return wsPhysIndex;
}

void Albany::FMDBDiscretization::outputToExodus(const Epetra_Vector& soln, const double time, const bool overlapped)
{
  // Put solution as Epetra_Vector into FMDB Mesh
  if(!overlapped)
    setSolutionField(soln);

  // soln coming in is overlapped
  else
    setOvlpSolutionField(soln);

#if 0

  if (fmdbMeshStruct->exoOutput) {

    // Skip this write unless the proper interval has been reached
    if(outputInterval++ % fmdbMeshStruct->exoOutputInterval)

      return;

    double time_label = monotonicTimeLabel(time);

//    int out_step = stk::io::process_output_request(*mesh_data, *fmdbMeshStruct->bulkData, time_label);

    if (map->Comm().MyPID()==0) {
      *out << "Albany::FMDBDiscretization::outputToExodus: writing time " << time;
      if (time_label != time) *out << " with label " << time_label;
      *out << " to index " <<out_step<<" in file "<<fmdbMeshStruct->exoOutFile<< endl;
    }
  }
#endif
}

double
Albany::FMDBDiscretization::monotonicTimeLabel(const double time) 
{
  // If increasing, then all is good
  if (time > previous_time_label) {
    previous_time_label = time;
    return time;
  }
// Try absolute value
  double time_label = fabs(time);
  if (time_label > previous_time_label) {
    previous_time_label = time_label;
    return time_label;
  }

  // Try adding 1.0 to time
  if (time_label+1.0 > previous_time_label) {
    previous_time_label = time_label+1.0;
    return time_label+1.0;
  }

  // Otherwise, just add 1.0 to previous
  previous_time_label += 1.0;
  return previous_time_label;
}

void 
Albany::FMDBDiscretization::setResidualField(const Epetra_Vector& residual) 
{
  pPart part;
  FMDB_Mesh_GetPart(fmdbMeshStruct->getMesh(), 0, part);

  pPartEntIter node_it;
  pMeshEnt node;
  int owner_part_id, counter=0;
  double* res = new double[neq];
  // iterate over all vertices (nodes)
  int iterEnd = FMDB_PartEntIter_Init(part, FMDB_VERTEX, FMDB_ALLTOPO, node_it);
  while (!iterEnd)
  {
    iterEnd = FMDB_PartEntIter_GetNext(node_it, node);
    if(iterEnd) break; 
    // get node's owner part id and skip if not owned
    FMDB_Ent_GetOwnPartID(node, part, &owner_part_id);
    if (FMDB_Part_ID(part)!=owner_part_id) continue; 

    for (std::size_t j=0; j<neq; j++)
      res[j] = residual[getOwnedDOF(FMDB_Ent_LocalID(node),j)]; 
    FMDB_Ent_SetDblArrTag (fmdbMeshStruct->getMesh(), node, fmdbMeshStruct->residual_field_tag, res, neq);
  }
  FMDB_PartEntIter_Del (node_it);
  delete [] res;
}

Teuchos::RCP<Epetra_Vector>
Albany::FMDBDiscretization::getSolutionField() const
{
  // Copy soln vector into solution field, one node at a time
  Teuchos::RCP<Epetra_Vector> soln = Teuchos::rcp(new Epetra_Vector(*map));

  // get the first (0th) part handle on local process -- assumption: single part per process/mesh_instance
  pPart part;
  FMDB_Mesh_GetPart(fmdbMeshStruct->getMesh(), 0, part);

  pPartEntIter node_it;
  pMeshEnt node;
  int owner_part_id, sol_size;
  double* sol = new double[neq];
  // iterate over all vertices (nodes)
  int iterEnd = FMDB_PartEntIter_Init(part, FMDB_VERTEX, FMDB_ALLTOPO, node_it);
  while (!iterEnd)
  {
    iterEnd = FMDB_PartEntIter_GetNext(node_it, node);
    if(iterEnd) break; 
    // get node's owner part id and skip if not owned
    FMDB_Ent_GetOwnPartID(node, part, &owner_part_id);
    if (FMDB_Part_ID(part)!=owner_part_id) continue; 

    FMDB_Ent_GetDblArrTag (fmdbMeshStruct->getMesh(), node, fmdbMeshStruct->solution_field_tag, &sol, &sol_size);
    for (std::size_t j=0; j<neq; j++)
      (*soln)[getOwnedDOF(FMDB_Ent_LocalID(node),j)] = sol[j]; 
  }
  FMDB_PartEntIter_Del (node_it);
  delete [] sol;

  return soln;
}

/*****************************************************************/
/*** Private functions follow. These are just used in above code */
/*****************************************************************/

void 
Albany::FMDBDiscretization::setSolutionField(const Epetra_Vector& soln) 
{
  // get the first (0th) part handle on local process -- assumption: single part per process/mesh_instance
  pPart part;
  FMDB_Mesh_GetPart(fmdbMeshStruct->getMesh(), 0, part);

  pPartEntIter node_it;
  pMeshEnt node;
  int owner_part_id;
  double* sol = new double[neq];
  // iterate over all vertices (nodes)
  int iterEnd = FMDB_PartEntIter_Init(part, FMDB_VERTEX, FMDB_ALLTOPO, node_it);
  while (!iterEnd)
  {
    iterEnd = FMDB_PartEntIter_GetNext(node_it, node);
    if(iterEnd) break; 
    // get node's owner part id and skip if not owned
    FMDB_Ent_GetOwnPartID(node, part, &owner_part_id);
    if (FMDB_Part_ID(part)!=owner_part_id) continue; 

    for (std::size_t j=0; j<neq; j++)
      sol[j] = soln[getOwnedDOF(FMDB_Ent_LocalID(node),j)];  
    FMDB_Ent_SetDblArrTag (fmdbMeshStruct->getMesh(), node, fmdbMeshStruct->solution_field_tag, sol, neq);
  }
  FMDB_PartEntIter_Del (node_it);
  delete [] sol;
}

void 
Albany::FMDBDiscretization::setOvlpSolutionField(const Epetra_Vector& soln) 
{
#if 0
  // Copy soln vector into solution field, one node at a time
  // Note that soln coming in is the local+ghost (overlapped) soln
  for (std::size_t i=0; i < overlapnodes.size(); i++)  {
//    double* sol = stk::mesh::field_data(*fmdbMeshStruct->solution_field, *overlapnodes[i]);
    for (std::size_t j=0; j<neq; j++)
      sol[j] = soln[getOwnedDOF(i,j)]; 
  }
#endif
  // get the first (0th) part handle on local process -- assumption: single part per process/mesh_instance
  pPart part;
  FMDB_Mesh_GetPart(fmdbMeshStruct->getMesh(), 0, part);

  pPartEntIter node_it;
  pMeshEnt node;
  int owner_part_id, counter=0;
  double* sol = new double[neq];
  // iterate over all vertices (nodes)
  int iterEnd = FMDB_PartEntIter_Init(part, FMDB_VERTEX, FMDB_ALLTOPO, node_it);
  while (!iterEnd)
  {
    iterEnd = FMDB_PartEntIter_GetNext(node_it, node);
    if(iterEnd) break; 

    // get node's owner part id and skip if not owned
    FMDB_Ent_GetOwnPartID(node, part, &owner_part_id);
    if (FMDB_Part_ID(part)!=owner_part_id) continue; 

    for (std::size_t j=0; j<neq; j++)
      sol[j] = soln[getOverlapDOF(FMDB_Ent_ID(node),j)]; 
    FMDB_Ent_SetDblArrTag (fmdbMeshStruct->getMesh(), node, fmdbMeshStruct->solution_field_tag, sol, neq);
    ++counter;
  }
  FMDB_PartEntIter_Del (node_it);
  delete [] sol;
}

//inline int Albany::FMDBDiscretization::gid(const stk::mesh::Entity& node) const
//{ return node.identifier()-1; }

//inline int Albany::FMDBDiscretization::gid(const stk::mesh::Entity* node) const
//{ return gid(*node); }

inline int Albany::FMDBDiscretization::getOwnedDOF(const int inode, const int eq) const
{
  if (interleavedOrdering) return inode*neq + eq;
  else  return inode + numOwnedNodes*eq;
}

inline int Albany::FMDBDiscretization::getOverlapDOF(const int inode, const int eq) const
{
  if (interleavedOrdering) return inode*neq + eq;
  else  return inode + numOverlapNodes*eq;
}

inline int Albany::FMDBDiscretization::getGlobalDOF(const int inode, const int eq) const
{
  if (interleavedOrdering) return inode*neq + eq;
  else  return inode + numGlobalNodes*eq;
}

int Albany::FMDBDiscretization::nonzeroesPerRow(const int neq) const
{
  int numDim;
  FMDB_Mesh_GetDim(fmdbMeshStruct->getMesh(), &numDim);

  int estNonzeroesPerRow;
  switch (numDim) {
  case 0: estNonzeroesPerRow=1*neq; break;
  case 1: estNonzeroesPerRow=3*neq; break;
  case 2: estNonzeroesPerRow=9*neq; break;
  case 3: estNonzeroesPerRow=27*neq; break;
  default: TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
			      "FMDBDiscretization:  Bad numDim"<< numDim);
  }
  return estNonzeroesPerRow;
}

void Albany::FMDBDiscretization::computeOwnedNodesAndUnknowns()
{
  // Loads member data:  ownednodes, numOwnedNodes, node_map, numGlobalNodes, map
  // maps for owned nodes and unknowns

  // get the first (0th) part handle on local process -- assumption: single part per process/mesh_instance
  pPart part;
  FMDB_Mesh_GetPart(fmdbMeshStruct->getMesh(), 0, part);

  // compute owned nodes
  pPartEntIter node_it;
  pMeshEnt node;
  vector<pMeshEnt> owned_nodes;
  std::vector<int> indices;
  int owner_part_id;

  // iterate over all vertices (nodes) and save owned nodes
  int iterEnd = FMDB_PartEntIter_Init(part, FMDB_VERTEX, FMDB_ALLTOPO, node_it);
  while (!iterEnd)
  {
    iterEnd = FMDB_PartEntIter_GetNext(node_it, node);
    if(iterEnd) break; 
    // get node's owner part id and skip if not owned
    FMDB_Ent_GetOwnPartID(node, part, &owner_part_id);
    if (FMDB_Part_ID(part)!=owner_part_id) continue; 

    owned_nodes.push_back(node);
    indices.push_back(FMDB_Ent_ID(node));
  }
  FMDB_PartEntIter_Del (node_it);

  numOwnedNodes = owned_nodes.size();

  node_map = Teuchos::rcp(new Epetra_Map(-1, numOwnedNodes,
					 &(indices[0]), 0, *comm));

  MPI_Allreduce(&numOwnedNodes,&numGlobalNodes,1,MPI_INT,MPI_SUM, Albany::getMpiCommFromEpetraComm(*comm));

  indices.resize(numOwnedNodes * neq);
  for (int i=0; i < numOwnedNodes; ++i)
    for (std::size_t j=0; j < neq; ++j)
      indices[getOwnedDOF(i,j)] = getGlobalDOF(FMDB_Ent_ID(owned_nodes[i]),j);

  map = Teuchos::rcp(new Epetra_Map(-1, indices.size(), &(indices[0]), 0, *comm));
}

void Albany::FMDBDiscretization::computeOverlapNodesAndUnknowns()
{
  std::vector<int> indices;

  // get the first (0th) part handle on local process -- assumption: single part per process/mesh_instance
  pPart part;
  FMDB_Mesh_GetPart(fmdbMeshStruct->getMesh(), 0, part);

  pPartEntIter node_it;
  pMeshEnt node;

  // get # all (owned, duplicate copied on part boundary and ghosted) nodes
  FMDB_Part_GetNumEnt (part, FMDB_VERTEX, FMDB_ALLTOPO, &numOverlapNodes);
  indices.resize(numOverlapNodes * neq);

  // get global id of all nodes
  int i=0, iterEnd = FMDB_PartEntIter_Init(part, FMDB_VERTEX, FMDB_ALLTOPO, node_it);
  while (!iterEnd)
  {
    iterEnd = FMDB_PartEntIter_GetNext(node_it, node);
    if(iterEnd) break; 
    for (std::size_t j=0; j < neq; j++)
      indices[getOverlapDOF(i,j)] = getGlobalDOF(FMDB_Ent_ID(node),j);
    ++i;
  }

  overlap_map = Teuchos::rcp(new Epetra_Map(-1, indices.size(),
					    &(indices[0]), 0, *comm));

  // Set up epetra map of node IDs
  indices.resize(numOverlapNodes);
  iterEnd = FMDB_PartEntIter_Reset(node_it);
  i=0;
  while (!iterEnd)
  {
    iterEnd = FMDB_PartEntIter_GetNext(node_it, node);
    if(iterEnd) break; 
    indices[i] = FMDB_Ent_ID(node);
  }
  FMDB_PartEntIter_Del (node_it);

  overlap_node_map = Teuchos::rcp(new Epetra_Map(-1, indices.size(),
						 &(indices[0]), 0, *comm));
  coordinates.resize(3*numOverlapNodes);
}


void Albany::FMDBDiscretization::computeGraphs()
{
  // GAH: the following assumes all element blocks in the problem have the same
  // number of nodes per element and that the cell topologies are the same.

  // get mesh dimension and part handle
  int mesh_dim, counter=0;
  FMDB_Mesh_GetDim(fmdbMeshStruct->getMesh(), &mesh_dim);
  pPart part;
  FMDB_Mesh_GetPart(fmdbMeshStruct->getMesh(), 0, part);

  // let's get an element (region) to query element topology
  pPartEntIter elem_it;
  pMeshEnt elem;
  FMDB_PartEntIter_Init(part, FMDB_REGION, FMDB_ALLTOPO, elem_it);
  FMDB_PartEntIter_GetNext(elem_it, elem);
  FMDB_PartEntIter_Del(elem_it);

  // query element topology
  int elem_topology;
  FMDB_Ent_GetTopo(elem, &elem_topology);

  // query # nodes per element topology
  int nodes_per_element = FMDB_Topo_NumDownAdj(elem_topology, FMDB_VERTEX);

  // Loads member data:  overlap_graph, numOverlapodes, overlap_node_map, coordinates, graphs
  overlap_graph = Teuchos::null; // delete existing graph happens here on remesh

  overlap_graph =
    Teuchos::rcp(new Epetra_CrsGraph(Copy, *overlap_map,
                                     neq*nodes_per_element, false));

  // get cells 
  std::vector<pMeshEnt> cells; 
  pPartEntIter cell_it;
  pMeshEnt cell;
  int iterEnd = FMDB_PartEntIter_Init(part, FMDB_REGION, FMDB_ALLTOPO, cell_it);
  while (!iterEnd)
  {
    iterEnd = FMDB_PartEntIter_GetNext(cell_it, cell);
    if(iterEnd) break; 
    cells.push_back(cell);
  }
  FMDB_PartEntIter_Del (cell_it);

  if (SCUTIL_CommRank()==0)
    *out <<__func__<<": "<<cells.size() << " elements on Proc 0 " << endl;

  int row, col;
  std::vector<pMeshEnt> rel;
  for (std::size_t i=0; i < cells.size(); i++) 
  {
    pMeshEnt e = cells[i];
    rel.clear();
    FMDB_Ent_GetAdj(cell, FMDB_VERTEX, 1, rel);

    // loop over local nodes
    for (std::size_t j=0; j < rel.size(); j++) 
    {
      pMeshEnt rowNode = rel[j];

      // loop over eqs
      for (std::size_t k=0; k < neq; k++) 
      {
        row = getGlobalDOF(FMDB_Ent_ID(rowNode), k);
        for (std::size_t l=0; l < rel.size(); l++) 
        {
          pMeshEnt colNode = rel[l];
          for (std::size_t m=0; m < neq; m++) 
          {
            col = getGlobalDOF(FMDB_Ent_ID(colNode), m);
            overlap_graph->InsertGlobalIndices(row, 1, &col);
          }
        }
      }
    }
  }

  overlap_graph->FillComplete();

  // Create Owned graph by exporting overlap with known row map

  graph = Teuchos::null; // delete existing graph happens here on remesh

  graph = Teuchos::rcp(new Epetra_CrsGraph(Copy, *map, nonzeroesPerRow(neq), false));

  // Create non-overlapped matrix using two maps and export object
  Epetra_Export exporter(*overlap_map, *map);
  graph->Export(*overlap_graph, exporter, Insert);
  graph->FillComplete();
}

void Albany::FMDBDiscretization::computeWorksetInfo()
{
#if 0

  stk::mesh::Selector select_owned_in_part =
    stk::mesh::Selector( metaData.universal_part() ) &
    stk::mesh::Selector( metaData.locally_owned_part() );

  std::vector< stk::mesh::Bucket * > buckets ;
  stk::mesh::get_buckets( select_owned_in_part ,
                          bulkData.buckets( metaData.element_rank() ) ,
                          buckets);

  int numBuckets =  buckets.size();

  wsEBNames.resize(numBuckets);
  for (int i=0; i<numBuckets; i++) {
    std::vector< stk::mesh::Part * >  bpv;
    buckets[i]->supersets(bpv);
    for (std::size_t j=0; j<bpv.size(); j++) {
      if (bpv[j]->primary_entity_rank() == metaData.element_rank()) {
	if (bpv[j]->name()[0] != '{') {
	  // *out << "Bucket " << i << " is in Element Block:  " << bpv[j]->name() 
	  //      << "  and has " << buckets[i]->size() << " elements." << endl;
	  wsEBNames[i]=bpv[j]->name();
	}
      }
    }
  }

  wsPhysIndex.resize(numBuckets);
  if (fmdbMeshStruct->allElementBlocksHaveSamePhysics)
    for (int i=0; i<numBuckets; i++) wsPhysIndex[i]=0;
  else 
    for (int i=0; i<numBuckets; i++) wsPhysIndex[i]=fmdbMeshStruct->ebNameToIndex[wsEBNames[i]];

  // Fill  wsElNodeEqID(workset, el_LID, local node, Eq) => unk_LID

  wsElNodeEqID.resize(numBuckets);
  coords.resize(numBuckets);

  for (int b=0; b < numBuckets; b++) {

    stk::mesh::Bucket& buck = *buckets[b];
    wsElNodeEqID[b].resize(buck.size());
    coords[b].resize(buck.size());

    // i is the element index within bucket b

    for (std::size_t i=0; i < buck.size(); i++) {
  
      // Traverse all the elements in this bucket
      stk::mesh::Entity& element = buck[i];

      // Now, save a map from element GID to workset on this PE
      elemGIDws[gid(element)].ws = b;

      // Now, save a map from element GID to local id on this workset on this PE
      elemGIDws[gid(element)].LID = i;

      stk::mesh::PairIterRelation rel = element.relations(metaData.NODE_RANK);

      int nodes_per_element = rel.size();
      wsElNodeEqID[b][i].resize(nodes_per_element);
      coords[b][i].resize(nodes_per_element);
      // loop over local nodes
      for (int j=0; j < nodes_per_element; j++) {
        stk::mesh::Entity& rowNode = * rel[j].entity();
        int node_gid = gid(rowNode);
        int node_lid = overlap_node_map->LID(node_gid);
        
        TEUCHOS_TEST_FOR_EXCEPTION(node_lid<0, std::logic_error,
			   "FMDB1D_Disc: node_lid out of range " << node_lid << endl);
        coords[b][i][j] = stk::mesh::field_data(*fmdbMeshStruct->coordinates_field, rowNode);

        wsElNodeEqID[b][i][j].resize(neq);
        for (std::size_t eq=0; eq < neq; eq++) 
          wsElNodeEqID[b][i][j][eq] = getOverlapDOF(node_lid,eq);
      }
    }
  }

  for (int d=0; d<fmdbMeshStruct->numDim; d++) {
  if (fmdbMeshStruct->PBCStruct.periodic[d]) {
    for (int b=0; b < numBuckets; b++) {
      for (std::size_t i=0; i < buckets[b]->size(); i++) {
        int nodes_per_element = (*buckets[b])[i].relations(metaData.NODE_RANK).size();
        bool anyXeqZero=false;
        for (int j=0; j < nodes_per_element; j++)  if (coords[b][i][j][d]==0.0) anyXeqZero=true;
        if (anyXeqZero)  {
          bool flipZeroToScale=false;
          for (int j=0; j < nodes_per_element; j++) 
              if (coords[b][i][j][d] > fmdbMeshStruct->PBCStruct.scale[d]/1.9) flipZeroToScale=true;
          if (flipZeroToScale) {  
            for (int j=0; j < nodes_per_element; j++)  {
              if (coords[b][i][j][d] == 0.0) {
                double* xleak = new double [fmdbMeshStruct->numDim];
                for (int k=0; k < fmdbMeshStruct->numDim; k++) 
                  if (k==d) xleak[d]=fmdbMeshStruct->PBCStruct.scale[d];
                  else xleak[k] = coords[b][i][j][k];
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

  // Pull out pointers to shards::Arrays for every bucket, for every state
  // Code is data-type dependent
  stateArrays.resize(numBuckets);
  for (std::size_t b=0; b < buckets.size(); b++) {
    stk::mesh::Bucket& buck = *buckets[b];
    for (std::size_t i=0; i<fmdbMeshStruct->qpscalar_states.size(); i++) {
      stk::mesh::BucketArray<Albany::FMDBMeshStruct::QPScalarFieldType> array(*fmdbMeshStruct->qpscalar_states[i], buck);
      MDArray ar = array;
      stateArrays[b][fmdbMeshStruct->qpscalar_states[i]->name()] = ar;
    }
    for (std::size_t i=0; i<fmdbMeshStruct->qpvector_states.size(); i++) {
      stk::mesh::BucketArray<Albany::FMDBMeshStruct::QPVectorFieldType> array(*fmdbMeshStruct->qpvector_states[i], buck);
      MDArray ar = array;
      stateArrays[b][fmdbMeshStruct->qpvector_states[i]->name()] = ar;
    }
    for (std::size_t i=0; i<fmdbMeshStruct->qptensor_states.size(); i++) {
      stk::mesh::BucketArray<Albany::FMDBMeshStruct::QPTensorFieldType> array(*fmdbMeshStruct->qptensor_states[i], buck);
      MDArray ar = array;
      stateArrays[b][fmdbMeshStruct->qptensor_states[i]->name()] = ar;
    }    
    for (std::size_t i=0; i<fmdbMeshStruct->scalarValue_states.size(); i++) {      
      const int size = 1;
      shards::Array<double, shards::NaturalOrder, Cell> array(&(fmdbMeshStruct->time), size);
      MDArray ar = array;
      stateArrays[b][fmdbMeshStruct->scalarValue_states[i]] = ar;
    }
  }
#endif
}

void Albany::FMDBDiscretization::computeSideSets()
{
#if 0

  const stk::mesh::EntityRank element_rank = metaData.element_rank();

  // iterator over all side_rank parts found in the mesh
  std::map<std::string, stk::mesh::Part*>::iterator ss = fmdbMeshStruct->ssPartVec.begin();

  int numBuckets = wsEBNames.size();

  sideSets.resize(numBuckets); // Need a sideset list per workset

  while ( ss != fmdbMeshStruct->ssPartVec.end() ) { 

    // Get all owned sides in this side set
    stk::mesh::Selector select_owned_in_sspart =

      // get only entities in the ss part (ss->second is the current sideset part)
      stk::mesh::Selector( *(ss->second) ) &
      // and only if the part is local
      stk::mesh::Selector( metaData.locally_owned_part() );

    std::vector< stk::mesh::Entity * > sides ;
    stk::mesh::get_selected_entities( select_owned_in_sspart , // sides local to this processor
				      bulkData.buckets( metaData.side_rank() ) ,
				      sides ); // store the result in "sides"

    *out << "FMDBDisc: sideset "<< ss->first <<" has size " << sides.size() << "  on Proc 0." << endl;

 //   sideSets[ss->first].resize(sides.size()); // build the data holder

    // loop over the sides to see what they are, then fill in the data holder
    // for side set options, look at $TRILINOS_DIR/packages/stk/stk_usecases/mesh/UseCase_13.cpp

    for (std::size_t localSideID=0; localSideID < sides.size(); localSideID++) {

      stk::mesh::Entity &sidee = *sides[localSideID];

      const stk::mesh::PairIterRelation side_elems = sidee.relations(element_rank); // get the elements
            // containing the side. Note that if the side is internal, it will show up twice in the
            // element list, once for each element that contains it.

      TEUCHOS_TEST_FOR_EXCEPTION(side_elems.size() != 1, std::logic_error,
			   "FMDBDisc: cannot figure out side set topology for side set " << ss->first << endl);

      const stk::mesh::Entity & elem = *side_elems[0].entity();

      SideStruct sStruct;

      // Save elem id. This is the global element id
      sStruct.elem_GID = gid(elem);

      int workset = elemGIDws[sStruct.elem_GID].ws; // Get the ws that this element lives in

      // Save elem id. This is the local element id within the workset
      sStruct.elem_LID = elemGIDws[sStruct.elem_GID].LID;

      // Save the side identifier inside of the element. This starts at zero here.
      sStruct.side_local_id = determine_local_side_id(elem, sidee);

      // Save the index of the element block that this elem lives in
      sStruct.elem_ebIndex = fmdbMeshStruct->ebNameToIndex[wsEBNames[workset]];

      SideSetList& ssList = sideSets[workset];   // Get a ref to the side set map for this ws
      SideSetList::iterator it = ssList.find(ss->first); // Get an iterator to the correct sideset (if
                                                                // it exists)

      if(it != ssList.end()) // The sideset has already been created

        it->second.push_back(sStruct); // Save this side to the vector that belongs to the name ss->first

      else { // Add the key ss->first to the map, and the side vector to that map

        std::vector<SideStruct> tmpSSVec;
        tmpSSVec.push_back(sStruct);
        
        ssList.insert(SideSetList::value_type(ss->first, tmpSSVec));

      }

    }

    ss++;
  }
#endif
}

// From $TRILINOS_DIR/packages/stk/stk_usecases/mesh/UseCase_13.cpp (GAH)

#if 0
unsigned 
Albany::FMDBDiscretization::determine_local_side_id( const stk::mesh::Entity & elem , stk::mesh::Entity & side ) {

  using namespace stk;

  const CellTopologyData * const elem_top = mesh::fem::get_cell_topology( elem ).getCellTopologyData();

  const mesh::PairIterRelation elem_nodes = elem.relations( mesh::fem::FEMMetaData::NODE_RANK );
  const mesh::PairIterRelation side_nodes = side.relations( mesh::fem::FEMMetaData::NODE_RANK );

  int side_id = -1 ;

  for ( unsigned i = 0 ; side_id == -1 && i < elem_top->side_count ; ++i ) {
    const CellTopologyData & side_top = * elem_top->side[i].topology ;
    const unsigned     * side_map =   elem_top->side[i].node ;

    if ( side_nodes.size() == side_top.node_count ) {

      side_id = i ;

      for ( unsigned j = 0 ;
            side_id == static_cast<int>(i) && j < side_top.node_count ; ++j ) {

        mesh::Entity * const elem_node = elem_nodes[ side_map[j] ].entity();

        bool found = false ;

        for ( unsigned k = 0 ; ! found && k < side_top.node_count ; ++k ) {
          found = elem_node == side_nodes[k].entity();
        }

        if ( ! found ) { side_id = -1 ; }
      }
    }
  }

  if ( side_id < 0 ) {
    std::ostringstream msg ;
    msg << "determine_local_side_id( " ;
    msg << elem_top->name ;
    msg << " , Element[ " ;
    msg << elem.identifier();
    msg << " ]{" ;
    for ( unsigned i = 0 ; i < elem_nodes.size() ; ++i ) {
      msg << " " << elem_nodes[i].entity()->identifier();
    }
    msg << " } , Side[ " ;
    msg << side.identifier();
    msg << " ]{" ;
    for ( unsigned i = 0 ; i < side_nodes.size() ; ++i ) {
      msg << " " << side_nodes[i].entity()->identifier();
    }
    msg << " } ) FAILED" ;
    throw std::runtime_error( msg.str() );
  }

  return static_cast<unsigned>(side_id) ;
}
#endif


void Albany::FMDBDiscretization::computeNodeSets()
{

#if 0
  std::map<std::string, stk::mesh::Part*>::iterator ns = fmdbMeshStruct->nsPartVec.begin();
  while ( ns != fmdbMeshStruct->nsPartVec.end() ) { // Iterate over Node Sets
    // Get all owned nodes in this node set
    stk::mesh::Selector select_owned_in_nspart =
      stk::mesh::Selector( *(ns->second) ) &
      stk::mesh::Selector( metaData.locally_owned_part() );

    std::vector< stk::mesh::Entity * > nodes ;
    stk::mesh::get_selected_entities( select_owned_in_nspart ,
				      bulkData.buckets( metaData.node_rank() ) ,
				      nodes );

    nodeSets[ns->first].resize(nodes.size());
    nodeSetCoords[ns->first].resize(nodes.size());
//    nodeSetIDs.push_back(ns->first); // Grab string ID
    cout << "FMDBDisc: nodeset "<< ns->first <<" has size " << nodes.size() << "  on Proc 0." << endl;
    for (std::size_t i=0; i < nodes.size(); i++) {
      int node_gid = gid(nodes[i]);
      int node_lid = node_map->LID(node_gid);
      nodeSets[ns->first][i].resize(neq);
      for (std::size_t eq=0; eq < neq; eq++)  nodeSets[ns->first][i][eq] = getOwnedDOF(node_lid,eq);
      nodeSetCoords[ns->first][i] = stk::mesh::field_data(*fmdbMeshStruct->coordinates_field, *nodes[i]);
    }
    ns++;
  }
#endif
  int mesh_dim;
  FMDB_Mesh_GetDim(fmdbMeshStruct->getMesh(), &mesh_dim);

  pPart part;
  FMDB_Mesh_GetPart(fmdbMeshStruct->getMesh(), 0, part);

  int owner_part_id;
  vector<pNodeSet> node_set;
  
  FMDB_Mesh_GetNodeSet (fmdbMeshStruct->getMesh(), node_set);

  for (vector<pNodeSet>::iterator node_set_it=node_set.begin(); node_set_it!=node_set.end(); ++node_set_it)
  {
    vector<pMeshEnt> node_set_nodes;
    FMDB_NodeSet_GetNode(fmdbMeshStruct->getMesh(), *node_set_it, node_set_nodes);
    // compute owned nodes
    vector<pMeshEnt> owned_nodes;
    for (vector<pMeshEnt>::iterator node_it=node_set_nodes.begin(); node_it!=node_set_nodes.end(); ++node_it)
    {
      FMDB_Ent_GetOwnPartID(*node_it, part, &owner_part_id);

      // if the node is owned by the local part, save it
      if (FMDB_Part_ID(part)==owner_part_id) 
        owned_nodes.push_back(*node_it);
    }

    std::string NS_name;
    FMDB_NodeSet_GetName(fmdbMeshStruct->getMesh(), *node_set_it, NS_name);
    nodeSets[NS_name].resize(owned_nodes.size());
    nodeSetCoords[NS_name].resize(owned_nodes.size());

    cout << "FMDBDisc: nodeset "<< NS_name <<" has size " << owned_nodes.size() << "  on Proc "<<SCUTIL_CommRank()<< endl;
    for (std::size_t i=0; i < owned_nodes.size(); i++) 
    {
      nodeSets[NS_name][i].resize(neq);
      for (std::size_t eq=0; eq < neq; eq++)  
        nodeSets[NS_name][i][eq] = getOwnedDOF(FMDB_Ent_LocalID(owned_nodes[i]), eq);  
      double* node_coords=new double[mesh_dim];
      FMDB_Vtx_GetCoord (owned_nodes[i], &node_coords);
      nodeSetCoords[NS_name][i] = node_coords; // FIXME: deallocate memory for nodeSetCoords[NS_name][i]
    }
  }
}

#if 0
void Albany::FMDBDiscretization::setupExodusOutput()
{
#ifdef ALBANY_SEACAS
  if (fmdbMeshStruct->exoOutput) {

    outputInterval = 0;

    Ioss::Init::Initializer io;
    mesh_data = new stk::io::MeshData();
    stk::io::create_output_mesh(fmdbMeshStruct->exoOutFile,
		  Albany::getMpiCommFromEpetraComm(*comm),
		  bulkData, *mesh_data);

    stk::io::define_output_fields(*mesh_data, metaData);

   // writes out the mesh
    FMDB_Mesh_WriteToFile (fmdbMeshStruct->getMesh(), "output.sms", 1);  // write a mesh into sms or vtk. The third argument is 0 if the mesh is a serial mesh. 1, otherwise.


  }
#else
  if (fmdbMeshStruct->exoOutput) 
    *out << "\nWARNING: exodus output requested but SEACAS not compiled in:"
         << " disabling exodus output \n" << endl;
  
#endif
}
#endif

void
Albany::FMDBDiscretization::updateMesh(Teuchos::RCP<Albany::FMDBMeshStruct> fmdbMeshStruct,
				      const Teuchos::RCP<const Epetra_Comm>& comm)
{
  computeOwnedNodesAndUnknowns();

  computeOverlapNodesAndUnknowns();

  transformMesh(); 

  computeGraphs();

  computeWorksetInfo();

  computeNodeSets();

  computeSideSets();

//  setupExodusOutput();
}
