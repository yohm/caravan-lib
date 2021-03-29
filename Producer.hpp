//
// Created by Yohsuke Murase on 2021/02/23.
//

#ifndef CARAVAN_LIB_PRODUCER_HPP
#define CARAVAN_LIB_PRODUCER_HPP
#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <chrono>
#include <mpi.h>
#include "Base.hpp"
#include "TaskResult.hpp"
#include "Logger.hpp"


namespace caravan_impl {

  using namespace caravan;

  enum MsgTag {
    PROD_BUF_SEND_TASKS = 1,
    PROD_BUF_TERMINATE_REQUEST,
    BUF_PROD_TASK_REQUEST,
    BUF_PROD_SEND_RESULT,
    BUF_CONS_SEND_TASK,
    BUF_CONS_TERMINATE_REQUEST,
    CONS_BUF_TASK_REQUEST,
    CONS_BUF_SEND_RESULT
  };

  int my_MPI_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status) {
    int received = 0, ret = 0;
    while (true) {
      ret = MPI_Iprobe(source, tag, comm, &received, status);
      if (received) break;
      usleep(1000);
    }
    return ret;
  }

  class Producer {
    public:
    Producer(Logger &_logger, const std::string& _dump_log = "") : logger(_logger), dump_log(_dump_log) {};
    Queue tasks;
    Logger &logger;
    std::string dump_log;
    std::map<int, long > elapse_times;  // {rank_id, elapse_time_sum }

    void Run(const std::vector<int> &buffers, const std::function<void(int64_t, const json&, const json&, Queue&)>& callback) {
      logger.d("Producer started : ", buffers.size());

      std::set<int64_t> running_task_ids;
      std::map<int, size_t> requesting_buffers;

      MPI_Request send_req = MPI_REQUEST_NULL;
      std::vector<uint8_t> send_buf;

      bool save_dump = !dump_log.empty();
      std::map<int64_t, TaskResult> results_map;

      while (true) {
        logger.d("Producer has %d tasks %d running_tasks", tasks.Size(), running_task_ids.size());
        bool has_something_to_send = (tasks.Size() > 0 && !requesting_buffers.empty());
        bool has_something_to_receive = (!running_task_ids.empty() || requesting_buffers.size() < buffers.size());
        if (!has_something_to_send && !has_something_to_receive) { break; }

        MPI_Status st;
        int received = 0;
        int sent = 0;
        if (has_something_to_receive && has_something_to_send) {
          while (true) {  // wait until message is ready
            MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &received, &st);
            if (received) break;
            MPI_Test(&send_req, &sent, MPI_STATUS_IGNORE); // MPI_Test on MPI_REQUEST_NULL returns true
            if (sent) break;
            usleep(1000);
          }
        } else if (has_something_to_receive) {
          my_MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
          received = 1;
        } else if (has_something_to_send) {
          MPI_Wait(&send_req, MPI_STATUS_IGNORE);
          sent = 1;
        }

        // receiving
        if (received) {
          if (st.MPI_TAG == MsgTag::BUF_PROD_TASK_REQUEST) {
            std::pair<int, int> worker_nreq = ReceiveRequest(st);
            requesting_buffers.insert(worker_nreq);
          } else if (st.MPI_TAG == MsgTag::BUF_PROD_SEND_RESULT) {
            TaskResult result = ReceiveResult(st);
            int64_t task_id = result.task_id;
            logger.d("Producer received result for %d", task_id);
            if (save_dump) { results_map.emplace(task_id, result); }
            running_task_ids.erase(task_id);
            if (elapse_times.find(result.rank) == elapse_times.end() ) { elapse_times[result.rank] = 0; }
            elapse_times[result.rank] += (result.finish_at - result.start_at);
            callback(task_id, result.input, result.output, tasks);
          } else {  // must not happen
            assert(false);
            MPI_Abort(MPI_COMM_WORLD, 1);
          }
        } else if (sent) {
          auto it = requesting_buffers.begin();
          int worker = it->first;
          size_t n_request = it->second;
          const auto task_ids = SendTasks(worker, n_request, &send_req, send_buf);
          for (long task_id: task_ids) {
            running_task_ids.insert(task_id);
          }
          requesting_buffers.erase(worker);
        }
      }

      MPI_Wait(&send_req, MPI_STATUS_IGNORE);

      for (auto req_w: requesting_buffers) {
        TerminateWorker(req_w.first);
      }

      if (save_dump) {
        std::ofstream binout(dump_log, std::ios::binary);
        const std::vector<uint8_t> dumped = json::to_msgpack(results_map);
        binout.write((const char *) dumped.data(), dumped.size());
        binout.flush();
        binout.close();
      }
    }

    std::vector<int64_t> SendTasks(int worker, size_t num_tasks, MPI_Request *p_req, std::vector<uint8_t> &send_buf) {
      json j;
      std::vector<int64_t> task_ids;
      for (size_t i = 0; i < num_tasks && tasks.Size() > 0; i++) {
        const task_t t = tasks.Pop();
        j.push_back(t);
        task_ids.push_back(t.first);
      }
      send_buf = std::move(json::to_msgpack(j));
      size_t n = send_buf.size();
      MPI_Isend(send_buf.data(), n, MPI_BYTE, worker, MsgTag::PROD_BUF_SEND_TASKS, MPI_COMM_WORLD, p_req);
      logger.d("%d tasks are assigned to %d", task_ids.size(), worker);
      return task_ids;
    }

    void TerminateWorker(int worker) {
      logger.d("terminating %d", worker);
      std::vector<char> buf;
      MPI_Send(&buf[0], 0, MPI_BYTE, worker, MsgTag::PROD_BUF_TERMINATE_REQUEST, MPI_COMM_WORLD);
    }

    std::pair<int, int> ReceiveRequest(const MPI_Status &st) {
      assert(st.MPI_TAG == MsgTag::BUF_PROD_TASK_REQUEST);
      int n_req = 0;
      MPI_Recv(&n_req, 1, MPI_INT, st.MPI_SOURCE, st.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      assert(n_req > 0);
      logger.d("Producer received request %d from %d", n_req, st.MPI_SOURCE);
      return std::make_pair(st.MPI_SOURCE, n_req);
    }

    TaskResult ReceiveResult(const MPI_Status &st) {
      assert(st.MPI_TAG == MsgTag::BUF_PROD_SEND_RESULT);
      int msg_size;
      MPI_Get_count(&st, MPI_CHAR, &msg_size);
      std::vector<unsigned char> buf(msg_size);
      MPI_Recv(&buf[0], msg_size, MPI_BYTE, st.MPI_SOURCE, st.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      logger.d("result was sent by %d", st.MPI_SOURCE);
      const json j = json::from_msgpack(buf);
      return j.get<TaskResult>();
    }
  };
}

#endif //CARAVAN_LIB_PRODUCER_HPP
