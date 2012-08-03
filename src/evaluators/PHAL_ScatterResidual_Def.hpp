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


#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

// **********************************************************************
// Base Class Generic Implemtation
// **********************************************************************
namespace PHAL {

template<typename EvalT, typename Traits>
ScatterResidualBase<EvalT, Traits>::
ScatterResidualBase(const Teuchos::ParameterList& p)
{
  std::string fieldName;
  if (p.isType<std::string>("Scatter Field Name"))
    fieldName = p.get<std::string>("Scatter Field Name");
  else fieldName = "Scatter";
  
  scatter_operation = Teuchos::rcp(new PHX::Tag<ScalarT>
    (fieldName, p.get< Teuchos::RCP<PHX::DataLayout> >("Dummy Data Layout")));

  const Teuchos::ArrayRCP<std::string>& names =
    p.get< Teuchos::ArrayRCP<std::string> >("Residual Names");

  Teuchos::RCP<PHX::DataLayout> dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("Data Layout");

  if (p.isType<bool>("Vector Field"))
	  vectorField = p.get<bool>("Vector Field");
  else vectorField = false;

  // scalar
  if (!vectorField) {
    numFieldsBase = names.size();
    const std::size_t num_val = numFieldsBase;
    val.resize(num_val);
    for (std::size_t eq = 0; eq < numFieldsBase; ++eq) {
      PHX::MDField<ScalarT,Cell,Node> mdf(names[eq],dl);
      val[eq] = mdf;
      this->addDependentField(val[eq]);
    }
  }

  // vector
  else {
    valVec.resize(1);
    PHX::MDField<ScalarT,Cell,Node,Dim> mdf(names[0],dl);
    valVec[0] = mdf;
    this->addDependentField(valVec[0]);
    numFieldsBase = dl->dimension(2);
  }

  if (p.isType<int>("Offset of First DOF"))
    offset = p.get<int>("Offset of First DOF");
  else offset = 0;

  this->addEvaluatedField(*scatter_operation);

  this->setName(fieldName+PHX::TypeString<EvalT>::value);
}

// **********************************************************************
template<typename EvalT, typename Traits>
void ScatterResidualBase<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  if (!vectorField) {
    for (std::size_t eq = 0; eq < numFieldsBase; ++eq)
      this->utils.setFieldData(val[eq],fm);
    numNodes = val[0].dimension(1);
  }
  else {
    this->utils.setFieldData(valVec[0],fm);
    numNodes = valVec[0].dimension(1);
  }
}

// **********************************************************************
// Specialization: Residual
// **********************************************************************
template<typename Traits>
ScatterResidual<PHAL::AlbanyTraits::Residual,Traits>::
ScatterResidual(const Teuchos::ParameterList& p)
  : ScatterResidualBase<PHAL::AlbanyTraits::Residual,Traits>(p),
  numFields(ScatterResidualBase<PHAL::AlbanyTraits::Residual,Traits>::numFieldsBase)

{
}

template<typename Traits>
void ScatterResidual<PHAL::AlbanyTraits::Residual, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<Epetra_Vector> f = workset.f;
  ScalarT *valptr;

  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {

      for (std::size_t eq = 0; eq < numFields; eq++) {
    	if (this->vectorField) valptr = &(this->valVec[0])(cell,node,eq);
        else                   valptr = &(this->val[eq])(cell,node);
	(*f)[nodeID[node][this->offset + eq]] += *valptr;
      }
    }
  }
}

// **********************************************************************
// Specialization: Jacobian
// **********************************************************************

template<typename Traits>
ScatterResidual<PHAL::AlbanyTraits::Jacobian, Traits>::
ScatterResidual(const Teuchos::ParameterList& p)
  : ScatterResidualBase<PHAL::AlbanyTraits::Jacobian,Traits>(p),
  numFields(ScatterResidualBase<PHAL::AlbanyTraits::Jacobian,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void ScatterResidual<PHAL::AlbanyTraits::Jacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<Epetra_Vector> f = workset.f;
  Teuchos::RCP<Epetra_CrsMatrix> Jac = workset.Jac;
  ScalarT *valptr;

  int row, lcol, col;

  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {
     
      for (std::size_t eq = 0; eq < numFields; eq++) {
    	  if (this->vectorField) valptr = &(this->valVec[0])(cell,node,eq);
    	  else                   valptr = &(this->val[eq])(cell,node);

        row = nodeID[node][this->offset + eq];
        int neq = nodeID[node].size();
        if (f != Teuchos::null) {
          f->SumIntoMyValue(row, 0, valptr->val());
        }

        // Check derivative array is nonzero
        if (valptr->hasFastAccess()) {

          // Loop over nodes in element
          for (unsigned int node_col=0; node_col<this->numNodes; node_col++){

            // Loop over equations per node
            for (unsigned int eq_col=0; eq_col<neq; eq_col++) {
              lcol = neq * node_col + eq_col;

              // Global column
              col =  nodeID[node_col][eq_col];
              
              if (workset.is_adjoint) {
                // Sum Jacobian transposed
                Jac->SumIntoMyValues(col, 1, &(valptr->fastAccessDx(lcol)), &row);
              }
              else {
                // Sum Jacobian
                Jac->SumIntoMyValues(row, 1, &(valptr->fastAccessDx(lcol)), &col);
              }
            } // column equations
          } // column nodes
        } // has fast access
      }
    }
  }
}

