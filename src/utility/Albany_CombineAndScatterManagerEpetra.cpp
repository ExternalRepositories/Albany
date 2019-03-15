#include "Albany_CombineAndScatterManagerEpetra.hpp"

#include "Albany_EpetraThyraUtils.hpp"

namespace {
Epetra_CombineMode combineModeE (const Albany::CombineMode modeA)
{
  Epetra_CombineMode modeE;
  switch (modeA) {
    case Albany::CombineMode::ADD:
      modeE = Epetra_CombineMode::Add;
      break;
    case Albany::CombineMode::INSERT:
      modeE = Epetra_CombineMode::Insert;
      break;
    case Albany::CombineMode::ZERO:
      modeE = Epetra_CombineMode::Zero;
      break;
    default:
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "Error! Unknown Albany combine mode. Please, contact developers.\n");
  }
  return modeE;
}

} // anonymous namespace

namespace Albany
{

CombineAndScatterManagerEpetra::
CombineAndScatterManagerEpetra(const Teuchos::RCP<const Thyra_VectorSpace>& owned,
                               const Teuchos::RCP<const Thyra_VectorSpace>& overlapped)
 : owned_vs      (owned)
 , overlapped_vs (overlapped)
{
  auto ownedE = Albany::getEpetraMap(owned);
  auto overlappedE = Albany::getEpetraMap(overlapped);

  importer = Teuchos::rcp( new Epetra_Import(*overlappedE, *ownedE) );
}

void CombineAndScatterManagerEpetra::
combine (const Thyra_Vector& src,
               Thyra_Vector& dst,
         const CombineMode CM) const
{
  auto cmE = combineModeE(CM);
  auto srcE = Albany::getConstEpetraVector(src);
  auto dstE = Albany::getEpetraVector(dst);

#ifdef ALBANY_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(!srcE->getMap()->isSameAs(*importer->getTargetMap()), std::runtime_error,
                             "Error! The map of the input src vector does not match the importer's target map.\n");
  TEUCHOS_TEST_FOR_EXCEPTION(!dstE->getMap()->isSameAs(*importer->getSourceMap()), std::runtime_error,
                             "Error! The map of the input dst vector does not match the importer's source map.\n");
#endif

  dstE->Export(*srcE,*importer,cmE);
}

void CombineAndScatterManagerEpetra::
combine (const Thyra_MultiVector& src,
               Thyra_MultiVector& dst,
         const CombineMode CM) const
{
  // There's a catch here!
  // Legend: V = Vector, MV = MultiVector, TV = Epetra_Vector, TMV = Epetra_MultiVector, T_xyz = Thyra_xyz
  // One can create a T_TV, then pass it to routines expecting a T_MV, since T_TV inherits from T_V,
  // which inherits from T_MV. However, T_TV does NOT inherit from T_TMV, so such routines would
  // try to cast the input to T_TMV and fail. This would be solved if T_TV also inherited
  // from T_TMV, but that's hard to do (without code duplication), since T_T(M)V store
  // ConstNonConstObj containers to the Epetra objects, which I _think_ do not support polymorphism.
  // So, given what we have, we _try_ to extract a TMV from the T_MV, and, if we fail,
  // we try again, this time extracting a TV. If we still fail, then we can error out.
  Teuchos::RCP<const Epetra_MultiVector> srcE = Albany::getConstEpetraMultiVector(src,false);
  Teuchos::RCP<Epetra_MultiVector> dstE = Albany::getEpetraMultiVector(dst,false);

  if (srcE.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Epetra_Vector
    const Thyra_Vector* srcV = dynamic_cast<const Thyra_Vector*>(&src);

    TEUCHOS_TEST_FOR_EXCEPTION (srcV==nullptr, std::runtime_error,
                                "Error! Input src does not seem to be a Epetra_MultiVector or a Epetra_Vector.\n");

    // This time throw if extraction fails
    srcE = Albany::getConstEpetraVector(*srcV);
  }

  if (dstE.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Epetra_Vector
    Thyra_Vector* dstV = dynamic_cast<Thyra_Vector*>(&dst);

    TEUCHOS_TEST_FOR_EXCEPTION (dstV==nullptr, std::runtime_error,
                                "Error! Input dst does not seem to be a Epetra_MultiVector or a Epetra_Vector.\n");

    // This time throw if extraction fails
    dstE = Albany::getEpetraVector(*dstV);
  }

#ifdef ALBANY_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(!srcE->getMap()->isSameAs(*importer->getTargetMap()), std::runtime_error,
                             "Error! The map of the input src multi vector does not match the importer's target map.\n");
  TEUCHOS_TEST_FOR_EXCEPTION(!dstE->getMap()->isSameAs(*importer->getSourceMap()), std::runtime_error,
                             "Error! The map of the input dst multi vector does not match the importer's source map.\n");
