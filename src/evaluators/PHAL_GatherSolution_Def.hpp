//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

//IK, 9/13/14: only Epetra is SG and MP 

#include <vector>
#include <string>

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"

namespace PHAL {

template<typename EvalT, typename Traits>
GatherSolutionBase<EvalT,Traits>::
GatherSolutionBase(const Teuchos::ParameterList& p,
                   const Teuchos::RCP<Albany::Layouts>& dl): numNodes(0)
{
  
  if (p.isType<int>("Tensor Rank"))
    tensorRank = p.get<int>("Tensor Rank");
  else
  if (p.isType<bool>("Vector Field")){
    if (p.get<bool>("Vector Field") == true)
      tensorRank = 1;
    else tensorRank = 0;
  }

  if (p.isType<bool>("Disable Transient"))
    enableTransient = !p.get<bool>("Disable Transient");
  else enableTransient = true;

  if (p.isType<bool>("Enable Acceleration"))
    enableAcceleration = p.get<bool>("Enable Acceleration");
  else enableAcceleration = false;

  Teuchos::ArrayRCP<std::string> solution_names;
  if (p.getEntryPtr("Solution Names")) {
    solution_names = p.get< Teuchos::ArrayRCP<std::string> >("Solution Names");
  }

  // scalar
  if ( tensorRank == 0 ) {
    val.resize(solution_names.size());
    for (std::size_t eq = 0; eq < solution_names.size(); ++eq) {
      PHX::MDField<ScalarT,Cell,Node> f(solution_names[eq],dl->node_scalar);
      val[eq] = f;
      this->addEvaluatedField(val[eq]);
    }
    // repeat for xdot if transient is enabled
    if (enableTransient) {
      const Teuchos::ArrayRCP<std::string>& names_dot =
        p.get< Teuchos::ArrayRCP<std::string> >("Time Dependent Solution Names");

      val_dot.resize(names_dot.size());
      for (std::size_t eq = 0; eq < names_dot.size(); ++eq) {
        PHX::MDField<ScalarT,Cell,Node> f(names_dot[eq],dl->node_scalar);
        val_dot[eq] = f;
        this->addEvaluatedField(val_dot[eq]);
      }
    }
    // repeat for xdotdot if acceleration is enabled
    if (enableAcceleration) {
      const Teuchos::ArrayRCP<std::string>& names_dotdot =
        p.get< Teuchos::ArrayRCP<std::string> >("Solution Acceleration Names");

      val_dotdot.resize(names_dotdot.size());
      for (std::size_t eq = 0; eq < names_dotdot.size(); ++eq) {
        PHX::MDField<ScalarT,Cell,Node> f(names_dotdot[eq],dl->node_scalar);
        val_dotdot[eq] = f;
        this->addEvaluatedField(val_dotdot[eq]);
      }
    }
    numFieldsBase = val.size();
  }
  // vector
  else 
  if ( tensorRank == 1 ) {
    //valVec.resize(1);
    PHX::MDField<ScalarT,Cell,Node,VecDim> f(solution_names[0],dl->node_vector);
    valVec= f;
    this->addEvaluatedField(valVec);
    // repeat for xdot if transient is enabled
    if (enableTransient) {
      const Teuchos::ArrayRCP<std::string>& names_dot =
        p.get< Teuchos::ArrayRCP<std::string> >("Time Dependent Solution Names");

    //  valVec_dot.resize(1);
      PHX::MDField<ScalarT,Cell,Node,VecDim> f(names_dot[0],dl->node_vector);
      valVec_dot= f;
      this->addEvaluatedField(valVec_dot);
    }
    // repeat for xdotdot if acceleration is enabled
    if (enableAcceleration) {
      const Teuchos::ArrayRCP<std::string>& names_dotdot =
        p.get< Teuchos::ArrayRCP<std::string> >("Solution Acceleration Names");

     // valVec_dotdot.resize(1);
      PHX::MDField<ScalarT,Cell,Node,VecDim> f(names_dotdot[0],dl->node_vector);
      valVec_dotdot = f;
      this->addEvaluatedField(valVec_dotdot);
    }
    numFieldsBase = dl->node_vector->dimension(2);
  }
  // tensor
  else 
  if ( tensorRank == 2 ) {
    valTensor.resize(1);
    PHX::MDField<ScalarT,Cell,Node,VecDim,VecDim> f(solution_names[0],dl->node_tensor);
    valTensor[0] = f;
    this->addEvaluatedField(valTensor[0]);
    // repeat for xdot if transient is enabled
    if (enableTransient) {
      const Teuchos::ArrayRCP<std::string>& names_dot =
        p.get< Teuchos::ArrayRCP<std::string> >("Time Dependent Solution Names");

      valTensor_dot.resize(1);
      PHX::MDField<ScalarT,Cell,Node,VecDim,VecDim> f(names_dot[0],dl->node_tensor);
      valTensor_dot[0] = f;
      this->addEvaluatedField(valTensor_dot[0]);
    }
    // repeat for xdotdot if acceleration is enabled
    if (enableAcceleration) {
      const Teuchos::ArrayRCP<std::string>& names_dotdot =
        p.get< Teuchos::ArrayRCP<std::string> >("Solution Acceleration Names");

      valTensor_dotdot.resize(1);
      PHX::MDField<ScalarT,Cell,Node,VecDim,VecDim> f(names_dotdot[0],dl->node_tensor);
      valTensor_dotdot[0] = f;
      this->addEvaluatedField(valTensor_dotdot[0]);
    }
    numFieldsBase = (dl->node_tensor->dimension(2))*(dl->node_tensor->dimension(3));
  }

  if (p.isType<int>("Offset of First DOF"))
    offset = p.get<int>("Offset of First DOF");
  else offset = 0;

  Index=Kokkos::View <int***, PHX::Device>("Index_kokkos", dl->node_vector->dimension(0), dl->node_vector->dimension(1), dl->node_vector->dimension(2));

  this->setName("Gather Solution"+PHX::typeAsString<EvalT>() );
}

// **********************************************************************
template<typename EvalT, typename Traits>
void GatherSolutionBase<EvalT,Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  if (tensorRank == 0) {
    for (std::size_t eq = 0; eq < numFieldsBase; ++eq)
      this->utils.setFieldData(val[eq],fm);
    if (enableTransient) {
      for (std::size_t eq = 0; eq < val_dot.size(); ++eq)
        this->utils.setFieldData(val_dot[eq],fm);
    }
    if (enableAcceleration) {
      for (std::size_t eq = 0; eq < val_dotdot.size(); ++eq)
        this->utils.setFieldData(val_dotdot[eq],fm);
    }
    numNodes = val[0].dimension(1);
  }
  else 
  if (tensorRank == 1) {
    this->utils.setFieldData(valVec,fm);
    if (enableTransient) this->utils.setFieldData(valVec_dot,fm);
    if (enableAcceleration) this->utils.setFieldData(valVec_dotdot,fm);
    numNodes = valVec.dimension(1);
  }
  else 
  if (tensorRank == 2) {
    this->utils.setFieldData(valTensor[0],fm);
    if (enableTransient) this->utils.setFieldData(valTensor_dot[0],fm);
    if (enableAcceleration) this->utils.setFieldData(valTensor_dotdot[0],fm);
    numNodes = valTensor[0].dimension(1);
  }
}

// **********************************************************************

