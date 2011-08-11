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
NSTauT<EvalT, Traits>::
NSTauT(const Teuchos::ParameterList& p) :
  V           (p.get<std::string>                   ("Velocity QP Variable Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout") ),
  ThermalCond (p.get<std::string>                   ("Thermal Conductivity Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  Gc            (p.get<std::string>                   ("Contravarient Metric Tensor Name"),  
                 p.get<Teuchos::RCP<PHX::DataLayout> >("QP Tensor Data Layout") ),
  rho       (p.get<std::string>                   ("Density QP Variable Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  Cp       (p.get<std::string>                  ("Specific Heat QP Variable Name"),
	       p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") ),
  TauT            (p.get<std::string>                 ("Tau T Name"),
                 p.get<Teuchos::RCP<PHX::DataLayout> >("QP Scalar Data Layout") )
  
{
  this->addDependentField(V);
  this->addDependentField(ThermalCond);
  this->addDependentField(Gc);
  this->addDependentField(rho);
  this->addDependentField(Cp);
 
  this->addEvaluatedField(TauT);

  Teuchos::RCP<PHX::DataLayout> vector_dl =
    p.get< Teuchos::RCP<PHX::DataLayout> >("QP Vector Data Layout");
  std::vector<PHX::DataLayout::size_type> dims;
  vector_dl->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];

  // Allocate workspace
  normGc.resize(dims[0], numQPs);

  this->setName("NSTauT"+PHX::TypeString<EvalT>::value);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void NSTauT<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(V,fm);
  this->utils.setFieldData(ThermalCond,fm);
  this->utils.setFieldData(Gc,fm);
  this->utils.setFieldData(rho,fm);
  this->utils.setFieldData(Cp,fm);
  
  this->utils.setFieldData(TauT,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void NSTauT<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{ 
    for (std::size_t cell=0; cell < workset.numCells; ++cell) {
      for (std::size_t qp=0; qp < numQPs; ++qp) {       
        TauT(cell,qp) = 0.0;
        normGc(cell,qp) = 0.0;
        double r = Albany::ADValue(rho(cell,qp));
        double cp = Albany::ADValue(Cp(cell,qp));
        double Th = Albany::ADValue(ThermalCond(cell,qp));
        for (std::size_t i=0; i < numDims; ++i) {
          double Vi = Albany::ADValue(V(cell,qp,i));
          for (std::size_t j=0; j < numDims; ++j) {
            double Vj = Albany::ADValue(V(cell,qp,j));
            double gc = Albany::ADValue(Gc(cell,qp,i,j));
            TauT(cell,qp) += r * cp * r * cp* Vi*gc*Vj;
            normGc(cell,qp) += gc*gc;          
          }
        }
        TauT(cell,qp) += 12*Th*Th*std::sqrt(normGc(cell,qp));
        TauT(cell,qp) = 1/std::sqrt(TauT(cell,qp));
      }
    }
  
}

//**********************************************************************
}

