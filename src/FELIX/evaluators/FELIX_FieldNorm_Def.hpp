//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Phalanx_DataLayout.hpp"
#include "Phalanx_TypeStrings.hpp"

#include "FELIX_HomotopyParameter.hpp"

//uncomment the following line if you want debug output to be printed to screen
#define OUTPUT_TO_SCREEN

namespace FELIX {

//**********************************************************************
template<typename EvalT, typename Traits>
FieldNorm<EvalT, Traits>::FieldNorm (const Teuchos::ParameterList& p,
                                     const Teuchos::RCP<Albany::Layouts>& dl)
{
  std::string fieldName = p.get<std::string> ("Field Name");
  std::string fieldNormName = p.get<std::string> ("Field Norm Name");

  std::string layout = p.get<std::string>("Field Layout");
  if (layout=="Cell Node")
  {
    PHX::MDField<ScalarT> f(fieldName, dl->node_vector);
    PHX::MDField<ScalarT> fn(fieldNormName, dl->node_scalar);
    field = f;
    field_norm = fn;

    dl->node_vector->dimensions(dims);
  }
  else if (layout=="Cell QuadPoint")
  {
    PHX::MDField<ScalarT> f(fieldName, dl->qp_vector);
    PHX::MDField<ScalarT> fn(fieldNormName, dl->qp_scalar);
    field = f;
    field_norm = fn;

    dl->qp_vector->dimensions(dims);
  }
  else if (layout=="Cell Side Node")
  {
    PHX::MDField<ScalarT> f(fieldName, dl->side_node_vector);
    PHX::MDField<ScalarT> fn(fieldNormName, dl->side_node_scalar);
    field = f;
    field_norm = fn;

    dl->side_node_vector->dimensions(dims);

    sideSetName = p.get<std::string>("Side Set Name");
  }
  else if (layout=="Cell Side QuadPoint")
  {
    PHX::MDField<ScalarT> f(fieldName, dl->side_qp_vector);
    PHX::MDField<ScalarT> fn(fieldNormName, dl->side_qp_scalar);
    field = f;
    field_norm = fn;

    dl->side_qp_vector->dimensions(dims);

    sideSetName = p.get<std::string>("Side Set Name");
  }
  else
  {
    TEUCHOS_TEST_FOR_EXCEPTION (true, Teuchos::Exceptions::InvalidParameter, "Error! Invalid field layout.\n");
  }

  this->addDependentField(field);
  this->addEvaluatedField(field_norm);

  if (p.isParameter("Regularization"))
  {
    regularizationParam = p.get<double>("Regularization");
    regularizationParamPtr = &regularizationParam;
  }
  else
  {
    regularizationParamPtr = &FELIX::HomotopyParameter<EvalT>::value;
  }

  numDims = dims.size();

  this->setName("FieldNorm"+PHX::typeAsString<EvalT>());
}

//**********************************************************************
template<typename EvalT, typename Traits>
void FieldNorm<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(field,fm);
  this->utils.setFieldData(field_norm,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void FieldNorm<EvalT, Traits>::evaluateFields (typename Traits::EvalData workset)
{
#ifdef OUTPUT_TO_SCREEN
    Teuchos::RCP<Teuchos::FancyOStream> output(Teuchos::VerboseObjectBase::getDefaultOStream());

    if (regularizationParamPtr!=0 &&  std::fabs(printedH-*regularizationParamPtr)>0.0001)
    {
        *output << "[Field Norm<" << PHX::typeAsString<EvalT>() << ">]] h = " << *regularizationParamPtr << "\n";
        printedH = *regularizationParamPtr;
    }
#endif

  ScalarT ff = 0;
  if (regularizationParamPtr!=0 && *regularizationParamPtr!=0)
    ff = pow(10.0, -10.0*(*regularizationParamPtr));

  ScalarT norm;
  switch (numDims)
  {
    case 2:
      // Cell vector
      for (int cell(0); cell<dims[0]; ++cell)
      {
        norm = 0;
        for (int dim(0); dim<dims[1]; ++dim)
        {
          norm += std::pow(field(cell,dim),2);
        }
        field_norm(cell) = std::sqrt(norm + ff);
      }
      break;
    case 3:
      // Cell Node/QuadPoint Vector
      for (int cell(0); cell<dims[0]; ++cell)
      {
        norm = 0;
        for (int i(0); i<dims[1]; ++i)
        {
          for (int dim(0); dim<dims[2]; ++dim)
          {
            norm += std::pow(field(cell,i,dim),2);
          }
          field_norm(cell,i) = std::sqrt(norm + ff);
        }
      }
      break;
    case 4:
      // Cell Side Node/QuadPoint Vector
      {
        const Albany::SideSetList& ssList = *(workset.sideSets);
        Albany::SideSetList::const_iterator it_ss = ssList.find(sideSetName);

        if (it_ss==ssList.end())
          return;

        const std::vector<Albany::SideStruct>& sideSet = it_ss->second;
        std::vector<Albany::SideStruct>::const_iterator iter_s;
        for (iter_s=sideSet.begin(); iter_s!=sideSet.end(); ++iter_s)
        {
          // Get the local data of side and cell
          const int cell = iter_s->elem_LID;
          const int side = iter_s->side_local_id;

          norm = 0;
          for (int i(0); i<dims[2]; ++i)
          {
            for (int dim(0); dim<dims[3]; ++dim)
            {
              norm += std::pow(field(cell,side,i,dim),2);
            }
            field_norm(cell,side,i) = std::sqrt(norm + ff);
          }
        }
      }
      break;
    default:
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "Error! Invalid field layout.\n");
  }
}

} // Namespace FELIX