// **********************************************************************
// Specialization: Residual
// **********************************************************************

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::Residual, Traits>::
GatherSolution(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl) :
  GatherSolutionBase<PHAL::AlbanyTraits::Residual, Traits>(p,dl),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::Residual,Traits>::numFieldsBase)
{
}

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::Residual, Traits>::
GatherSolution(const Teuchos::ParameterList& p) :
  GatherSolutionBase<PHAL::AlbanyTraits::Residual, Traits>(p,p.get<Teuchos::RCP<Albany::Layouts> >("Layouts Struct")),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::Residual,Traits>::numFieldsBase)
{
}
// ********************************************************************
//Kokkos functors for Residual
template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Residual, Traits>::
operator() (const tensorRank_1Tag& tag, const int& i) const{
  
  for (int node = 0; node < this->numNodes; ++node) 
    for (int eq = 0; eq < numFields; eq++) 
     (this->valVec)(i,node,eq)= xT_constView[wsID_kokkos(i, node,this->offset+eq)];
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Residual, Traits>::
operator() (const tensorRank_1_enableTransientTag& tag, const int& i) const{

  for (int node = 0; node < this->numNodes; ++node)
    for (int eq = 0; eq < numFields; eq++)
     (this->valVec_dot)(i,node,eq)= xdotT_constView[wsID_kokkos(i, node, this->offset+eq)];
}


template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Residual, Traits>::
operator() (const tensorRank_1_enableAccelerationTag& tag, const int& i) const{

  for (int node = 0; node < this->numNodes; ++node)
    for (int eq = 0; eq < numFields; eq++)
     (this->valVec_dotdot)(i,node,eq)= xdotdotT_constView[wsID_kokkos(i, node, this->offset+eq)];
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Residual, Traits>::
operator() (const tensorRank_2Tag& tag, const int& i) const{

  for (int node = 0; node < this->numNodes; ++node)
    for (int eq = 0; eq < numFields; eq++)
        (this->valTensor[0])(i,node,eq/numDim,eq%numDim)= xT_constView[wsID_kokkos(i, node, this->offset+eq)];
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Residual, Traits>::
operator() (const tensorRank_2_enableTransientTag& tag, const int& i) const{

  for (int node = 0; node < this->numNodes; ++node)
    for (int eq = 0; eq < numFields; eq++)
       (this->valTensor_dot[0])(i,node,eq/numDim,eq%numDim)= xdotT_constView[wsID_kokkos (i, node, this->offset+eq)];
}


template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Residual, Traits>::
operator() (const tensorRank_2_enableAccelerationTag& tag, const int& i) const{


  for (int node = 0; node < this->numNodes; ++node)
    for (int eq = 0; eq < numFields; eq++)
       (this->valTensor_dotdot[0])(i,node,eq/numDim,eq%numDim)= xdotdotT_constView[wsID_kokkos(i, node, this->offset+eq)];
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Residual, Traits>::
operator() (const tensorRank_0Tag& tag, const int& i) const{

  for (int node = 0; node < this->numNodes; ++node)
    for (int eq = 0; eq < numFields; eq++)
        (this->val[eq])(i,node)= xT_constView[wsID_kokkos(i, node, this->offset+eq)];
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Residual, Traits>::
operator() (const tensorRank_0_enableTransientTag& tag, const int& i) const{

  for (int node = 0; node < this->numNodes; ++node)
    for (int eq = 0; eq < numFields; eq++)
       (this->val_dot[eq])(i,node)= xdotT_constView[wsID_kokkos (i, node, this->offset+eq)];
}


template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Residual, Traits>::
operator() (const tensorRank_0_enableAccelerationTag& tag, const int& i) const{

  for (int node = 0; node < this->numNodes; ++node)
    for (int eq = 0; eq < numFields; eq++)
       (this->val_dotdot[eq])(i,node)= xdotdotT_constView[wsID_kokkos(i, node, this->offset+eq)];
}


// **********************************************************************
template<typename Traits>
void GatherSolution<PHAL::AlbanyTraits::Residual, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<const Tpetra_Vector> xT = workset.xT;
  Teuchos::RCP<const Tpetra_Vector> xdotT = workset.xdotT;
  Teuchos::RCP<const Tpetra_Vector> xdotdotT = workset.xdotdotT;

 //In purpose to use Kokkos functores declaration of the temporary data has been mooved to the class
  //get const (read-only) view of xT and xdotT
//  Teuchos::ArrayRCP<const ST> xT_constView = xT->get1dView();
//  Teuchos::ArrayRCP<const ST> xdotT_constView = xdotT->get1dView();
//  Teuchos::ArrayRCP<const ST> xdotdotT_constView = xdotdotT->get1dView();
  
  xT_constView = xT->get1dView();
  xdotT_constView = xdotT->get1dView();
  xdotdotT_constView = xdotdotT->get1dView();

#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
  if (this->tensorRank == 1) {
    for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
      const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];
      for (std::size_t node = 0; node < this->numNodes; ++node) {
      const Teuchos::ArrayRCP<int>& eqID  = nodeID[node];
        for (std::size_t eq = 0; eq < numFields; eq++) 
          (this->valVec)(cell,node,eq) = xT_constView[eqID[this->offset + eq]];
        if (workset.transientTerms && this->enableTransient) {
          for (std::size_t eq = 0; eq < numFields; eq++) 
            (this->valVec_dot)(cell,node,eq) = xdotT_constView[eqID[this->offset + eq]];
        }
        if (workset.accelerationTerms && this->enableAcceleration) {
          for (std::size_t eq = 0; eq < numFields; eq++) 
            (this->valVec_dotdot)(cell,node,eq) = xdotdotT_constView[eqID[this->offset + eq]];
        }
      }
    }
  } else 
  if (this->tensorRank == 2) {
    int numDim = this->valTensor[0].dimension(2);
    for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
      const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

      for (std::size_t node = 0; node < this->numNodes; ++node) {
      const Teuchos::ArrayRCP<int>& eqID  = nodeID[node];
        for (std::size_t eq = 0; eq < numFields; eq++) 
          (this->valTensor[0])(cell,node,eq/numDim,eq%numDim) = xT_constView[eqID[this->offset + eq]];
        if (workset.transientTerms && this->enableTransient) {
          for (std::size_t eq = 0; eq < numFields; eq++) 
            (this->valTensor_dot[0])(cell,node,eq/numDim,eq%numDim) = xdotT_constView[eqID[this->offset + eq]];
        }
        if (workset.accelerationTerms && this->enableAcceleration) {
          for (std::size_t eq = 0; eq < numFields; eq++) 
            (this->valTensor_dotdot[0])(cell,node,eq/numDim,eq%numDim) = xdotdotT_constView[eqID[this->offset + eq]];
        }
      }
    }
  } else {
    for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
      const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

      for (std::size_t node = 0; node < this->numNodes; ++node) {
      const Teuchos::ArrayRCP<int>& eqID  = nodeID[node];
        for (std::size_t eq = 0; eq < numFields; eq++) 
          (this->val[eq])(cell,node) = xT_constView[eqID[this->offset + eq]];
        if (workset.transientTerms && this->enableTransient) {
          for (std::size_t eq = 0; eq < numFields; eq++) 
            (this->val_dot[eq])(cell,node) = xdotT_constView[eqID[this->offset + eq]];
        }
        if (workset.accelerationTerms && this->enableAcceleration) {
          for (std::size_t eq = 0; eq < numFields; eq++) 
            (this->val_dotdot[eq])(cell,node) = xdotdotT_constView[eqID[this->offset + eq]];
        }
      }
    }
  }
#else

 wsID_kokkos=workset.wsElNodeEqID_kokkos;

   if (this->tensorRank == 2){

     numDim = this->valTensor[0].dimension(2);

     Kokkos::parallel_for(tensorRank_2Policy(0,workset.numCells),*this);

     if (workset.transientTerms && this->enableTransient) 
        Kokkos::parallel_for(tensorRank_2_enableTransientPolicy(0,workset.numCells),*this);    
     if (workset.accelerationTerms && this->enableAcceleration) 
        Kokkos::parallel_for(tensorRank_2_enableAccelerationPolicy(0,workset.numCells),*this);

   } else if (this->tensorRank == 1){
 
     Kokkos::parallel_for(tensorRank_1Policy(0,workset.numCells),*this);
  
     if (workset.transientTerms && this->enableTransient) 
       Kokkos::parallel_for(tensorRank_1_enableTransientPolicy(0,workset.numCells),*this);
     if (workset.accelerationTerms && this->enableAcceleration) 
        Kokkos::parallel_for(tensorRank_1_enableAccelerationPolicy(0,workset.numCells),*this);
 
   } else  {

     Kokkos::parallel_for(tensorRank_0Policy(0,workset.numCells),*this);

     if (workset.transientTerms && this->enableTransient) 
        Kokkos::parallel_for(tensorRank_0_enableTransientPolicy(0,workset.numCells),*this);
     if (workset.accelerationTerms && this->enableAcceleration) 
        Kokkos::parallel_for(tensorRank_0_enableAccelerationPolicy(0,workset.numCells),*this);
   
   }

#endif

}

