//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//


#ifndef ALBANY_STKNODEFIELDCONTAINER_HPP
#define ALBANY_STKNODEFIELDCONTAINER_HPP

#include "Teuchos_RCP.hpp"
#include "Albany_AbstractNodeFieldContainer.hpp"
#include "Albany_StateInfoStruct.hpp"

#include <stk_mesh/fem/FEMMetaData.hpp>
#include <stk_mesh/base/Field.hpp>
#include <stk_mesh/base/FieldTraits.hpp>
#include <stk_mesh/fem/CoordinateSystems.hpp>

namespace Albany {

/*!
 * \brief Abstract interface for an STK NodeField container
 *
 */

class AbstractSTKNodeFieldContainer : public AbstractNodeFieldContainer {

  public:

    AbstractSTKNodeFieldContainer(){}
    virtual ~AbstractSTKNodeFieldContainer(){}

    virtual void saveFieldBlock(const Teuchos::RCP<const Tpetra_BlockMultiVector>& block_mv, 
            int offset) = 0;
    virtual void saveFieldVector(const Teuchos::RCP<const Tpetra_MultiVector>& mv, 
            int offset) = 0;
    virtual Albany::MDArray getMDA(const stk_classic::mesh::Bucket& buck) = 0;

};


Teuchos::RCP<Albany::AbstractNodeFieldContainer>
buildSTKNodeField(const std::string& name, const std::vector<PHX::DataLayout::size_type>& dim,
                    stk_classic::mesh::fem::FEMMetaData* metaData,
                    stk_classic::mesh::BulkData* bulkData, const bool output);


  // Helper class for NodeData
  template<typename DataType, unsigned ArrayDim>
  struct NodeData_Traits { };

  template<typename DataType, unsigned ArrayDim, class traits = NodeData_Traits<DataType, ArrayDim> >
  class STKNodeField : public AbstractSTKNodeFieldContainer {

  public:

    //! Type of traits class being used
    typedef traits traits_type;

    //! Define the field type
    typedef typename traits_type::field_type field_type;


    STKNodeField(const std::string& name, const std::vector<PHX::DataLayout::size_type>& dim,
                 stk_classic::mesh::fem::FEMMetaData* metaData, stk_classic::mesh::BulkData* bulkData,
                 const bool output = false);

    virtual ~STKNodeField(){}

    void saveFieldBlock(const Teuchos::RCP<const Tpetra_BlockMultiVector>& block_mv, int offset);
    void saveFieldVector(const Teuchos::RCP<const Tpetra_MultiVector>& mv, int offset);

    Albany::MDArray getMDA(const stk_classic::mesh::Bucket& buck);

  private:

    std::string name;      // Name of data field
    field_type *node_field;  // stk_classic::mesh::field
    std::vector<PHX::DataLayout::size_type> dims;
    stk_classic::mesh::fem::FEMMetaData* metaData;
    stk_classic::mesh::BulkData* bulkData;

  };

// Explicit template definitions in support of the above

  // Node Scalar
  template <typename T>
  struct NodeData_Traits<T, 1> { 

    enum { size = 1 }; // Three array dimension tags (Node, Dim, Dim), store type T values
    typedef stk_classic::mesh::Field<T> field_type ;
    static field_type* createField(const std::string& name, const std::vector<PHX::DataLayout::size_type>& dim,
                                   stk_classic::mesh::fem::FEMMetaData* metaData){

        field_type *fld = & metaData->declare_field<field_type>(name);
        // Multi-dim order is Fortran Ordering, so reversed here
        stk_classic::mesh::put_field(*fld , metaData->node_rank(), metaData->universal_part());

        return fld; // Address is held by stk

    }

    static void saveFieldData(const Teuchos::RCP<const Tpetra_BlockMultiVector>& overlap_node_vec,
                              const stk_classic::mesh::BucketVector& all_elements,
                              field_type *fld, int offset){

      const Teuchos::RCP<const Tpetra_BlockMap>& overlap_node_map = overlap_node_vec->getBlockMap();
      Teuchos::ArrayRCP<const ST> const_overlap_node_view = overlap_node_vec->get1dView();

      for(stk_classic::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

        const stk_classic::mesh::Bucket& bucket = **it;

        stk_classic::mesh::BucketArray<field_type> solution_array(*fld, bucket);

        const int num_nodes_in_bucket = solution_array.dimension(0);

        for(std::size_t i = 0; i < num_nodes_in_bucket; i++)  {

          const GO global_block_id = bucket[i].identifier() - 1; // global node in mesh
          LO local_block_id = overlap_node_map->getLocalBlockID(global_block_id);
          LO block_start = overlap_node_map->getFirstLocalPointInLocalBlock(local_block_id);

          solution_array(i) = const_overlap_node_view[block_start + offset];

        }
      }
    }

