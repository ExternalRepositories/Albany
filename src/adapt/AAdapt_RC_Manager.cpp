//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <Intrepid_MiniTensor.h>
#include <Phalanx_FieldManager.hpp>
#include "AAdapt_AdaptiveSolutionManagerT.hpp"
#include "AAdapt_RC_DataTypes.hpp"
#include "AAdapt_RC_Reader.hpp"
#include "AAdapt_RC_Writer.hpp"
#include "AAdapt_RC_DataTypes_impl.hpp"
#include "AAdapt_RC_Manager.hpp"

//#define pr(msg) std::cout << "amb: " << msg << std::endl;
#define pr(msg)
#define amb_do_transform

namespace AAdapt {
namespace rc {

Manager* Manager::create (const Teuchos::RCP<Albany::StateManager>& state_mgr) {
  return new Manager(state_mgr);
}

Teuchos::RCP<Manager> Manager::
create (const Teuchos::RCP<Albany::StateManager>& state_mgr,
        Teuchos::ParameterList& problem_params) {
  if ( ! problem_params.isSublist("Adaptation")) return Teuchos::null;
  Teuchos::ParameterList&
    adapt_params = problem_params.sublist("Adaptation", true);
  if (adapt_params.get<bool>("Reference Configuration: Update", false))
    return Teuchos::rcp(create(state_mgr));
  else return Teuchos::null;
}

void Manager::setSolutionManager(
  const Teuchos::RCP<AdaptiveSolutionManagerT>& sol_mgr)
{ sol_mgr_ = sol_mgr; }

void Manager::
getValidParameters (Teuchos::RCP<Teuchos::ParameterList>& valid_pl) {
  valid_pl->set<bool>("Reference Configuration: Update", false,
                      "Send coordinates + solution to SCOREC.");
}

void Manager::init_x_if_not (const Teuchos::RCP<const Tpetra_Map>& map) {
  if (Teuchos::nonnull(x_)) return;
  x_ = Teuchos::rcp(new Tpetra_Vector(map));
  x_->putScalar(0);
}

void Manager::update_x (const Tpetra_Vector& soln_nol) {
  x_->update(1, soln_nol, 1);
}

Teuchos::RCP<const Tpetra_Vector> Manager::
add_x (const Teuchos::RCP<const Tpetra_Vector>& a) const {
  Teuchos::RCP<Tpetra_Vector> c = Teuchos::rcp(new Tpetra_Vector(*a));
  c->update(1, *x_, 1);
  return c;
}

Teuchos::RCP<const Tpetra_Vector> Manager::
add_x_ol (const Teuchos::RCP<const Tpetra_Vector>& a_ol) const {
  Teuchos::RCP<Tpetra_Vector>
    c = Teuchos::rcp(new Tpetra_Vector(a_ol->getMap()));
  c->doImport(*x_, *sol_mgr_->get_importerT(), Tpetra::INSERT);
  c->update(1, *a_ol, 1);
  return c;
}

Teuchos::RCP<Tpetra_Vector>& Manager::get_x () { return x_; }

#define loop(a, i, dim)                                                 \
  for (PHX::MDField<RealType>::size_type i = 0; i < a.dimension(dim); ++i)
namespace {
void read (const Albany::MDArray& mda, PHX::MDField<RealType>& f) {
  switch (f.rank()) {
  case 2:
    loop(f, cell, 0) loop(f, qp, 1)
      f(cell, qp) = mda(cell, qp);
    break;
  case 3:
    loop(f, cell, 0) loop(f, qp, 1) loop(f, i0, 2)
      f(cell, qp, i0) = mda(cell, qp, i0);
    break;
  case 4:
    loop(f, cell, 0) loop(f, qp, 1) loop(f, i0, 2) loop(f, i1, 3)
      f(cell, qp, i0, i1) = mda(cell, qp, i0, i1);
    break;
  default:
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                               "dims.size() \notin {2,3,4}.");
  }
}

template<typename MDArray>
void write (Albany::MDArray& mda, const MDArray& f) {
  switch (f.rank()) {
  case 2:
    loop(f, cell, 0) loop(f, qp, 1)
      mda(cell, qp) = f(cell, qp);
    break;
  case 3:
    loop(f, cell, 0) loop(f, qp, 1) loop(f, i0, 2)
      mda(cell, qp, i0) = f(cell, qp, i0);
    break;
  case 4:
    loop(f, cell, 0) loop(f, qp, 1) loop(f, i0, 2) loop(f, i1, 3) {
      if (f(cell, qp, i0, i1) != 0)
        mda(cell, qp, i0, i1) = f(cell, qp, i0, i1);
    }
    break;
  default:
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                               "dims.size() \notin {2,3,4}.");
  }
}
} // namespace

