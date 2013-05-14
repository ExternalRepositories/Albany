//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#ifdef ALBANY_SEACAS

#include <iostream>

#include "Albany_IossSTKMeshStruct.hpp"
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

Albany::IossSTKMeshStruct::IossSTKMeshStruct(
                                             const Teuchos::RCP<Teuchos::ParameterList>& params, 
                                             const Teuchos::RCP<Teuchos::ParameterList>& adaptParams_, 
                                             const Teuchos::RCP<const Epetra_Comm>& comm) :
  GenericSTKMeshStruct(params, adaptParams_),
  out(Teuchos::VerboseObjectBase::getDefaultOStream()),
  useSerialMesh(false),
  periodic(params->get("Periodic BC", false)),
  m_hasRestartSolution(false),
  m_restartDataTime(-1.0),
  m_solutionFieldHistoryDepth(0)
{
  params->validateParameters(*getValidDiscretizationParameters(),0);

  mesh_data = new stk::io::MeshData();

  usePamgen = (params->get("Method","Exodus") == "Pamgen");

  std::vector<std::string> entity_rank_names;

  // eMesh needs "FAMILY_TREE" entity
  if(buildEMesh)
    entity_rank_names.push_back("FAMILY_TREE");

#ifdef ALBANY_ZOLTAN  // rebalance requires Zoltan

  if (params->get<bool>("Use Serial Mesh", false) && comm->NumProc() > 1){ 
    // We are parallel but reading a single exodus file

    useSerialMesh = true;

    readSerialMesh(comm, entity_rank_names);

  }
  else 
#endif
    if (!usePamgen) {
      *out << "Albany_IOSS: Loading STKMesh from Exodus file  " 
           << params->get<string>("Exodus Input File Name") << endl;

//      stk::io::create_input_mesh("exodusii",
      create_input_mesh("exodusii",
                                 params->get<string>("Exodus Input File Name"),
                                 Albany::getMpiCommFromEpetraComm(*comm), 
                                 *metaData, *mesh_data,
                                 entity_rank_names); 
    }
    else {
      *out << "Albany_IOSS: Loading STKMesh from Pamgen file  " 
           << params->get<string>("Pamgen Input File Name") << endl;

//      stk::io::create_input_mesh("pamgen",
      create_input_mesh("pamgen",
                                 params->get<string>("Pamgen Input File Name"),
                                 Albany::getMpiCommFromEpetraComm(*comm), 
                                 *metaData, *mesh_data,
                                 entity_rank_names); 

    }

  numDim = metaData->spatial_dimension();

  stk::io::put_io_part_attribute(metaData->universal_part());

  // Set element blocks, side sets and node sets
  const stk::mesh::PartVector & all_parts = metaData->get_parts();
  std::vector<std::string> ssNames;
  std::vector<std::string> nsNames;
  int numEB = 0;

  for (stk::mesh::PartVector::const_iterator i = all_parts.begin();
       i != all_parts.end(); ++i) {

    stk::mesh::Part * const part = *i ;

    if ( part->primary_entity_rank() == metaData->element_rank()) {
      if (part->name()[0] != '{') {
        //*out << "IOSS-STK: Element part \"" << part->name() << "\" found " << endl;
        partVec[numEB] = part;
        numEB++;
      }
    }
    else if ( part->primary_entity_rank() == metaData->node_rank()) {
      if (part->name()[0] != '{') {
        //*out << "Mesh has Node Set ID: " << part->name() << endl;
        nsPartVec[part->name()]=part;
        nsNames.push_back(part->name());
      }
    }
    else if ( part->primary_entity_rank() == metaData->side_rank()) {
      if (part->name()[0] != '{') {
        print(*out, "Found side_rank entity:\n", *part);
        ssPartVec[part->name()]=part;
      }
    }
  }

  cullSubsetParts(ssNames, ssPartVec); // Eliminate sidesets that are subsets of other sidesets

#if 0
  // for debugging, print out the parts now
  std::map<std::string, stk::mesh::Part*>::iterator it;

  for(it = ssPartVec.begin(); it != ssPartVec.end(); ++it){ // loop over the parts in the map

    // for each part in turn, get the name of parts that are a subset of it

    print(*out, "Found \n", *it->second);
  }
  // end debugging
#endif

  int cub = params->get("Cubature Degree",3);
  int worksetSizeMax = params->get("Workset Size",50);

  // Get number of elements per element block using Ioss for use
  // in calculating an upper bound on the worksetSize.
  std::vector<int> el_blocks;
  stk::io::get_element_block_sizes(*mesh_data, el_blocks);
  TEUCHOS_TEST_FOR_EXCEPT(el_blocks.size() != partVec.size());

  int ebSizeMax =  *std::max_element(el_blocks.begin(), el_blocks.end());
  int worksetSize = this->computeWorksetSize(worksetSizeMax, ebSizeMax);

  // Build a map to get the EB name given the index

  for (int eb=0; eb<numEB; eb++) 

    this->ebNameToIndex[partVec[eb]->name()] = eb;

  // Construct MeshSpecsStruct
  if (!params->get("Separate Evaluators by Element Block",false)) {

    const CellTopologyData& ctd = *metaData->get_cell_topology(*partVec[0]).getCellTopologyData();
    this->meshSpecs[0] = Teuchos::rcp(new Albany::MeshSpecsStruct(ctd, numDim, cub,
                                                                  nsNames, ssNames, worksetSize, partVec[0]->name(), 
                                                                  this->ebNameToIndex, this->interleavedOrdering));

  }
  else {

    *out << "MULTIPLE Elem Block in Ioss: DO worksetSize[eb] max?? " << endl; 
    this->allElementBlocksHaveSamePhysics=false;
    this->meshSpecs.resize(numEB);
    for (int eb=0; eb<numEB; eb++) {
      const CellTopologyData& ctd = *metaData->get_cell_topology(*partVec[eb]).getCellTopologyData();
      this->meshSpecs[eb] = Teuchos::rcp(new Albany::MeshSpecsStruct(ctd, numDim, cub,
                                                                     nsNames, ssNames, worksetSize, partVec[eb]->name(), 
                                                                     this->ebNameToIndex, this->interleavedOrdering));
      cout << "el_block_size[" << eb << "] = " << el_blocks[eb] << "   name  " << partVec[eb]->name() << endl; 
    }

  }

  {
    const Ioss::Region *inputRegion = mesh_data->m_input_region;
    m_solutionFieldHistoryDepth = inputRegion->get_property("state_count").get_int();
  }
}