// **********************************************************************
// Specialization: Jacobian
// **********************************************************************

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
GatherSolution(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl) :
  GatherSolutionBase<PHAL::AlbanyTraits::Jacobian, Traits>(p,dl),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::Jacobian,Traits>::numFieldsBase)
{
}

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
GatherSolution(const Teuchos::ParameterList& p) :
  GatherSolutionBase<PHAL::AlbanyTraits::Jacobian, Traits>(p,p.get<Teuchos::RCP<Albany::Layouts> >("Layouts Struct")),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::Jacobian,Traits>::numFieldsBase)
{
}
//********************************************************************
////Kokkos functors for Jacobian

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
operator() (const tensorRank_2Tag& tag, const int& i) const{

  const int neq=wsID_kokkos.dimension(2);
  const int num_dof = neq * this->numNodes;

  for (int node = 0; node < this->numNodes; ++node){
    int firstunk = neq * node + this->offset;
    for (int eq = 0; eq < numFields; eq++){
       (this->valTensor[0])(i,node,eq/numDim,eq%numDim)=FadType(num_dof, xT_constView[wsID_kokkos(i,node,this->offset+eq)]);
       ((this->valTensor[0])(i,node,eq/numDim,eq%numDim)).setUpdateValue(!ignore_residual);
       ((this->valTensor[0])(i,node,eq/numDim,eq%numDim)).fastAccessDx(firstunk + eq) =j_coeff;
    }
  }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
operator() (const tensorRank_2_enableTransientTag& tag, const int& i) const{

  const int neq=wsID_kokkos.dimension(2);
  const int num_dof = neq * this->numNodes;
  
  for (int node = 0; node < this->numNodes; ++node){
    int firstunk = neq * node + this->offset;
    for (int eq = 0; eq < numFields; eq++){
       (this->valTensor_dot[0])(i,node,eq/numDim,eq%numDim)=FadType(num_dof, xdotT_constView[wsID_kokkos(i,node,this->offset+eq)]);
       ((this->valTensor_dot[0])(i,node,eq/numDim,eq%numDim)).fastAccessDx(firstunk + eq) =m_coeff;
    }
  }

}


template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
operator() (const tensorRank_2_enableAccelerationTag& tag, const int& i) const{

  const int neq=wsID_kokkos.dimension(2);
  const int num_dof = neq * this->numNodes;

  for (int node = 0; node < this->numNodes; ++node){
    int firstunk = neq * node + this->offset;
    for (int eq = 0; eq < numFields; eq++){
       (this->valTensor_dotdot[0])(i,node,eq/numDim,eq%numDim)=FadType(num_dof, xdotdotT_constView[wsID_kokkos(i,node,this->offset+eq)]);
       ((this->valTensor_dotdot[0])(i,node,eq/numDim,eq%numDim)).fastAccessDx(firstunk + eq) =n_coeff;
    }
  }

}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
operator() (const tensorRank_1Tag& tag, const int& i) const{

  const int neq=wsID_kokkos.dimension(2);
  const int num_dof = neq * this->numNodes;


  for (int node = 0; node < this->numNodes; node++){
    int firstunk = neq * node + this->offset;
    for (int eq = 0; eq < numFields; eq++){
       this->valVec(i,node,eq)=FadType(num_dof, xT_constView[wsID_kokkos(i,node,this->offset+eq)]);
       ((this->valVec)(i,node,eq)).setUpdateValue(!ignore_residual);
       ((this->valVec)(i,node,eq)).fastAccessDx(firstunk + eq) =j_coeff;
    }
  }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
operator() (const tensorRank_1_enableTransientTag& tag, const int& i) const{

  const int neq=wsID_kokkos.dimension(2);
  const int num_dof = neq * this->numNodes;

  for (int node = 0; node < this->numNodes; ++node){
    int firstunk = neq * node + this->offset;
    for (int eq = 0; eq < numFields; eq++){
       (this->valVec_dot)(i,node,eq)=FadType(num_dof, xdotT_constView[wsID_kokkos(i,node,this->offset+eq)]);
       ((this->valVec_dot)(i,node,eq)).fastAccessDx(firstunk + eq) =m_coeff;
    }
  }  

}


template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
operator() (const tensorRank_1_enableAccelerationTag& tag, const int& i) const{
 
  const int neq=wsID_kokkos.dimension(2);
  const int num_dof = neq * this->numNodes;

  for (int node = 0; node < this->numNodes; ++node){
    int firstunk = neq * node + this->offset;
    for (int eq = 0; eq < numFields; eq++){
       (this->valVec_dotdot)(i,node,eq)=FadType(num_dof, xdotdotT_constView[wsID_kokkos(i,node,this->offset+eq)]);
       ((this->valVec_dotdot)(i,node,eq)).fastAccessDx(firstunk + eq) =n_coeff;
    }
  }

}


template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
operator() (const tensorRank_0Tag& tag, const int& i) const{

  const int neq=wsID_kokkos.dimension(2);
  const int num_dof = neq * this->numNodes;

  for (int node = 0; node < this->numNodes; ++node){
    int firstunk = neq * node + this->offset;
    for (int eq = 0; eq < numFields; eq++){
       (this->val[eq])(i,node)=FadType(num_dof, xT_constView[wsID_kokkos(i,node,this->offset+eq)]);
       ((this->val[eq])(i,node)).setUpdateValue(!ignore_residual);
       ((this->val[eq])(i,node)).fastAccessDx(firstunk + eq) =j_coeff;
    }
  }
}

template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
operator() (const tensorRank_0_enableTransientTag& tag, const int& i) const{

  const int neq=wsID_kokkos.dimension(2);
  const int num_dof = neq * this->numNodes;

  for (int node = 0; node < this->numNodes; ++node){
    int firstunk = neq * node + this->offset;
    for (int eq = 0; eq < numFields; eq++){
       (this->val_dot[eq])(i,node)=FadType(num_dof, xdotT_constView[wsID_kokkos(i,node,this->offset+eq)]);
       ((this->val_dot[eq])(i,node)).fastAccessDx(firstunk + eq) =m_coeff;
    }
  }

}


template<typename Traits>
KOKKOS_INLINE_FUNCTION
void GatherSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
operator() (const tensorRank_0_enableAccelerationTag& tag, const int& i) const{

  const int neq=wsID_kokkos.dimension(2);
  const int num_dof = neq * this->numNodes;

  for (int node = 0; node < this->numNodes; ++node){
    int firstunk = neq * node + this->offset;
    for (int eq = 0; eq < numFields; eq++){
       (this->val_dotdot[eq])(i,node)=FadType(num_dof, xdotdotT_constView[wsID_kokkos(i,node,this->offset+eq)]);
       ((this->val_dotdot[eq])(i,node)).fastAccessDx(firstunk + eq) =n_coeff;
    }
  }

}



// **********************************************************************
template<typename Traits>
void GatherSolution<PHAL::AlbanyTraits::Jacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<const Tpetra_Vector> xT = workset.xT;
  Teuchos::RCP<const Tpetra_Vector> xdotT = workset.xdotT;
  Teuchos::RCP<const Tpetra_Vector> xdotdotT = workset.xdotdotT;

  xT_constView = xT->get1dView();
  xdotT_constView = xdotT->get1dView();
  xdotdotT_constView = xdotdotT->get1dView();
#ifndef ALBANY_KOKKOS_UNDER_DEVELOPMENT
  int numDim = 0;
  if (this->tensorRank==2) numDim = this->valTensor[0].dimension(2); // only needed for tensor fields

  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];
    const int neq = nodeID[0].size();
    const std::size_t num_dof = neq * this->numNodes;

    for (std::size_t node = 0; node < this->numNodes; ++node) {
      const Teuchos::ArrayRCP<int>& eqID  = nodeID[node];
      int firstunk = neq * node + this->offset;
      for (std::size_t eq = 0; eq < numFields; eq++) {
        typename PHAL::Ref<ScalarT>::type
          valptr = (this->tensorRank == 0 ? this->val[eq](cell,node) :
                    this->tensorRank == 1 ? this->valVec(cell,node,eq) :
                    this->valTensor[0](cell,node, eq/numDim, eq%numDim));
	valptr = FadType(num_dof, xT_constView[eqID[this->offset + eq]]);
        valptr.setUpdateValue(!workset.ignore_residual);
        valptr.fastAccessDx(firstunk + eq) = workset.j_coeff;
      }
      if (workset.transientTerms && this->enableTransient) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
        typename PHAL::Ref<ScalarT>::type
          valptr = (this->tensorRank == 0 ? this->val_dot[eq](cell,node) :
                    this->tensorRank == 1 ? this->valVec_dot(cell,node,eq) :
                    this->valTensor_dot[0](cell,node, eq/numDim, eq%numDim));
          valptr = FadType(num_dof, xdotT_constView[eqID[this->offset + eq]]);
          valptr.fastAccessDx(firstunk + eq) = workset.m_coeff;
        }
      }
      if (workset.accelerationTerms && this->enableAcceleration) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
        typename PHAL::Ref<ScalarT>::type
          valptr = (this->tensorRank == 0 ? this->val_dotdot[eq](cell,node) :
                    this->tensorRank == 1 ? this->valVec_dotdot(cell,node,eq) :
                    this->valTensor_dotdot[0](cell,node, eq/numDim, eq%numDim));
          valptr = FadType(num_dof, xdotdotT_constView[eqID[this->offset + eq]]);
          valptr.fastAccessDx(firstunk + eq) = workset.n_coeff;
        }
      }
    }
  }
