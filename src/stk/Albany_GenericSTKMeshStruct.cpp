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


#include <iostream>

#include "Albany_GenericSTKMeshStruct.hpp"

#include <Shards_BasicTopologies.hpp>

#include <stk_mesh/base/Entity.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include <stk_mesh/base/GetBuckets.hpp>
#include <stk_mesh/base/FieldData.hpp>
#include <stk_mesh/base/Selector.hpp>

#include <stk_mesh/fem/FieldDeclarations.hpp>
#include <stk_mesh/fem/TopologyHelpers.hpp>
#include <stk_mesh/fem/EntityRanks.hpp>

#ifdef ALBANY_IOSS
#include <stk_io/IossBridge.hpp>
#endif
#include "Albany_Utils.hpp"

enum { field_data_chunk_size = 1001 };


Albany::GenericSTKMeshStruct::GenericSTKMeshStruct(
    const Teuchos::RCP<const Epetra_Comm>& comm)
{
  metaData = new stk::mesh::MetaData(stk::mesh::fem_entity_rank_names() );
  bulkData = new stk::mesh::BulkData(*metaData , Albany::getMpiCommFromEpetraComm(*comm), field_data_chunk_size );
}

void Albany::GenericSTKMeshStruct::SetupMetaData(
		  const Teuchos::RCP<Teuchos::ParameterList>& params,
                  const unsigned int neq_, const unsigned int nstates_,
                  const int numDim_) 
{
  numDim = numDim_;
  neq = neq_;
  nstates = nstates_;

  cubatureDegree = params->get("Cubature Degree", 3);

  //Start STK stuff
  coordinates_field = & metaData->declare_field< VectorFieldType >( "coordinates" );
  solution_field = & metaData->declare_field< VectorFieldType >( "solution" );
  residual_field = & metaData->declare_field< VectorFieldType >( "residual" );
  state_field = & metaData->declare_field< VectorFieldType >( "state" );

  stk::mesh::put_field( *coordinates_field , stk::mesh::Node , metaData->universal_part(), numDim );
  stk::mesh::put_field( *solution_field , stk::mesh::Node , metaData->universal_part(), neq );
  stk::mesh::put_field( *residual_field , stk::mesh::Node , metaData->universal_part() , neq );
  if (nstates>0) stk::mesh::put_field( *state_field , stk::mesh::Element , metaData->universal_part(), nstates );
  
#ifdef ALBANY_IOSS
  stk::io::set_field_role(*coordinates_field, Ioss::Field::ATTRIBUTE);
  stk::io::set_field_role(*solution_field, Ioss::Field::TRANSIENT);
  stk::io::set_field_role(*residual_field, Ioss::Field::TRANSIENT);
  if (nstates>0) stk::io::set_field_role(*state_field, Ioss::Field::TRANSIENT);
#endif

  exoOutput = params->isType<string>("Exodus Output File Name");
  if (exoOutput)
    exoOutFile = params->get<string>("Exodus Output File Name");
}

void Albany::GenericSTKMeshStruct::DeclareParts(std::vector<std::string> nsNames)
{
  // HandCoded meshes have 1 element block
  partVec[0] = &  metaData->declare_part( "Block_1", stk::mesh::Element );
#ifdef ALBANY_IOSS
  stk::io::put_io_part_attribute(*partVec[0]);
#endif

  // NodeSets
  for (int i=0; i<nsNames.size(); i++) {
    std::string nsn = nsNames[i];
    nsPartVec[nsn] = & metaData->declare_part(nsn, stk::mesh::Node );
#ifdef ALBANY_IOSS
    stk::io::put_io_part_attribute(*nsPartVec[nsn]);
#endif
  }
}


Albany::GenericSTKMeshStruct::~GenericSTKMeshStruct()
{
  delete metaData;
  delete bulkData;
}


Teuchos::RCP<Teuchos::ParameterList>
Albany::GenericSTKMeshStruct::getValidGenericSTKParameters(std::string listname) const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL = rcp(new Teuchos::ParameterList(listname));;
  validPL->set<string>("Cell Topology", "Quad" , "Quad or Tri Cell Topology");
  validPL->set<std::string>("Exodus Output File Name", "",
    "Request exodus output to given file name. Requires IOSS build");
  validPL->set<std::string>("Method", "",
    "The discretization method, parsed in the Discretization Factory");
  validPL->set<int>("Cubature Degree", 3, "Integration order sent to Intrepid");

  return validPL;
}
