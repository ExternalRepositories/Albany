//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <vector>
#include <string>

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Aeras_Layouts.hpp"

namespace Aeras {

template<typename EvalT, typename Traits>
ScatterResidualBase<EvalT, Traits>::
ScatterResidualBase(const Teuchos::ParameterList& p,
                    const Teuchos::RCP<Aeras::Layouts>& dl) :
  worksetSize(dl->node_scalar             ->dimension(0)),
  numNodes   (dl->node_scalar             ->dimension(1)),
  numLevels  (dl->node_scalar_level       ->dimension(2)), 
  numFields  (0), numNodeVar(0), numLevelVar(0), numTracerVar(0)
{
  const Teuchos::ArrayRCP<std::string> node_names       = p.get< Teuchos::ArrayRCP<std::string> >("Node Residual Names");
  const Teuchos::ArrayRCP<std::string> level_names      = p.get< Teuchos::ArrayRCP<std::string> >("Level Residual Names");
  const Teuchos::ArrayRCP<std::string> tracer_names     = p.get< Teuchos::ArrayRCP<std::string> >("Tracer Residual Names");

  numNodeVar   = node_names  .size();
  numLevelVar  = level_names .size();
  numTracerVar = tracer_names.size();
  numFields = numNodeVar +  numLevelVar + numTracerVar;

  val.resize(numFields);

  int eq = 0;
  for (int i = 0; i < numNodeVar; ++i, ++eq) {
    PHX::MDField<ScalarT,Cell,Node> mdf(node_names[i],dl->node_scalar);
    val[eq] = mdf;
    this->addDependentField(val[eq]);
  }   
  for (int i = 0; i < numLevelVar; ++i, ++eq) {
    PHX::MDField<ScalarT,Cell,Node> mdf(level_names[i],dl->node_scalar_level);
    val[eq] = mdf;
    this->addDependentField(val[eq]);
  }
  for (int i = 0; i < numTracerVar; ++i, ++eq) {
    PHX::MDField<ScalarT,Cell,Node> mdf(tracer_names[i],dl->node_scalar_level);
    val[eq] = mdf;
    this->addDependentField(val[eq]);
  }

  const std::string fieldName = p.get<std::string>("Scatter Field Name");
  scatter_operation = Teuchos::rcp(new PHX::Tag<ScalarT>(fieldName, dl->dummy));

  this->addEvaluatedField(*scatter_operation);

  this->setName(fieldName);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void ScatterResidualBase<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm) 
{
  for (int eq = 0; eq < numFields; ++eq) this->utils.setFieldData(val[eq],fm);
}

// **********************************************************************
// Specialization: Residual
// **********************************************************************
template<typename Traits>
ScatterResidual<PHAL::AlbanyTraits::Residual,Traits>::
ScatterResidual(const Teuchos::ParameterList& p,
                const Teuchos::RCP<Aeras::Layouts>& dl)
  : ScatterResidualBase<PHAL::AlbanyTraits::Residual,Traits>(p,dl)
{}

template<typename Traits>
void ScatterResidual<PHAL::AlbanyTraits::Residual, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<Tpetra_Vector> fT = workset.fT;
  //get non-const view of fT 
  Teuchos::ArrayRCP<ST> fT_nonconstView = fT->get1dViewNonConst();

  for (int cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];
    for (int node = 0; node < this->numNodes; ++node) {
      const Teuchos::ArrayRCP<int>& eqID  = nodeID[node];
      int n = 0, eq = 0;
      for (int j = eq; j < eq+this->numNodeVar; ++j, ++n) {
        fT_nonconstView[eqID[n]] += (this->val[j])(cell,node);
      }
      eq += this->numNodeVar;
//Irina TOFIX
/*
      for (int level = 0; level < this->numLevels; level++) { 
        for (int j = eq; j < eq+this->numLevelVar; ++j, ++n) {
          fT_nonconstView[eqID[n]] += (this->val[j])(cell,node,level);
        }
      }
      eq += this->numLevelVar;
      for (int level = 0; level < this->numLevels; ++level) { 
        for (int j = eq; j < eq+this->numTracerVar; ++j, ++n) {
          fT_nonconstView[eqID[n]] += (this->val[j])(cell,node,level);
        }
      }
      eq += this->numTracerVar;
*/    }
  }
}

// **********************************************************************
// Specialization: Jacobian
// **********************************************************************

