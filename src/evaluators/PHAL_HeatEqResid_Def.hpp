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

#include "Intrepid_FunctionSpaceTools.hpp"

namespace PHAL {

//**********************************************************************
template<typename EvalT, typename Traits>
HeatEqResid<EvalT, Traits>::
HeatEqResid(const Teuchos::ParameterList& p) :
  wBF         (p.get<std::string>                   ("Weighted BF Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("Node QP Scalar Data Layout") ),
  Temperature (p.get<std::string>                   ("QP Variable Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  Tdot        (p.get<std::string>                   ("QP Time Derivative Variable Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  ThermalCond (p.get<std::string>                   ("Thermal Conductivity Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  wGradBF     (p.get<std::string>                   ("Weighted Gradient BF Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("Node QP Vector Data Layout") ),
  TGrad       (p.get<std::string>                   ("Gradient QP Variable Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout") ),
  Source      (p.get<std::string>                   ("Source Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  TResidual   (p.get<std::string>                   ("Residual Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("Node Scalar Data Layout") ),
  haveSource  (p.get<bool>("Have Source")),
  transient   (p.get<bool>("Is Transient")),
  haveConvection(false)
{
  this->addDependentField(wBF);
 // this->addDependentField(Temperature);
  this->addDependentField(ThermalCond);
  if (transient) this->addDependentField(Tdot);
  this->addDependentField(TGrad);
  this->addDependentField(wGradBF);
  if (haveSource) this->addDependentField(Source);

  this->addEvaluatedField(TResidual);

  Teuchos::RCP<PHX::DataLayout> vector_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];

  // Allocate workspace
  flux.resize(dims[0], numQPs, numDims);

  convectionVels = Teuchos::getArrayFromStringParameter<double> (p,
                           "Convection Velocity", numDims, false);
  cout << "  CV  " << convectionVels.size() << endl;
  if (convectionVels.size()>0) {
    haveConvection = true;
    
    PHX::MDField<ScalarT,Cell,QuadPoint> tmp(p.get<string>("Rho Cp Name"),
          p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout"));
    rhoCp = tmp;
    this->addDependentField(rhoCp);
  }

  this->setName("HeatEqResid"+PHX::TypeString<EvalT>::value);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void HeatEqResid<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(wBF,fm);
 // this->utils.setFieldData(Temperature,fm);
  this->utils.setFieldData(ThermalCond,fm);
  this->utils.setFieldData(TGrad,fm);
  this->utils.setFieldData(wGradBF,fm);
  if (haveSource)  this->utils.setFieldData(Source,fm);
  if (transient)  this->utils.setFieldData(Tdot,fm);

  if (haveConvection)  this->utils.setFieldData(rhoCp,fm);

  this->utils.setFieldData(TResidual,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void HeatEqResid<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  typedef Intrepid::FunctionSpaceTools FST;

  FST::scalarMultiplyDataData<ScalarT> (flux, ThermalCond, TGrad);

  FST::integrate<ScalarT>(TResidual, flux, wGradBF, Intrepid::COMP_CPP, false); // "false" overwrites

  if (haveSource) {
    for (int i=0; i<Source.size(); i++) Source[i] *= -1.0;
    FST::integrate<ScalarT>(TResidual, Source, wBF, Intrepid::COMP_CPP, true); // "true" sums into
  }

  if (transient) 
    FST::integrate<ScalarT>(TResidual, Tdot, wBF, Intrepid::COMP_CPP, true); // "true" sums into

  if (haveConvection)  {
    Intrepid::FieldContainer<ScalarT> convection(workset.worksetSize, numQPs);

    for (std::size_t cell=0; cell < workset.numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {
        convection(cell,qp) = 0.0;
        for (std::size_t i=0; i < numDims; ++i) {
          convection(cell,qp) += rhoCp(cell,qp) * convectionVels[i] * TGrad(cell,qp,i);
        }
      }
    }

    FST::integrate<ScalarT>(TResidual, convection, wBF, Intrepid::COMP_CPP, true); // "true" sums into
  }

}

//**********************************************************************
}