#else
    wsID_kokkos=workset.wsElNodeEqID_kokkos;

    ignore_residual=workset.ignore_residual;
    j_coeff=workset.j_coeff;
    m_coeff=workset.m_coeff;
    n_coeff=workset.n_coeff;


   if(this->tensorRank==2) numDim = this->valTensor[0].dimension(2);
  
   if (this->tensorRank == 2){
 
     Kokkos::parallel_for(tensorRank_2Policy(0,workset.numCells),*this);

     if (workset.transientTerms && this->enableTransient)
        Kokkos::parallel_for(tensorRank_2_enableTransientPolicy(0,workset.numCells),*this);
     if (workset.accelerationTerms && this->enableAcceleration)
        Kokkos::parallel_for(tensorRank_2_enableAccelerationPolicy(0,workset.numCells),*this);

   } else if (this->tensorRank == 1){
     Kokkos::parallel_for(tensorRank_1Policy(0,workset.numCells),*this);

     if (workset.transientTerms && this->enableTransient){
       Kokkos::parallel_for(tensorRank_1_enableTransientPolicy(0,workset.numCells),*this);}
     if (workset.accelerationTerms && this->enableAcceleration){
        Kokkos::parallel_for(tensorRank_1_enableAccelerationPolicy(0,workset.numCells),*this);}
   } else  {

     Kokkos::parallel_for(tensorRank_0Policy(0,workset.numCells),*this);

     if (workset.transientTerms && this->enableTransient)
        Kokkos::parallel_for(tensorRank_0_enableTransientPolicy(0,workset.numCells),*this);
     if (workset.accelerationTerms && this->enableAcceleration)
        Kokkos::parallel_for(tensorRank_0_enableAccelerationPolicy(0,workset.numCells),*this);
   }
#endif

}

// **********************************************************************

// **********************************************************************
// Specialization: Tangent
// **********************************************************************

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::Tangent, Traits>::
GatherSolution(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl) :
  GatherSolutionBase<PHAL::AlbanyTraits::Tangent, Traits>(p,dl),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::Tangent,Traits>::numFieldsBase)
{
}

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::Tangent, Traits>::
GatherSolution(const Teuchos::ParameterList& p) :
  GatherSolutionBase<PHAL::AlbanyTraits::Tangent, Traits>(p,p.get<Teuchos::RCP<Albany::Layouts> >("Layouts Struct")),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::Tangent,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void GatherSolution<PHAL::AlbanyTraits::Tangent, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<const Tpetra_Vector> xT = workset.xT;
  Teuchos::RCP<const Tpetra_Vector> xdotT = workset.xdotT;
  Teuchos::RCP<const Tpetra_Vector> xdotdotT = workset.xdotdotT;
  Teuchos::RCP<const Tpetra_MultiVector> VxT = workset.VxT;
  Teuchos::RCP<const Tpetra_MultiVector> VxdotT = workset.VxdotT;
  Teuchos::RCP<const Tpetra_MultiVector> VxdotdotT = workset.VxdotdotT;
  Teuchos::RCP<const Tpetra_MultiVector> VpT = workset.VpT;

  //get const (read-only) view of xT and xdotT
  Teuchos::ArrayRCP<const ST> xT_constView = xT->get1dView();
  Teuchos::ArrayRCP<const ST> xdotT_constView = xdotT->get1dView();
  Teuchos::ArrayRCP<const ST> xdotdotT_constView = xdotdotT->get1dView();

  Teuchos::RCP<ParamVec> params = workset.params;
  int num_cols_tot = workset.param_offset + workset.num_cols_p;

  int numDim = 0;
  if(this->tensorRank==2) numDim = this->valTensor[0].dimension(2); // only needed for tensor fields
  
  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {
      const Teuchos::ArrayRCP<int>& eqID  = nodeID[node];
      for (std::size_t eq = 0; eq < numFields; eq++) {
        typename PHAL::Ref<ScalarT>::type
          valptr = ((this->tensorRank == 2) ? (this->valTensor[0])(cell,node,eq/numDim,eq%numDim) :
                    (this->tensorRank == 1) ? (this->valVec)(cell,node,eq) :
                    (this->val[eq])(cell,node));
        if (VxT != Teuchos::null && workset.j_coeff != 0.0) {
          valptr = TanFadType(num_cols_tot, xT_constView[eqID[this->offset + eq]]);
          for (int k=0; k<workset.num_cols_x; k++)
            valptr.fastAccessDx(k) =
              workset.j_coeff*VxT->getData(k)[eqID[this->offset + eq]];
        }
        else
          valptr = TanFadType(xT_constView[eqID[this->offset + eq]]);
      }
      if (workset.transientTerms && this->enableTransient) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
        typename PHAL::Ref<ScalarT>::type
          valptr = ((this->tensorRank == 2) ? (this->valTensor_dot[0])(cell,node,eq/numDim,eq%numDim) :
                    (this->tensorRank == 1) ? (this->valVec_dot)(cell,node,eq) :
                    (this->val_dot[eq])(cell,node));
          if (VxdotT != Teuchos::null && workset.m_coeff != 0.0) {
            valptr = TanFadType(num_cols_tot, xdotT_constView[eqID[this->offset + eq]]);
            for (int k=0; k<workset.num_cols_x; k++)
              valptr.fastAccessDx(k) =
                workset.m_coeff*VxdotT->getData(k)[eqID[this->offset + eq]];
          }
          else
            valptr = TanFadType(xdotT_constView[eqID[this->offset + eq]]);
        }
      }
      if (workset.accelerationTerms && this->enableAcceleration) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
        typename PHAL::Ref<ScalarT>::type
          valptr = ((this->tensorRank == 2) ? (this->valTensor_dotdot[0])(cell,node,eq/numDim,eq%numDim) :
                    (this->tensorRank == 1) ? (this->valVec_dotdot)(cell,node,eq) :
                    (this->val_dotdot[eq])(cell,node));
          if (VxdotdotT != Teuchos::null && workset.n_coeff != 0.0) {
            valptr = TanFadType(num_cols_tot, xdotdotT_constView[eqID[this->offset + eq]]);
            for (int k=0; k<workset.num_cols_x; k++)
              valptr.fastAccessDx(k) =
                workset.n_coeff*VxdotdotT->getData(k)[eqID[this->offset + eq]];
          }
          else
            valptr = TanFadType(xdotdotT_constView[eqID[this->offset + eq]]);
        }
      }
    }
  }
}