Albany::IossSTKMeshStruct::~IossSTKMeshStruct()
{
  delete mesh_data;
}

void
Albany::IossSTKMeshStruct::readSerialMesh(const Teuchos::RCP<const Epetra_Comm>& comm,
                                          std::vector<std::string>& entity_rank_names){

#ifdef ALBANY_ZOLTAN // rebalance needs Zoltan

  MPI_Group group_world;
  MPI_Group peZero;
  MPI_Comm peZeroComm;

  // Read a single exodus mesh on Proc 0 then rebalance it across the machine

  MPI_Comm theComm = Albany::getMpiCommFromEpetraComm(*comm);

  int process_rank[1]; // the reader process

  process_rank[0] = 0;
  int my_rank;

  //get the group under theComm
  MPI_Comm_group(theComm, &group_world);
  // create the new group 
  MPI_Group_incl(group_world, 1, process_rank, &peZero);
  // create the new communicator 
  MPI_Comm_create(theComm, peZero, &peZeroComm);

  // Who am i?
  MPI_Comm_rank(peZeroComm, &my_rank);

  if(my_rank == 0){

    *out << "Albany_IOSS: Loading serial STKMesh from Exodus file  " 
         << params->get<string>("Exodus Input File Name") << endl;

  }

  /* 
   * This checks the existence of the file, checks to see if we can open it, builds a handle to the region
   * and puts it in mesh_data (in_region), and reads the metaData into metaData.
   */

//  stk::io::create_input_mesh("exodusii",
  create_input_mesh("exodusii",
                             params->get<string>("Exodus Input File Name"), 
                             peZeroComm, 
                             *metaData, *mesh_data,
                             entity_rank_names); 

  // Here, all PEs have read the metaData from the input file, and have a pointer to in_region in mesh_data

#endif

}