// **********************************************************************
// Specialization: Tangent
// **********************************************************************

template<typename Traits>
ScatterResidual<PHAL::AlbanyTraits::Tangent, Traits>::
ScatterResidual(const Teuchos::ParameterList& p)
  : ScatterResidualBase<PHAL::AlbanyTraits::Tangent,Traits>(p),
  numFields(ScatterResidualBase<PHAL::AlbanyTraits::Tangent,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void ScatterResidual<PHAL::AlbanyTraits::Tangent, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<Epetra_Vector> f = workset.f;
  Teuchos::RCP<Epetra_MultiVector> JV = workset.JV;
  Teuchos::RCP<Epetra_MultiVector> fp = workset.fp;
  ScalarT *valptr;

  const Epetra_BlockMap *row_map = NULL;
  if (f != Teuchos::null)
    row_map = &(f->Map());
  else if (JV != Teuchos::null)
    row_map = &(JV->Map());
  else if (fp != Teuchos::null)
    row_map = &(fp->Map());
  else
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                     "One of f, JV, or fp must be non-null! " << std::endl);

  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {
      for (std::size_t eq = 0; eq < numFields; eq++) {
    	  if (this->vectorField) valptr = &(this->valVec[0])(cell,node,eq);
    	  else                   valptr = &(this->val[eq])(cell,node);

        int row = nodeID[node][this->offset + eq];

        if (f != Teuchos::null)
          f->SumIntoMyValue(row, 0, valptr->val());

	if (JV != Teuchos::null)
	  for (int col=0; col<workset.num_cols_x; col++)
	    JV->SumIntoMyValue(row, col, valptr->dx(col));

	if (fp != Teuchos::null)
	  for (int col=0; col<workset.num_cols_p; col++)
	    fp->SumIntoMyValue(row, col, valptr->dx(col+workset.param_offset));
      }
    }
  }
}

// **********************************************************************
// Specialization: Stochastic Galerkin Residual
// **********************************************************************

template<typename Traits>
ScatterResidual<PHAL::AlbanyTraits::SGResidual, Traits>::
ScatterResidual(const Teuchos::ParameterList& p)
  : ScatterResidualBase<PHAL::AlbanyTraits::SGResidual,Traits>(p),
  numFields(ScatterResidualBase<PHAL::AlbanyTraits::SGResidual,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void ScatterResidual<PHAL::AlbanyTraits::SGResidual, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly > f = workset.sg_f;
  ScalarT *valptr;

  int nblock = f->size();
  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {

      for (std::size_t eq = 0; eq < numFields; eq++) {
    	  if (this->vectorField) valptr = &(this->valVec[0])(cell,node,eq);
    	  else                   valptr = &(this->val[eq])(cell,node);
	for (int block=0; block<nblock; block++)
	  (*f)[block][nodeID[node][this->offset + eq]] += valptr->coeff(block);
      }
    }
  }
}

// **********************************************************************
// Specialization: Stochastic Galerkin Jacobian
// **********************************************************************

