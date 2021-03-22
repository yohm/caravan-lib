# CARAVAN lib

CARAVAN-lib is a header-only C++ library for master-worker task scheduling on multiple processes using MPI.


## A first-look

With CARAVAN-lib, you can distribute tasks to multiple processes and executes them in parallel. The code to define tasks look like the following.

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

## Usage

As shown above, what we need to define is the three functions `on_init`, `on_result_receive`, and `do_task`. Once we define these functions, we can execute these tasks in parallel on a high-performance computers.

- The first function `on_init` is executed at the first step at the master process. It is supposed to prepare inputs for the tasks. Its argument is `caravan::Queue` to which we enqueue inputs like `q.Push(input)`, where `input` is a json object having the input of a task.
- The second function `on_result_receive = [](int64_t task_id, const json& input, const json& output, caravan::Queue& q)` is executed when a task gets complete. Note that this function is executed at the master process, namely, the output of a task is transfered from the worker process to the master process and then this function is executed.
    - You can get both `input` and `output` for a task in this function. You can also add additional tasks if you want.
- The third function `std::function<json(const json& input)> do_task = [](const json& input)` defines what are done at the worker process for a given input json object. This function is executed at a worker process. Return an output as a json object, which will then transferred to the master process.

Please note that `do_task` is executed at **a worker process** while the other two functions are executed at **the master process**. So you can not refer to a variable which is defined at the master process in `do_task` function and vice versa. Use `input` and `output` to send/receive values between different processes.

## Features

- Highly scalable: This library is designed to work on a massive parallel computers having tens of thousands of CPUs.
- Dynamical balancing: The task execution is scheduled dynamically, making it efficiently work with highly heterogeneous tasks.
- Generic input/output: We can make tasks with arbitrary types of inputs and outputs as long as they are serializable into JSON.

On the other hand, we have the following limitations:

- The library is not designed for tiny-scale tasks which typically finish in milliseconds or less. Typical duration of tasks should be order of seconds or longer.
- You need more than three processes because one process is used for the master process, at least one process is used for communication, and other processess are used as workers.