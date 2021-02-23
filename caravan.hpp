#ifndef CARAVAN_HPP
#define CARAVAN_HPP

#include <iostream>
#include <queue>
#include <functional>
#include <mpi.h>
#include <nlohmann/json.hpp>
#include <icecream.hpp>

namespace caravan {
  using json = nlohmann::json;
  using task_t = std::pair<int64_t,json>;

  class Queue {
    public:
    Queue() : num_tasks(0) {};
    int64_t Push(const json& input) {
      q.push( std::make_pair(num_tasks,input) );
      return num_tasks++;
    };
    task_t Pop() {
      task_t t = q.front();
      q.pop();
      return t;
    }
    std::queue<task_t> q;
    int64_t num_tasks;
  };
  void Start( std::function<void(Queue&)> on_init,
              std::function<void(int64_t,const json&,const json&,Queue&)> on_result_receive,
              std::function<json(const json&)> do_task,
              MPI_Comm comm = MPI_COMM_WORLD,
              int num_proc_per_buf = 384) {
    Queue q;
    on_init(q);
    task_t task = q.Pop();
    json input = task.second;
    json output = do_task(input);
    on_result_receive(task.first, input, output, q);
  }
}

#endif // CARAVAN_HPP