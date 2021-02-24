#include <iostream>
#include <mpi.h>
#include <nlohmann/json.hpp>
#include "caravan.hpp"


using json = nlohmann::json;

int main(int argc, char* argv[]) {

  MPI_Init(&argc, &argv);

  auto on_init = [](caravan::Queue& q) {
    // pre-process: create json_object that contains parameters of Tasks
    for (int i = 0; i < 3; i++) {
      json input = { {"message","hello"}, {"param", i} };
      uint64_t task_id = q.Push(input);
      std::cerr << "task: " << task_id << " has been created: " << input << "\n";
    }
  };

  auto on_result_receive = [](int64_t task_id, const json& input, const json& output, caravan::Queue& q) {
    std::cerr << "task: " << task_id << " has finished, input: " << input << ", output: " << output << "\n";

    if (input["message"].get<std::string>() == "hello") {
      json input_2 = { {"message", "bye"}, {"param", input["param"].get<int>()}};
      q.Push(input_2);
    }
  };

  std::function<json(const json& input)> do_task = [](const json& input) {
    // do some job
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::cerr << "doing tasks: " << input << " at rank " << rank << "\n";
    sleep(1);
    json output;
    output["result"] = input["message"].get<std::string>() + " world";
    return output;
  };

  caravan::Start(on_init, on_result_receive, do_task, MPI_COMM_WORLD);

  MPI_Finalize();

  return 0;
}