#endif

  auto cmE = combineModeE(CM);
  dstE->Export(*srcE,*importer,cmE);
}

void CombineAndScatterManagerEpetra::
combine (const Thyra_LinearOp& src,
               Thyra_LinearOp& dst,
         const CombineMode CM) const
{
  auto cmE = combineModeE(CM);
  auto srcE = Albany::getConstEpetraMatrix(src);
  auto dstE = Albany::getEpetraMatrix(dst);

#ifdef ALBANY_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(!srcE->getMap()->isSameAs(*importer->getSourceMap()), std::runtime_error,
                             "Error! The row map of the input src matrix does not match the importer's source map.\n");
  TEUCHOS_TEST_FOR_EXCEPTION(!dstE->getMap()->isSameAs(*importer->getTargetMap()), std::runtime_error,
                             "Error! The row map of the input dst matrix does not match the importer's target map.\n");
#endif

  dstE->Export(*srcE,*importer,cmE);
}

void CombineAndScatterManagerEpetra::
combine (const Teuchos::RCP<const Thyra_Vector>& src,
         const Teuchos::RCP<      Thyra_Vector>& dst,
         const CombineMode CM) const
{
  auto cmE = combineModeE(CM);
  auto srcE = Albany::getConstEpetraVector(src);
  auto dstE = Albany::getEpetraVector(dst);

#ifdef ALBANY_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(!srcE->getMap()->isSameAs(*importer->getTargetMap()), std::runtime_error,
                             "Error! The map of the input src vector does not match the importer's target map.\n");
  TEUCHOS_TEST_FOR_EXCEPTION(!dstE->getMap()->isSameAs(*importer->getSourceMap()), std::runtime_error,
                             "Error! The map of the input dst vector does not match the importer's source map.\n");
#endif

  dstE->Export(*srcE,*importer,cmE);
}

