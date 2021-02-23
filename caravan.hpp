#ifndef CARAVAN_HPP
#define CARAVAN_HPP

#include <iostream>
#include <vector>
#include <queue>
#include <set>
#include <functional>
#include <unistd.h>
#include <mpi.h>
#include <nlohmann/json.hpp>
#include <icecream.hpp>

namespace caravan {
  using json = nlohmann::json;
  using task_t = std::pair<int64_t,json>; // id,input
  using result_t = std::tuple<int64_t,json,json>;  // id,input,output
  namespace caravan_impl {
    class Producer;
  };

  class Queue {
    public:
    Queue() : num_tasks(0) {};
    int64_t Push(const json& input) {
      q.push( std::make_pair(num_tasks,input) );
      return num_tasks++;
    };
    task_t Pop() {
      task_t t = q.front();
      q.pop();
      return t;
    }
    size_t Size() const { return q.size(); }
    private:
    std::queue<task_t> q;
    int64_t num_tasks;
  };
  void Start( std::function<void(Queue&)> on_init,
              std::function<void(int64_t,const json&,const json&,Queue&)> on_result_receive,
              std::function<json(const json&)> do_task,
              MPI_Comm comm = MPI_COMM_WORLD,
              int num_proc_per_buf = 384) {
    int rank, procs;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &procs);

    Queue q;
    on_init(q);
    task_t task = q.Pop();
    json input = task.second;
    json output = do_task(input);
    on_result_receive(task.first, input, output, q);
  }

  namespace caravan_impl {

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

    class Producer {
      public:
      Producer() {};
      Queue tasks;

      void Run(const std::vector<int> &buffers, std::function<void(int64_t,const json&,const json&,Queue&)> callback) {
        IC("Producer started : ", buffers.size());
        std::set<int64_t> running_task_ids;
        std::map<int, size_t> requesting_buffers;

        MPI_Request send_req = MPI_REQUEST_NULL;
        std::vector<uint8_t> send_buf;

        while (true) {
          IC("Producer", tasks.Size(), running_task_ids.size());
          bool has_something_to_send = (tasks.Size() > 0 && requesting_buffers.size() > 0);
          bool has_something_to_receive = (running_task_ids.size() > 0 || requesting_buffers.size() < buffers.size());
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
              result_t result = ReceiveResult(st);
              int64_t task_id = std::get<0>(result);
              IC("Producer received result for %d", task_id);
              running_task_ids.erase(task_id);
              callback(task_id, std::get<1>(result), std::get<2>(result), tasks);
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
      }

      int my_MPI_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status) {
        int received = 0, ret = 0;
        while(true) {
          ret = MPI_Iprobe(source, tag, comm, &received, status);
          if (received) break;
          usleep(1000);
        }
        return ret;
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
        IC(task_ids.size(), worker);
        return task_ids;
      }

      void TerminateWorker(int worker) {
        IC("terminating", worker);
        std::vector<char> buf;
        MPI_Send(&buf[0], 0, MPI_BYTE, worker, MsgTag::PROD_BUF_TERMINATE_REQUEST, MPI_COMM_WORLD);
      }

      std::pair<int, int> ReceiveRequest(const MPI_Status &st) {
        assert(st.MPI_TAG == MsgTag::BUF_PROD_TASK_REQUEST);
        int n_req = 0;
        MPI_Recv(&n_req, 1, MPI_INT, st.MPI_SOURCE, st.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        assert(n_req > 0);
        IC("Producer received request %d from %d", n_req, st.MPI_SOURCE);
        return std::make_pair(st.MPI_SOURCE, n_req);
      }

      result_t ReceiveResult(const MPI_Status &st) {
        assert(st.MPI_TAG == MsgTag::BUF_PROD_SEND_RESULT);
        int msg_size;
        MPI_Get_count(&st, MPI_CHAR, &msg_size);
        std::vector<unsigned char> buf(msg_size);
        MPI_Recv(&buf[0], msg_size, MPI_BYTE, st.MPI_SOURCE, st.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        const json j = json::from_msgpack(buf);
        IC("result was sent by %d", st.MPI_SOURCE, j.dump());
        return std::make_tuple(j[0].get<int64_t>(), j[1], j[2]);
      }
    };
  }
}

#endif // CARAVAN_HPP