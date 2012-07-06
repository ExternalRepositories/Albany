/********************************************************************  \
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
#include<ctime>

#include <Teuchos_UnitTestHarness.hpp>
#include "Tensor.h"
#include "Intrepid_FieldContainer.hpp"

using namespace std;

typedef double ScalarT;

namespace {

  TEUCHOS_UNIT_TEST( TensorUtils, Initialization )
  {
    Intrepid::FieldContainer<ScalarT> FC(3, 3);
    FC(0, 0) = 1.0;
    FC(0, 1) = 2.0;
    FC(0, 2) = 3.0;
    FC(1, 0) = 4.0;
    FC(1, 1) = 5.0;
    FC(1, 2) = 6.0;
    FC(2, 0) = 7.0;
    FC(2, 1) = 8.0;
    FC(2, 2) = 9.0;

    const ScalarT * dataPtr0 = &FC(0, 0);

    LCM::Vector<ScalarT, 3> u(dataPtr0);

    TEST_COMPARE( u(0), ==, 1.0);
    TEST_COMPARE( u(1), ==, 2.0);
    TEST_COMPARE( u(2), ==, 3.0);

    const ScalarT * dataPtr1 = &FC(1, 0);

    u = LCM::Vector<ScalarT, 3>(dataPtr1);

    TEST_COMPARE( u(0), ==, 4.0);
    TEST_COMPARE( u(1), ==, 5.0);
    TEST_COMPARE( u(2), ==, 6.0);

    const ScalarT * dataPtr2 = &FC(2, 0);

    u = LCM::Vector<ScalarT, 3>(dataPtr2);

    TEST_COMPARE( u(0), ==, 7.0);
    TEST_COMPARE( u(1), ==, 8.0);
    TEST_COMPARE( u(2), ==, 9.0);
  }

  TEUCHOS_UNIT_TEST( TensorUtils, VectorAddition )
  {
    LCM::Vector<ScalarT, 3> u(1.0, 0.0, 0.0);
    LCM::Vector<ScalarT, 3> v(0.0, 1.0, 0.0);
    LCM::Vector<ScalarT, 3> w(1.0, 1.0, 0.0);

    TEST_COMPARE( u + v == w, !=, 0);
  }

  TEUCHOS_UNIT_TEST( TensorUtils, VectorSubtraction )
  {
    LCM::Vector<ScalarT, 3> u(0.0);
    LCM::Vector<ScalarT, 3> v(1.0);
    u(0) = 1.0;
    u(1) = 2.0;
    u(2) = 3.0;

    v = u - u;

    TEST_COMPARE( LCM::norm(v), <=, std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST( TensorUtils, VectorScalarMultipliaction )
  {
    LCM::Vector<ScalarT, 3> u(0.0);
    LCM::Vector<ScalarT, 3> v(1.0);
    LCM::Vector<ScalarT, 3> w(1.0);
    u(0) = 1.0;
    u(1) = 2.0;
    u(2) = 3.0;

    v(0) = -2.0;
    v(1) = -4.0;
    v(2) = -6.0;

    w = 4.0 * u + 2.0 * v;

    TEST_COMPARE( LCM::norm(w), <=, std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST( TensorUtils, TensorInstantiation )
  {
    Intrepid::FieldContainer<ScalarT> FC(2, 3, 3);
    FC(0, 0, 0) = 1.0;
    FC(0, 0, 1) = 2.0;
    FC(0, 0, 2) = 3.0;
    FC(0, 1, 0) = 4.0;
    FC(0, 1, 1) = 5.0;
    FC(0, 1, 2) = 6.0;
    FC(0, 2, 0) = 7.0;
    FC(0, 2, 1) = 8.0;
    FC(0, 2, 2) = 9.0;
    FC(1, 0, 0) = 10.0;
    FC(1, 0, 1) = 11.0;
    FC(1, 0, 2) = 12.0;
    FC(1, 1, 0) = 13.0;
    FC(1, 1, 1) = 14.0;
    FC(1, 1, 2) = 15.0;
    FC(1, 2, 0) = 16.0;
    FC(1, 2, 1) = 17.0;
    FC(1, 2, 2) = 18.0;

    const ScalarT * dataPtr0 = &FC(0, 0, 0);

    LCM::Tensor<ScalarT, 3> A(dataPtr0);

    TEST_COMPARE( A(0,0), ==, 1.0);
    TEST_COMPARE( A(0,1), ==, 2.0);
    TEST_COMPARE( A(0,2), ==, 3.0);
    TEST_COMPARE( A(1,0), ==, 4.0);
    TEST_COMPARE( A(1,1), ==, 5.0);
    TEST_COMPARE( A(1,2), ==, 6.0);
    TEST_COMPARE( A(2,0), ==, 7.0);
    TEST_COMPARE( A(2,1), ==, 8.0);
    TEST_COMPARE( A(2,2), ==, 9.0);

    const ScalarT * dataPtr1 = &FC(1, 0, 0);

    LCM::Tensor<ScalarT, 3> B(dataPtr1);

    TEST_COMPARE( B(0,0), ==, 10.0);
    TEST_COMPARE( B(0,1), ==, 11.0);
    TEST_COMPARE( B(0,2), ==, 12.0);
    TEST_COMPARE( B(1,0), ==, 13.0);
    TEST_COMPARE( B(1,1), ==, 14.0);
    TEST_COMPARE( B(1,2), ==, 15.0);
    TEST_COMPARE( B(2,0), ==, 16.0);
    TEST_COMPARE( B(2,1), ==, 17.0);
    TEST_COMPARE( B(2,2), ==, 18.0);
  }

  TEUCHOS_UNIT_TEST( TensorUtils, TensorAddition )
  {
    LCM::Tensor<ScalarT, 3> A(1.0);
    LCM::Tensor<ScalarT, 3> B(2.0);
    LCM::Tensor<ScalarT, 3> C(3.0);

    TEST_COMPARE( C == A + B, !=, 0);
  }

  TEUCHOS_UNIT_TEST( TensorUtils, TensorManipulation )
  {
    LCM::Tensor<ScalarT, 3> A = LCM::eye<ScalarT, 3>();
    LCM::Tensor<ScalarT, 3> B(0.0);
    LCM::Tensor<ScalarT, 3> C(0.0);
    LCM::Vector<ScalarT, 3> u(0.0);

    A = 2.0 * A;
    A(1, 0) = A(0, 1) = 1.0;
    A(2, 1) = A(1, 2) = 1.0;

    B = LCM::inverse(A);

    C = A * B;

    TEST_COMPARE( LCM::norm(C - LCM::eye<ScalarT, 3>()), <=,
        std::numeric_limits<ScalarT>::epsilon());

    ScalarT I1 = LCM::I1(A);
    ScalarT I2 = LCM::I2(A);
    ScalarT I3 = LCM::I3(A);

    u(0) = I1 - 6;
    u(1) = I2 - 10;
    u(2) = I3 - 4;

    TEST_COMPARE( LCM::norm(u), <=, std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST( TensorUtils, TensorLogExp )
  {
    LCM::Tensor<ScalarT, 3> A = LCM::eye<ScalarT, 3>();
    LCM::Tensor<ScalarT, 3> B = LCM::log(A);

    TEST_COMPARE( LCM::norm(B), <=, std::numeric_limits<ScalarT>::epsilon());

    LCM::Tensor<ScalarT, 3> C = LCM::exp(B);

    C -= A;

    TEST_COMPARE( LCM::norm(C), <=, std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST( TensorUtils, TensorEig_Spd )
  {
    LCM::Tensor<ScalarT, 3> A = LCM::eye<ScalarT, 3>();
    A(0, 1) = 0.1;
    A(1, 0) = 0.1;
    LCM::Tensor<ScalarT, 3> eVec;
    LCM::Tensor<ScalarT, 3> eVal;
    boost::tie(eVec, eVal) = LCM::eig_spd(A);

    TEST_COMPARE( std::abs(eVal(0,0) - 1.0), <=,
        std::numeric_limits<ScalarT>::epsilon());
    TEST_COMPARE( std::abs(eVal(1,1) - 0.9), <=,
        std::numeric_limits<ScalarT>::epsilon());
    TEST_COMPARE( std::abs(eVal(2,2) - 1.1), <=,
        std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST( TensorUtils, TensorLeftPolarDecomposition )
  {
    LCM::Tensor<ScalarT, 3> V0(1.1, 0.2, 0.0, 0.2, 1.0, 0.0, 0.0, 0.0, 1.2);
    LCM::Tensor<ScalarT, 3> R0(sqrt(2) / 2, -sqrt(2) / 2, 0.0, sqrt(2) / 2,
        sqrt(2) / 2, 0.0, 0.0, 0.0, 1.0);

    LCM::Tensor<ScalarT, 3> F = V0 * R0;
    LCM::Tensor<ScalarT, 3> V;
    LCM::Tensor<ScalarT, 3> R;
    boost::tie(V, R) = LCM::polar_left(F);

    TEST_COMPARE( LCM::norm(V-V0), <=,
        10*std::numeric_limits<ScalarT>::epsilon());
    TEST_COMPARE( LCM::norm(R-R0), <=, std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST( TensorUtils, TensorLogRotation )
  {
    LCM::Tensor<ScalarT, 3> R = LCM::identity<ScalarT, 3>();
    LCM::Tensor<ScalarT, 3> R0(sqrt(2) / 2, -sqrt(2) / 2, 0.0, sqrt(2) / 2,
        sqrt(2) / 2, 0.0, 0.0, 0.0, 1.0);

    LCM::Tensor<ScalarT, 3> r = log_rotation(R);
    LCM::Tensor<ScalarT, 3> r0 = log_rotation(R0);

    TEST_COMPARE( LCM::norm(r), <=, std::numeric_limits<ScalarT>::epsilon());
    TEST_COMPARE( std::abs(r0(0,1) + 0.785398163397448), <=,
        10*std::numeric_limits<ScalarT>::epsilon());
    TEST_COMPARE( std::abs(r0(0,1) + r0(1,0)), <=,
        std::numeric_limits<ScalarT>::epsilon());

    ScalarT theta = std::acos(-1.0)
        + 10 * std::numeric_limits<ScalarT>::epsilon(); // rotation angle

    R(0, 0) = cos(theta);
    R(1, 1) = cos(theta);
    R(0, 1) = sin(theta);
    R(1, 0) = -sin(theta);
    R(2, 2) = 1.0;

    LCM::Tensor<ScalarT, 3> logR = log_rotation(R);

    LCM::Tensor<ScalarT, 3> Rref(0.0);
    Rref(0, 1) = -theta;
    Rref(1, 0) = theta;

    TEST_COMPARE( LCM::norm(logR - Rref), <=,
        100*std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST( TensorUtils, TensorBCHExp )
  {
    LCM::Tensor<ScalarT, 3> F = 3.0 * LCM::identity<ScalarT, 3>();
    LCM::Tensor<ScalarT, 3> V, R, logV, logR;

    boost::tie(V, R, logV) = polar_left_logV(F);
    logR = log_rotation(R);

    LCM::Tensor<ScalarT, 3> f = LCM::bch(logV, logR);

    TEST_COMPARE( std::abs(f(0,0) - std::log(3.0)), <=,
        std::numeric_limits<ScalarT>::epsilon());

    LCM::Vector<ScalarT, 3> u(0.0);
    u(0) = std::acos(-1.0) / std::sqrt(2.0);
    u(1) = u(0);
    u(2) = 0.0;

    LCM::Tensor<ScalarT, 3> R1(0.0);
    LCM::Tensor<ScalarT, 3> logR2(0.0);
    logR2(0, 2) = u(1);
    logR2(1, 2) = -u(0);
    logR2(2, 0) = -u(1);
    logR2(2, 1) = u(0);
    logR2(0, 1) = -u(2);
    logR2(1, 0) = u(2);

    R1 = LCM::exp_skew_symmetric(logR2);
    LCM::Tensor<ScalarT, 3> Rref = LCM::zero<ScalarT, 3>();
    Rref(0, 1) = 1.0;
    Rref(1, 0) = 1.0;
    Rref(2, 2) = -1.0;

    TEST_COMPARE( norm(Rref-R1), <=,
        100*std::numeric_limits<ScalarT>::epsilon());
    TEST_COMPARE( norm(exp_skew_symmetric(logR) - R), <=,
        100*std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST( TensorUtils, TensorVolDev )
  {
    LCM::Tensor<ScalarT, 3> A = 3.0 * LCM::eye<ScalarT, 3>();

    TEST_COMPARE( norm(A - vol(A)), <=,
        100*std::numeric_limits<ScalarT>::epsilon());

    A = 3.0;
    LCM::Tensor<ScalarT, 3> B = dev(A);
    A(0, 0) = 0.0;
    A(1, 1) = 0.0;
    A(2, 2) = 0.0;
    TEST_COMPARE( norm(A - B), <=, 100*std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST(TensorUtils, TensorSVD2x2BiDiag)
  {
    const ScalarT
    f = 1.0;

    const ScalarT
    g = 3.0;

    const ScalarT
    h = 2.0;

    LCM::Tensor<ScalarT, 2>
    U, S, V;

    boost::tie(U, S, V) = LCM::svd_bidiagonal(f, g, h);

    const LCM::Tensor<ScalarT, 2>
    B = U * S * LCM::transpose(V);

    const LCM::Tensor<ScalarT, 2>
    A(f, g, 0, h);

    TEST_COMPARE(norm(A - B), <=, 100*std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST(TensorUtils, TensorSVD2x2)
  {
    const ScalarT
    phi = 1.0;

    const ScalarT
    psi = 2.0;

    const ScalarT
    s0 = sqrt(3.0);

    const ScalarT
    s1 = sqrt(2.0);

    const ScalarT cl = cos(phi);
    const ScalarT sl = sin(phi);

    const ScalarT cr = cos(psi);
    const ScalarT sr = sin(psi);

    LCM::Tensor<ScalarT, 2>
    X(cl, -sl, sl, cl);

    LCM::Tensor<ScalarT, 2>
    Y(cr, -sr, sr, cr);

    LCM::Tensor<ScalarT, 2>
    D(s0, 0.0, 0.0, s1);

    const LCM::Tensor<ScalarT, 2>
    A = X * D * LCM::transpose(Y);

    LCM::Tensor<ScalarT, 2>
    U, S, V;

    boost::tie(U, S, V) = LCM::svd(A);

    LCM::Tensor<ScalarT, 2>
    B = U * S * LCM::transpose(V);

    TEST_COMPARE(norm(A - B), <=, 100*std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST(TensorUtils, TensorSVD3x3)
  {
    LCM::Tensor<ScalarT, 3>
    A(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0);

    LCM::Tensor<ScalarT, 3>
    U, S, V;

    boost::tie(U, S, V) = LCM::svd(A);

    LCM::Tensor<ScalarT, 3>
    B = U * S * LCM::transpose(V);

    TEST_COMPARE(norm(A - B), <=, 100*std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST(TensorUtils, TensorEigenSym2x2)
  {
    LCM::Tensor<ScalarT, 2>
    A(2.0, 1.0, 1.0, 2.0);

    LCM::Tensor<ScalarT, 2>
    V, D;

    boost::tie(V, D) = LCM::eig_sym(A);

    LCM::Tensor<ScalarT, 2>
    B = V * D * LCM::transpose(V);

    TEST_COMPARE(norm(A - B), <=, 100*std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST(TensorUtils, TensorEigenSym3x3)
  {
    LCM::Tensor<ScalarT, 3>
    A(2.0, 1.0, 0.0, 1.0, 2.0, 1.0, 0.0, 1.0, 2.0);

    LCM::Tensor<ScalarT, 3>
    V, D;

    boost::tie(V, D) = LCM::eig_sym(A);

    LCM::Tensor<ScalarT, 3>
    B = V * D * LCM::transpose(V);

    TEST_COMPARE(norm(A - B), <=, 100*std::numeric_limits<ScalarT>::epsilon());
  }

  TEUCHOS_UNIT_TEST(TensorUtils, TensorInverse4x4)
  {
    LCM::Tensor<ScalarT, 4>
    A(
        2.0, 1.0, 0.0, 0.0,
        1.0, 2.0, 1.0, 0.0,
        0.0, 1.0, 2.0, 1.0,
        0.0, 0.0, 1.0, 2.0
    );

    LCM::Tensor<ScalarT, 4>
    B = inverse(A);

    LCM::Tensor<ScalarT, 4>
    C = A * B;

    LCM::Tensor<ScalarT, 4>
    D = LCM::eye<ScalarT, 4>();

    TEST_COMPARE(norm(C - D), <=, 100*std::numeric_limits<ScalarT>::epsilon());
  }

} // namespace