void CombineAndScatterManagerEpetra::
combine (const Teuchos::RCP<const Thyra_MultiVector>& src,
         const Teuchos::RCP<      Thyra_MultiVector>& dst,
         const CombineMode CM) const
{
  // There's a catch here!
  // Legend: V = Vector, MV = MultiVector, TV = Epetra_Vector, TMV = Epetra_MultiVector, T_xyz = Thyra_xyz
  // One can create a T_TV, then pass it to routines expecting a T_MV, since T_TV inherits from T_V,
  // which inherits from T_MV. However, T_TV does NOT inherit from T_TMV, so such routines would
  // try to cast the input to T_TMV and fail. This would be solved if T_TV also inherited
  // from T_TMV, but that's hard to do (without code duplication), since T_T(M)V store
  // ConstNonConstObj containers to the Epetra objects, which I _think_ do not support polymorphism.
  // So, given what we have, we _try_ to extract a TMV from the T_MV, and, if we fail,
  // we try again, this time extracting a TV. If we still fail, then we can error out.
  Teuchos::RCP<const Epetra_MultiVector> srcE = Albany::getConstEpetraMultiVector(src,false);
  Teuchos::RCP<Epetra_MultiVector> dstE = Albany::getEpetraMultiVector(dst,false);

  if (srcE.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Epetra_Vector
    Teuchos::RCP<const Thyra_Vector> srcV = Teuchos::rcp_dynamic_cast<const Thyra_Vector>(src);

    TEUCHOS_TEST_FOR_EXCEPTION (srcV.is_null(), std::runtime_error,
                                "Error! Input src does not seem to be a Epetra_MultiVector or a Epetra_Vector.\n");

    // This time throw if extraction fails
    srcE = Albany::getConstEpetraVector(srcV);
  }

  if (dstE.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Epetra_Vector
    Teuchos::RCP<Thyra_Vector> dstV = Teuchos::rcp_dynamic_cast<Thyra_Vector>(dst);

    TEUCHOS_TEST_FOR_EXCEPTION (dstV.is_null(), std::runtime_error,
                                "Error! Input dst does not seem to be a Epetra_MultiVector or a Epetra_Vector.\n");

    // This time throw if extraction fails
    dstE = Albany::getEpetraVector(dstV);
  }

#ifdef ALBANY_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(!srcE->getMap()->isSameAs(*importer->getTargetMap()), std::runtime_error,
                             "Error! The map of the input src multi vector does not match the importer's target map.\n");
  TEUCHOS_TEST_FOR_EXCEPTION(!dstE->getMap()->isSameAs(*importer->getSourceMap()), std::runtime_error,
                             "Error! The map of the input dst multi vector does not match the importer's source map.\n");
#endif

  auto cmE = combineModeE(CM);
  dstE->Export(*srcE,*importer,cmE);
}

void CombineAndScatterManagerEpetra::
combine (const Teuchos::RCP<const Thyra_LinearOp>& src,
         const Teuchos::RCP<      Thyra_LinearOp>& dst,
         const CombineMode CM) const
{
  auto cmE = combineModeE(CM);
  auto srcE = Albany::getConstEpetraMatrix(src);
  auto dstE = Albany::getEpetraMatrix(dst);

#ifdef ALBANY_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(!srcE->getMap()->isSameAs(*importer->getSourceMap()), std::runtime_error,
                             "Error! The row map of the input src matrix does not match the importer's source map.\n");
  TEUCHOS_TEST_FOR_EXCEPTION(!dstE->getMap()->isSameAs(*importer->getTargetMap()), std::runtime_error,
                             "Error! The row map of the input dst matrix does not match the importer's target map.\n");
#endif

  dstE->Export(*srcE,*importer,cmE);
}

// Scatter methods
void CombineAndScatterManagerEpetra::
scatter (const Thyra_Vector& src,
               Thyra_Vector& dst,
         const CombineMode CM) const
{
  auto cmE = combineModeE(CM);
  auto srcE = Albany::getConstEpetraVector(src);
  auto dstE = Albany::getEpetraVector(dst);

#ifdef ALBANY_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(!srcE->getMap()->isSameAs(*importer->getSourceMap()), std::runtime_error,
                             "Error! The map of the input src vector does not match the importer's source map.\n");
  TEUCHOS_TEST_FOR_EXCEPTION(!dstE->getMap()->isSameAs(*importer->getTargetMap()), std::runtime_error,
                             "Error! The map of the input dst vector does not match the importer's target map.\n");
#endif

  dstE->Import(*srcE,*importer,cmE);
}

