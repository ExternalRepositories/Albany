//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Albany_RandomFracture.hpp"
#include "Albany_RandomCriterion.hpp"
#include "Teuchos_TimeMonitor.hpp"
#include <stk_util/parallel/ParallelReduce.hpp>

#include <boost/foreach.hpp>

using stk::mesh::EntityKey;
using stk::mesh::Entity;

namespace Albany {

  typedef stk::mesh::Entity Entity;
  typedef stk::mesh::EntityRank EntityRank;
  typedef stk::mesh::RelationIdentifier EdgeId;
  typedef stk::mesh::EntityKey EntityKey;

  //----------------------------------------------------------------------------
  Albany::RandomFracture::
  RandomFracture(const Teuchos::RCP<Teuchos::ParameterList>& params,
                 const Teuchos::RCP<ParamLib>& param_lib,
                 Albany::StateManager& state_mgr,
                 const Teuchos::RCP<const Epetra_Comm>& comm) :
    Albany::AbstractAdapter(params, param_lib, state_mgr, comm),

    remesh_file_index_(1),
    fracture_interval_(params->get<int>("Adaptivity Step Interval", 1)),
    fracture_probability_(params->get<double>("Fracture Probability", 1.0))
  {

    discretization_ = state_mgr_.getDiscretization();

    stk_discretization_ = 
      static_cast<Albany::STKDiscretization *>(discretization_.get());

    stk_mesh_struct_ = stk_discretization_->getSTKMeshStruct();

    bulk_data_ = stk_mesh_struct_->bulkData;
    meta_data_ = stk_mesh_struct_->metaData;

    // The entity ranks
    node_rank_ = meta_data_->NODE_RANK;
    edge_rank_ = meta_data_->EDGE_RANK;
    face_rank_ = meta_data_->FACE_RANK;
    element_rank_ = meta_data_->element_rank();

    fracture_criterion_ =
      Teuchos::rcp(new LCM::RandomCriterion(num_dim_, 
                                            element_rank_,
                                            *stk_discretization_));

    num_dim_ = stk_mesh_struct_->numDim;

    // Save the initial output file name
    base_exo_filename_ = stk_mesh_struct_->exoOutFile;

<<<<<<< HEAD
}

Albany::RandomFracture::
~RandomFracture()
{
}

bool
Albany::RandomFracture::queryAdaptationCriteria()
{
  // iter is a member variable elsewhere, NOX::Epetra::AdaptManager.H
  if ( iter % fracture_interval_ == 0) {

    // Get a vector containing the face set of the mesh where
    // fractures can occur
    std::vector<Entity*> face_list;

// Get the faces owned by this processor
    stk::mesh::Selector select_owned = metaData->locally_owned_part();


    // get all the faces owned by this processor
    stk::mesh::get_selected_entities( select_owned,
				    bulkData->buckets( numDim - 1 ) ,
				    face_list );


#ifdef ALBANY_VERBOSE
    std::cout << "Num faces owned by PE " << bulkData->parallel_rank() << " is: " << face_list.size() << std::endl;
#endif

    // keep count of total fractured faces
    int total_fractured;

=======
    fracture_criterion_ =
      Teuchos::rcp(new LCM::RandomCriterion(num_dim_, 
                                            element_rank_,
                                            *stk_discretization_));

    // Modified by GAH from LCM::NodeUpdate.cc
    topology_ =
      Teuchos::rcp(new LCM::Topology(discretization_, fracture_criterion_));


  }
>>>>>>> LCM and Adapt: changes stemming from code review to Topology class

  //----------------------------------------------------------------------------
  Albany::RandomFracture::
  ~RandomFracture()
  {
  }

<<<<<<< HEAD
      Entity & face = *(face_list[i]);

      if(sfcriterion->fracture_criterion(face, fracture_probability_)) {
        fractured_edges.push_back(face_list[i]);
      }
    }