void
Albany::IossSTKMeshStruct::setFieldAndBulkData(
                                               const Teuchos::RCP<const Epetra_Comm>& comm,
                                               const Teuchos::RCP<Teuchos::ParameterList>& params,
                                               const unsigned int neq_,
                                               const AbstractFieldContainer::FieldContainerRequirements& req,
                                               const Teuchos::RCP<Albany::StateInfoStruct>& sis,
                                               const unsigned int worksetSize)
{
  this->SetupFieldData(comm, neq_, req, sis, worksetSize);

  *out << "IOSS-STK: number of node sets = " << nsPartVec.size() << endl;
  *out << "IOSS-STK: number of side sets = " << ssPartVec.size() << endl;

  metaData->commit();

  // Restart index to read solution from exodus file.
  int index = params->get("Restart Index",-1); // Default to no restart
  double res_time = params->get<double>("Restart Time",-1.0); // Default to no restart
  Ioss::Region *region = mesh_data->m_input_region;

  /*
   * The following code block reads a single mesh on PE 0, then distributes the mesh across
   * the other processors. stk_rebalance is used, which requires Zoltan
   *
   * This code is only compiled if ALBANY_MPI and ALBANY_ZOLTAN are true
   */

#ifdef ALBANY_ZOLTAN // rebalance needs Zoltan

  if(useSerialMesh){

    bulkData->modification_begin();

    if(comm->MyPID() == 0){ // read in the mesh on PE 0

      stk::io::process_mesh_bulk_data(region, *bulkData);

      // Read solution from exodus file.
      if (index >= 0) { // User has specified a time step to restart at
        *out << "Restart Index set, reading solution index : " << index << endl;
        stk::io::input_mesh_fields(region, *bulkData, index);
        m_restartDataTime = region->get_state_time(index);
        m_hasRestartSolution = true;
      }
      else if (res_time >= 0) { // User has specified a time to restart at
        *out << "Restart solution time set, reading solution time : " << res_time << endl;
        stk::io::input_mesh_fields(region, *bulkData, res_time);
        m_restartDataTime = res_time;
        m_hasRestartSolution = true;
      }
      else {

        *out << "Neither restart index or time are set. Not reading solution data from exodus file"<< endl;

      }
    }

    bulkData->modification_end();

  } // End UseSerialMesh - reading mesh on PE 0

  else 
#endif

    /*
     * The following code block reads a single mesh when Albany is compiled serially, or a
     * Nemspread fileset if ALBANY_MPI is true.
     *
     */

  { // running in Serial or Parallel read from Nemspread files

    stk::io::populate_bulk_data(*bulkData, *mesh_data);

    if (!usePamgen)  {

      // Read solution from exodus file.
      if (index >= 0) { // User has specified a time step to restart at
        *out << "Restart Index set, reading solution index : " << index << endl;
        stk::io::process_input_request(*mesh_data, *bulkData, index);
        m_restartDataTime = region->get_state_time(index);
        m_hasRestartSolution = true;
      }
      else if (res_time >= 0) { // User has specified a time to restart at
        *out << "Restart solution time set, reading solution time : " << res_time << endl;
        stk::io::process_input_request(*mesh_data, *bulkData, res_time);
        m_restartDataTime = res_time;
        m_hasRestartSolution = true;
      }
      else {
        *out << "Restart Index not set. Not reading solution from exodus (" 
             << index << ")"<< endl;

      }
    }

    bulkData->modification_end();

  } // End Parallel Read - or running in serial

  if(m_hasRestartSolution){

    Teuchos::Array<std::string> default_field;
    default_field.push_back("solution");
    Teuchos::Array<std::string> restart_fields =
      params->get<Teuchos::Array<std::string> >("Restart Fields", default_field);

    // Get the fields to be used for restart

    // See what state data was initialized from the stk::io request
    // This should be propagated into stk::io
    const Ioss::ElementBlockContainer& elem_blocks = region->get_element_blocks();

    /*
    // Uncomment to print what fields are in the exodus file
    Ioss::NameList exo_fld_names;
    elem_blocks[0]->field_describe(&exo_fld_names);
    for(std::size_t i = 0; i < exo_fld_names.size(); i++){
    *out << "Found field \"" << exo_fld_names[i] << "\" in exodus file" << std::endl;
    }
    */

    for (std::size_t i=0; i<sis->size(); i++) {
      Albany::StateStruct& st = *((*sis)[i]);

      if(elem_blocks[0]->field_exists(st.name))

        for(std::size_t j = 0; j < restart_fields.size(); j++)

          if(boost::iequals(st.name, restart_fields[j])){

            *out << "Restarting from field \"" << st.name << "\" found in exodus file." << std::endl;
            st.restartDataAvailable = true;
            break;

          }
    }
  }

//  coordinates_field = metaData->get_field<VectorFieldType>(std::string("coordinates"));
//  proc_rank_field = metaData->get_field<IntScalarFieldType>(std::string("proc_rank"));
//#ifdef ALBANY_FELIX
//  surfaceHeight_field = metaData->get_field<ScalarFieldType>(std::string("surface height"));
//#endif

  // Refine the mesh before starting the simulation if indicated
  uniformRefineMesh(comm);

  // Rebalance the mesh before starting the simulation if indicated
  rebalanceMesh(comm);

  // Build additional mesh connectivity needed for mesh fracture (if indicated)
  computeAddlConnectivity();

}