// **********************************************************************

// **********************************************************************
// Specialization: Distributed Parameter Derivative
// **********************************************************************

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
GatherSolution(const Teuchos::ParameterList& p,
               const Teuchos::RCP<Albany::Layouts>& dl) :
  GatherSolutionBase<PHAL::AlbanyTraits::DistParamDeriv, Traits>(p,dl),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::DistParamDeriv,Traits>::numFieldsBase)
{
}

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
GatherSolution(const Teuchos::ParameterList& p) :
  GatherSolutionBase<PHAL::AlbanyTraits::DistParamDeriv, Traits>(p,p.get<Teuchos::RCP<Albany::Layouts> >("Layouts Struct")),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::DistParamDeriv,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void GatherSolution<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<const Tpetra_Vector> xT = workset.xT;
  Teuchos::RCP<const Tpetra_Vector> xdotT = workset.xdotT;
  Teuchos::RCP<const Tpetra_Vector> xdotdotT = workset.xdotdotT;

  //get const (read-only) view of xT and xdotT
  Teuchos::ArrayRCP<const ST> xT_constView = xT->get1dView();
  Teuchos::ArrayRCP<const ST> xdotT_constView = xdotT->get1dView();
  Teuchos::ArrayRCP<const ST> xdotdotT_constView = xdotdotT->get1dView();

  if (this->tensorRank == 1) {
    for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
      const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

      for (std::size_t node = 0; node < this->numNodes; ++node) {
      const Teuchos::ArrayRCP<int>& eqID  = nodeID[node];
        for (std::size_t eq = 0; eq < numFields; eq++)
          (this->valVec)(cell,node,eq) = xT_constView[eqID[this->offset + eq]];
        if (workset.transientTerms && this->enableTransient) {
          for (std::size_t eq = 0; eq < numFields; eq++)
            (this->valVec_dot)(cell,node,eq) = xdotT_constView[eqID[this->offset + eq]];
        }
        if (workset.accelerationTerms && this->enableAcceleration) {
          for (std::size_t eq = 0; eq < numFields; eq++)
            (this->valVec_dotdot)(cell,node,eq) = xdotdotT_constView[eqID[this->offset + eq]];
        }
      }
    } 
  } else
  if (this->tensorRank == 2) {
    int numDim = this->valTensor[0].dimension(2);
    for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
      const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

      for (std::size_t node = 0; node < this->numNodes; ++node) {
      const Teuchos::ArrayRCP<int>& eqID  = nodeID[node];
        for (std::size_t eq = 0; eq < numFields; eq++)
          (this->valTensor[0])(cell,node,eq/numDim,eq%numDim) = xT_constView[eqID[this->offset + eq]];
        if (workset.transientTerms && this->enableTransient) {
          for (std::size_t eq = 0; eq < numFields; eq++)
            (this->valTensor_dot[0])(cell,node,eq/numDim,eq%numDim) = xdotdotT_constView[eqID[this->offset + eq]];
        }
        if (workset.accelerationTerms && this->enableAcceleration) {
          for (std::size_t eq = 0; eq < numFields; eq++)
            (this->valTensor_dotdot[0])(cell,node,eq/numDim,eq%numDim) = xdotdotT_constView[eqID[this->offset + eq]];
        }
      }
    }
  } else {
    for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
      const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

      for (std::size_t node = 0; node < this->numNodes; ++node) {
      const Teuchos::ArrayRCP<int>& eqID  = nodeID[node];
        for (std::size_t eq = 0; eq < numFields; eq++)
          (this->val[eq])(cell,node) = xT_constView[eqID[this->offset + eq]];
        if (workset.transientTerms && this->enableTransient) {
          for (std::size_t eq = 0; eq < numFields; eq++)
            (this->val_dot[eq])(cell,node) = xdotT_constView[eqID[this->offset + eq]];
        }
        if (workset.accelerationTerms && this->enableAcceleration) {
          for (std::size_t eq = 0; eq < numFields; eq++)
            (this->val_dotdot[eq])(cell,node) = xdotdotT_constView[eqID[this->offset + eq]];
        }
      }
    }
  }
}

// **********************************************************************

// **********************************************************************
// Specialization: Stochastic Galerkin Residual
// **********************************************************************

#ifdef ALBANY_SG_MP
template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::SGResidual, Traits>::
GatherSolution(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl) :
  GatherSolutionBase<PHAL::AlbanyTraits::SGResidual, Traits>(p,dl),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::SGResidual,Traits>::numFieldsBase)
{
}

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::SGResidual, Traits>::
GatherSolution(const Teuchos::ParameterList& p) :
  GatherSolutionBase<PHAL::AlbanyTraits::SGResidual, Traits>(p,p.get<Teuchos::RCP<Albany::Layouts> >("Layouts Struct")),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::SGResidual,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void GatherSolution<PHAL::AlbanyTraits::SGResidual, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,RealType> > sg_expansion =
    workset.sg_expansion;
  Teuchos::RCP<const Stokhos::EpetraVectorOrthogPoly > x =
    workset.sg_x;
  Teuchos::RCP<const Stokhos::EpetraVectorOrthogPoly > xdot =
    workset.sg_xdot;
  Teuchos::RCP<const Stokhos::EpetraVectorOrthogPoly > xdotdot =
    workset.sg_xdotdot;
  ScalarT* valptr;

  int numDim = 0;
  if(this->tensorRank==2) numDim = this->valTensor[0].dimension(2); // only needed for tensor fields

  int nblock = x->size();
  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {
      for (std::size_t eq = 0; eq < numFields; eq++) {
        if (this->tensorRank == 2) valptr = &(this->valTensor[0])(cell,node,eq/numDim,eq%numDim);
        else if (this->tensorRank == 1) valptr = &(this->valVec)(cell,node,eq);
        else                   valptr = &(this->val[eq])(cell,node);
        valptr->reset(sg_expansion);
        valptr->copyForWrite();
        for (int block=0; block<nblock; block++)
          valptr->fastAccessCoeff(block) =
            (*x)[block][nodeID[node][this->offset + eq]];
      }
      if (workset.transientTerms && this->enableTransient) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          if (this->tensorRank == 2) valptr = &(this->valTensor_dot[0])(cell,node,eq/numDim,eq%numDim);
          else if (this->tensorRank == 1) valptr = &(this->valVec_dot)(cell,node,eq);
          else                   valptr = &(this->val_dot[eq])(cell,node);
          valptr->reset(sg_expansion);
          valptr->copyForWrite();
          for (int block=0; block<nblock; block++)
            valptr->fastAccessCoeff(block) =
              (*xdot)[block][nodeID[node][this->offset + eq]];
        }
      }
      if (workset.accelerationTerms && this->enableAcceleration) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          if (this->tensorRank == 2) valptr = &(this->valTensor_dotdot[0])(cell,node,eq/numDim,eq%numDim);
          else if (this->tensorRank == 1) valptr = &(this->valVec_dotdot)(cell,node,eq);
          else                   valptr = &(this->val_dotdot[eq])(cell,node);
          valptr->reset(sg_expansion);
          valptr->copyForWrite();
          for (int block=0; block<nblock; block++)
            valptr->fastAccessCoeff(block) =
              (*xdotdot)[block][nodeID[node][this->offset + eq]];
        }
      }
    }
  }

}

