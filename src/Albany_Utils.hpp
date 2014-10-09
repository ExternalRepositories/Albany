//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef ALBANY_UTILS_H
#define ALBANY_UTILS_H

#ifdef ALBANY_MPI
  #define Albany_MPI_Comm MPI_Comm
  #define Albany_MPI_COMM_WORLD MPI_COMM_WORLD
  #define Albany_MPI_COMM_NULL MPI_COMM_NULL
  #include "Epetra_MpiComm.h"
  #include "Teuchos_DefaultMpiComm.hpp"
#else
  #define Albany_MPI_Comm int
  #define Albany_MPI_COMM_WORLD 0  // This is compatible with Dakota
  #define Albany_MPI_COMM_NULL 99
  #include "Epetra_SerialComm.h"
  #include "Teuchos_DefaultSerialComm.hpp"
#endif
#include "Teuchos_RCP.hpp"
#include "Albany_DataTypes.hpp"

namespace Albany {

  const Albany_MPI_Comm getMpiCommFromEpetraComm(const Epetra_Comm& ec);

  Albany_MPI_Comm getMpiCommFromEpetraComm(Epetra_Comm& ec);

  Teuchos::RCP<Epetra_Comm> createEpetraCommFromMpiComm(const Albany_MPI_Comm& mc);
  Teuchos::RCP<Teuchos_Comm> createTeuchosCommFromMpiComm(const Albany_MPI_Comm& mc);
  Teuchos::RCP<Epetra_Comm> createEpetraCommFromTeuchosComm(const Teuchos::RCP<const Teuchos_Comm>& tc);
  Teuchos::RCP<Teuchos_Comm> createTeuchosCommFromEpetraComm(const Teuchos::RCP<const Epetra_Comm>& ec);

  //! Utility to make a string out of a string + int: strint("dog",2) = "dog 2"
  std::string strint(const std::string s, const int i);

  //! Returns true of the given string is a valid initialization string of the format "initial value 1.54"
  bool isValidInitString(const std::string& initString);

  //! Converts a double to an initialization string:  doubleToInitString(1.54) = "initial value 1.54"
  std::string doubleToInitString(double val);

  //! Converts an init string to a double:  initStringToDouble("initial value 1.54") = 1.54
  double initStringToDouble(const std::string& initString);

  //! Splits a std::string on a delimiter
  void splitStringOnDelim(const std::string &s, char delim, std::vector<std::string> &elems);

  //! Nicely prints out a Tpetra Vector
  void printTpetraVector(std::ostream &os, const Teuchos::RCP<const Tpetra_Vector>& vec);
  void printTpetraVector(std::ostream &os, const Teuchos::Array<std::string>& names,
         const Teuchos::RCP<const Tpetra_Vector>& vec);

  //! Nicely prints out a Tpetra MultiVector
  void printTpetraVector(std::ostream &os, const Teuchos::RCP<const Tpetra_MultiVector>& vec);
  void printTpetraVector(std::ostream &os, const Teuchos::Array<Teuchos::RCP<Teuchos::Array<std::string> > >& names,
         const Teuchos::RCP<const Tpetra_MultiVector>& vec);
}
#endif //ALBANY_UTILS
