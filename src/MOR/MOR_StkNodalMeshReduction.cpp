//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#include "MOR_StkNodalMeshReduction.hpp"

#include "stk_mesh/base/MetaData.hpp"
#include "stk_mesh/base/GetEntities.hpp"
#include "stk_mesh/base/BulkModification.hpp"

#include "Teuchos_Ptr.hpp"

#include <boost/iterator/indirect_iterator.hpp>

#include <algorithm>
#include <iterator>

namespace MOR {

class BulkModification {
public:
  explicit BulkModification(stk::mesh::BulkData &target) :
    target_(target)
  { target_.modification_begin(); }

  ~BulkModification() { target_.modification_end(); }

  const stk::mesh::BulkData &target() const { return target_; }
  stk::mesh::BulkData &target() { return target_; }

private:
  stk::mesh::BulkData &target_;

  BulkModification(const BulkModification &);
  BulkModification &operator=(const BulkModification &);
};

void addNodesToPart(
    const Teuchos::ArrayView<const stk::mesh::EntityId> &nodeIds,
    stk::mesh::Part &samplePart,
    stk::mesh::BulkData& bulkData)
{
  const stk::mesh::EntityRank nodeEntityRank(0);
  const stk::mesh::PartVector samplePartVec(1, &samplePart);
  const stk::mesh::Selector isLocallyOwned = stk::mesh::MetaData::get(bulkData).locally_owned_part();

  BulkModification mod(bulkData);
  typedef Teuchos::ArrayView<const stk::mesh::EntityId>::const_iterator Iter;
  for (Iter it = nodeIds.begin(), it_end = nodeIds.end(); it != it_end; ++it) {
    const Teuchos::Ptr<stk::mesh::Entity> node(bulkData.get_entity(nodeEntityRank, *it));
    if (Teuchos::nonnull(node) && isLocallyOwned(*node)) {
      bulkData.change_entity_parts(*node, samplePartVec);
    }
  }
}

class EntityDestructor : public std::iterator<std::output_iterator_tag, void, void, void, void> {
public:
  EntityDestructor() : modification_() {}
  explicit EntityDestructor(BulkModification &m) : modification_(&m) {}

  // Trivial operations (implemented as noops)
  EntityDestructor &operator++() { return *this; }
  EntityDestructor &operator++(int) { return *this; }
  EntityDestructor &operator*() { return *this; }

  EntityDestructor &operator=(stk::mesh::Entity *&e) {
    (void) modification_->target().destroy_entity(e); // Ignore return value, may silently fails
    return *this;
  }
  EntityDestructor &operator=(stk::mesh::Entity *const &e) {
    stk::mesh::Entity *e_copy = e;
    return this->operator=(e_copy);
  }

private:
  BulkModification *modification_;
};

void performNodalMeshReduction(
    stk::mesh::Part &samplePart,
    stk::mesh::BulkData& bulkData)
{
  const stk::mesh::EntityRank nodeEntityRank(0);
  const stk::mesh::MetaData &metaData = stk::mesh::MetaData::get(bulkData);

  std::vector<stk::mesh::Entity *> sampleNodes;
  stk::mesh::get_selected_entities(samplePart, bulkData.buckets(nodeEntityRank), sampleNodes);

  const stk::mesh::Selector locallyOwnedPredicate = metaData.locally_owned_part();

  std::vector<stk::mesh::Entity *> neighboringEntities;
  typedef boost::indirect_iterator<std::vector<stk::mesh::Entity *>::const_iterator> EntityIterator;
  for (EntityIterator it(sampleNodes.begin()), it_end(sampleNodes.end()); it != it_end; ++it) {
    const stk::mesh::PairIterRelation relations = it->relations();
    typedef stk::mesh::PairIterRelation::first_type RelationIterator;
    for (RelationIterator rel_it = relations.first, rel_it_end = relations.second; rel_it != rel_it_end; ++rel_it) {
      const Teuchos::Ptr<stk::mesh::Entity> relatedEntity(rel_it->entity());
      if (Teuchos::nonnull(relatedEntity) && locallyOwnedPredicate(*relatedEntity)) {
        neighboringEntities.push_back(relatedEntity.get());
      }
    }
  }
  std::sort(neighboringEntities.begin(), neighboringEntities.end(), stk::mesh::EntityLess());
  neighboringEntities.erase(
      std::unique(neighboringEntities.begin(), neighboringEntities.end(), stk::mesh::EntityEqual()),
      neighboringEntities.end());

  std::vector<stk::mesh::Entity *> sampleClosure;
  stk::mesh::find_closure(bulkData, neighboringEntities, sampleClosure);

  // Keep only the closure, remove the rest, by decreasing entityRanks
  {
    typedef boost::indirect_iterator<std::vector<stk::mesh::Entity *>::const_iterator> EntityIterator;
    EntityIterator allKeepersEnd(sampleClosure.end());
    const EntityIterator allKeepersBegin(sampleClosure.begin());
    for (stk::mesh::EntityRank candidateRankCount = metaData.entity_rank_count(); candidateRankCount > 0; --candidateRankCount) {
      const stk::mesh::EntityRank candidateRank = candidateRankCount - 1;
      const EntityIterator keepersBegin = std::lower_bound(allKeepersBegin, allKeepersEnd,
                                                           stk::mesh::EntityKey(candidateRank, 0),
                                                           stk::mesh::EntityLess());
      const EntityIterator keepersEnd = allKeepersEnd;
      std::vector<stk::mesh::Entity *> candidates;
      stk::mesh::get_selected_entities(metaData.locally_owned_part(), bulkData.buckets(candidateRank), candidates);
      {
        BulkModification modification(bulkData);
        std::set_difference(candidates.begin(), candidates.end(),
                            keepersBegin.base(), keepersEnd.base(),
                            EntityDestructor(modification),
                            stk::mesh::EntityLess());
      }
      allKeepersEnd = keepersBegin;
    }
  }
}

} // end namespace MOR
