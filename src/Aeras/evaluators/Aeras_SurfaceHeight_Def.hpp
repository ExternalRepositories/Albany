//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Sacado_ParameterRegistration.hpp" 

#include "Intrepid_FunctionSpaceTools.hpp"
#include "Albany_Layouts.hpp"


namespace Aeras {

const double pi = 3.1415926535897932385;
 
//**********************************************************************
template<typename EvalT, typename Traits>
SurfaceHeight<EvalT, Traits>::
SurfaceHeight(const Teuchos::ParameterList& p,
            const Teuchos::RCP<Albany::Layouts>& dl) :
  sphere_coord (p.get<std::string> ("Spherical Coord Name"), dl->qp_gradient),
  hs    (p.get<std::string> ("Aeras Surface Height QP Variable Name"), dl->qp_scalar) 
{
  Teuchos::ParameterList* hs_list = 
   p.get<Teuchos::ParameterList*>("Parameter List");

  std::string hsType = hs_list->get("Type", "None");
  //I think gravity is not needed here...  IK, 2/5/14
  gravity = hs_list->get<double>("Gravity", 1.0); //Default: g=1
  lengthScale = hs_list->get<double>("LengthScale", 6.3712e6); //Default

  Teuchos::RCP<Teuchos::FancyOStream> out(Teuchos::VerboseObjectBase::getDefaultOStream());
  if (hsType == "None"){ 
    *out << "Zero surface height!" << std::endl;
    hs_type = NONE;
  }
  else if (hsType == "Mountain") {
   *out << "Mountain surface height!" << std::endl;
   hs_type = MOUNTAIN;
   sphere_coord = PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim>(
            p.get<std::string>("Spherical Coord Name"),dl->qp_gradient);
   this->addDependentField(sphere_coord);
  }

  this->addEvaluatedField(hs);

  std::vector<PHX::DataLayout::size_type> dims;
  dl->qp_gradient->dimensions(dims);
  numQPs  = dims[1];
  numDims = dims[2];

  this->setName("SurfaceHeight"+PHX::TypeString<EvalT>::value);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void SurfaceHeight<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(hs,fm);
  if (hs_type == MOUNTAIN)  
    this->utils.setFieldData(sphere_coord,fm); 
}

//**********************************************************************
//IK, 2/5/14
//A concrete (non-virtual) implementation of getValue is needed for code to compile. 
//Do we really need it though for this problem...?
template<typename EvalT,typename Traits>
typename SurfaceHeight<EvalT,Traits>::ScalarT& 
SurfaceHeight<EvalT,Traits>::getValue(const std::string &n)
{
  static ScalarT junk(0);
  return junk;
}


//**********************************************************************
template<typename EvalT, typename Traits>
void SurfaceHeight<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  switch (hs_type) {
    case NONE: //no surface height: hs = 0
      for (std::size_t cell=0; cell < workset.numCells; ++cell) {
        for (std::size_t qp=0; qp < numQPs; ++qp) 
          hs(cell,qp) = 0.0; 
      }
      break; 
    case MOUNTAIN:  //surface height for test case 5
      const double R = pi/9.0; 
      const double lambdac = 2.0*pi/3.0; 
      const double thetac = pi/6.0;  
      const double hs0 = 2000/lengthScale; //meters are units
      for (std::size_t cell=0; cell < workset.numCells; ++cell) {
        for (std::size_t qp = 0; qp < numQPs; ++qp) {
          MeshScalarT theta = sphere_coord(cell,qp,0);
          MeshScalarT lambda = sphere_coord(cell,qp,1);
          MeshScalarT radius2 = (lambda-lambdac)*(lambda-lambdac) + (theta-thetac)*(theta-thetac);
          //r^2 = min(R^2, (lambda-lambdac)^2 + (theta-thetac)^2); 
          MeshScalarT r;  
          if (radius2 > R*R) r = R; 
          else r = sqrt(radius2); 
          //hs = hs0*(1-r/R) for test case 5 
          hs(cell,qp) = hs0*(1.0-r/R); 
        }
      }
      break; 
}
}
}
