//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <Intrepid_MiniTensor.h>
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include <typeinfo>

namespace LCM
{

  //**********************************************************************
  template<typename EvalT, typename Traits>
  KirchhoffStVenant<EvalT, Traits>::KirchhoffStVenant(
      const Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl) :
      defGrad(p.get<std::string>("DefGrad Name"), dl->qp_tensor), J(
          p.get<std::string>("DetDefGrad Name"), dl->qp_scalar), elasticModulus(
          p.get<std::string>("Elastic Modulus Name"), dl->qp_scalar), poissonsRatio(
          p.get<std::string>("Poissons Ratio Name"), dl->qp_scalar), stress(
          p.get<std::string>("Stress Name"), dl->qp_tensor)
  {
    // Pull out numQPs and numDims from a Layout
    std::vector<PHX::DataLayout::size_type> dims;
    dl->qp_tensor->dimensions(dims);
    numQPs = dims[1];
    numDims = dims[2];
    worksetSize = dims[0];

    this->addDependentField(defGrad);
    this->addDependentField(J);
    this->addDependentField(elasticModulus);
    this->addDependentField(poissonsRatio);

    this->addEvaluatedField(stress);

    this->setName("Neohookean Stress" + PHX::TypeString<EvalT>::value);

  }

  //**********************************************************************
  template<typename EvalT, typename Traits>
  void KirchhoffStVenant<EvalT, Traits>::postRegistrationSetup(
      typename Traits::SetupData d, PHX::FieldManager<Traits>& fm)
  {
    this->utils.setFieldData(stress, fm);
    this->utils.setFieldData(defGrad, fm);
    this->utils.setFieldData(J, fm);
    this->utils.setFieldData(elasticModulus, fm);
    this->utils.setFieldData(poissonsRatio, fm);
  }

  //**********************************************************************
  template<typename EvalT, typename Traits>
  void KirchhoffStVenant<EvalT, Traits>::evaluateFields(
      typename Traits::EvalData workset)
  {
    bool print = false;
    if (typeid(ScalarT) == typeid(RealType))
      print = true;

    cout.precision(15);
    ScalarT lambda;
    ScalarT mu;
    for (std::size_t cell = 0; cell < workset.numCells; ++cell) {
      if (print)
        std::cout << "Cell : " << cell << std::endl;
      for (std::size_t qp = 0; qp < numQPs; ++qp) {
        if (print)
          std::cout << "   QP : " << qp << std::endl;
        lambda = (elasticModulus(cell, qp) * poissonsRatio(cell, qp))
            / (1. + poissonsRatio(cell, qp))
            / (1 - 2 * poissonsRatio(cell, qp));
        mu = elasticModulus(cell, qp) / (2. * (1. + poissonsRatio(cell, qp)));

        Intrepid::Tensor<ScalarT> F(numDims, &defGrad(cell, qp, 0, 0));
        Intrepid::Tensor<ScalarT> C(Intrepid::transpose(F) * F);
        Intrepid::Tensor<ScalarT> I(Intrepid::eye<ScalarT>(numDims));
        Intrepid::Tensor<ScalarT> E(0.5 * (C - I));
        ScalarT J = Intrepid::det(F);

        Intrepid::Tensor<ScalarT> S = lambda * Intrepid::trace(E) * I
            + 2 * mu * E;
        Intrepid::Tensor<ScalarT> sigma = (1 / J) * F * S
            * Intrepid::transpose(F);

        if (print)
          std::cout << "       F   :\n" << F << std::endl;
        if (print)
          std::cout << "       sig :\n" << sigma << std::endl;

        for (std::size_t i = 0; i < numDims; ++i)
          for (std::size_t j = 0; j < numDims; ++j)
            stress(cell, qp, i, j) = sigma(i, j);
      }
    }
  }

//**********************************************************************
}