void
Albany::IossSTKMeshStruct::loadSolutionFieldHistory(int step)
{
  TEUCHOS_TEST_FOR_EXCEPT(step < 0 || step >= m_solutionFieldHistoryDepth);

  const int index = step + 1; // 1-based step indexing
  stk::io::process_input_request(*mesh_data, *bulkData, index);
}

// ========================================================================
static void process_surface_entity(Ioss::SideSet *sset, stk::mesh::fem::FEMMetaData &fem_meta)
{
  assert(sset->type() == Ioss::SIDESET);
  const Ioss::SideBlockContainer& blocks = sset->get_side_blocks();
  stk::io::default_part_processing(blocks, fem_meta);

  stk::mesh::Part* const ss_part = fem_meta.get_part(sset->name());
  assert(ss_part != NULL);

  stk::mesh::Field<double, stk::mesh::ElementNode> *distribution_factors_field = NULL;
  bool surface_df_defined = false; // Has the surface df field been defined yet?

  size_t block_count = sset->block_count();
  for (size_t i=0; i < block_count; i++) {
    Ioss::SideBlock *sb = sset->get_block(i);
    if (stk::io::include_entity(sb)) {
      stk::mesh::Part * const sb_part = fem_meta.get_part(sb->name());
      assert(sb_part != NULL);
      fem_meta.declare_part_subset(*ss_part, *sb_part);

      if (sb->field_exists("distribution_factors")) {
        if (!surface_df_defined) {
          std::string field_name = sset->name() + "_df";
          distribution_factors_field =
            &fem_meta.declare_field<stk::mesh::Field<double, stk::mesh::ElementNode> >(field_name);
          stk::io::set_field_role(*distribution_factors_field, Ioss::Field::MESH);
          stk::io::set_distribution_factor_field(*ss_part, *distribution_factors_field);
          surface_df_defined = true;
        }
        stk::io::set_distribution_factor_field(*sb_part, *distribution_factors_field);
        int side_node_count = sb->topology()->number_nodes();
        stk::mesh::put_field(*distribution_factors_field,
                             stk::io::part_primary_entity_rank(*sb_part),
                             *sb_part, side_node_count);
      }
    }
  }
}

static void process_elementblocks(Ioss::Region &region, stk::mesh::fem::FEMMetaData &fem_meta)
{
  const Ioss::ElementBlockContainer& elem_blocks = region.get_element_blocks();
  stk::io::default_part_processing(elem_blocks, fem_meta);
}

static void process_nodesets(Ioss::Region &region, stk::mesh::fem::FEMMetaData &fem_meta)
{
  const Ioss::NodeSetContainer& node_sets = region.get_nodesets();
  stk::io::default_part_processing(node_sets, fem_meta);

  stk::mesh::Field<double> & distribution_factors_field =
    fem_meta.declare_field<stk::mesh::Field<double> >("distribution_factors");
  stk::io::set_field_role(distribution_factors_field, Ioss::Field::MESH);

  /** \todo REFACTOR How to associate distribution_factors field
   * with the nodeset part if a node is a member of multiple
   * nodesets
   */

  for(Ioss::NodeSetContainer::const_iterator it = node_sets.begin();
      it != node_sets.end(); ++it) {
    Ioss::NodeSet *entity = *it;

    if (stk::io::include_entity(entity)) {
      stk::mesh::Part* const part = fem_meta.get_part(entity->name());
      assert(part != NULL);
      assert(entity->field_exists("distribution_factors"));

      stk::mesh::put_field(distribution_factors_field, fem_meta.node_rank(), *part);
    }
  }
}

// ========================================================================
// ========================================================================
static void process_sidesets(Ioss::Region &region, stk::mesh::fem::FEMMetaData &fem_meta)
{
  const Ioss::SideSetContainer& side_sets = region.get_sidesets();
  stk::io::default_part_processing(side_sets, fem_meta);

  for(Ioss::SideSetContainer::const_iterator it = side_sets.begin();
      it != side_sets.end(); ++it) {
    Ioss::SideSet *entity = *it;

    if (stk::io::include_entity(entity)) {
      process_surface_entity(entity, fem_meta);
    }
  }
}