void CombineAndScatterManagerEpetra::
scatter (const Thyra_MultiVector& src,
               Thyra_MultiVector& dst,
         const CombineMode CM) const
{
  // There's a catch here!
  // Legend: V = Vector, MV = MultiVector, TV = Epetra_Vector, TMV = Epetra_MultiVector, T_xyz = Thyra_xyz
  // One can create a T_TV, then pass it to routines expecting a T_MV, since T_TV inherits from T_V,
  // which inherits from T_MV. However, T_TV does NOT inherit from T_TMV, so such routines would
  // try to cast the input to T_TMV and fail. This would be solved if T_TV also inherited
  // from T_TMV, but that's hard to do (without code duplication), since T_T(M)V store
  // ConstNonConstObj containers to the Epetra objects, which I _think_ do not support polymorphism.
  // So, given what we have, we _try_ to extract a TMV from the T_MV, and, if we fail,
  // we try again, this time extracting a TV. If we still fail, then we can error out.
  Teuchos::RCP<const Epetra_MultiVector> srcE = Albany::getConstEpetraMultiVector(src,false);
  Teuchos::RCP<Epetra_MultiVector> dstE = Albany::getEpetraMultiVector(dst,false);

  if (srcE.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Epetra_Vector
    const Thyra_Vector* srcV = dynamic_cast<const Thyra_Vector*>(&src);

    TEUCHOS_TEST_FOR_EXCEPTION (srcV==nullptr, std::runtime_error,
                                "Error! Input src does not seem to be a Epetra_MultiVector or a Epetra_Vector.\n");

    // This time throw if extraction fails
    srcE = Albany::getConstEpetraVector(*srcV);
  }

  if (dstE.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Epetra_Vector
    Thyra_Vector* dstV = dynamic_cast<Thyra_Vector*>(&dst);

    TEUCHOS_TEST_FOR_EXCEPTION (dstV==nullptr, std::runtime_error,
                                "Error! Input dst does not seem to be a Epetra_MultiVector or a Epetra_Vector.\n");

    // This time throw if extraction fails
    dstE = Albany::getEpetraVector(*dstV);
  }

#ifdef ALBANY_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(!srcE->getMap()->isSameAs(*importer->getSourceMap()), std::runtime_error,
                             "Error! The map of the input src multi vector does not match the importer's source map.\n");
  TEUCHOS_TEST_FOR_EXCEPTION(!dstE->getMap()->isSameAs(*importer->getTargetMap()), std::runtime_error,
                             "Error! The map of the input dst multi vector does not match the importer's target map.\n");
#endif

  auto cmE = combineModeE(CM);
  dstE->Import(*srcE,*importer,cmE);
}

void CombineAndScatterManagerEpetra::
scatter (const Thyra_LinearOp& src,
               Thyra_LinearOp& dst,
         const CombineMode CM) const
{
  auto cmE  = combineModeE(CM);
  auto srcE = Albany::getConstEpetraMatrix(src);
  auto dstE = Albany::getEpetraMatrix(dst);

#ifdef ALBANY_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(!srcE->getMap()->isSameAs(*importer->getSourceMap()), std::runtime_error,
                             "Error! The row map of the input src matrix does not match the importer's source map.\n");
  TEUCHOS_TEST_FOR_EXCEPTION(!dstE->getMap()->isSameAs(*importer->getTargetMap()), std::runtime_error,
                             "Error! The row map of the input dst matrix does not match the importer's target map.\n");
#endif

  dstE->Import(*srcE,*importer,cmE);
}

void CombineAndScatterManagerEpetra::
scatter (const Teuchos::RCP<const Thyra_Vector>& src,
         const Teuchos::RCP<      Thyra_Vector>& dst,
         const CombineMode CM) const
{
  auto cmE = combineModeE(CM);
  auto srcE = Albany::getConstEpetraVector(src);
  auto dstE = Albany::getEpetraVector(dst);

#ifdef ALBANY_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(!srcE->getMap()->isSameAs(*importer->getSourceMap()), std::runtime_error,
                             "Error! The map of the input src vector does not match the importer's source map.\n");
  TEUCHOS_TEST_FOR_EXCEPTION(!dstE->getMap()->isSameAs(*importer->getTargetMap()), std::runtime_error,
                             "Error! The map of the input dst vector does not match the importer's target map.\n");
#endif

  dstE->Import(*srcE,*importer,cmE);
}

