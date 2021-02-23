#include <iostream>
#include <mpi.h>
#include <nlohmann/json.hpp>
#include "caravan.hpp"


int main(int argc, char* argv[]) {

  MPI_Init(&argc, &argv);

  MPI_Finalize();

  return 0;
}