// Handles storing and retrieving MDFields. Its implementation uses the
// machinery provided by Albany::StateManager. I also use this class to hide the
// details of the implementation of Manager.
class Manager::FieldDatabase {
public:
  FieldDatabase (const Teuchos::RCP<Albany::StateManager>& state_mgr)
    : state_mgr_(state_mgr)
  { init(); }

  void registerField (
    const std::string& name, const Teuchos::RCP<PHX::DataLayout>& dl,
    const Init::Enum init_G, const Transformation::Enum transformation,
    const Teuchos::RCP<Teuchos::ParameterList>& p)
  {
    const std::string name_rc = decorate(name);
    p->set<std::string>(name_rc + " Name", name_rc);
    p->set< Teuchos::RCP<PHX::DataLayout> >(name_rc + " Data Layout", dl);

    Map::iterator it = ifield_map_.find(name_rc);
    if (it != ifield_map_.end()) return;
    ifield_map_.insert(Pair(name_rc, IField(transformation)));

    fields_.push_back(Field());
    Field& f = fields_.back();
    f.name = name;
    f.layout = dl;

    // Depending on the state variable, different quantities need to be read and
    // written. In all cases, we need two fields.
    //   Holds G and g1.
    registerStateVariable(name_rc, f.layout, init_G);
    //   Holds provisional G and, if needed, g2. If g2 is not needed, then this
    // provisional field is a waste of space and also incurs wasted work in the
    // QP transfer. However, I would need LOCA::AdaptiveSolver to always
    // correctly say, before printSolution is called, whether the mesh will be
    // adapted to avoid this extra storage and work. Maybe in the future.
    registerStateVariable(name_rc + "_1", f.layout, Init::zero);    
  }

  void beginAdapt () {
    // Transform G -> g and write to the primary or, depending on state, primary
    // and provisional fields.
    pr("beginAdapt: write final");
    for (Map::const_iterator it = ifield_map_.begin(); it != ifield_map_.end();
         ++it)
      for (WsIdx wi = 0; wi < is_g_.size(); ++wi)
        transform(it->first, wi, Direction::G2g);
  }

  void endAdapt () {
    pr("is_g_.size() was " << is_g_.size() << " and now will be "
       << state_mgr_->getStateArrays().elemStateArrays.size());
    is_g_.clear();
    is_g_.resize(state_mgr_->getStateArrays().elemStateArrays.size(), true);
  }

  void readField (PHX::MDField<RealType>& f,
                  const PHAL::Workset& workset) {
    if (workset.wsIndex == 0) pr("readField " << f.fieldTag().name());
    if (is_g_.empty()) {
      // At startup, is_g_.size() is 0. We also initialized fields to their G,
      // not g, values.
      is_g_.resize(state_mgr_->getStateArrays().elemStateArrays.size(), false);
    } else if (is_g_[workset.wsIndex]) {
      // If this is the first read after an RCU, transform g -> G.
      transform(f.fieldTag().name(), workset.wsIndex, Direction::g2G);
      is_g_[workset.wsIndex] = false;
    }
    // Read from the primary field.
    read(getMDArray(f.fieldTag().name(), workset.wsIndex), f);
  }
  void writeField (const PHX::MDField<RealType>& f,
                   const PHAL::Workset& workset) {
    if (workset.wsIndex == 0)
      pr("writeField (provisional) " << f.fieldTag().name());
    const std::string name_rc = decorate(f.fieldTag().name());
    // Write to the provisional field.
    write(getMDArray(name_rc + "_1", workset.wsIndex), f);
  }

