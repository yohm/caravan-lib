//
// Created by Yohsuke Murase on 2021/02/23.
//

#ifndef CARAVAN_LIB_TASKRESULT_HPP
#define CARAVAN_LIB_TASKRESULT_HPP

#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <sstream>
#include "Base.hpp"

namespace caravan_impl {

using json = nlohmann::json;

class TaskResult {
 public:
  TaskResult(int64_t _task_id, const json& _input, long _rank, long _start_at, long _finish_at)
      : task_id(_task_id), input(_input), rank(_rank), start_at(_start_at), finish_at(_finish_at) {};
  TaskResult() : task_id(0), rank(-1), start_at(-1), finish_at(-1) {};
  int64_t task_id;
  json input;
  long rank;
  long start_at;
  long finish_at;
  json output;
  double ElapsedTime() const {
    return static_cast<double>(finish_at - start_at) / 1000.0;
  }
};

void to_json(json &j, const TaskResult &tr) {
  j = json{{"id", tr.task_id}, {"input",  tr.input}, {"rank", tr.rank}, {"start_at", tr.start_at}, {"finish_at", tr.finish_at},
           {"output", tr.output}};
}

void from_json(const json &j, TaskResult &tr) {
  j.at("id").get_to(tr.task_id);
  tr.input = j.at("input");
  j.at("rank").get_to(tr.rank);
  j.at("start_at").get_to(tr.start_at);
  j.at("finish_at").get_to(tr.finish_at);
  tr.output = j.at("output");
}

}
#endif //CARAVAN_LIB_TASKRESULT_HPP
