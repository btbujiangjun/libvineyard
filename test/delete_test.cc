/** Copyright 2020 Alibaba Group Holding Limited.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#if defined(__APPLE__) && defined(__clang__)
#define private public
#endif

#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "arrow/status.h"
#include "arrow/util/io_util.h"
#include "arrow/util/logging.h"
#include "glog/logging.h"

#include "basic/ds/array.h"
#include "basic/ds/pair.h"
#include "basic/ds/tuple.h"
#include "client/client.h"
#include "client/ds/object_meta.h"

using namespace vineyard;  // NOLINT(build/namespaces)

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("usage ./delete_test <ipc_socket>");
    return 1;
  }
  std::string ipc_socket = std::string(argv[1]);

  Client client;
  VINEYARD_CHECK_OK(client.Connect(ipc_socket));
  LOG(INFO) << "Connected to IPCServer: " << ipc_socket;

  ObjectID id = InvalidObjectID(), blob_id = InvalidObjectID();
  bool exists;

  {
    // prepare data
    std::vector<double> double_array = {1.0, 7.0, 3.0, 4.0, 2.0};
    ArrayBuilder<double> builder(client, double_array);
    auto sealed_double_array =
        std::dynamic_pointer_cast<Array<double>>(builder.Seal(client));
    id = sealed_double_array->id();
    blob_id = VYObjectIDFromString(sealed_double_array->meta()
                                       .MetaData()["buffer_"]["id"]
                                       .get_ref<std::string const&>());
    CHECK(blob_id != InvalidObjectID());
  }

  {
    std::unordered_map<ObjectID, Payload> objects;
    auto s = client.GetBuffers({blob_id}, objects);
    CHECK(s.ok() && objects.size() == 1);
  }

  // delete transient object
  VINEYARD_CHECK_OK(client.Exists(id, exists));
  CHECK(exists);
  VINEYARD_CHECK_OK(client.Exists(blob_id, exists));
  CHECK(exists);
  LOG(INFO) << "delete id: " << id << ": " << VYObjectIDToString(id);
  VINEYARD_CHECK_OK(client.DelData(id, false, true));
  VINEYARD_CHECK_OK(client.Exists(id, exists));
  CHECK(!exists);
  VINEYARD_CHECK_OK(client.Exists(blob_id, exists));
  CHECK(!exists);

  {
    std::unordered_map<ObjectID, Payload> objects;
    auto s = client.GetBuffers({blob_id}, objects);
    CHECK(s.ok() && objects.size() == 0);
  }

  {
    // prepare data
    std::vector<double> double_array = {1.0, 7.0, 3.0, 4.0, 2.0};
    ArrayBuilder<double> builder(client, double_array);
    auto sealed_double_array =
        std::dynamic_pointer_cast<Array<double>>(builder.Seal(client));
    VINEYARD_CHECK_OK(client.Persist(sealed_double_array->id()));
    id = sealed_double_array->id();
    blob_id = VYObjectIDFromString(sealed_double_array->meta()
                                       .MetaData()["buffer_"]["id"]
                                       .get_ref<std::string const&>());
    CHECK(blob_id != InvalidObjectID());
  }

  {
    std::unordered_map<ObjectID, Payload> objects;
    auto s = client.GetBuffers({blob_id}, objects);
    CHECK(s.ok() && objects.size() == 1);
  }

  // deep deletion
  VINEYARD_CHECK_OK(client.Exists(id, exists));
  CHECK(exists);
  VINEYARD_CHECK_OK(client.Exists(blob_id, exists));
  CHECK(exists);
  VINEYARD_CHECK_OK(client.DelData(id, false, true));
  VINEYARD_CHECK_OK(client.Exists(id, exists));
  CHECK(!exists);
  VINEYARD_CHECK_OK(client.Exists(blob_id, exists));
  CHECK(!exists);

  // the blob should have been removed
  {
    std::unordered_map<ObjectID, Payload> objects;
    auto s = client.GetBuffers({blob_id}, objects);
    CHECK(s.ok() && objects.size() == 0);
  }

  {
    // prepare data
    std::vector<double> double_array = {1.0, 7.0, 3.0, 4.0, 2.0};
    ArrayBuilder<double> builder(client, double_array);
    auto sealed_double_array =
        std::dynamic_pointer_cast<Array<double>>(builder.Seal(client));
    VINEYARD_CHECK_OK(client.Persist(sealed_double_array->id()));
    id = sealed_double_array->id();
    blob_id = VYObjectIDFromString(sealed_double_array->meta()
                                       .MetaData()["buffer_"]["id"]
                                       .get_ref<std::string const&>());
    CHECK(blob_id != InvalidObjectID());
  }

  {
    std::unordered_map<ObjectID, Payload> objects;
    auto s = client.GetBuffers({blob_id}, objects);
    CHECK(s.ok() && objects.size() == 1);
  }

  // shallow deletion
  VINEYARD_CHECK_OK(client.Exists(id, exists));
  CHECK(exists);
  VINEYARD_CHECK_OK(client.Exists(blob_id, exists));
  CHECK(exists);
  VINEYARD_CHECK_OK(client.DelData(id, false, false));
  VINEYARD_CHECK_OK(client.Exists(id, exists));
  CHECK(!exists);
  VINEYARD_CHECK_OK(client.Exists(blob_id, exists));
  CHECK(exists);

  {
    std::unordered_map<ObjectID, Payload> objects;
    auto s = client.GetBuffers({blob_id}, objects);
    CHECK(s.ok() && objects.size() == 1);  // the deletion is shallow
  }

  {
    // prepare data
    std::vector<double> double_array = {1.0, 7.0, 3.0, 4.0, 2.0};
    ArrayBuilder<double> builder(client, double_array);
    auto sealed_double_array =
        std::dynamic_pointer_cast<Array<double>>(builder.Seal(client));
    VINEYARD_CHECK_OK(client.Persist(sealed_double_array->id()));
    id = sealed_double_array->id();
    blob_id = sealed_double_array->meta().GetMemberMeta("buffer_").GetId();
    CHECK(blob_id != InvalidObjectID());
  }

  // force deletion
  VINEYARD_CHECK_OK(client.Exists(id, exists));
  CHECK(exists);
  VINEYARD_CHECK_OK(client.Exists(blob_id, exists));
  CHECK(exists);
  VINEYARD_CHECK_OK(client.DelData(blob_id, true, false));
  VINEYARD_CHECK_OK(client.Exists(id, exists));
  CHECK(!exists);
  VINEYARD_CHECK_OK(client.Exists(blob_id, exists));
  CHECK(!exists);

  // the blob should have been removed
  {
    std::unordered_map<ObjectID, Payload> objects;
    auto s = client.GetBuffers({blob_id}, objects);
    CHECK(s.ok() && objects.size() == 0);
  }

  {
    // prepare data
    std::vector<double> double_array = {1.0, 7.0, 3.0, 4.0, 2.0};
    ArrayBuilder<double> builder(client, double_array);
    auto sealed_double_array =
        std::dynamic_pointer_cast<Array<double>>(builder.Seal(client));
    VINEYARD_CHECK_OK(client.Persist(sealed_double_array->id()));
    id = sealed_double_array->id();
    blob_id = sealed_double_array->meta().GetMemberMeta("buffer_").GetId();
    CHECK(blob_id != InvalidObjectID());
  }

  // shallow delete multiple objects
  VINEYARD_CHECK_OK(client.Exists(id, exists));
  CHECK(exists);
  VINEYARD_CHECK_OK(client.Exists(blob_id, exists));
  CHECK(exists);
  VINEYARD_CHECK_OK(client.DelData({id, blob_id}, false, false));
  VINEYARD_CHECK_OK(client.Exists(id, exists));
  CHECK(!exists);
  VINEYARD_CHECK_OK(client.Exists(blob_id, exists));
  CHECK(!exists);

  // the blob should have been removed
  {
    std::unordered_map<ObjectID, Payload> objects;
    auto s = client.GetBuffers({blob_id}, objects);
    CHECK(s.ok() && objects.size() == 0);
  }

  // delete on complex data: and empty blob is quite special, since it cannot
  // been truely deleted.
  std::shared_ptr<InstanceStatus> status_before;
  VINEYARD_CHECK_OK(client.InstanceStatus(status_before));

  // prepare data
  ObjectID nested_tuple_id;
  {
    // prepare data
    std::vector<double> double_array1 = {1.0, 7.0, 3.0, 4.0, 2.0};
    ArrayBuilder<double> builder1(client, double_array1);

    std::vector<double> double_array2 = {1.0, 7.0, 3.0, 4.0, 2.0};
    ArrayBuilder<double> builder2(client, double_array2);

    std::vector<double> double_array3 = {1.0, 7.0, 3.0, 4.0, 2.0};
    ArrayBuilder<double> builder3(client, double_array3);

    std::vector<double> double_array4 = {1.0, 7.0, 3.0, 4.0, 2.0};
    ArrayBuilder<double> builder4(client, double_array4);

    PairBuilder pair_builder1(client);
    pair_builder1.SetFirst(builder1.Seal(client));
    pair_builder1.SetSecond(builder2.Seal(client));

    PairBuilder pair_builder2(client);
    pair_builder2.SetFirst(builder3.Seal(client));
    pair_builder2.SetSecond(Blob::MakeEmpty(client));

    PairBuilder pair_builder3(client);
    pair_builder3.SetFirst(Blob::MakeEmpty(client));
    pair_builder3.SetSecond(builder4.Seal(client));

    TupleBuilder tuple_builder(client);
    tuple_builder.SetSize(3);
    tuple_builder.SetValue(0, pair_builder1.Seal(client));
    tuple_builder.SetValue(1, pair_builder2.Seal(client));
    tuple_builder.SetValue(2, pair_builder3.Seal(client));
    nested_tuple_id = tuple_builder.Seal(client)->id();
  }
  VINEYARD_CHECK_OK(client.DelData(nested_tuple_id, true, true));

  std::shared_ptr<InstanceStatus> status_after;
  VINEYARD_CHECK_OK(client.InstanceStatus(status_after));

  // validate memory usage
  CHECK_EQ(status_before->memory_limit, status_after->memory_limit);
  CHECK_EQ(status_before->memory_usage, status_after->memory_usage);

  LOG(INFO) << "Passed delete tests...";

  client.Disconnect();

  return 0;
}