static void process_nodeblocks(Ioss::Region &region, stk::mesh::fem::FEMMetaData &fem_meta)
{
  const Ioss::NodeBlockContainer& node_blocks = region.get_node_blocks();
  assert(node_blocks.size() == 1);

  Ioss::NodeBlock *nb = node_blocks[0];

  assert(nb->field_exists("mesh_model_coordinates"));
  Ioss::Field coordinates = nb->get_field("mesh_model_coordinates");
  int spatial_dim = coordinates.transformed_storage()->component_count();

  stk::mesh::Field<double,stk::mesh::Cartesian> & coord_field =
    fem_meta.declare_field<stk::mesh::Field<double,stk::mesh::Cartesian> >("coordinates");

  stk::mesh::put_field( coord_field, fem_meta.node_rank(), fem_meta.universal_part(), spatial_dim);
  stk::io::define_io_fields(nb, Ioss::Field::ATTRIBUTE, fem_meta.universal_part(), 0);
}



void 
Albany::IossSTKMeshStruct::create_input_mesh(const std::string &mesh_type,
                       const std::string &mesh_filename,
                       stk::ParallelMachine comm,
                       stk::mesh::fem::FEMMetaData &fem_meta,
                       stk::io::MeshData &mesh_data,
                       std::vector<std::string>& names_to_add)
{
  Ioss::Region *in_region = mesh_data.m_input_region;
  if (in_region == NULL) {
	// If in_region is NULL, then open the file;
	// If in_region is non-NULL, then user has given us a valid Ioss::Region that
	// should be used.
	Ioss::DatabaseIO *dbi = Ioss::IOFactory::create(mesh_type, mesh_filename,
                                                    Ioss::READ_MODEL, comm,
                                                    mesh_data.m_property_manager);
	if (dbi == NULL || !dbi->ok()) {
	  std::cerr  << "ERROR: Could not open database '" << mesh_filename
                 << "' of type '" << mesh_type << "'\n";
	  Ioss::NameList db_types;
	  Ioss::IOFactory::describe(&db_types);
	  std::cerr << "\nSupported database types:\n\t";
	  for (Ioss::NameList::const_iterator IF = db_types.begin(); IF != db_types.end(); ++IF) {
	    std::cerr << *IF << "  ";
	  }
	  std::cerr << "\n\n";
	}

	// NOTE: 'in_region' owns 'dbi' pointer at this time...
	in_region = new Ioss::Region(dbi, "input_model");
	mesh_data.m_input_region = in_region;
  }

  size_t spatial_dimension = in_region->get_property("spatial_dimension").get_int();
/*
This can go back to stk::io once the entity rank names function below is addressed (GAH)

*/

//  stk::io::initialize_spatial_dimension(fem_meta, spatial_dimension, stk::mesh::fem::entity_rank_names(spatial_dimension));

    std::vector<std::string> entity_rank_names = stk::mesh::fem::entity_rank_names(spatial_dimension);
//    entity_rank_names.push_back("FAMILY_TREE");
    for(int i = 0; i < names_to_add.size(); i++)
      entity_rank_names.push_back(names_to_add[i]);

  stk::io::initialize_spatial_dimension(fem_meta, spatial_dimension, entity_rank_names);

  process_elementblocks(*in_region, fem_meta);
  process_nodeblocks(*in_region,    fem_meta);
  process_sidesets(*in_region,      fem_meta);
  process_nodesets(*in_region,      fem_meta);
}


Teuchos::RCP<const Teuchos::ParameterList>
Albany::IossSTKMeshStruct::getValidDiscretizationParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
    this->getValidGenericSTKParameters("Valid IOSS_DiscParams");
  validPL->set<bool>("Periodic BC", false, "Flag to indicate periodic a mesh");
  validPL->set<string>("Exodus Input File Name", "", "File Name For Exodus Mesh Input");
  validPL->set<string>("Pamgen Input File Name", "", "File Name For Pamgen Mesh Input");
  validPL->set<int>("Restart Index", 1, "Exodus time index to read for inital guess/condition.");
  validPL->set<double>("Restart Time", 1.0, "Exodus solution time to read for inital guess/condition.");
  validPL->set<bool>("Use Serial Mesh", false, "Read in a single mesh on PE 0 and rebalance");

  return validPL;
}
#endif