  Manager::Field::iterator fieldsBegin () { return fields_.begin(); }
  Manager::Field::iterator fieldsEnd () { return fields_.end(); }

  void set_building_sfm (const bool value) {
    building_sfm_ = value;
  }
  bool building_sfm () const { return building_sfm_; }

  Transformation::Enum get_transformation (const std::string& name_rc) {
    return ifield_map_[name_rc].transformation;
  }

private:
  typedef unsigned int WsIdx;
  struct IField {
    Transformation::Enum transformation;
    IField (const Transformation::Enum t) : transformation(t) {}
    IField () {}
  };
  typedef std::pair<std::string, IField> Pair;
  typedef std::map<std::string, IField> Map;

  Teuchos::RCP<Albany::StateManager> state_mgr_;
  Map ifield_map_;
  std::vector<Field> fields_;
  bool building_sfm_;
  std::vector<bool> is_g_;

private:
  void init () {
    building_sfm_ = false;
  }

  void registerStateVariable (
    const std::string& name, const Teuchos::RCP<PHX::DataLayout>& layout,
    const Init::Enum init)
  {
    state_mgr_->registerStateVariable(
      name, layout, "", init == Init::zero ? "scalar" : "identity", 0,
      false, false);
  }

  Albany::MDArray& getMDArray (
    const std::string& name, const WsIdx wi, const bool is_const=true)
  {
    Albany::StateArray& esa = state_mgr_->getStateArrays().elemStateArrays[wi];
    Albany::StateArray::iterator it = esa.find(name);
    TEUCHOS_TEST_FOR_EXCEPTION(
      it == esa.end(), std::logic_error, "elemStateArrays is missing " + name);
    return it->second;
  }

  struct Direction { enum Enum { g2G, G2g }; };

  void transform (const std::string& name_rc, const WsIdx wi,
                  const Direction::Enum dir) {
    if (wi == 0) pr("transform " << name_rc);
    // Name decoration coordinates with registerField's calls to
    // registerStateVariable.
    const Transformation::Enum transformation = get_transformation(name_rc);
    switch (transformation) {
    case Transformation::none: {
      if (wi == 0) pr("none " << (dir == Direction::g2G));
      if (dir == Direction::G2g) {
        // Copy from the provisional to the primary field.
        Albany::MDArray& mda1 = getMDArray(name_rc, wi);
        Albany::MDArray& mda2 = getMDArray(name_rc + "_1", wi);
        write(mda1, mda2);
      } else {
        // In the g -> G direction, the values are already in the primary field,
        // so there's nothing to do.
      }
    } break;
    case Transformation::right_polar_LieR_LieS: {
#ifndef amb_do_transform
    return;
#endif
      if (wi == 0) pr("right_polar_LieR_LieS " << (dir == Direction::g2G));
      Albany::MDArray& mda1 = getMDArray(name_rc, wi);
      Albany::MDArray& mda2 = getMDArray(name_rc + "_1", wi);
      loop(mda1, cell, 0) loop(mda1, qp, 1) {
        if (dir == Direction::G2g) {
          // Copy mda2 (provisional) -> local.
          Intrepid::Tensor<RealType> F(mda1.dimension(2));
          loop(mda2, i, 2) loop(mda2, j, 3) F(i, j) = mda2(cell, qp, i, j);
          // Math.
          std::pair< Intrepid::Tensor<RealType>, Intrepid::Tensor<RealType> >
            RS = Intrepid::polar_right(F);
          if (wi == 0 && cell == 0 && qp == 0)
            pr("F = [\n" << F << "];\nR = [\n" << RS.first << "];\nS = [\n"
               << RS.second << "];");
          RS.first = Intrepid::log_rotation(RS.first);
          RS.second = Intrepid::log_sym(RS.second);
          if (wi == 0 && cell == 0 && qp == 0)
            pr("r =\n" << RS.first << " s =\n" << RS.second);
          // Copy local -> mda1, mda2.
          loop(mda1, i, 2) loop(mda1, j, 3) {
            mda1(cell, qp, i, j) = RS.first(i, j);
            mda2(cell, qp, i, j) = RS.second(i, j);
          }
        } else {
          // Copy mda1,2 -> local.
          Intrepid::Tensor<RealType> R(mda1.dimension(2)), S(mda1.dimension(2));
          loop(mda1, i, 2) loop(mda1, j, 3) {
            R(i, j) = mda1(cell, qp, i, j);
            S(i, j) = mda2(cell, qp, i, j);
          }
          if (wi == 0 && cell == 0 && qp == 0)
            pr("r = [\n" << R << "];\ns = [\n" << S << "];");
          // Math.
          R = Intrepid::exp_skew_symmetric(R);
          S = Intrepid::exp(S);
          R = Intrepid::dot(R, S);
          if (wi == 0 && cell == 0 && qp == 0) pr("F = [\n" << R << "];");
          // Copy local -> mda1. mda2 is unused after g -> G.
          loop(mda1, i, 2) loop(mda1, j, 3) mda1(cell, qp, i, j) = R(i, j);
        }
      }
    } break;
    }
  }
};
#undef loop

