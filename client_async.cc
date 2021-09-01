#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <thread>

#include "hello.grpc.pb.h"


using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;

using simple_service::HelloReply;
using simple_service::HelloRequest;
using simple_service::SimpleService;

class ClientAsync {
 public:
  ClientAsync(std::shared_ptr<Channel> channel)
    : stub_(SimpleService::NewStub(channel)) {}

  std::string SayHello(const std::string& user) {
    // Fill request
    HelloRequest request;
    request.set_name(user);

    // Response from the server
    HelloReply reply;

    // Client context
    ClientContext context;

    // The producer-consumer queue we use to communicate asynchronously
    // with the gRPC runtime.
    CompletionQueue cq;

    // Status of the rpc completion
    Status status;

    // stub_->PrepareAsyncSayHello() creates an RPC object, returning
    // an instance to store in "call" but does not actually start the RPC
    // Because we are using the asynchronous API, we need to hold on to
    // the "call" instance in order to get updates on the ongoing RPC.
    std::cout << "preparing async say hello..." << std::endl;
    std::unique_ptr<ClientAsyncResponseReader<HelloReply>> rpc(
        stub_->PrepareAsyncSayHello(&context, request, &cq));

    std::cout << "start client async say hello..." << std::endl;
    // Start rpc SayHello
    rpc->StartCall();

    std::cout << "client finish call..." << std::endl;
    // Request that, upon completion of the RPC, "reply" be updated with the
    // server's response; "status" with the indication of whether the operation
    // was successful. Tag the request with the integer 1.
    rpc->Finish(&reply, &status, (void*) 1);
    void* got_tag;
    bool ok = false;
    std::cout << "client call Next event..." << std::endl;
    // Block until the next result is available in the completion queue "cq".
    // The return value of Next should always be checked. This return value
    // tells us whether there is any kind of event or the cq_ is shutting down.
    if (!cq.Next(&got_tag, &ok)) {
      return "";
    }
    assert(got_tag == (void*) 1);
    assert(ok == true);
    // Act upon the status of the actual RPC.
    if (status.ok()) {
      return reply.message();
    } else {
      return "RPC failed";
    }
  }

 private:
  std::unique_ptr<SimpleService::Stub> stub_;
};

int main(int argc, char** argv) {
  ClientAsync client(grpc::CreateChannel(
      "localhost:50051",
      grpc::InsecureChannelCredentials()));
  std::string user{"Artur"};
  std::string response = client.SayHello(user);
  std::cout << "Received from server: "
            << response << std::endl;
  return std::cout.good() ? EXIT_SUCCESS : EXIT_FAILURE;
}