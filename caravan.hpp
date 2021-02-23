#ifndef CARAVAN_HPP
#define CARAVAN_HPP

#include <iostream>
#include <functional>
#include <mpi.h>
#include <nlohmann/json.hpp>

namespace caravan {
  using json = nlohmann::json;
  class Queue {
  };
  void Start( std::function<void(Queue&)> on_init,
              std::function<void(Queue&,const json&)> on_result_receive,
              std::function<json(const json&)> do_task,
              MPI_Comm comm = MPI_COMM_WORLD,
              int num_proc_per_buf = 384);
}

#endif // CARAVAN_HPP