// **********************************************************************

// **********************************************************************
// Specialization: Stochastic Galerkin Jacobian
// **********************************************************************

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::SGJacobian, Traits>::
GatherSolution(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl) :
  GatherSolutionBase<PHAL::AlbanyTraits::SGJacobian, Traits>(p,dl),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::SGJacobian,Traits>::numFieldsBase)
{
}

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::SGJacobian, Traits>::
GatherSolution(const Teuchos::ParameterList& p) :
  GatherSolutionBase<PHAL::AlbanyTraits::SGJacobian, Traits>(p,p.get<Teuchos::RCP<Albany::Layouts> >("Layouts Struct")),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::SGJacobian,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void GatherSolution<PHAL::AlbanyTraits::SGJacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,RealType> > sg_expansion =
    workset.sg_expansion;
  Teuchos::RCP<const Stokhos::EpetraVectorOrthogPoly > x =
    workset.sg_x;
  Teuchos::RCP<const Stokhos::EpetraVectorOrthogPoly > xdot =
    workset.sg_xdot;
  Teuchos::RCP<const Stokhos::EpetraVectorOrthogPoly > xdotdot =
    workset.sg_xdotdot;
  ScalarT* valptr;

  int numDim = 0;
  if(this->tensorRank==2) numDim = this->valTensor[0].dimension(2); // only needed for tensor fields

  int nblock = x->size();
  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {
      int neq = nodeID[node].size();
      std::size_t num_dof = neq * this->numNodes;

      for (std::size_t eq = 0; eq < numFields; eq++) {
        if (this->tensorRank == 2) valptr = &(this->valTensor[0])(cell,node,eq/numDim,eq%numDim);
        else if (this->tensorRank == 1) valptr = &(this->valVec)(cell,node,eq);
        else                   valptr = &(this->val[eq])(cell,node);
        *valptr = SGFadType(num_dof, 0.0);
        valptr->setUpdateValue(!workset.ignore_residual);
        valptr->fastAccessDx(neq * node + eq + this->offset) = workset.j_coeff;
        valptr->val().reset(sg_expansion);
        valptr->val().copyForWrite();
        for (int block=0; block<nblock; block++)
          valptr->val().fastAccessCoeff(block) = (*x)[block][nodeID[node][this->offset + eq]];
      }
      if (workset.transientTerms && this->enableTransient) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          if (this->tensorRank == 2) valptr = &(this->valTensor_dot[0])(cell,node,eq/numDim,eq%numDim);
          else if (this->tensorRank == 1) valptr = &(this->valVec_dot)(cell,node,eq);
          else                   valptr = &(this->val_dot[eq])(cell,node);
          *valptr = SGFadType(num_dof, 0.0);
          valptr->fastAccessDx(neq * node + eq + this->offset) = workset.m_coeff;
          valptr->val().reset(sg_expansion);
          valptr->val().copyForWrite();
          for (int block=0; block<nblock; block++)
            valptr->val().fastAccessCoeff(block) = (*xdot)[block][nodeID[node][this->offset + eq]];
        }
      }
      if (workset.accelerationTerms && this->enableAcceleration) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          if (this->tensorRank == 2) valptr = &(this->valTensor_dotdot[0])(cell,node,eq/numDim,eq%numDim);
          else if (this->tensorRank == 1) valptr = &(this->valVec_dotdot)(cell,node,eq);
          else                   valptr = &(this->val_dotdot[eq])(cell,node);
          *valptr = SGFadType(num_dof, 0.0);
          valptr->fastAccessDx(neq * node + eq + this->offset) = workset.n_coeff;
          valptr->val().reset(sg_expansion);
          valptr->val().copyForWrite();
          for (int block=0; block<nblock; block++)
            valptr->val().fastAccessCoeff(block) = (*xdotdot)[block][nodeID[node][this->offset + eq]];
        }
      }
    }
  }

}

// **********************************************************************

// **********************************************************************
// Specialization: Stochastic Galerkin Tangent
// **********************************************************************

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::SGTangent, Traits>::
GatherSolution(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl) :
  GatherSolutionBase<PHAL::AlbanyTraits::SGTangent, Traits>(p,dl),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::SGTangent,Traits>::numFieldsBase)
{
}

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::SGTangent, Traits>::
GatherSolution(const Teuchos::ParameterList& p) :
  GatherSolutionBase<PHAL::AlbanyTraits::SGTangent, Traits>(p,p.get<Teuchos::RCP<Albany::Layouts> >("Layouts Struct")),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::SGTangent,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void GatherSolution<PHAL::AlbanyTraits::SGTangent, Traits>::
evaluateFields(typename Traits::EvalData workset)
{

  Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,RealType> > sg_expansion =
    workset.sg_expansion;
  Teuchos::RCP<const Stokhos::EpetraVectorOrthogPoly > x =
    workset.sg_x;
  Teuchos::RCP<const Stokhos::EpetraVectorOrthogPoly > xdot =
    workset.sg_xdot;
  Teuchos::RCP<const Stokhos::EpetraVectorOrthogPoly > xdotdot =
    workset.sg_xdotdot;
  Teuchos::RCP<const Epetra_MultiVector> Vx = workset.Vx;
  Teuchos::RCP<const Epetra_MultiVector> Vxdot = workset.Vxdot;
  Teuchos::RCP<const Epetra_MultiVector> Vxdotdot = workset.Vxdotdot;
  Teuchos::RCP<const Epetra_MultiVector> Vp = workset.Vp;
  Teuchos::RCP<ParamVec> params = workset.params;
  int num_cols_tot = workset.param_offset + workset.num_cols_p;
  //ScalarT* valptr;

  int numDim = 0;
  if(this->tensorRank==2) numDim = this->valTensor[0].dimension(2); // only needed for tensor fields
  TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "tpetra_kokoks not impl'ed");
