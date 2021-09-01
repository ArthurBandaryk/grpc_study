#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>

#include "hello.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;

using simple_service::HelloReply;
using simple_service::HelloRequest;
using simple_service::SimpleService;

class ServerAsync {
 public:
  ~ServerAsync() {
    server->Shutdown();
    cq->Shutdown();
  }

  void run() {
    ServerBuilder builder;
    builder.AddListeningPort(
        "0.0.0.0:50051",
        grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    cq = builder.AddCompletionQueue();
    server = builder.BuildAndStart();
    std::cout << "Server is listening on connections..." << std::endl;

    // Server's main loop
    loop();
  }

 private:
  class CallData {
   public:
    CallData(SimpleService::AsyncService* service_, ServerCompletionQueue* cq_)
      : service(service_),
        cq(cq_),
        responder(&context) {
      Proceed();
    }
    void Proceed() {
      if (status == CallStatus::CREATE) {
        status = CallStatus::PROCESS;
        std::cout << "server RequestSayHello..." << std::endl;
        service->RequestSayHello(&context, &request, &responder, cq, cq, this);
      } else if (status == CallStatus::PROCESS) {
        // Spawn a new CallData instance to serve new clients while we process
        // the one for this CallData. The instance will deallocate itself as
        // part of its FINISH state.
        new CallData(service, cq);

        // The actual processing.
        std::string prefix("Hello ");
        reply.set_message(prefix + request.name());

        // And we are done! Let the gRPC runtime know we've finished, using the
        // memory address of this instance as the uniquely identifying tag for
        // the event.
        status = CallStatus::FINISH;
        std::cout << "server Finish()..." << std::endl;
        responder.Finish(reply, Status::OK, this);
      } else {
        assert(status == CallStatus::FINISH);
        delete this;
      }
    }

   private:
    SimpleService::AsyncService* service;
    ServerCompletionQueue* cq;
    ServerContext context;
    HelloRequest request;
    HelloReply reply;
    ServerAsyncResponseWriter<HelloReply> responder;
    enum CallStatus { CREATE,
                      PROCESS,
                      FINISH };
    CallStatus status = CallStatus::CREATE;
  };
  // Server's main loop
  void loop() {
    // Spawn a new CallData instance to serve new clients.
    new CallData(&service, cq.get());
    void* tag; // uniquely identifies a request.
    bool ok = false;
    while (true) {
      // Block waiting to read the next event from the completion queue. The
      // event is uniquely identified by its tag, which in this case is the
      // memory address of a CallData instance.
      std::cout << "next event..." << std::endl;
      cq->Next(&tag, &ok);
      std::cout << "after next call" << std::endl;
      assert(ok == true);
      std::cout << "proceding..." << std::endl;
      static_cast<CallData*>(tag)->Proceed();
    }
  }

  std::unique_ptr<Server> server;
  std::unique_ptr<ServerCompletionQueue> cq;
  SimpleService::AsyncService service;
};

int main(int argc, char** argv) {
  ServerAsync server;
  server.run();
  return std::cout.good() ? EXIT_SUCCESS : EXIT_FAILURE;
}