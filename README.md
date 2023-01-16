# CARAVAN lib

CARAVAN-lib is a header-only C++ library for master-worker task scheduling on multiple processes using MPI.


## A first-look

With CARAVAN-lib, you can distribute tasks to multiple processes and executes them in parallel. The code to define tasks which look like the following.

```cpp
  // define a pre-process: create json object that contains parameters of tasks
  // This function is called only at the master process.
  std::function<void(caravan::Queue&)> on_init = [](caravan::Queue& q) {
    json input = { {"message","hello"} };
    uint64_t task_id = q.Push(input);
    std::cerr << "task: " << task_id << " has been created: " << input << "\n";
  };

  // After the task was executed at a worker process, its result is returned to the master process.
  // When the master process receives the result, this callback function is called at the master process.
  std::function<void(int64_t, const json&, const json&, caravan::Queue&)> on_result_receive = [](int64_t task_id, const json& input, const json& output, caravan::Queue& q) {
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

This is a header-only library. Just clone the repository and include "caravan.hpp", which is the easiest way to use.
Or, add the repository as a submodule to your repository.
Since caravan-lib contains dependent libraries as submodules, do not forget to update the submodules recursively.

```shell
git submodule add https://github.com/yohm/caravan-lib
git submodule update --init --recursive
```

One of the dependent libraries is [nlohmann/json](https://github.com/nlohmann/json).
This is used to serialize data used in tasks. Please read its documentation. You need to use it to define the inputs and outputs of tasks.
To install json library on macOS, you can use [homebrew](https://brew.sh/).

```shell
brew install nlohmann_json
```

To use the json library in your cmake project, you can use `find_package` command. Also specify json and MPI libraries in `target_link_libraries` command.

```cmake
find_package(nlohmann_json)
#  ...
target_link_libraries(your_target MPI::MPI_CXX nlohmann_json::nlohmann_json)
```

After you add this repository as a submodule, you can use it just by adding it to the include paths.
If you are using cmake, add the following line to your `CMakeLists.txt`.

```cmake
include_directories(${CMAKE_SOURCE_DIR}/caravan-lib)
```

## Usage

As shown above, what we need to define are the three functions `on_init`, `on_result_receive`, and `do_task`. Once we define these functions, we can execute these tasks in parallel on high-performance computers.

- The first function `on_init` is first executed **at the master process**. It is supposed to prepare inputs for the tasks. Its argument is `caravan::Queue` to which we enqueue inputs like `q.Push(input)`, where `input` is a JSON object having the input of a task.
    - When you call `Push()` to enqueue your task, its `task_id` is returned. This id is a serial integer assigned for each task starting from 0.
- The second function `on_result_receive = [](int64_t task_id, const json& input, const json& output, caravan::Queue& q)` is executed when a task gets complete. Note that this function is executed **at the master process**, namely, this function is executed after the output of a task is transferred from the worker process to the master process.
    - `output` is the json object returned from a task executed at a worker process.
    - `input` is the json object which is enqueued in `on_init` or `on_result_receive`.
    - You can also add additional tasks if you want. call `Push` method against the fourth argument `q`.
- The third function `std::function<json(const json& input)> do_task = [](const json& input)` defines the actual task, which is done **at a worker process** for a given input. Return a json object as an output, which will then transferred to the master process.

Please note that `do_task` is executed at **a worker process** while the other two functions are executed at **the master process**. So you can not refer to a variable that is defined in the `do_task` function at the master process and vice versa. Use `input` and `output` to send/receive values between different processes.

## Features

- Highly scalable: This library is designed to work on massive parallel computers having tens of thousands of CPUs.
- Dynamical balancing: The task execution is scheduled dynamically, making it efficiently work with highly heterogeneous loads.
- Generic input/output: We can make tasks with arbitrary types of inputs and outputs as long as they are serializable into JSON.

On the other hand, we have the following limitations:

- The library is not designed for tiny-scale tasks which typically finish in milliseconds or less. The typical duration of tasks should be in the order of seconds or longer.
- You need more than three processes because one process is used for the master process, at least one process is used for communication, and other processes are used as workers.

## Reference

This project is a part of [caravan](https://github.com/crest-cassia/caravan).
When you use this library for a research, please cite [this article](https://link.springer.com/chapter/10.1007/978-3-030-20937-7_9) as follows.

```bibtex
@inproceedings{murase2018caravan,
  title={Caravan: a framework for comprehensive simulations on massive parallel machines},
  author={Murase, Yohsuke and Matsushima, Hiroyasu and Noda, Itsuki and Kamada, Tomio},
  booktitle={International Workshop on Massively Multiagent Systems},
  pages={130--143},
  year={2018},
  organization={Springer}
}
```
