//
// Created by Yohsuke Murase on 2020/02/26.
//

#ifndef CARAVAN_SCHEDULER_CONSUMER_HPP
#define CARAVAN_SCHEDULER_CONSUMER_HPP

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include "mpi.h"
#include "Logger.hpp"
#include "TaskResult.hpp"
#include "Producer.hpp"

namespace caravan_impl {

  class Consumer {
    public:
    Consumer(int _parent, Logger &_logger)
      : parent(_parent), logger(_logger) {};
    const int parent;
    Logger &logger;

    void Run(const std::function<json(const json&)>& do_task) {
      std::chrono::system_clock::time_point ref_time = logger.BaseTime();
      int my_rank = logger.MPIRank();
      while (true) {
        SendRequest();

        task_t t = ReceiveTask();
        int64_t task_id = t.first;
        // If the length is zero, all tasks are completed.
        if (task_id < 0) {
          logger.d("Finish OK");
          break;
        }

        logger.d("running %d", task_id);
        const json& input = t.second;

        auto start_at = std::chrono::system_clock::now();
        long s_at = std::chrono::duration_cast<std::chrono::milliseconds>(start_at - ref_time).count();

        json output = do_task(input);

        auto finish_at = std::chrono::system_clock::now();
        long f_at = std::chrono::duration_cast<std::chrono::milliseconds>(finish_at - ref_time).count();

        TaskResult tr(task_id, input, my_rank, s_at, f_at);
        tr.output = output;

        SendResult(tr);
      }
    }

    void SendRequest() {
      int request = 1;
      logger.d("sending request");
      MPI_Send(&request, 1, MPI_INT, parent, MsgTag::CONS_BUF_TASK_REQUEST, MPI_COMM_WORLD);
    }

    task_t ReceiveTask() {
      int task_len = 0;
      MPI_Status st;
      // receive the length of a command
      my_MPI_Probe(parent, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
      if (st.MPI_TAG == MsgTag::BUF_CONS_TERMINATE_REQUEST) {
        char buf;
        MPI_Recv(&buf, 0, MPI_CHAR, parent, st.MPI_TAG, MPI_COMM_WORLD, &st);
        return std::make_pair(-1, nullptr);
      }
      assert(st.MPI_TAG == MsgTag::BUF_CONS_SEND_TASK);
      MPI_Get_count(&st, MPI_CHAR, &task_len);
      std::vector<unsigned char> buf(task_len);
      MPI_Recv(&buf[0], buf.size(), MPI_CHAR, parent, st.MPI_TAG, MPI_COMM_WORLD, &st);
      const json j = json::from_msgpack(buf);
      int64_t task_id = j.at(0).get<int64_t>();
      return std::make_pair(task_id, j.at(1));
    }

    void SendResult(const TaskResult &res) {
      auto buf = json::to_msgpack(res);
      int n = buf.size();
      MPI_Send(&buf[0], n, MPI_BYTE, parent, MsgTag::CONS_BUF_SEND_RESULT, MPI_COMM_WORLD);
      logger.d("sent task result %d", res.task_id);
    }
  };
}

#endif //CARAVAN_SCHEDULER_CONSUMER_HPP