    static void saveFieldData(const Teuchos::RCP<const Tpetra_MultiVector>& overlap_node_vec,
                              const stk_classic::mesh::BucketVector& all_elements,
                              field_type *fld, int offset){

      Teuchos::ArrayRCP<const ST> const_overlap_node_view = overlap_node_vec->getVector(offset)->get1dView();

      for(stk_classic::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

        const stk_classic::mesh::Bucket& bucket = **it;

        stk_classic::mesh::BucketArray<field_type> solution_array(*fld, bucket);

        const int num_nodes_in_bucket = solution_array.dimension(0);

        for(std::size_t i = 0; i < num_nodes_in_bucket; i++)  {

          const GO global_id = bucket[i].identifier() - 1; // global node in mesh

          solution_array(i) = const_overlap_node_view[global_id];

        }
      }
    }

  };

  // Node Vector
  template <typename T>
  struct NodeData_Traits<T, 2> { 

    enum { size = 2 }; // Two array dimension tags (Node, Dim), store type T values
    typedef stk_classic::mesh::Field<T, stk_classic::mesh::Cartesian> field_type ;
    static field_type* createField(const std::string& name, const std::vector<PHX::DataLayout::size_type>& dim,
                                   stk_classic::mesh::fem::FEMMetaData* metaData){

        field_type *fld = & metaData->declare_field<field_type>(name);
        // Multi-dim order is Fortran Ordering, so reversed here
        stk_classic::mesh::put_field(*fld , metaData->node_rank(),
                           metaData->universal_part(), dim[1]);

        return fld; // Address is held by stk

    }

    static void saveFieldData(const Teuchos::RCP<const Tpetra_BlockMultiVector>& overlap_node_vec,
                              const stk_classic::mesh::BucketVector& all_elements,
                              field_type *fld, int offset){

      const Teuchos::RCP<const Tpetra_BlockMap>& overlap_node_map = overlap_node_vec->getBlockMap();
      Teuchos::ArrayRCP<const ST> const_overlap_node_view = overlap_node_vec->get1dView();

      for(stk_classic::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

        const stk_classic::mesh::Bucket& bucket = **it;

        stk_classic::mesh::BucketArray<field_type> solution_array(*fld, bucket);

        const int num_vec_components = solution_array.dimension(0);
        const int num_nodes_in_bucket = solution_array.dimension(1);

        for(std::size_t i = 0; i < num_nodes_in_bucket; i++)  {

          const GO global_block_id = bucket[i].identifier() - 1; // global node in mesh
          LO local_block_id = overlap_node_map->getLocalBlockID(global_block_id);
          LO block_start = overlap_node_map->getFirstLocalPointInLocalBlock(local_block_id);

          for(std::size_t j = 0; j < num_vec_components; j++){

            solution_array(j, i) = const_overlap_node_view[block_start + offset + j];

          }
        }
      }
    }

    static void saveFieldData(const Teuchos::RCP<const Tpetra_MultiVector>& overlap_node_vec,
                              const stk_classic::mesh::BucketVector& all_elements,
                              field_type *fld, int offset){

      for(stk_classic::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

        const stk_classic::mesh::Bucket& bucket = **it;

        stk_classic::mesh::BucketArray<field_type> solution_array(*fld, bucket);

        const int num_vec_components = solution_array.dimension(0);
        const int num_nodes_in_bucket = solution_array.dimension(1);

        for(std::size_t j = 0; j < num_vec_components; j++){

          Teuchos::ArrayRCP<const ST> const_overlap_node_view = overlap_node_vec->getVector(offset + j)->get1dView();

          for(std::size_t i = 0; i < num_nodes_in_bucket; i++)  {

            const GO global_id = bucket[i].identifier() - 1; // global node in mesh

            solution_array(j, i) = const_overlap_node_view[global_id];

          }
        }
      }
    }

  };

  // Node Tensor
  template <typename T>
  struct NodeData_Traits<T, 3> { 

