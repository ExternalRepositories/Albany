#include "Albany_EpetraThyraUtils.hpp"
#include "Albany_CommTypes.hpp"
#include "Albany_Utils.hpp"
#include "Albany_ThyraUtils.hpp"

// We only use Thyra's Epetra wrapper for the linear op.
// For vec/mvec, we rely on spmd interface
#include "Thyra_EpetraLinearOp.hpp"

#include "Thyra_DefaultSpmdVectorSpace.hpp"
#include "Thyra_DefaultSpmdMultiVector.hpp"
#include "Thyra_DefaultSpmdVector.hpp"
#include "Thyra_ScalarProdVectorSpaceBase.hpp"

// To convert Teuchos_Comm to Thyra's comm
#include "Thyra_TpetraThyraWrappers.hpp"

namespace Albany
{

struct BadThyraEpetraCast : public std::bad_cast {
  BadThyraEpetraCast (const std::string& msg)
   : m_msg (msg)
  {}

  const char * what () const noexcept { return m_msg.c_str(); }

private:
  const std::string& m_msg;
};

// ============ Epetra->Thyra conversion routines ============ //

Teuchos::RCP<const Thyra_SpmdVectorSpace>
createThyraVectorSpace (const Teuchos::RCP<const Epetra_BlockMap> bmap)
{
  Teuchos::RCP<const Thyra_SpmdVectorSpace> vs;
  if (!bmap.is_null()) {

    auto comm = createTeuchosCommFromEpetraComm(bmap->Comm());
    vs = Thyra::defaultSpmdVectorSpace<ST>(Thyra::convertTpetraToThyraComm(comm), bmap->NumMyElements(), bmap->NumGlobalElements64(), !bmap->DistributedGlobal());

    // Attach the two new RCP's to the map ptr
    Teuchos::set_extra_data(bmap, "Teuchos_Comm", inoutArg(comm));
    Teuchos::set_extra_data(bmap, "Epetra_BlockMap", inoutArg(vs) );
  }

  return vs;
}

Teuchos::RCP<Thyra_Vector> 
createThyraVector (const Teuchos::RCP<Epetra_Vector> v)
{
  Teuchos::RCP<Thyra_Vector> v_thyra = Teuchos::null;
  if (!v.is_null()) {
    auto vs = createThyraVectorSpace(Teuchos::rcpFromRef(v->Map()));
    Teuchos::ArrayRCP<ST> vals(v->Values(),0,v->MyLength(),false);
    v_thyra = Teuchos::rcp( new Thyra::DefaultSpmdVector<ST>(vs,vals,1) );

    // Attach the new RCP to the vector ptr
    Teuchos::set_extra_data(v, "Epetra_Vector", inoutArg(v_thyra));
  }

  return v_thyra;
}

Teuchos::RCP<const Thyra_Vector> 
createConstThyraVector (const Teuchos::RCP<const Epetra_Vector> v)
{
  Teuchos::RCP<const Thyra_Vector> v_thyra = Teuchos::null;
  if (!v.is_null()) {
    auto vs = createThyraVectorSpace(Teuchos::rcpFromRef(v->Map()));
    Teuchos::ArrayRCP<ST> vals(v->Values(),0,v->MyLength(),false);
    v_thyra = Teuchos::rcp( new Thyra::DefaultSpmdVector<ST>(vs,vals,1) );

    // Attach the new RCP to the vector ptr
    Teuchos::set_extra_data(v, "Epetra_Vector", inoutArg(v_thyra));
  }

  return v_thyra;
}

Teuchos::RCP<Thyra_MultiVector>
createThyraMultiVector (const Teuchos::RCP<Epetra_MultiVector> mv)
{
  Teuchos::RCP<Thyra_MultiVector> mv_thyra = Teuchos::null;
  if (!mv.is_null()) {
    Teuchos::RCP<const Thyra_SpmdVectorSpace> range  = createThyraVectorSpace(Teuchos::rcpFromRef(mv->Map()));
    // LB: I have NO IDEA why the rcp_implicit_cast is needed (RCP should already be polymorphic). Yet, the compiler complains without it.
    Teuchos::RCP<const Thyra::ScalarProdVectorSpaceBase<ST>> domain =
      Thyra::createSmallScalarProdVectorSpaceBase(Teuchos::rcp_implicit_cast<const Thyra_VectorSpace>(range),mv->NumVectors());

    Teuchos::ArrayRCP<ST> vals(mv->Values(),0,mv->MyLength(),false);
    mv_thyra = Teuchos::rcp(new Thyra::DefaultSpmdMultiVector<ST>(range,domain,vals));

    // Attach the new RCP to the vector ptr
    Teuchos::set_extra_data(mv_thyra, "Epetra_MultiVector", inoutArg(mv_thyra));
  }

  return mv_thyra;
}

Teuchos::RCP<const Thyra_MultiVector>
createConstThyraMultiVector (const Teuchos::RCP<const Epetra_MultiVector> mv)
{
  Teuchos::RCP<const Thyra_MultiVector> mv_thyra = Teuchos::null;
  if (!mv.is_null()) {
    Teuchos::RCP<const Thyra_SpmdVectorSpace> range  = createThyraVectorSpace(Teuchos::rcpFromRef(mv->Map()));
    // LB: I have NO IDEA why the rcp_implicit_cast is needed (RCP should already be polymorphic). Yet, the compiler complains without it.
    Teuchos::RCP<const Thyra::ScalarProdVectorSpaceBase<ST>> domain =
      Thyra::createSmallScalarProdVectorSpaceBase(Teuchos::rcp_implicit_cast<const Thyra_VectorSpace>(range),mv->NumVectors());

    Teuchos::ArrayRCP<ST> vals(mv->Values(),0,mv->MyLength(),false);
    mv_thyra = Teuchos::rcp( new Thyra::DefaultSpmdMultiVector<ST>(range,domain,vals) );

    // Attach the new RCP to the vector ptr
    Teuchos::set_extra_data(mv_thyra, "Epetra_MultiVector", inoutArg(mv_thyra));
  }

  return mv_thyra;
}

Teuchos::RCP<Thyra_LinearOp>
createThyraLinearOp (const Teuchos::RCP<Epetra_Operator> op)
{
  Teuchos::RCP<Thyra_LinearOp> lop;
  if (!op.is_null()) {
    lop = Thyra::nonconstEpetraLinearOp(op);
  }

  return lop;
}

Teuchos::RCP<const Thyra_LinearOp>
createConstThyraLinearOp (const Teuchos::RCP<const Epetra_Operator> op)
{
  Teuchos::RCP<const Thyra_LinearOp> lop;
  if (!op.is_null()) {
    lop = Thyra::epetraLinearOp(op);
  }

  return lop;
}

// ============ Thyra->Epetra conversion routines ============ //

Teuchos::RCP<const Epetra_BlockMap>
getEpetraBlockMap (const Teuchos::RCP<const Thyra_VectorSpace> vs,
                   const bool throw_on_failure)
{
  Teuchos::RCP<const Epetra_BlockMap> map;
  if (!vs.is_null()) {
    auto data = Teuchos::get_optional_extra_data<Teuchos::RCP<const Epetra_BlockMap>>(vs,"Epetra_BlockMap");
    TEUCHOS_TEST_FOR_EXCEPTION (throw_on_failure && data.is_null(), std::runtime_error,
                                "Error! Could not extract Epetra_BlockMap from Thyra_VectorSpace.\n");
    if (!data.is_null()) {
      map = *data;
    }
  }

  return map;
}

Teuchos::RCP<const Epetra_Map>
getEpetraMap (const Teuchos::RCP<const Thyra_VectorSpace> vs,
              const bool throw_on_failure)
{
  Teuchos::RCP<const Epetra_BlockMap> bmap = getEpetraBlockMap(vs,throw_on_failure);

  // If we are failure-tolerant, if the call failed, we must exit now
  if (!throw_on_failure && bmap.is_null()) {
    return Teuchos::null;
  }

  const Epetra_Map* raw_map = reinterpret_cast<const Epetra_Map*>(bmap.get());

  Teuchos::RCP<const Epetra_Map> map(raw_map,bmap.access_private_node());

  return map;
}

Teuchos::RCP<Epetra_Vector>
getEpetraVector (const Teuchos::RCP<Thyra_Vector> v,
                 const bool throw_on_failure)
{
  Teuchos::RCP<Epetra_Vector> v_epetra;
  if (!v.is_null()) {
    auto data = Teuchos::get_optional_extra_data<Teuchos::RCP<Epetra_Vector>>(v,"Epetra_Vector");

    TEUCHOS_TEST_FOR_EXCEPTION (throw_on_failure && data.is_null(), std::runtime_error,
                                "Error! Could not extract Epetra_Vector from Thyra_Vector.\n");
    if (!data.is_null()) {
      v_epetra = *data;
    }
  }

  return v_epetra;
}

Teuchos::RCP<const Epetra_Vector>
getConstEpetraVector (const Teuchos::RCP<const Thyra_Vector> v,
                      const bool throw_on_failure)

{
  Teuchos::RCP<const Epetra_Vector> v_epetra;
  if (!v.is_null()) {
    // The thyra vector may have been originally created from a non-const Epetra_Vector,
    // so we need to check both const and nonconst
    auto data_const    = Teuchos::get_optional_extra_data<Teuchos::RCP<const Epetra_Vector>>(v,"Epetra_Vector");
    auto data_nonconst = Teuchos::get_optional_extra_data<Teuchos::RCP<Epetra_Vector>>(v,"Epetra_Vector");

    TEUCHOS_TEST_FOR_EXCEPTION (throw_on_failure && data_const.is_null() && data_nonconst.is_null(), std::runtime_error,
                                "Error! Could not extract Epetra_Vector from Thyra_Vector.\n");
    if (!data_const.is_null()) {
      v_epetra = *data_const;
    } else if (!data_nonconst.is_null()) {
      v_epetra = *data_nonconst;
    }
  }

  return v_epetra;
}

Teuchos::RCP<Epetra_MultiVector>
getEpetraMultiVector (const Teuchos::RCP<Thyra_MultiVector> mv,
                      const bool throw_on_failure)
{
  Teuchos::RCP<Epetra_MultiVector> mv_epetra;
  if (!mv.is_null()) {
    auto data = Teuchos::get_optional_extra_data<Teuchos::RCP<Epetra_MultiVector>>(mv,"Epetra_MultiVector");

    TEUCHOS_TEST_FOR_EXCEPTION (throw_on_failure && data.is_null(), std::runtime_error,
                                "Error! Could not extract Epetra_MultiVector from Thyra_MultiVector.\n");
    if (!data.is_null()) {
      mv_epetra = *data;
    }
  }

  return mv_epetra;
}

Teuchos::RCP<const Epetra_MultiVector>
getConstEpetraMultiVector (const Teuchos::RCP<const Thyra_MultiVector> mv,
                           const bool throw_on_failure)
{
  Teuchos::RCP<const Epetra_MultiVector> mv_epetra;
  if (!mv.is_null()) {
    // The thyra vector may have been originally created from a non-const Epetra_Vector,
    // so we need to check both const and nonconst
    auto data_const    = Teuchos::get_optional_extra_data<Teuchos::RCP<const Epetra_MultiVector>>(mv,"Epetra_MVector");
    auto data_nonconst = Teuchos::get_optional_extra_data<Teuchos::RCP<Epetra_MultiVector>>(mv,"Epetra_MultiVector");

    TEUCHOS_TEST_FOR_EXCEPTION (throw_on_failure && data_const.is_null() && data_nonconst.is_null(), std::runtime_error,
                                "Error! Could not extract Epetra_MultiVector from Thyra_MultiVector.\n");
    if (!data_const.is_null()) {
      mv_epetra = *data_const;
    } else if (!data_nonconst.is_null()) {
      mv_epetra = *data_nonconst;
    }
  }

  return mv_epetra;
}

Teuchos::RCP<Epetra_Operator>
getEpetraOperator (const Teuchos::RCP<Thyra_LinearOp> lop,
                   const bool throw_on_failure)
{
  Teuchos::RCP<Epetra_Operator> op;
  if (!lop.is_null()) {
    auto tmp = Teuchos::rcp_dynamic_cast<Thyra::EpetraLinearOp>(lop,throw_on_failure);
    if (!tmp.is_null()) {
      op = tmp->epetra_op();
    }
  }

  return op;
}

Teuchos::RCP<const Epetra_Operator>
getConstEpetraOperator (const Teuchos::RCP<const Thyra_LinearOp> lop,
                        const bool throw_on_failure)
{
  Teuchos::RCP<const Epetra_Operator> op;
  if (!lop.is_null()) {
    auto tmp = Teuchos::rcp_dynamic_cast<const Thyra::EpetraLinearOp>(lop,throw_on_failure);
    if (!tmp.is_null()) {
      op = tmp->epetra_op();
    }
  }

  return op;
}

Teuchos::RCP<Epetra_CrsMatrix>
getEpetraMatrix (const Teuchos::RCP<Thyra_LinearOp> lop,
                 const bool throw_on_failure)
{
  Teuchos::RCP<Epetra_CrsMatrix> mat;
  if (!lop.is_null()) {
    auto op = getEpetraOperator(lop,throw_on_failure);
    mat = Teuchos::rcp_dynamic_cast<Epetra_CrsMatrix>(op,throw_on_failure);
  }

  return mat;
}

Teuchos::RCP<const Epetra_CrsMatrix>
getConstEpetraMatrix (const Teuchos::RCP<const Thyra_LinearOp> lop,
                      const bool throw_on_failure)
{
  Teuchos::RCP<const Epetra_CrsMatrix> mat;
  if (!lop.is_null()) {
    auto op = getConstEpetraOperator(lop,throw_on_failure);
    mat = Teuchos::rcp_dynamic_cast<const Epetra_CrsMatrix>(op,throw_on_failure);
  }

  return mat;
}

// --- Casts taking references as inputs --- //

Teuchos::RCP<Epetra_Vector>
getEpetraVector (Thyra_Vector& v,
                 const Epetra_BlockMap& emap,
                 const bool throw_on_failure)
{
  auto* spmd_v = dynamic_cast<Thyra::DefaultSpmdVector<ST>*>(&v);

  TEUCHOS_TEST_FOR_EXCEPTION(spmd_v==nullptr && throw_on_failure, BadThyraEpetraCast,
                             "Error! Could not cast input Thyra_Vector to Thyra::DefaultSpmdVector<ST>.\n");

  ST* vals = spmd_v->getPtr();

  return Teuchos::rcp(new Epetra_Vector(View,emap,vals));
}

Teuchos::RCP<const Epetra_Vector>
getConstEpetraVector (const Thyra_Vector& v,
                      const Epetra_BlockMap& emap,
                      const bool throw_on_failure)
{
  auto* spmd_v = dynamic_cast<const Thyra::DefaultSpmdVector<ST>*>(&v);

  TEUCHOS_TEST_FOR_EXCEPTION(spmd_v==nullptr && throw_on_failure, BadThyraEpetraCast,
                             "Error! Could not cast input Thyra_Vector to Thyra::DefaultSpmdVector<ST>.\n");

  const ST* vals = spmd_v->getPtr();

  // LB: I don't see any way around the const cast, since Epetra expects double* as input.
  return Teuchos::rcp(new Epetra_Vector(View,emap,const_cast<ST*>(vals)));
}

Teuchos::RCP<Epetra_MultiVector>
getEpetraMultiVector (Thyra_MultiVector& mv,
                      const Epetra_BlockMap& emap,
                      const bool throw_on_failure)
{
  auto* spmd_mv = dynamic_cast<Thyra::DefaultSpmdMultiVector<ST>*>(&mv);

  TEUCHOS_TEST_FOR_EXCEPTION(spmd_mv==nullptr && throw_on_failure, BadThyraEpetraCast,
                             "Error! Could not cast input Thyra_MultiVector to Thyra::DefaultSpmdMultiVector<ST>.\n");

  Teuchos::ArrayRCP<ST> vals;
  GO leadingDim;
  spmd_mv->getNonconstLocalData(Teuchos::inOutArg(vals),Teuchos::inOutArg(leadingDim));

  return Teuchos::rcp(new Epetra_MultiVector(View,emap,vals.get(),leadingDim,mv.domain()->dim()));
}

Teuchos::RCP<const Epetra_MultiVector>
getConstEpetraMultiVector (const Thyra_MultiVector& mv,
                           const Epetra_BlockMap& emap,
                           const bool throw_on_failure)
{
  auto* spmd_mv = dynamic_cast<const Thyra::DefaultSpmdMultiVector<ST>*>(&mv);

  TEUCHOS_TEST_FOR_EXCEPTION(spmd_mv==nullptr && throw_on_failure, BadThyraEpetraCast,
                             "Error! Could not cast input Thyra_MultiVector to Thyra::DefaultSpmdMultiVector<ST>.\n");

  Teuchos::ArrayRCP<const ST> vals;
  GO leadingDim;
  spmd_mv->getLocalData(Teuchos::inOutArg(vals),Teuchos::inOutArg(leadingDim));

  // LB: I don't see any way around the const cast, since Epetra expects double* as input.
  return Teuchos::rcp(new Epetra_MultiVector(View,emap,const_cast<ST*>(vals.get()),leadingDim,mv.domain()->dim()));
}

Teuchos::RCP<Epetra_Operator>
getEpetraOperator (Thyra_LinearOp& lop,
                   const bool throw_on_failure)
{
  Thyra::EpetraLinearOp* eop = dynamic_cast<Thyra::EpetraLinearOp*>(&lop);

  TEUCHOS_TEST_FOR_EXCEPTION(eop==nullptr && throw_on_failure, BadThyraEpetraCast,
                             "Error! Could not cast input Thyra_LinearOp to Thyra::EpetraLinearOp.\n");

  // We allow bad cast, but once cast goes through, we *expect* pointers to be valid
  TEUCHOS_TEST_FOR_EXCEPTION(eop->epetra_op().is_null(), std::runtime_error,
                             "Error! The Thyra::EpetraLinearOp object stores a null pointer.\n") 
  return eop->epetra_op();
}

Teuchos::RCP<const Epetra_Operator>
getConstEpetraOperator (const Thyra_LinearOp& lop,
                        const bool throw_on_failure)
{
  const Thyra::EpetraLinearOp* eop = dynamic_cast<const Thyra::EpetraLinearOp*>(&lop);

  TEUCHOS_TEST_FOR_EXCEPTION(eop==nullptr && throw_on_failure, BadThyraEpetraCast,
                             "Error! Could not cast input Thyra_LinearOp to Thyra::EpetraLinearOp.\n");

  // We allow bad cast, but once cast goes through, we *expect* pointers to be valid
  TEUCHOS_TEST_FOR_EXCEPTION(eop->epetra_op().is_null(), std::runtime_error,
                             "Error! The Thyra::EpetraLinearOp object stores a null pointer.\n") 
  return eop->epetra_op();
}

Teuchos::RCP<Epetra_CrsMatrix>
getEpetraMatrix (Thyra_LinearOp& lop,
                 const bool throw_on_failure)
{
  auto eop = getEpetraOperator(lop,throw_on_failure);
  auto emat = Teuchos::rcp_dynamic_cast<Epetra_CrsMatrix>(eop);

  // We allow bad cast, but once cast goes through, we *expect* the operator to store a crs matrix
  TEUCHOS_TEST_FOR_EXCEPTION(emat.is_null(), std::runtime_error,
                             "Error! The Thyra_EpetraLinearOp object does not store a Epetra_CrsMatrix.\n") 
  return emat;
}

Teuchos::RCP<const Epetra_CrsMatrix>
getConstEpetraMatrix (const Thyra_LinearOp& lop,
                      const bool throw_on_failure)
{
  auto eop = getConstEpetraOperator(lop,throw_on_failure);
  auto emat = Teuchos::rcp_dynamic_cast<const Epetra_CrsMatrix>(eop);

  // We allow bad cast, but once cast goes through, we *expect* the operator to store a crs matrix
  TEUCHOS_TEST_FOR_EXCEPTION(emat.is_null(), std::runtime_error,
                             "Error! The Thyra_EpetraLinearOp object does not store a Epetra_CrsMatrix.\n") 
  return emat;
}

} // namespace Albany
