#include <grpcpp/grpcpp.h>

#include <array>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <thread>

#ifdef BAZEL_BUILD
#include "hello.grpc.pb.h"
#else
#include "hello.grpc.pb.h"
#endif

using namespace std::chrono_literals;
using grpc::Status;
using simple_service::BusNameRequest;
using simple_service::BusNameRequestBi;
using simple_service::ReplyArea;
using simple_service::RequestArea;
using simple_service::StopNameReply;
using simple_service::StopNameReplyBi;
using simple_service::Vector;

class Client {
 public:
  Client(std::shared_ptr<grpc::Channel> channel)
    : stub_(simple_service::SimpleService::NewStub(channel)) {}

  std::string SayHello(const std::string& name_user) {
    simple_service::HelloRequest request;
    request.set_name(name_user);
    simple_service::HelloReply reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->SayHello(&context, request, &reply);
    if (status.ok()) {
      return reply.message();
    } else {
      std::cerr << status.error_code() << ": "
                << status.error_message() << std::endl;
      return "RPC failed!";
    }
  }

  float CalcArea() {
    ReplyArea final_area;
    grpc::ClientContext context;
    struct Point {
      float x;
      float y;
    };
    const std::array<Point, 4> arr_points = {
        Point{0.f, 0.f},
        Point{10.f, 0.f},
        Point{0.f, -10.f},
        Point{10.f, -10.f}};

    std::unique_ptr<grpc::ClientWriter<RequestArea>> writer(
        stub_->CalcArea(&context, &final_area));
    for (const auto& el : arr_points) {
      Vector* v = new Vector();
      v->set_x(el.x);
      v->set_y(el.y);
      RequestArea ra;
      ra.set_allocated_pos(v);
      if (!writer->Write(ra))
        break;
    }
    writer->WritesDone();
    Status status = writer->Finish();
    if (status.ok()) {
      return final_area.area();
    } else {
      std::cout << "CalcArea rpc failed." << std::endl;
    }
    return -1.f;
  }

  std::vector<std::string> StopsInfo(const std::string& bus) {
    BusNameRequest request;
    request.set_bus_name(bus);

    grpc::ClientContext context;
    StopNameReply reply;
    std::unique_ptr<grpc::ClientReader<StopNameReply>> reader(
        stub_->StopsInfo(&context, request));
    std::vector<std::string> result;
    while (reader->Read(&reply)) {
      result.emplace_back(reply.stop_name());
    }
    Status status = reader->Finish();
    if (status.ok()) {
      std::cout << "StopsInfo rpc succeeded." << std::endl;
      return result;
    } else {
      std::cerr << status.error_code() << ": "
                << status.error_message() << std::endl;
      return std::vector<std::string>{};
    }
  }

  void StopsInfoBi() {
    grpc::ClientContext context;
    BusNameRequestBi request;
    StopNameReplyBi reply;
    std::unique_ptr<
        grpc::ClientReaderWriter<BusNameRequestBi, StopNameReplyBi>>
        stream(stub_->StopsInfoBi(&context));
    std::vector<std::string> v_buses{"1", "2", "3", "4"};

    for (const auto& el : v_buses) {
      request.add_arr_bus_names(el);
    }
    stream->Write(request);
    stream->WritesDone();

    while (stream->Read(&reply)) {
      std::vector<std::string> v_stops;
      for (const auto& el : reply.arr_stop_names()) {
        v_stops.emplace_back(el);
      }
      std::cout << "stops: ";
      for (const auto& el : v_stops) {
        std::cout << el << " ";
      }
      std::cout << std::endl;
    }
    Status status = stream->Finish();
    if (!status.ok()) {
      std::cerr << "StopsInfoBi rpc failed." << std::endl;
    }
  }

 private:
  std::unique_ptr<simple_service::SimpleService::Stub>
      stub_;
};

void print_vector_bus_info(
    const std::vector<std::string>& stops,
    const std::string& bus) {
  std::cout << bus << ":";
  for (const auto& el : stops) {
    std::cout << el << " ";
  }
  std::cout << std ::endl;
}

int main(int argc, char** argv) {
  std::string user{"Artur"};
  const std::string server_address = "localhost:50051";
  Client client(grpc::CreateChannel(
      server_address,
      grpc::InsecureChannelCredentials()));

  std::string first_response = client.SayHello(user);
  std::cout << "first response from server: " << first_response << std::endl;

  std::this_thread::sleep_for(1s);

  std::string second_response = client.SayHello(user);
  std::cout << "second response from server: " << second_response << std::endl;

  std::cout << "=========area of the rectangle (client streaming)====\n";

  std::cout << "area1 = " << client.CalcArea() << std::endl;
  std::this_thread::sleep_for(1s);
  std::cout << "area2 = " << client.CalcArea() << std::endl;

  std::cout << "=========stops info (server streaming)===============\n";
  std::string bus1{"1"};
  std::string bus2{"2"};
  std::string bus3{"3"};
  std::string bus4{"4"};
  print_vector_bus_info(client.StopsInfo(bus1), bus1);
  print_vector_bus_info(client.StopsInfo(bus2), bus2);
  print_vector_bus_info(client.StopsInfo(bus3), bus3);
  print_vector_bus_info(client.StopsInfo(bus4), bus4);
  client.StopsInfoBi();

  return EXIT_SUCCESS;
}