    enum { size = 3 }; // Three array dimension tags (Node, Dim, Dim), store type T values
    typedef stk_classic::mesh::Field<T, stk_classic::mesh::Cartesian, stk_classic::mesh::Cartesian> field_type ;
    static field_type* createField(const std::string& name, const std::vector<PHX::DataLayout::size_type>& dim,
                                   stk_classic::mesh::fem::FEMMetaData* metaData){

        field_type *fld = & metaData->declare_field<field_type>(name);
        // Multi-dim order is Fortran Ordering, so reversed here
        stk_classic::mesh::put_field(*fld , metaData->node_rank(),
                           metaData->universal_part(), dim[2], dim[1]);

        return fld; // Address is held by stk

    }

    static void saveFieldData(const Teuchos::RCP<const Tpetra_BlockMultiVector>& overlap_node_vec,
                              const stk_classic::mesh::BucketVector& all_elements,
                              field_type *fld, int offset){

      const Teuchos::RCP<const Tpetra_BlockMap>& overlap_node_map = overlap_node_vec->getBlockMap();
      Teuchos::ArrayRCP<const ST> const_overlap_node_view = overlap_node_vec->get1dView();

      for(stk_classic::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

        const stk_classic::mesh::Bucket& bucket = **it;

        stk_classic::mesh::BucketArray<field_type> solution_array(*fld, bucket);

        const int num_i_components = solution_array.dimension(0);
        const int num_j_components = solution_array.dimension(1);
        const int num_nodes_in_bucket = solution_array.dimension(2);

        for(std::size_t i = 0; i < num_nodes_in_bucket; i++)  {

          const GO global_block_id = bucket[i].identifier() - 1; // global node in mesh
          LO local_block_id = overlap_node_map->getLocalBlockID(global_block_id);
          LO block_start = overlap_node_map->getFirstLocalPointInLocalBlock(local_block_id);

          for(std::size_t j = 0; j < num_j_components; j++)
            for(std::size_t k = 0; k < num_i_components; k++)

              solution_array(k, j, i) = const_overlap_node_view[block_start + offset + j*num_i_components + k];

        }
      }
    }

    static void saveFieldData(const Teuchos::RCP<const Tpetra_MultiVector>& overlap_node_vec,
                              const stk_classic::mesh::BucketVector& all_elements,
                              field_type *fld, int offset){


      for(stk_classic::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

        const stk_classic::mesh::Bucket& bucket = **it;

        stk_classic::mesh::BucketArray<field_type> solution_array(*fld, bucket);

        const int num_i_components = solution_array.dimension(0);
        const int num_j_components = solution_array.dimension(1);
        const int num_nodes_in_bucket = solution_array.dimension(2);


        for(std::size_t j = 0; j < num_j_components; j++)
          for(std::size_t k = 0; k < num_i_components; k++){

            Teuchos::ArrayRCP<const ST> const_overlap_node_view = 
                     overlap_node_vec->getVector(offset + j*num_i_components + k)->get1dView();

            for(std::size_t i = 0; i < num_nodes_in_bucket; i++)  {

              const GO global_id = bucket[i].identifier() - 1; // global node in mesh

              solution_array(k, j, i) = const_overlap_node_view[global_id];

            }
         }
      }
    }

  };


}

// Define macro for explicit template instantiation
#define STKNODEFIELDCONTAINER_INSTANTIATE_TEMPLATE_CLASS_SCAL(name, type) \
  template class name<type, 1>;
#define STKNODEFIELDCONTAINER_INSTANTIATE_TEMPLATE_CLASS_VEC(name, type) \
  template class name<type, 2>;
#define STKNODEFIELDCONTAINER_INSTANTIATE_TEMPLATE_CLASS_TENS(name, type) \
  template class name<type, 3>;


#define STKNODEFIELDCONTAINER_INSTANTIATE_TEMPLATE_CLASS(name) \
  STKNODEFIELDCONTAINER_INSTANTIATE_TEMPLATE_CLASS_SCAL(name, double) \
  STKNODEFIELDCONTAINER_INSTANTIATE_TEMPLATE_CLASS_VEC(name, double) \
  STKNODEFIELDCONTAINER_INSTANTIATE_TEMPLATE_CLASS_TENS(name, double)

#endif // ALBANY_STKNODEFIELDCONTAINER_HPP