template<typename Traits>
ScatterResidual<PHAL::AlbanyTraits::SGJacobian, Traits>::
ScatterResidual(const Teuchos::ParameterList& p)
  : ScatterResidualBase<PHAL::AlbanyTraits::SGJacobian,Traits>(p),
  numFields(ScatterResidualBase<PHAL::AlbanyTraits::SGJacobian,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void ScatterResidual<PHAL::AlbanyTraits::SGJacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly > f = workset.sg_f;
  Teuchos::RCP< Stokhos::VectorOrthogPoly<Epetra_CrsMatrix> > Jac = 
    workset.sg_Jac;
  ScalarT *valptr;

  int row, lcol, col;
  int nblock = 0;
  if (f != Teuchos::null)
    nblock = f->size();
  int nblock_jac = Jac->size();
  double c; // use double since it goes into CrsMatrix

  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {

      for (std::size_t eq = 0; eq < numFields; eq++) {
    	  if (this->vectorField) valptr = &(this->valVec[0])(cell,node,eq);
    	  else                   valptr = &(this->val[eq])(cell,node);

        row = nodeID[node][this->offset + eq];
        int neq = nodeID[node].size();

        if (f != Teuchos::null) {
	  for (int block=0; block<nblock; block++)
	    (*f)[block].SumIntoMyValue(row, 0, valptr->val().coeff(block));
        }

        // Check derivative array is nonzero
        if (valptr->hasFastAccess()) {

          // Loop over nodes in element
          for (unsigned int node_col=0; node_col<this->numNodes; node_col++){

            // Loop over equations per node
            for (unsigned int eq_col=0; eq_col<neq; eq_col++) {
              lcol = neq * node_col + eq_col;

              // Global column
              col =  nodeID[node_col][eq_col];

              // Sum Jacobian
	      for (int block=0; block<nblock_jac; block++) {
		c = valptr->fastAccessDx(lcol).coeff(block);
                if (workset.is_adjoint) { 
		  (*Jac)[block].SumIntoMyValues(col, 1, &c, &row);
                }
                else {
		  (*Jac)[block].SumIntoMyValues(row, 1, &c, &col);
                }
	      }
            } // column equations
          } // column nodes
        } // has fast access
      }
    }
  }
}

// **********************************************************************
// Specialization: Stochastic Galerkin Tangent
// **********************************************************************

template<typename Traits>
ScatterResidual<PHAL::AlbanyTraits::SGTangent, Traits>::
ScatterResidual(const Teuchos::ParameterList& p)
  : ScatterResidualBase<PHAL::AlbanyTraits::SGTangent,Traits>(p),
  numFields(ScatterResidualBase<PHAL::AlbanyTraits::SGTangent,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void ScatterResidual<PHAL::AlbanyTraits::SGTangent, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly > f = workset.sg_f;
  Teuchos::RCP< Stokhos::EpetraMultiVectorOrthogPoly > JV = workset.sg_JV;
  Teuchos::RCP< Stokhos::EpetraMultiVectorOrthogPoly > fp = workset.sg_fp;
  ScalarT *valptr;

  int nblock = 0;
  if (f != Teuchos::null)
    nblock = f->size();
  else if (JV != Teuchos::null)
    nblock = JV->size();
  else if (fp != Teuchos::null)
    nblock = fp->size();
  else
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
		       "One of sg_f, sg_JV, or sg_fp must be non-null! " << 
		       std::endl);

  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {
      for (std::size_t eq = 0; eq < numFields; eq++) {
    	  if (this->vectorField) valptr = &(this->valVec[0])(cell,node,eq);
    	  else                   valptr = &(this->val[eq])(cell,node);

        int row = nodeID[node][this->offset + eq];

        if (f != Teuchos::null)
	  for (int block=0; block<nblock; block++)
	    (*f)[block].SumIntoMyValue(row, 0, valptr->val().coeff(block));

	if (JV != Teuchos::null)
	  for (int col=0; col<workset.num_cols_x; col++)
	    for (int block=0; block<nblock; block++)
	      (*JV)[block].SumIntoMyValue(row, col, valptr->dx(col).coeff(block));

	if (fp != Teuchos::null)
	  for (int col=0; col<workset.num_cols_p; col++)
	    for (int block=0; block<nblock; block++)
	      (*fp)[block].SumIntoMyValue(row, col, valptr->dx(col+workset.param_offset).coeff(block));
      }
    }
  }
}

// **********************************************************************
// Specialization: Multi-point Residual
// **********************************************************************

template<typename Traits>
ScatterResidual<PHAL::AlbanyTraits::MPResidual, Traits>::
ScatterResidual(const Teuchos::ParameterList& p)
  : ScatterResidualBase<PHAL::AlbanyTraits::MPResidual,Traits>(p),
  numFields(ScatterResidualBase<PHAL::AlbanyTraits::MPResidual,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void ScatterResidual<PHAL::AlbanyTraits::MPResidual, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP< Stokhos::ProductEpetraVector > f = workset.mp_f;
  ScalarT *valptr;

  int nblock = f->size();
  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {

      for (std::size_t eq = 0; eq < numFields; eq++) {
    	  if (this->vectorField) valptr = &(this->valVec[0])(cell,node,eq);
    	  else                   valptr = &(this->val[eq])(cell,node);
	for (int block=0; block<nblock; block++)
	  (*f)[block][nodeID[node][this->offset + eq]] += valptr->coeff(block);
      }
    }
  }
}