template<typename EvalT>
void Manager::createEvaluators (PHX::FieldManager<PHAL::AlbanyTraits>& fm) {
  fm.registerEvaluator<EvalT>(Teuchos::rcp(
    new Reader<EvalT, PHAL::AlbanyTraits>(Teuchos::rcp(this, false))));
  if (db_->building_sfm()) {
    Teuchos::RCP< Writer<EvalT, PHAL::AlbanyTraits> > writer = Teuchos::rcp(
      new Writer<EvalT, PHAL::AlbanyTraits>(Teuchos::rcp(this, false)));
    fm.registerEvaluator<EvalT>(writer);
    fm.requireField<EvalT>(*writer->getNoOutputTag());
  }
}
  
void Manager::registerField (
  const std::string& name, const Teuchos::RCP<PHX::DataLayout>& dl,
  const Init::Enum init, const Transformation::Enum transformation,
  const Teuchos::RCP<Teuchos::ParameterList>& p)
{ db_->registerField(name, dl, init, transformation, p); }

void Manager::
readField (PHX::MDField<RealType>& f, const PHAL::Workset& workset) {
  db_->readField(f, workset);
}

void Manager::
writeField (const PHX::MDField<RealType>& f, const PHAL::Workset& workset) {
  db_->writeField(f, workset);
}
 
Manager::Field::iterator Manager::fieldsBegin () { return db_->fieldsBegin(); }
Manager::Field::iterator Manager::fieldsEnd () { return db_->fieldsEnd(); }

void Manager::beginBuildingSfm () { db_->set_building_sfm(true); }
void Manager::endBuildingSfm () { db_->set_building_sfm(false); }
void Manager::beginAdapt () { db_->beginAdapt(); }
void Manager::endAdapt () { db_->endAdapt(); }

Manager::Manager (const Teuchos::RCP<Albany::StateManager>& state_mgr)
  : db_(Teuchos::rcp(new FieldDatabase(state_mgr)))
{}

#define eti_fn(EvalT)                                   \
  template void Manager::createEvaluators<EvalT>(       \
    PHX::FieldManager<PHAL::AlbanyTraits>& fm);
aadapt_rc_apply_to_all_eval_types(eti_fn)
#undef eti_fn

} // namespace rc
} // namespace AAdapt
