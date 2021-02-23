#include <iostream>
#include <mpi.h>
#include <nlohmann/json.hpp>
#include "caravan.hpp"


using json = nlohmann::json;

int main(int argc, char* argv[]) {

  MPI_Init(&argc, &argv);

  auto on_init = [](caravan::Queue& q) {
    // pre-process: create json_object that contains parameters of Tasks
    json input = { {"message","hello"} };
    uint64_t task_id = q.Push(input);

    std::cerr << "task: " << task_id << " has been created.\n";
  };

  auto on_result_receive = [](int64_t task_id, const json& output, caravan::Queue& q) {
    std::cerr << "task: " << task_id << " has finished : " << output << "\n";
  };

  std::function<json(const json& input)> do_task = [](const json& input) {
    // do some job
    json output;
    output["result"] = input["message"].get<std::string>() + " world";
    return output;
  };

  caravan::Start(on_init, on_result_receive, do_task, MPI_COMM_WORLD);

  MPI_Finalize();

  return 0;
}
