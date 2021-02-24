//
// Created by Yohsuke Murase on 2021/02/23.
//

#ifndef CARAVAN_LIB_BASE_HPP
#define CARAVAN_LIB_BASE_HPP

#include <utility>
#include <nlohmann/json.hpp>
#include <icecream.hpp>

namespace caravan {
  using json = nlohmann::json;
  using task_t = std::pair<int64_t, json>; // id,input

  class Queue {
    public:
    Queue() : num_tasks(0) {};

    int64_t Push(const json &input) {
      q.push(std::make_pair(num_tasks, input));
      return num_tasks++;
    };

    task_t Pop() {
      task_t t = q.front();
      q.pop();
      return t;
    }

    size_t Size() const { return q.size(); }

    private:
    std::queue <task_t> q;
    int64_t num_tasks;
  };
}

#endif // CARAVAN_LIB_BASE_HPP