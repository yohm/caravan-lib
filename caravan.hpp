#ifndef CARAVAN_HPP
#define CARAVAN_HPP

#include <iostream>
#include <queue>
#include <functional>
#include <mpi.h>
#include <nlohmann/json.hpp>

namespace caravan {
  using json = nlohmann::json;
  class Queue {
    public:
    int64_t Push(const json& input) {};
  };
  void Start( std::function<void(Queue&)> on_init,
              std::function<void(int64_t,const json&,Queue&)> on_result_receive,
              std::function<json(const json&)> do_task,
              MPI_Comm comm = MPI_COMM_WORLD,
              int num_proc_per_buf = 384) {
    Queue q;
    on_init(q);
    json input = { {"message","hello"} };
    json output = do_task(input);
    on_result_receive(0, output, q);
  }
}

#endif // CARAVAN_HPP