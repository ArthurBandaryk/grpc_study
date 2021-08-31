#include <grpcpp/grpcpp.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#ifdef BAZEL_BUILD
#include "hello.grpc.pb.h"
#else
#include "hello.grpc.pb.h"
#endif

using simple_service::BusNameRequest;
using simple_service::BusNameRequestBi;
using simple_service::HelloReply;
using simple_service::HelloRequest;
using simple_service::ReplyArea;
using simple_service::RequestArea;
using simple_service::StopNameReply;
using simple_service::StopNameReplyBi;

class MyService : public simple_service::SimpleService::Service {
 private:
  // Synchronous and unary way of sending data
  grpc::Status SayHello(
      grpc::ServerContext* context,
      const HelloRequest* request,
      HelloReply* reply) override {
    std::string prefix{"Hello dear "};
    reply->set_message(prefix + request->name() + "!");
    return grpc::Status::OK;
  }
  // Synchronous and client-side streaming RPC
  grpc::Status CalcArea(
      grpc::ServerContext* context,
      grpc::ServerReader<RequestArea>* reader,
      ReplyArea* final_area) override {
    RequestArea ra;
    std::vector<RequestArea> v_rect_points;
    size_t i = 0;
    while (reader->Read(&ra)) {
      std::cout << "reading point[" << i << "] from client: ("
                << ra.pos().x() << ", " << ra.pos().y() << ")" << std::endl;

      v_rect_points.push_back(ra);
      ++i;
    }
    auto res_size = v_rect_points.size();
    if (res_size == 4) {
      auto l1 = sqrt(
          powf(v_rect_points[0].pos().x() - v_rect_points[1].pos().x(), 2)
          + powf(v_rect_points[0].pos().y() - v_rect_points[1].pos().y(), 2));
      auto l2 = sqrt(
          powf(v_rect_points[0].pos().x() - v_rect_points[2].pos().x(), 2)
          + powf(v_rect_points[0].pos().y() - v_rect_points[2].pos().y(), 2));
      std::cout << "l1 = " << l1 << " l2 = " << l2 << std::endl;
      final_area->set_area(l1 * l2);
      return grpc::Status::OK;
    } else {
      final_area->set_area(-1.f);
      return grpc::Status::OK;
    }
  }
  // Synchronous and server streaming RPC
  grpc::Status StopsInfo(
      grpc::ServerContext* context,
      const BusNameRequest* bus_name_request,
      grpc::ServerWriter<StopNameReply>* writer) override {
    auto iter_result = std::find_if(
        std::begin(db),
        std::end(db),
        [&bus_name_request](const auto& el) {
          return bus_name_request->bus_name() == el.first;
        });
    if (iter_result == db.end()) {
      StopNameReply reply;
      reply.set_stop_name("no such bus!");
      writer->Write(reply);
    } else {
      for (const auto& stop : iter_result->second) {
        StopNameReply reply;
        reply.set_stop_name(stop);
        writer->Write(reply);
      }
    }
    return grpc::Status::OK;
  }

  // Synchronous bi-directional streaming RPC
  grpc::Status StopsInfoBi(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<StopNameReplyBi, BusNameRequestBi>* stream)
      override {
    BusNameRequestBi request;
    while (stream->Read(&request)) {
      for (const auto& el : request.arr_bus_names()) {
        auto iter_result = std::find_if(
            std::begin(db),
            std::end(db),
            [&el](const auto& db_el) {
              return el == db_el.first;
            });
        if (iter_result == db.end()) {
          StopNameReplyBi reply;
          reply.add_arr_stop_names("no such bus!");
          stream->Write(reply);
        } else {
          StopNameReplyBi reply;
          for (const auto& stop : iter_result->second) {
            reply.add_arr_stop_names(stop);
          }
          stream->Write(reply);
        }
      }
    }
    return grpc::Status::OK;
  }

  std::map<std::string, std::vector<std::string>> db;

 public:
  MyService() {
    std::string bus1{"1"};
    std::string bus2{"2"};
    std::string bus3{"3"};
    db[bus1] = std::vector<std::string>{"stop1", "stop2", "stop3"};
    db[bus2] = std::vector<std::string>{"stop13", "stop23", "stop345"};
    db[bus3] = std::vector<std::string>{"stop1", "stop2"};
  }
};

int main(int argc, char** argv) {
  const std::string address = "0.0.0.0", port = "50051";
  const std::string server_address = address + ":" + port;
  MyService service;
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> p_server{builder.BuildAndStart()};
  std::cout << "Server is listening on connections ..." << std::endl;
  p_server->Wait();
  return EXIT_SUCCESS;
}