# CARAVAN lib

CARAVAN-lib is a header-only C++ library for master-worker task scheduling on multiple processes using MPI.


## A first-look

With CARAVAN-lib, you can distribute tasks to multiple processes and executes them in parallel. The code to define tasks look like the following.

```cpp
  // define a pre-process: create json object that contains parameters of tasks
  // This function is called only at the master process.
  auto on_init = [](caravan::Queue& q) {
    json input = { {"message","hello"} };
    uint64_t task_id = q.Push(input);
    std::cerr << "task: " << task_id << " has been created: " << input << "\n";
  };

  // After the task was executed at a worker process, its result is returned to the master process.
  // When the master process receives the result, this callback function is called at the master process.
  auto on_result_receive = [](int64_t task_id, const json& input, const json& output, caravan::Queue& q) {
    std::cerr << "task: " << task_id << " has finished: input: " << input << ", output: " << output << "\n";
  };

  // Define the function which is executed at a worker process.
  // The input parameter for the task is given as the argument.
  std::function<json(const json& input)> do_task = [](const json& input) {
    // do some job
    std::cerr << "doing task:" << input << "\n";
    sleep(1);
    json output;
    output["result"] = input["message"].get<std::string>() + " world";
    return output;
  };

  // call "Start" function to start scheduling. Tasks are dynamically allocated to worker processes.
  caravan::Start(on_init, on_result_receive, do_task, MPI_COMM_WORLD);
```

After running this code, you'll find the output like the following. Note that the second line was printed by a worker process.
If you create more tasks, the tasks are executed simultaneously.

```
task: 0 has been created: {"message":"hello"}
doing tasks: {"message":"hello"}
task: 0 has finished, input: {"message":"hello"}, output: {"result":"hello world"}
```

## Install

## Usage