//Irina TOFIX
/*  
 int nblock = x->size();
  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {
      for (std::size_t eq = 0; eq < numFields; eq++) {
        typename PHAL::Ref<ScalarT>::type
        valptr = (this->tensorRank == 2 ? (this->valTensor[0])(cell,node,eq/numDim,eq%numDim) :
                  this->tensorRank == 1 ? (this->valVec)(cell,node,eq) :
                  (this->val[eq])(cell,node));
        if (Vx != Teuchos::null && workset.j_coeff != 0.0) {
          valptr = SGFadType(num_cols_tot, 0.0);
          for (int k=0; k<workset.num_cols_x; k++)
            valptr.fastAccessDx(k) =
              workset.j_coeff*(*Vx)[k][nodeID[node][this->offset + eq]];
        }
        else
          valptr = SGFadType(0.0);
        (valptr.val()).reset(sg_expansion);
        (valptr.val()).copyForWrite();
        for (int block=0; block<nblock; block++)
          (valptr.val()).fastAccessCoeff(block) = (*x)[block][nodeID[node][this->offset + eq]];
      }
      if (workset.transientTerms && this->enableTransient) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          if (this->tensorRank == 2) valptr = &(this->valTensor_dot[0])(cell,node,eq/numDim,eq%numDim);
          else if (this->tensorRank == 1) valptr = &(this->valVec_dot)(cell,node,eq);
          else                   valptr = &(this->val_dot[eq])(cell,node);
          if (Vxdot != Teuchos::null && workset.m_coeff != 0.0) {
            *valptr = SGFadType(num_cols_tot, 0.0);
            for (int k=0; k<workset.num_cols_x; k++)
              valptr->fastAccessDx(k) =
                workset.m_coeff*(*Vxdot)[k][nodeID[node][this->offset + eq]];
          }
          else
            *valptr = SGFadType(0.0);
          valptr->val().reset(sg_expansion);
          valptr->val().copyForWrite();
          for (int block=0; block<nblock; block++)
            valptr->val().fastAccessCoeff(block) = (*xdot)[block][nodeID[node][this->offset + eq]];
        }
      }
      if (workset.accelerationTerms && this->enableAcceleration) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          if (this->tensorRank == 2) valptr = &(this->valTensor_dotdot[0])(cell,node,eq/numDim,eq%numDim);
          else if (this->tensorRank == 1) valptr = &(this->valVec_dotdot)(cell,node,eq);
          else                   valptr = &(this->val_dotdot[eq])(cell,node);
          if (Vxdotdot != Teuchos::null && workset.n_coeff != 0.0) {
            *valptr = SGFadType(num_cols_tot, 0.0);
            for (int k=0; k<workset.num_cols_x; k++)
              valptr->fastAccessDx(k) =
                workset.n_coeff*(*Vxdotdot)[k][nodeID[node][this->offset + eq]];
          }
       else
            *valptr = SGFadType(0.0);
          valptr->val().reset(sg_expansion);
          valptr->val().copyForWrite();
          for (int block=0; block<nblock; block++)
            valptr->val().fastAccessCoeff(block) = (*xdotdot)[block][nodeID[node][this->offset + eq]];
        }
      }*/
    }
  }

 
}

// **********************************************************************

// **********************************************************************
// Specialization: Multi-point Residual
// **********************************************************************

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::MPResidual, Traits>::
GatherSolution(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl) :
  GatherSolutionBase<PHAL::AlbanyTraits::MPResidual, Traits>(p,dl),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::MPResidual,Traits>::numFieldsBase)
{
}

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::MPResidual, Traits>::
GatherSolution(const Teuchos::ParameterList& p) :
  GatherSolutionBase<PHAL::AlbanyTraits::MPResidual, Traits>(p,p.get<Teuchos::RCP<Albany::Layouts> >("Layouts Struct")),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::MPResidual,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void GatherSolution<PHAL::AlbanyTraits::MPResidual, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<const Stokhos::ProductEpetraVector > x =
    workset.mp_x;
  Teuchos::RCP<const Stokhos::ProductEpetraVector > xdot =
    workset.mp_xdot;
  Teuchos::RCP<const Stokhos::ProductEpetraVector > xdotdot =
    workset.mp_xdotdot;
//  ScalarT* valptr;


  int numDim = 0;
  if(this->tensorRank==2) numDim = this->valTensor[0].dimension(2); // only needed for tensor fields
  TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "tpetra_kokoks not impl'ed");
  //Irina TOFIX
  /*
  int nblock = x->size();
  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {
      for (std::size_t eq = 0; eq < numFields; eq++) {
        if (this->tensorRank == 2) valptr = &(this->valTensor[0])(cell,node,eq/numDim,eq%numDim);
        else if (this->tensorRank == 1) valptr = &(this->valVec)(cell,node,eq);
        else                   valptr = &(this->val[eq])(cell,node);
        valptr->reset(nblock);
        valptr->copyForWrite();
        for (int block=0; block<nblock; block++)
          valptr->fastAccessCoeff(block) =
            (*x)[block][nodeID[node][this->offset + eq]];
      }
      if (workset.transientTerms && this->enableTransient) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          if (this->tensorRank == 2) valptr = &(this->valTensor_dot[0])(cell,node,eq/numDim,eq%numDim);
          else if (this->tensorRank == 1) valptr = &(this->valVec_dot)(cell,node,eq);
          else                   valptr = &(this->val_dot[eq])(cell,node);
          valptr->reset(nblock);
          valptr->copyForWrite();
          for (int block=0; block<nblock; block++)
            valptr->fastAccessCoeff(block) =
              (*xdot)[block][nodeID[node][this->offset + eq]];
        }
      }
      if (workset.accelerationTerms && this->enableAcceleration) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          if (this->tensorRank == 2) valptr = &(this->valTensor_dotdot[0])(cell,node,eq/numDim,eq%numDim);
          else if (this->tensorRank == 1) valptr = &(this->valVec_dotdot)(cell,node,eq);
          else                   valptr = &(this->val_dotdot[eq])(cell,node);
          valptr->reset(nblock);
          valptr->copyForWrite();
          for (int block=0; block<nblock; block++)
            valptr->fastAccessCoeff(block) =
              (*xdotdot)[block][nodeID[node][this->offset + eq]];
        }
      }
    }
  }

 */
}

// **********************************************************************