    if((total_fractured = accumulateFractured(fractured_edges.size())) == 0) {
=======
  //----------------------------------------------------------------------------
  bool
  Albany::RandomFracture::queryAdaptationCriteria()
  {
    // iter is a member variable elsewhere, NOX::Epetra::AdaptManager.H
    if ( iter % fracture_interval_ == 0) {

      // Get a vector containing the face set of the mesh where
      // fractures can occur
      std::vector<stk::mesh::Entity*> face_list;
      stk::mesh::get_entities(*bulk_data_, num_dim_-1, face_list);

#ifdef ALBANY_VERBOSE
      std::cout << "Num faces : " << face_list.size() << std::endl;
#endif
>>>>>>> LCM and Adapt: changes stemming from code review to Topology class

      // keep count of total fractured faces
      int total_fractured;

      // Iterate over the boundary entities
      for (int i(0); i < face_list.size(); ++i){

        stk::mesh::Entity& face = *(face_list[i]);

        if(fracture_criterion_->
           computeFractureCriterion(face, fracture_probability_)) {
          fractured_faces_.push_back(face_list[i]);
        }
      }

<<<<<<< HEAD
    *out << "RandomFractureification: Need to split \"" 
              << total_fractured << "\" mesh elements." << std::endl;
=======
      // if(fractured_edges.size() == 0) return false; // nothing to
      // do
      if ( (total_fractured = 
            accumulateFractured(fractured_faces_.size())) == 0) {
>>>>>>> LCM and Adapt: changes stemming from code review to Topology class

        fractured_faces_.clear();

        return false; // nothing to do
      }

      std::cout << "RandomFractureification: Need to split \"" 
        //    *out << "RandomFractureification: Need to split \"" 
                << total_fractured << "\" mesh elements." << std::endl;

      return true;
    }
    return false; 
   }

  //----------------------------------------------------------------------------
  bool
  Albany::RandomFracture::adaptMesh(){

<<<<<<< HEAD
  *out << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
  *out << "Adapting mesh using Albany::RandomFracture method   " << std::endl;
  *out << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
=======
    std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
    std::cout << "Adapting mesh using Albany::RandomFracture method   " << std::endl;
    std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;

    // Save the current element to node connectivity for solution
    // transfer purposes

    old_elem_to_node_.clear();
    new_elem_to_node_.clear();

    // buildElemToNodes(old_elem_to_node_);
>>>>>>> LCM and Adapt: changes stemming from code review to Topology class

    // Save the current results and close the exodus file

    // Create a remeshed output file naming convention by adding the
    // remeshFileIndex ahead of the period
    std::ostringstream ss;
    std::string str = base_exo_filename_;
    ss << "_" << remesh_file_index_ << ".";
    str.replace(str.find('.'), 1, ss.str());

<<<<<<< HEAD
  *out << "Remeshing: renaming output file to - " << str << endl;
=======
    std::cout << "Remeshing: renaming output file to - " << str << endl;
>>>>>>> LCM and Adapt: changes stemming from code review to Topology class

    // Open the new exodus file for results
    stk_discretization_->reNameExodusOutput(str);

    // increment name index
    remesh_file_index_++;

<<<<<<< HEAD
  // perform topology operations
  topology->remove_element_to_node_relations();

  // Check for failure criterion

  std::map<EntityKey, bool> local_entity_open;
  std::map<EntityKey, bool> global_entity_open;
  topology->set_entities_open(fractured_edges, local_entity_open);

  getGlobalOpenList(local_entity_open, global_entity_open);

  // begin mesh update

  topology->fracture_boundary(global_entity_open);
=======
    // perform topology operations
    topology_->removeElementToNodeConnectivity(old_elem_to_node_);

    // Check for failure criterion
    std::map<stk::mesh::EntityKey, bool> entity_open;
    topology_->setEntitiesOpen(fractured_faces_, entity_open);

    // begin mesh update
    bulk_data_->modification_begin();

    // FIXME parallel bug lies in here
    topology_->splitOpenFaces(entity_open, old_elem_to_node_, new_elem_to_node_);
>>>>>>> LCM and Adapt: changes stemming from code review to Topology class

    // Clear the list of fractured faces in preparation for the next
    // fracture event
    fractured_faces_.clear();

<<<<<<< HEAD
  // Recreates connectivity in stk mesh expected by
  // Albany_STKDiscretization Must be called each time at conclusion
  // of mesh modification
  topology->restore_element_to_node_relations();

showTopLevelRelations();
  // end mesh update
=======
    // Recreates connectivity in stk mesh expected by
    // Albany_STKDiscretization Must be called each time at conclusion
    // of mesh modification
    topology_->restoreElementToNodeConnectivity(new_elem_to_node_);

    // end mesh update
    bulk_data_->modification_end();
>>>>>>> LCM and Adapt: changes stemming from code review to Topology class

    // Throw away all the Albany data structures and re-build them from
    // the mesh
    stk_discretization_->updateMesh();

<<<<<<< HEAD
  *out << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
  *out << "Completed mesh adaptation                           " << std::endl;
  *out << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;

  return true;
=======
    return true;
>>>>>>> LCM and Adapt: changes stemming from code review to Topology class

  }

<<<<<<< HEAD
//! Transfer solution between meshes.
// THis is a no-op as the solution is copied to the newly created nodes by the topology->fracture_boundary() function.
void
Albany::RandomFracture::
solutionTransfer(const Epetra_Vector& oldSolution,
                 Epetra_Vector& newSolution){}


Teuchos::RCP<const Teuchos::ParameterList>
Albany::RandomFracture::getValidAdapterParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> validPL =
    this->getGenericAdapterParams("ValidRandomFractureificationParams");

  validPL->set<double>("Fracture Probability", 
                       1.0, 
                       "Probability of fracture");
  validPL->set<double>("Adaptivity Step Interval", 
                       1, 
                       "Interval to check for fracture");

  return validPL;
}

void
Albany::RandomFracture::showTopLevelRelations(){

  std::vector<Entity*> element_lst;
  stk::mesh::get_entities(*(bulkData),elementRank,element_lst);

  // Remove extra relations from element
  for (int i = 0; i < element_lst.size(); ++i){
    Entity & element = *(element_lst[i]);
    stk::mesh::PairIterRelation relations = element.relations();
    std::cout << "Entitiy " << element.identifier() << " relations are :" << std::endl;

    for (int j = 0; j < relations.size(); ++j){
      cout << "entity:\t" << relations[j].entity()->identifier() << ","
           << relations[j].entity()->entity_rank() << "\tlocal id: "
           << relations[j].identifier() << "\n";
    }
  }
}

void
Albany::RandomFracture::showRelations(){
=======
  //----------------------------------------------------------------------------
  //
  // Transfer solution between meshes.
  //
  void
  Albany::RandomFracture::
  solutionTransfer(const Epetra_Vector& oldSolution,
                   Epetra_Vector& newSolution){

    // Note: This code assumes that the number of elements and their
    // relationships between the old and new meshes do not change!

    // Logic: When we split an edge(s), the number of elements are
    // unchanged. On the two elements that share the split edge, the
    // node numbers along the split edge change, as does the location of
    // the "physics" in the solution vector for these nodes. Here, we
    // loop over the elements in the old mesh, and copy the "physics" at
    // the nodes to the proper locations for the element's new nodes in
    // the new mesh.

    int neq = (discretization_->getWsElNodeEqID())[0][0][0].size();

    for(int elem(0); elem < old_elem_to_node_.size(); ++elem) {

      for(int node(0); node < old_elem_to_node_[elem].size(); ++node) {

        int onode = old_elem_to_node_[elem][node]->identifier() - 1;
        int nnode = new_elem_to_node_[elem][node]->identifier() - 1;

        for(int eq(0); eq < neq; ++eq) {
        
          newSolution[nnode * neq + eq] =
            oldSolution[onode * neq + eq];
        }
      }
    }
  }

  //----------------------------------------------------------------------------
  Teuchos::RCP<const Teuchos::ParameterList>
  Albany::RandomFracture::getValidAdapterParameters() const
  {
    Teuchos::RCP<Teuchos::ParameterList> validPL =
      this->getGenericAdapterParams("ValidRandomFractureificationParams");

    validPL->set<double>("Fracture Probability", 
                         1.0, 
                         "Probability of fracture");
    validPL->set<double>("Adaptivity Step Interval", 
                         1, 
                         "Interval to check for fracture");

    return validPL;
  }

  //----------------------------------------------------------------------------
  void
  Albany::RandomFracture::showElemToNodes(){

    // Create a list of element entities
    std::vector<Entity*> element_lst;
    stk::mesh::get_entities(*(bulk_data_), element_rank_, element_lst);

    // Loop over the elements
    for (int i = 0; i < element_lst.size(); ++i){
      stk::mesh::PairIterRelation relations = element_lst[i]->relations(node_rank_);
      cout << "Nodes of Element " << element_lst[i]->identifier() - 1 << "\n";

      for (int j = 0; j < relations.size(); ++j){
        Entity& node = *(relations[j].entity());
        cout << ":"  << node.identifier() - 1;
      }
      cout << ":\n";
    }

    //topology::disp_relation(*(element_lst[0]));

    //std::vector<Entity*> face_lst;
    //stk::mesh::get_entities(*(bulk_data__),element_rank_-1,face_lst);
    //topology::disp_relation(*(face_lst[1]));

    return;
  }

  /*
    void
    Albany::RandomFracture::buildElemToNodes(std::vector<std::vector<int> >& connectivity){

    // Create a list of element entities
    std::vector<Entity*> element_lst;
    stk::mesh::get_entities(*(bulk_data_), element_rank_, element_lst);

    // Allocate storage for the elements

    connectivity.resize(element_lst.size());
>>>>>>> LCM and Adapt: changes stemming from code review to Topology class

    // Loop over the elements
    for (int i = 0; i < element_lst.size(); ++i){
    stk::mesh::PairIterRelation relations = element_lst[i]->relations(node_rank_);
    int element = element_lst[i]->identifier() - 1;

<<<<<<< HEAD
  // Remove extra relations from element
  for (int i = 0; i < element_lst.size(); ++i){
    Entity & element = *(element_lst[i]);
    showRelations(0, element);
  }
}

// Recursive print function
void
Albany::RandomFracture::showRelations(int level, const Entity& ent){

    stk::mesh::PairIterRelation relations = ent.relations();

    for(int i = 0; i < level; i++)
      std::cout << "     ";

    std::cout << metaData->entity_rank_name( ent.entity_rank()) <<
          " " << ent.identifier() << " relations are :" << std::endl;

    for (int j = 0; j < relations.size(); ++j){
      for(int i = 0; i < level; i++)
        std::cout << "     ";
      cout << "  " << metaData->entity_rank_name( relations[j].entity()->entity_rank()) << ":\t" 
           << relations[j].entity()->identifier() << ","
           << relations[j].entity()->entity_rank() << "\tlocal id: "
           << relations[j].identifier() << "\n";
=======
    // make room to hold the node ids
    connectivity[element].resize(relations.size());

    for (int j = 0; j < relations.size(); ++j){
    Entity& node = *(relations[j].entity());
    connectivity[element][j] = node.identifier() - 1;
    }
    }
    return;
    }
  */

  //----------------------------------------------------------------------------
  void
  Albany::RandomFracture::showRelations(){

    std::vector<Entity*> element_lst;
    stk::mesh::get_entities(*(bulk_data_),element_rank_,element_lst);

    // Remove extra relations from element
    for (int i = 0; i < element_lst.size(); ++i){
      Entity & element = *(element_lst[i]);
      stk::mesh::PairIterRelation relations = element.relations();
      std::cout << "Element "
                << element_lst[i]->identifier()
                <<" relations are :" << std::endl;

      for (int j = 0; j < relations.size(); ++j){
        cout << "entity:\t" << relations[j].entity()->identifier() << ","
             << relations[j].entity()->entity_rank() << "\tlocal id: "
             << relations[j].identifier() << "\n";
      }
>>>>>>> LCM and Adapt: changes stemming from code review to Topology class
    }
    for (int j = 0; j < relations.size(); ++j){
      if(relations[j].entity()->entity_rank() <= ent.entity_rank())
        showRelations(level + 1, *relations[j].entity());
  }


#ifdef ALBANY_MPI
  //----------------------------------------------------------------------------
  int
  Albany::RandomFracture::accumulateFractured(int num_fractured){

<<<<<<< HEAD
  int total_fractured;

  stk::all_reduce_sum(bulkData->parallel(), &num_fractured, &total_fractured, 1);

  return total_fractured;
}

// Parallel all-gatherv function. Communicates local open list to all processors to form global open list.
void 
Albany::RandomFracture::getGlobalOpenList( std::map<EntityKey, bool>& local_entity_open,  
        std::map<EntityKey, bool>& global_entity_open){

   // Make certain that we can send keys as MPI_UINT64_T types
   assert(sizeof(EntityKey::raw_key_type) >= sizeof(uint64_t));

   const unsigned parallel_size = bulkData->parallel_size();

// Build local vector of keys
   std::pair<EntityKey,bool> me; // what a map<EntityKey, bool> is made of
   std::vector<EntityKey::raw_key_type> v;     // local vector of open keys

   BOOST_FOREACH(me, local_entity_open) {
       v.push_back(me.first.raw_key());

// Debugging
/*
      const unsigned entity_rank = stk::mesh::entity_rank( me.first);
      const stk::mesh::EntityId entity_id = stk::mesh::entity_id( me.first );
      const std::string & entity_rank_name = metaData->entity_rank_name( entity_rank );
      Entity *entity = bulkData->get_entity(me.first);
      std::cout<<"Single proc fracture list contains "<<" "<<entity_rank_name<<" ["<<entity_id<<"] Proc:"
         <<entity->owner_rank() <<std::endl;
*/

   }

   int num_open_on_pe = v.size();

// Perform the allgatherv

   // gather the number of open entities on each processor
   int *sizes = new int[parallel_size];
   MPI_Allgather(&num_open_on_pe, 1, MPI_INT, sizes, 1, MPI_INT, bulkData->parallel()); 

   // Loop over each processor and calculate the array offset of its entities in the receive array
   int *offsets = new int[parallel_size];
   int count = 0; 

   for (int i = 0; i < parallel_size; i++){
     offsets[i] = count; 
     count += sizes[i];
   } 

   int total_number_of_open_entities = count;

   EntityKey::raw_key_type *result_array = new EntityKey::raw_key_type[total_number_of_open_entities];
   MPI_Allgatherv(&v[0], num_open_on_pe, MPI_UINT64_T, result_array, 
       sizes, offsets, MPI_UINT64_T, bulkData->parallel());

   // Save the global keys
   for(int i = 0; i < total_number_of_open_entities; i++){

      EntityKey key = EntityKey(&result_array[i]);
      global_entity_open[key] = true;

// Debugging
      const unsigned entity_rank = stk::mesh::entity_rank( key);
      const stk::mesh::EntityId entity_id = stk::mesh::entity_id( key );
      const std::string & entity_rank_name = metaData->entity_rank_name( entity_rank );
      Entity *entity = bulkData->get_entity(key);
      std::cout<<"Global proc fracture list contains "<<" "<<entity_rank_name<<" ["<<entity_id<<"] Proc:"
         <<entity->owner_rank() <<std::endl;
    }

   delete [] sizes;
   delete [] offsets;
   delete [] result_array;
}

#else
int
Albany::RandomFracture::accumulateFractured(int num_fractured){
  return num_fractured;
}

// Parallel all-gatherv function. Communicates local open list to all processors to form global open list.
void 
Albany::RandomFracture::getGlobalOpenList( std::map<EntityKey, bool>& local_entity_open,  
        std::map<EntityKey, bool>& global_entity_open){

   global_entity_open = local_entity_open;
}
=======
    int total_fractured;
    const Albany_MPI_Comm& mpi_comm =
      Albany::getMpiCommFromEpetraComm(*epetra_comm_);

    MPI_Allreduce(&num_fractured,
                  &total_fractured,
                  1,
                  MPI_INT,
                  MPI_SUM,
                  mpi_comm);

    return total_fractured;
  }
#else
  //----------------------------------------------------------------------------
  int
  Albany::RandomFracture::accumulateFractured(int num_fractured){
    return num_fractured;
  }
>>>>>>> LCM and Adapt: changes stemming from code review to Topology class
#endif
  //----------------------------------------------------------------------------
}
