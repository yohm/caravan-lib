#ifndef CARAVAN_HPP
#define CARAVAN_HPP

#include <iostream>
#include <vector>
#include <queue>
#include <set>
#include <functional>
#include <unistd.h>
#include <mpi.h>
#include <nlohmann/json.hpp>
#include <icecream.hpp>
#include "Logger.hpp"
#include "Base.hpp"
#include "Producer.hpp"
#include "Buffer.hpp"
#include "Consumer.hpp"

namespace caravan {

  namespace caravan_impl {

    // role(0:prod,1:buf,2:cons), parent, children
    std::tuple<int, int, std::vector<int>> GetRole(int rank, int procs, int num_proc_per_buf) {
      assert(procs >= 2);
      int role, parent;
      std::vector<int> children;
      if (rank == 0) {
        role = 0;
        parent = -1;
        children.push_back(1);
        for (int i = num_proc_per_buf; i < procs; i += num_proc_per_buf) { children.push_back(i); }
      } else if (rank == 1) {
        role = 1;
        parent = 0;
        for (int i = 2; i < num_proc_per_buf && i < procs; i++) { children.push_back(i); }
      } else if (rank % num_proc_per_buf == 0) {
        role = 1;
        parent = 0;
        for (int i = rank + 1; i < rank + num_proc_per_buf && i < procs; i++) { children.push_back(i); }
      } else {
        role = 2;
        parent = (rank / num_proc_per_buf) * num_proc_per_buf;
        if (parent == 0) { parent = 1; }
      }
      return std::make_tuple(role, parent, children);
    }
  }

  using json = nlohmann::json;

  void Start( const std::function<void(Queue&)>& on_init,
              const std::function<void(int64_t, const json&, const json&, Queue&)>& on_result_receive,
              const std::function<json(const json&)>& do_task,
              MPI_Comm comm = MPI_COMM_WORLD,
              int num_proc_per_buf = 384) {
    int rank, procs;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &procs);

    // synchronize clock
    std::chrono::system_clock::time_point start;
    if (rank == 0) {
      start = std::chrono::system_clock::now();
    }
    MPI_Bcast((void *) &start, sizeof(std::chrono::system_clock::time_point), MPI_CHAR, 0, MPI_COMM_WORLD);

    auto role = caravan_impl::GetRole(rank, procs, num_proc_per_buf);
    Logger logger(start, 2);

    if (std::get<0>(role) == 0) {  // Producer
      ::caravan_impl::Producer prod(logger);
      on_init(prod.tasks);
      prod.Run(std::get<2>(role), on_result_receive);
    }
    else if (std::get<0>(role) == 1) {  // Buffer
      ::caravan_impl::Buffer buf(std::get<1>(role), logger);
      buf.Run(std::get<2>(role));
    }
    else if (std::get<0>(role) == 2) {  // Consumer
      ::caravan_impl::Consumer cons(std::get<1>(role), logger);
      cons.Run(rank, start, do_task);
    }
  }
}

#endif // CARAVAN_HPP