// **********************************************************************
// Specialization: Multi-point Jacobian
// **********************************************************************

template<typename Traits>
ScatterResidual<PHAL::AlbanyTraits::MPJacobian, Traits>::
ScatterResidual(const Teuchos::ParameterList& p)
  : ScatterResidualBase<PHAL::AlbanyTraits::MPJacobian,Traits>(p),
  numFields(ScatterResidualBase<PHAL::AlbanyTraits::MPJacobian,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void ScatterResidual<PHAL::AlbanyTraits::MPJacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP< Stokhos::ProductEpetraVector > f = workset.mp_f;
  Teuchos::RCP< Stokhos::ProductContainer<Epetra_CrsMatrix> > Jac = 
    workset.mp_Jac;
  ScalarT *valptr;

  int row, lcol, col;
  int nblock = 0;
  if (f != Teuchos::null)
    nblock = f->size();
  int nblock_jac = Jac->size();
  double c; // use double since it goes into CrsMatrix

  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {

      for (std::size_t eq = 0; eq < numFields; eq++) {
    	  if (this->vectorField) valptr = &(this->valVec[0])(cell,node,eq);
    	  else                   valptr = &(this->val[eq])(cell,node);

        row = nodeID[node][this->offset + eq];
        int neq = nodeID[node].size();

        if (f != Teuchos::null) {
	  for (int block=0; block<nblock; block++)
	    (*f)[block].SumIntoMyValue(row, 0, valptr->val().coeff(block));
        }

        // Check derivative array is nonzero
        if (valptr->hasFastAccess()) {

          // Loop over nodes in element
          for (unsigned int node_col=0; node_col<this->numNodes; node_col++){

            // Loop over equations per node
            for (unsigned int eq_col=0; eq_col<neq; eq_col++) {
              lcol = neq * node_col + eq_col;

              // Global column
              col =  nodeID[node_col][eq_col];

              // Sum Jacobian
	      for (int block=0; block<nblock_jac; block++) {
		c = valptr->fastAccessDx(lcol).coeff(block);
		(*Jac)[block].SumIntoMyValues(row, 1, &c, &col);
	      }
            } // column equations
          } // column nodes
        } // has fast access
      }
    }
  }
}

// **********************************************************************
// Specialization: Multi-point Tangent
// **********************************************************************

template<typename Traits>
ScatterResidual<PHAL::AlbanyTraits::MPTangent, Traits>::
ScatterResidual(const Teuchos::ParameterList& p)
  : ScatterResidualBase<PHAL::AlbanyTraits::MPTangent,Traits>(p),
  numFields(ScatterResidualBase<PHAL::AlbanyTraits::MPTangent,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void ScatterResidual<PHAL::AlbanyTraits::MPTangent, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP< Stokhos::ProductEpetraVector > f = workset.mp_f;
  Teuchos::RCP< Stokhos::ProductEpetraMultiVector > JV = workset.mp_JV;
  Teuchos::RCP< Stokhos::ProductEpetraMultiVector > fp = workset.mp_fp;
  ScalarT *valptr;

  int nblock = 0;
  if (f != Teuchos::null)
    nblock = f->size();
  else if (JV != Teuchos::null)
    nblock = JV->size();
  else if (fp != Teuchos::null)
    nblock = fp->size();
  else
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
		       "One of mp_f, mp_JV, or mp_fp must be non-null! " << 
		       std::endl);

  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {
      for (std::size_t eq = 0; eq < numFields; eq++) {
    	  if (this->vectorField) valptr = &(this->valVec[0])(cell,node,eq);
    	  else                   valptr = &(this->val[eq])(cell,node);

        int row = nodeID[node][this->offset + eq];

        if (f != Teuchos::null)
	  for (int block=0; block<nblock; block++)
	    (*f)[block].SumIntoMyValue(row, 0, valptr->val().coeff(block));

	if (JV != Teuchos::null)
	  for (int col=0; col<workset.num_cols_x; col++)
	    for (int block=0; block<nblock; block++)
	      (*JV)[block].SumIntoMyValue(row, col, valptr->dx(col).coeff(block));

	if (fp != Teuchos::null)
	  for (int col=0; col<workset.num_cols_p; col++)
	    for (int block=0; block<nblock; block++)
	      (*fp)[block].SumIntoMyValue(row, col, valptr->dx(col+workset.param_offset).coeff(block));
      }
    }
  }
}

}