// **********************************************************************
// Specialization: Mulit-point Jacobian
// **********************************************************************

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::MPJacobian, Traits>::
GatherSolution(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl) :
  GatherSolutionBase<PHAL::AlbanyTraits::MPJacobian, Traits>(p,dl),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::MPJacobian,Traits>::numFieldsBase)
{
}

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::MPJacobian, Traits>::
GatherSolution(const Teuchos::ParameterList& p) :
  GatherSolutionBase<PHAL::AlbanyTraits::MPJacobian, Traits>(p,p.get<Teuchos::RCP<Albany::Layouts> >("Layouts Struct")),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::MPJacobian,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void GatherSolution<PHAL::AlbanyTraits::MPJacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  Teuchos::RCP<const Stokhos::ProductEpetraVector > x =
    workset.mp_x;
  Teuchos::RCP<const Stokhos::ProductEpetraVector > xdot =
    workset.mp_xdot;
  Teuchos::RCP<const Stokhos::ProductEpetraVector > xdotdot =
    workset.mp_xdotdot;
  //ScalarT* valptr;

  int numDim = 0;
  if(this->tensorRank==2) numDim = this->valTensor[0].dimension(2); // only needed for tensor fields
  TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "tpetra_kokoks not impl'ed");
  //Irina TOFIX
  /*
  int nblock = x->size();
  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {
      int neq = nodeID[node].size();
      std::size_t num_dof = neq * this->numNodes;

      for (std::size_t eq = 0; eq < numFields; eq++) {
        if (this->tensorRank == 2) valptr = &(this->valTensor[0])(cell,node,eq/numDim,eq%numDim);
        else if (this->tensorRank == 1) valptr = &(this->valVec)(cell,node,eq);
        else                   valptr = &(this->val[eq])(cell,node);
        *valptr = MPFadType(num_dof, 0.0);
        valptr->setUpdateValue(!workset.ignore_residual);
        valptr->fastAccessDx(neq * node + eq + this->offset) = workset.j_coeff;
        valptr->val().reset(nblock);
        valptr->val().copyForWrite();
        for (int block=0; block<nblock; block++)
          valptr->val().fastAccessCoeff(block) = (*x)[block][nodeID[node][this->offset + eq]];
      }
      if (workset.transientTerms && this->enableTransient) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          if (this->tensorRank == 2) valptr = &(this->valTensor_dot[0])(cell,node,eq/numDim,eq%numDim);
          else if (this->tensorRank == 1) valptr = &(this->valVec_dot)(cell,node,eq);
          else                   valptr = &(this->val_dot[eq])(cell,node);
          *valptr = MPFadType(num_dof, 0.0);
          valptr->fastAccessDx(neq * node + eq + this->offset) = workset.m_coeff;
          valptr->val().reset(nblock);
          valptr->val().copyForWrite();
          for (int block=0; block<nblock; block++)
            valptr->val().fastAccessCoeff(block) = (*xdot)[block][nodeID[node][this->offset + eq]];
        }
      }
      if (workset.accelerationTerms && this->enableAcceleration) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          if (this->tensorRank == 2) valptr = &(this->valTensor_dotdot[0])(cell,node,eq/numDim,eq%numDim);
          else if (this->tensorRank == 1) valptr = &(this->valVec_dotdot)(cell,node,eq);
          else                   valptr = &(this->val_dotdot[eq])(cell,node);
          *valptr = MPFadType(num_dof, 0.0);
          valptr->fastAccessDx(neq * node + eq + this->offset) = workset.n_coeff;
          valptr->val().reset(nblock);
          valptr->val().copyForWrite();
          for (int block=0; block<nblock; block++)
            valptr->val().fastAccessCoeff(block) = (*xdotdot)[block][nodeID[node][this->offset + eq]];
        }
      }
    }
  }

 */

}

// **********************************************************************

// **********************************************************************
// Specialization: Multi-point Galerkin Tangent
// **********************************************************************

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::MPTangent, Traits>::
GatherSolution(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl) :
  GatherSolutionBase<PHAL::AlbanyTraits::MPTangent, Traits>(p,dl),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::MPTangent,Traits>::numFieldsBase)
{
}

template<typename Traits>
GatherSolution<PHAL::AlbanyTraits::MPTangent, Traits>::
GatherSolution(const Teuchos::ParameterList& p) :
  GatherSolutionBase<PHAL::AlbanyTraits::MPTangent, Traits>(p,p.get<Teuchos::RCP<Albany::Layouts> >("Layouts Struct")),
  numFields(GatherSolutionBase<PHAL::AlbanyTraits::MPTangent,Traits>::numFieldsBase)
{
}

// **********************************************************************
template<typename Traits>
void GatherSolution<PHAL::AlbanyTraits::MPTangent, Traits>::
evaluateFields(typename Traits::EvalData workset)
{

  Teuchos::RCP<const Stokhos::ProductEpetraVector > x =
    workset.mp_x;
  Teuchos::RCP<const Stokhos::ProductEpetraVector > xdot =
    workset.mp_xdot;
  Teuchos::RCP<const Stokhos::ProductEpetraVector > xdotdot =
    workset.mp_xdotdot;
  Teuchos::RCP<const Epetra_MultiVector> Vx = workset.Vx;
  Teuchos::RCP<const Epetra_MultiVector> Vxdot = workset.Vxdot;
  Teuchos::RCP<const Epetra_MultiVector> Vxdotdot = workset.Vxdotdot;
  Teuchos::RCP<const Epetra_MultiVector> Vp = workset.Vp;
  Teuchos::RCP<ParamVec> params = workset.params;
  int num_cols_tot = workset.param_offset + workset.num_cols_p;
 // ScalarT* valptr;

  int numDim = 0;
  if(this->tensorRank==2) numDim = this->valTensor[0].dimension(2); // only needed for tensor fields
  TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "tpetra_kokoks not impl'ed");
  //Irina TOFIX
  /*
  int nblock = x->size();
  for (std::size_t cell=0; cell < workset.numCells; ++cell ) {
    const Teuchos::ArrayRCP<Teuchos::ArrayRCP<int> >& nodeID  = workset.wsElNodeEqID[cell];

    for (std::size_t node = 0; node < this->numNodes; ++node) {
      for (std::size_t eq = 0; eq < numFields; eq++) {
        if (this->tensorRank == 2) valptr = &(this->valTensor[0])(cell,node,eq/numDim,eq%numDim);
        else if (this->tensorRank == 1) valptr = &(this->valVec)(cell,node,eq);
        else                   valptr = &(this->val[eq])(cell,node);
        if (Vx != Teuchos::null && workset.j_coeff != 0.0) {
          *valptr = MPFadType(num_cols_tot, 0.0);
          for (int k=0; k<workset.num_cols_x; k++)
            valptr->fastAccessDx(k) =
              workset.j_coeff*(*Vx)[k][nodeID[node][this->offset + eq]];
        }
        else
          *valptr = MPFadType(0.0);
        valptr->val().reset(nblock);
        valptr->val().copyForWrite();
        for (int block=0; block<nblock; block++)
          valptr->val().fastAccessCoeff(block) = (*x)[block][nodeID[node][this->offset + eq]];
      }
      if (workset.transientTerms && this->enableTransient) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          if (this->tensorRank == 2) valptr = &(this->valTensor_dot[0])(cell,node,eq/numDim,eq%numDim);
          else if (this->tensorRank == 1) valptr = &(this->valVec_dot)(cell,node,eq);
          else                   valptr = &(this->val_dot[eq])(cell,node);
          if (Vxdot != Teuchos::null && workset.m_coeff != 0.0) {
            *valptr = MPFadType(num_cols_tot, 0.0);
            for (int k=0; k<workset.num_cols_x; k++)
              valptr->fastAccessDx(k) =
                workset.m_coeff*(*Vxdot)[k][nodeID[node][this->offset + eq]];
          }
          else
            *valptr = MPFadType(0.0);
          valptr->val().reset(nblock);
          valptr->val().copyForWrite();
          for (int block=0; block<nblock; block++)
            valptr->val().fastAccessCoeff(block) = (*xdot)[block][nodeID[node][this->offset + eq]];
        }
      }
      if (workset.accelerationTerms && this->enableAcceleration) {
        for (std::size_t eq = 0; eq < numFields; eq++) {
          if (this->tensorRank == 2) valptr = &(this->valTensor_dotdot[0])(cell,node,eq/numDim,eq%numDim);
          if (this->tensorRank == 1) valptr = &(this->valVec_dotdot)(cell,node,eq);
          else                   valptr = &(this->val_dotdot[eq])(cell,node);
          if (Vxdotdot != Teuchos::null && workset.n_coeff != 0.0) {
            *valptr = MPFadType(num_cols_tot, 0.0);
            for (int k=0; k<workset.num_cols_x; k++)
              valptr->fastAccessDx(k) =
                workset.n_coeff*(*Vxdotdot)[k][nodeID[node][this->offset + eq]];
          }
          else
            *valptr = MPFadType(0.0);
          valptr->val().reset(nblock);
          valptr->val().copyForWrite();
          for (int block=0; block<nblock; block++)
            valptr->val().fastAccessCoeff(block) = (*xdotdot)[block][nodeID[node][this->offset + eq]];
        }
      }
    }
  }
 */
}
#endif //ALBANY_SG_MP

}
