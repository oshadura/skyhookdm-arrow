// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// This API is EXPERIMENTAL.
#define _FILE_OFFSET_BITS 64

#pragma once

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "arrow/api.h"
#include "arrow/compute/exec/expression.h"
#include "arrow/dataset/dataset.h"
#include "arrow/dataset/discovery.h"
#include "arrow/dataset/file_base.h"
#include "arrow/dataset/file_parquet.h"
#include "arrow/dataset/scanner.h"
#include "arrow/dataset/type_fwd.h"
#include "arrow/dataset/visibility.h"
#include "arrow/filesystem/api.h"
#include "arrow/filesystem/path_util.h"
#include "arrow/io/api.h"
#include "arrow/ipc/api.h"
#include "arrow/util/iterator.h"
#include "arrow/util/macros.h"
#include "parquet/arrow/writer.h"
#include "parquet/exception.h"

#define SCAN_ERR_CODE 25
#define SCAN_ERR_MSG "failed to scan file fragment"

#define SCAN_REQ_DESER_ERR_CODE 26
#define SCAN_REQ_DESER_ERR_MSG "failed to deserialize scan request"

#define SCAN_RES_SER_ERR_CODE 27
#define SCAN_RES_SER_ERR_MSG "failed to serialize result table"

namespace arrow {
namespace dataset {

enum SkyhookFileType { PARQUET, IPC };

/// \addtogroup dataset-file-formats
///
/// @{

/// \class SkyhookFileFormat
/// \brief A ParquetFileFormat implementation that offloads the fragment
/// scan operations to the Ceph OSDs
class ARROW_DS_EXPORT SkyhookFileFormat : public ParquetFileFormat {
 public:
  SkyhookFileFormat(const std::string& format, const std::string& ceph_config_path,
                    const std::string& data_pool, const std::string& user_name,
                    const std::string& cluster_name, const std::string& cls_name);

  std::string type_name() const override { return "skyhook"; }

  bool splittable() const { return true; }

  bool Equals(const FileFormat& other) const override {
    return type_name() == other.type_name();
  }

  Result<bool> IsSupported(const FileSource& source) const override { return true; }

  /// \brief Return the schema of the file fragment.
  /// \param[in] source The source of the file fragment.
  /// \return The schema of the file fragment.
  Result<std::shared_ptr<Schema>> Inspect(const FileSource& source) const override;

  /// \brief Scan a file fragment.
  /// \param[in] options Options to pass.
  /// \param[in] file The file fragment.
  /// \return The scanned file fragment.
  Result<ScanTaskIterator> ScanFile(
      const std::shared_ptr<ScanOptions>& options,
      const std::shared_ptr<FileFragment>& file) const override;

  Result<std::shared_ptr<FileWriter>> MakeWriter(
      std::shared_ptr<io::OutputStream> destination, std::shared_ptr<Schema> schema,
      std::shared_ptr<FileWriteOptions> options) const {
    return Status::NotImplemented("Use the Python API");
  }

  std::shared_ptr<FileWriteOptions> DefaultWriteOptions() override { return NULLPTR; }

 protected:
  std::string fragment_format_;
};

/// \brief Serialize scan request to a bufferlist.
/// \param[in] options The scan options to use to build a ScanRequest.
/// \param[in] file_format The underlying file format to use.
/// \param[in] file_size The size of the file fragment.
/// \param[out] bl Output bufferlist.
/// \return Status.
ARROW_DS_EXPORT Status SerializeScanRequest(std::shared_ptr<ScanOptions>& options,
                                            int& file_format, int64_t& file_size,
                                            ceph::bufferlist& bl);

/// \brief Deserialize scan request from bufferlist.
/// \param[out] filter The filter expression to apply.
/// \param[out] partition The partition expression to use.
/// \param[out] projected_schema The schema to project the filtered record batches.
/// \param[out] dataset_schema The dataset schema to use.
/// \param[out] file_size The size of the file fragment.
/// \param[out] file_format The underlying file format to use.
/// \param[in] bl Input Ceph bufferlist.
/// \return Status.
ARROW_DS_EXPORT Status DeserializeScanRequest(compute::Expression* filter,
                                              compute::Expression* partition,
                                              std::shared_ptr<Schema>* projected_schema,
                                              std::shared_ptr<Schema>* dataset_schema,
                                              int64_t& file_size, int& file_format,
                                              ceph::bufferlist& bl);

/// \brief Serialize the result Table to a bufferlist.
/// \param[in] table The table to serialize.
/// \param[in] aggressive If true, use ZSTD compression instead of LZ4.
/// \param[out] bl Output bufferlist.
/// \return Status.
ARROW_DS_EXPORT Status SerializeTable(std::shared_ptr<Table>& table, ceph::bufferlist& bl,
                                      bool aggressive = false);

/// \brief Deserialize the result table from bufferlist.
/// \param[out] batches Output record batches.
/// \param[in] bl Input bufferlist.
/// \param[in] use_threads If true, use threads to deserialize a table from a bufferlist.
/// \return Status.
ARROW_DS_EXPORT Status DeserializeTable(RecordBatchVector& batches, ceph::bufferlist& bl,
                                        bool use_threads);

/// @}

}  // namespace dataset
}  // namespace arrow