void CombineAndScatterManagerEpetra::
scatter (const Teuchos::RCP<const Thyra_MultiVector>& src,
         const Teuchos::RCP<      Thyra_MultiVector>& dst,
         const CombineMode CM) const
{
  // There's a catch here!
  // Legend: V = Vector, MV = MultiVector, TV = Epetra_Vector, TMV = Epetra_MultiVector, T_xyz = Thyra_xyz
  // One can create a T_TV, then pass it to routines expecting a T_MV, since T_TV inherits from T_V,
  // which inherits from T_MV. However, T_TV does NOT inherit from T_TMV, so such routines would
  // try to cast the input to T_TMV and fail. This would be solved if T_TV also inherited
  // from T_TMV, but that's hard to do (without code duplication), since T_T(M)V store
  // ConstNonConstObj containers to the Epetra objects, which I _think_ do not support polymorphism.
  // So, given what we have, we _try_ to extract a TMV from the T_MV, and, if we fail,
  // we try again, this time extracting a TV. If we still fail, then we can error out.
  Teuchos::RCP<const Epetra_MultiVector> srcE = Albany::getConstEpetraMultiVector(src,false);
  Teuchos::RCP<Epetra_MultiVector> dstE = Albany::getEpetraMultiVector(dst,false);

  if (srcE.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Epetra_Vector
    Teuchos::RCP<const Thyra_Vector> srcV = Teuchos::rcp_dynamic_cast<const Thyra_Vector>(src);

    TEUCHOS_TEST_FOR_EXCEPTION (srcV.is_null(), std::runtime_error,
                                "Error! Input src does not seem to be a Epetra_MultiVector or a Epetra_Vector.\n");

    // This time throw if extraction fails
    srcE = Albany::getConstEpetraVector(srcV);
  }

  if (dstE.is_null()) {
    // Try to cast to Thyra_Vector, then extract the Epetra_Vector
    Teuchos::RCP<Thyra_Vector> dstV = Teuchos::rcp_dynamic_cast<Thyra_Vector>(dst);

    TEUCHOS_TEST_FOR_EXCEPTION (dstV.is_null(), std::runtime_error,
                                "Error! Input dst does not seem to be a Epetra_MultiVector or a Epetra_Vector.\n");

    // This time throw if extraction fails
    dstE = Albany::getEpetraVector(dstV);
  }

#ifdef ALBANY_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(!srcE->getMap()->isSameAs(*importer->getSourceMap()), std::runtime_error,
                             "Error! The map of the input src multi vector does not match the importer's source map.\n");
  TEUCHOS_TEST_FOR_EXCEPTION(!dstE->getMap()->isSameAs(*importer->getTargetMap()), std::runtime_error,
                             "Error! The map of the input dst multi vector does not match the importer's target map.\n");
#endif

  auto cmE = combineModeE(CM);
  dstE->Import(*srcE,*importer,cmE);
}

void CombineAndScatterManagerEpetra::
scatter (const Teuchos::RCP<const Thyra_LinearOp>& src,
         const Teuchos::RCP<      Thyra_LinearOp>& dst,
         const CombineMode CM) const
{
  auto cmE  = combineModeE(CM);
  auto srcE = Albany::getConstEpetraMatrix(src);
  auto dstE = Albany::getEpetraMatrix(dst);

#ifdef ALBANY_DEBUG
  TEUCHOS_TEST_FOR_EXCEPTION(!srcE->getMap()->isSameAs(*importer->getSourceMap()), std::runtime_error,
                             "Error! The row map of the input src matrix does not match the importer's source map.\n");
  TEUCHOS_TEST_FOR_EXCEPTION(!dstE->getMap()->isSameAs(*importer->getTargetMap()), std::runtime_error,
                             "Error! The row map of the input dst matrix does not match the importer's target map.\n");
#endif

  dstE->Import(*srcE,*importer,cmE);
}

} // namespace Albany