template<typename Traits>
ScatterResidual<PHAL::AlbanyTraits::Jacobian, Traits>::
ScatterResidual(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Aeras::Layouts>& dl)
  : ScatterResidualBase<PHAL::AlbanyTraits::Jacobian,Traits>(p,dl)
{ }

// **********************************************************************
template<typename Traits>
void ScatterResidual<PHAL::AlbanyTraits::Jacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<Tpetra_Vector>      fT = workset.fT;
  Teuchos::RCP<Tpetra_CrsMatrix> JacT = workset.JacT;

  const bool loadResid = (fT != Teuchos::null);
  LO rowT; 
  Teuchos::Array<LO> colT; 

  for (int cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    const int neq = nodeID[0].size();
    colT.resize(neq * this->numNodes); 
    
    for (int node=0; node<this->numNodes; node++){
      for (int eq_col=0; eq_col<neq; eq_col++) {
        colT[neq * node + eq_col] =  nodeID[node][eq_col];
      }
    }


    for (int node = 0; node < this->numNodes; ++node) {
      const Teuchos::ArrayRCP<int>& eqID  = nodeID[node];
      int n = 0, eq = 0;
      for (int j = eq; j < eq+this->numNodeVar; ++j, ++n) {
<<<<<<< HEAD
        //const ScalarT *valptr = &(this->val[j])(cell,node);
        if (loadResid) f->SumIntoMyValue(eqID[n], 0, ((this->val[j])(cell,node)).val());
        if (((this->val[j])(cell,node)).hasFastAccess()) {
          if (workset.is_adjoint) {
            // Sum Jacobian transposed
            for (int i=0; i<col.size(); ++i)
              Jac->SumIntoMyValues(col[i], 1, &(((this->val[j])(cell,node)).fastAccessDx(i)), &eqID[n]);
          }
          else {
            // Sum Jacobian entries all at once
            Jac->SumIntoMyValues(eqID[n], col.size(), &(((this->val[j])(cell,node)).fastAccessDx(0)), &col[0]);
=======
        const ScalarT *valptr = &(this->val[j])(cell,node);
        rowT = eqID[n]; 
        if (loadResid) fT->sumIntoLocalValue(rowT, valptr->val());
        if (valptr->hasFastAccess()) {
          if (workset.is_adjoint) {
            // Sum Jacobian transposed
            for (unsigned int i=0; i<colT.size(); ++i) {
              //Jac->SumIntoMyValues(colT[i], 1, &(valptr->fastAccessDx(i)), &eqID[n]);
              JacT->sumIntoLocalValues(colT[i], Teuchos::arrayView(&rowT,1), Teuchos::arrayView(&(valptr->fastAccessDx(i)),1));
            }
          } else {
            // Sum Jacobian entries all at once
            //Jac->SumIntoMyValues(eqID[n], colT.size(), &(valptr->fastAccessDx(0)), &colT[0]);
            JacT->sumIntoLocalValues(rowT, colT, Teuchos::arrayView(&(valptr->fastAccessDx(0)), colT.size()));
>>>>>>> tpetra
          }
        } // has fast access
      }
      eq += this->numNodeVar;
//Irina TOFIX
/*
      for (int level = 0; level < this->numLevels; level++) { 
        for (int j = eq; j < eq+this->numLevelVar; ++j, ++n) {
<<<<<<< HEAD
          //const ScalarT *valptr = &(this->val[j])(cell,node,level);
          if (loadResid) f->SumIntoMyValue(eqID[n], 0, ((this->val[j])(cell,node,level)).val());
          if (((this->val[j])(cell,node,level)).hasFastAccess()) {
            if (workset.is_adjoint) {
              // Sum Jacobian transposed
              for (int i=0; i<col.size(); ++i)
                Jac->SumIntoMyValues(col[i], 1, &(((this->val[j])(cell,node,level)).fastAccessDx(i)), &eqID[n]);
            }
            else {
              // Sum Jacobian entries all at once
              Jac->SumIntoMyValues(eqID[n], col.size(), &(((this->val[j])(cell,node,level)).fastAccessDx(0)), &col[0]);
=======
          const ScalarT *valptr = &(this->val[j])(cell,node,level);
          if (loadResid) fT->sumIntoLocalValue(eqID[n], valptr->val());
          if (valptr->hasFastAccess()) {
            if (workset.is_adjoint) {
              // Sum Jacobian transposed
              for (unsigned int i=0; i<colT.size(); ++i) {
                //Jac->SumIntoMyValues(colT[i], 1, &(valptr->fastAccessDx(i)), &eqID[n]);
                JacT->sumIntoLocalValues(colT[i], Teuchos::arrayView(&rowT,1), Teuchos::arrayView(&(valptr->fastAccessDx(i)),1));
              }
            } else {
              // Sum Jacobian entries all at once
              //Jac->SumIntoMyValues(eqID[n], colT.size(), &(valptr->fastAccessDx(0)), &colT[0]);
              JacT->sumIntoLocalValues(rowT, colT, Teuchos::arrayView(&(valptr->fastAccessDx(0)), colT.size()));
>>>>>>> tpetra
            }
          } // has fast access
        }
      }
      eq += this->numLevelVar;
      for (int level = 0; level < this->numLevels; ++level) { 
        for (int j = eq; j < eq+this->numTracerVar; ++j, ++n) {
<<<<<<< HEAD
          //const ScalarT *valptr = &(this->val[j])(cell,node,level);
          if (loadResid) f->SumIntoMyValue(eqID[n], 0, ((this->val[j])(cell,node,level)).val());
          if (((this->val[j])(cell,node,level)).hasFastAccess()) {
            if (workset.is_adjoint) {
              // Sum Jacobian transposed
              for (int i=0; i<col.size(); ++i)
                Jac->SumIntoMyValues(col[i], 1, &(((this->val[j])(cell,node,level)).fastAccessDx(i)), &eqID[n]);
            }
            else {
              // Sum Jacobian entries all at once
              Jac->SumIntoMyValues(eqID[n], col.size(), &(((this->val[j])(cell,node,level)).fastAccessDx(0)), &col[0]);
=======
          const ScalarT *valptr = &(this->val[j])(cell,node,level);
          if (loadResid) fT->sumIntoLocalValue(eqID[n], valptr->val());
          if (valptr->hasFastAccess()) {
            if (workset.is_adjoint) {
              // Sum Jacobian transposed
              for (unsigned int i=0; i<colT.size(); ++i) {
                //Jac->SumIntoMyValues(colT[i], 1, &(valptr->fastAccessDx(i)), &eqID[n]);
                JacT->sumIntoLocalValues(colT[i], Teuchos::arrayView(&rowT,1), Teuchos::arrayView(&(valptr->fastAccessDx(i)),1));
              }
            } else {
              // Sum Jacobian entries all at once
              //Jac->SumIntoMyValues(eqID[n], colT.size(), &(valptr->fastAccessDx(0)), &colT[0]);
              JacT->sumIntoLocalValues(rowT, colT, Teuchos::arrayView(&(valptr->fastAccessDx(0)), colT.size()));
>>>>>>> tpetra
            }
          } // has fast access
        }
      }
      eq += this->numTracerVar;
*/    }
  }
}

// **********************************************************************
// Specialization: Tangent
// **********************************************************************

template<typename Traits>
ScatterResidual<PHAL::AlbanyTraits::Tangent, Traits>::
ScatterResidual(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Aeras::Layouts>& dl)
  : ScatterResidualBase<PHAL::AlbanyTraits::Tangent,Traits>(p,dl)
{ }

// **********************************************************************
template<typename Traits>
void ScatterResidual<PHAL::AlbanyTraits::Tangent, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<Tpetra_Vector>       fT = workset.fT;
  Teuchos::RCP<Tpetra_MultiVector> JVT = workset.JVT;
  Teuchos::RCP<Tpetra_MultiVector> fpT = workset.fpT;
  ScalarT *valptr;

  int rowT; 

  //IK, 6/27/14: I think you don't actually need row_map here the way this function is written right now...
  //const Epetra_BlockMap *row_map = NULL;
  //if (f != Teuchos::null)       row_map = &( f->Map());
  //else if (JV != Teuchos::null) row_map = &(JV->Map());
  //else if (fp != Teuchos::null) row_map = &(fp->Map());
  //else
  if ((fT == Teuchos::null) & (JVT == Teuchos::null) & (fpT == Teuchos::null)) {  
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                     "One of f, JV, or fp must be non-null! " << std::endl);
  }

  for (int cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (int node = 0; node < this->numNodes; ++node) {
      const Teuchos::ArrayRCP<int>& eqID  = nodeID[node];
      int n = 0, eq = 0;
      for (int j = eq; j < eq+this->numNodeVar; ++j, ++n) {
<<<<<<< HEAD
       // valptr = &(this->val[j])(cell,node);
        if (f != Teuchos::null) f->SumIntoMyValue(eqID[n], 0, ((this->val[j])(cell,node)).val());
        if (JV != Teuchos::null)
          for (int col=0; col<workset.num_cols_x; col++)
            JV->SumIntoMyValue(eqID[n], col, ((this->val[j])(cell,node)).dx(col));
        if (fp != Teuchos::null)
          for (int col=0; col<workset.num_cols_p; col++)
            fp->SumIntoMyValue(eqID[n], col, ((this->val[j])(cell,node)).dx(col+workset.param_offset));
=======
        valptr = &(this->val[j])(cell,node);
        rowT = eqID[n]; 
        if (fT != Teuchos::null) fT->sumIntoLocalValue(rowT, valptr->val());
        if (JVT != Teuchos::null)
          for (int col=0; col<workset.num_cols_x; col++)
            JVT->sumIntoLocalValue(rowT, col, valptr->dx(col));
        if (fpT != Teuchos::null)
          for (int col=0; col<workset.num_cols_p; col++)
            fpT->sumIntoLocalValue(rowT, col, valptr->dx(col+workset.param_offset));
>>>>>>> tpetra
      }
      eq += this->numNodeVar;
//Irina TOFIX
/*
      for (int level = 0; level < this->numLevels; level++) { 
        for (int j = eq; j < eq+this->numLevelVar; ++j, ++n) {
<<<<<<< HEAD
          //valptr = &(this->val[j])(cell,node,level);
          if (f != Teuchos::null) f->SumIntoMyValue(eqID[n], 0, ((this->val[j])(cell,node,level)).val());
          if (JV != Teuchos::null)
            for (int col=0; col<workset.num_cols_x; col++)
              JV->SumIntoMyValue(eqID[n], col, ((this->val[j])(cell,node,level)).dx(col));
          if (fp != Teuchos::null)
            for (int col=0; col<workset.num_cols_p; col++)
              fp->SumIntoMyValue(eqID[n], col, ((this->val[j])(cell,node,level)).dx(col+workset.param_offset));
=======
          valptr = &(this->val[j])(cell,node,level);
          if (fT != Teuchos::null) fT->sumIntoLocalValue(rowT, valptr->val());
          if (JVT != Teuchos::null)
            for (int col=0; col<workset.num_cols_x; col++)
              JVT->sumIntoLocalValue(rowT, col, valptr->dx(col));
          if (fpT != Teuchos::null)
            for (int col=0; col<workset.num_cols_p; col++)
              fpT->sumIntoLocalValue(rowT, col, valptr->dx(col+workset.param_offset));
>>>>>>> tpetra
        }
      }
      eq += this->numLevelVar;
      for (int level = 0; level < this->numLevels; ++level) { 
        for (int j = eq; j < eq+this->numTracerVar; ++j, ++n) {
<<<<<<< HEAD
          //valptr = &(this->val[j])(cell,node,level);
          if (f != Teuchos::null) f->SumIntoMyValue(eqID[n], 0, ((this->val[j])(cell,node,level)).val());
          if (JV != Teuchos::null)
            for (int col=0; col<workset.num_cols_x; col++)
              JV->SumIntoMyValue(eqID[n], col, ((this->val[j])(cell,node,level)).dx(col));
          if (fp != Teuchos::null)
            for (int col=0; col<workset.num_cols_p; col++)
              fp->SumIntoMyValue(eqID[n], col, ((this->val[j])(cell,node,level)).dx(col+workset.param_offset));
=======
          valptr = &(this->val[j])(cell,node,level);
          if (fT != Teuchos::null) fT->sumIntoLocalValue(rowT, valptr->val());
          if (JVT != Teuchos::null)
            for (int col=0; col<workset.num_cols_x; col++)
              JVT->sumIntoLocalValue(rowT, col, valptr->dx(col));
          if (fpT != Teuchos::null)
            for (int col=0; col<workset.num_cols_p; col++)
              fpT->sumIntoLocalValue(rowT, col, valptr->dx(col+workset.param_offset));
>>>>>>> tpetra
        }
      }
      eq += this->numTracerVar;
*/    }
  }
}


}
