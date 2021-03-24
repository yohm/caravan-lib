#include <iostream>
#include <mpi.h>
#include "caravan.hpp"


using json = nlohmann::json;

void sample_1() {
  std::function<void(caravan::Queue&)> on_init = [](caravan::Queue& q) {
    // pre-process: create json_object that contains parameters of Tasks
    for (int i = 0; i < 3; i++) {
      json input = { {"message","hello"}, {"param", i} };
      uint64_t task_id = q.Push(input);
      std::cerr << "task: " << task_id << " has been created: " << input << "\n";
    }
  };

  std::function<void(int64_t, const json&, const json&, caravan::Queue&)> on_result_receive = [](int64_t task_id, const json& input, const json& output, caravan::Queue& q) {
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
}

void test_1() {
  std::function<void(caravan::Queue&)> on_init = [](caravan::Queue& q) {
    for (int i = 0; i < 3; i++) {
      json input = { {"message","hello"}, {"param", i} };
      uint64_t task_id = q.Push(input);
      assert(task_id == i);
    }
  };

  std::function<void(int64_t, const json&, const json&, caravan::Queue&)> on_result_receive = [](int64_t task_id, const json& input, const json& output, caravan::Queue& q) {
    assert(input["message"].get<std::string>() == "hello");
    assert(input["param"].get<int>() + 1 == output["result"].get<int>());
  };

  std::function<json(const json& input)> do_task = [](const json& input) {
    json output;
    output["result"] = input["param"].get<int>() + 1;
    return output;
  };

  caravan::Start(on_init, on_result_receive, do_task, MPI_COMM_WORLD);
}

void test_2() {
  uint64_t last_task_id = 0;
  std::function<void(caravan::Queue&)> on_init = [&last_task_id](caravan::Queue& q) {
    for (int i = 0; i < 3; i++) {
      json input = { {"param", i} };
      uint64_t task_id = q.Push(input);
      last_task_id = task_id;
    }
  };

  std::function<void(int64_t, const json&, const json&, caravan::Queue&)> on_result_receive = [&last_task_id](int64_t task_id, const json& input, const json& output, caravan::Queue& q) {
    assert(input["param"].get<int>() + 1 == output["result"].get<int>());
    int i = input["param"].get<int>();
    if (i % 2 == 0) {
      json input_2 = { {"param", i+1}};
      last_task_id = q.Push(input_2);
    }
  };

  std::function<json(const json& input)> do_task = [](const json& input) {
    json output;
    output["result"] = input["param"].get<int>() + 1;
    return output;
  };

  caravan::Start(on_init, on_result_receive, do_task, MPI_COMM_WORLD);

  int my_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  if (my_rank == 0) {
    assert(last_task_id == 4);
  }
}

int main(int argc, char* argv[]) {

  MPI_Init(&argc, &argv);

  // test cases
  test_1();
  test_2();

  sample_1();

  MPI_Finalize();

  return 0;
}
