/*
 * Copyright 2004-present Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <folly/experimental/logging/FileHandlerFactory.h>

#include <folly/Exception.h>
#include <folly/experimental/TestUtil.h>
#include <folly/experimental/logging/AsyncFileWriter.h>
#include <folly/experimental/logging/GlogStyleFormatter.h>
#include <folly/experimental/logging/ImmediateFileWriter.h>
#include <folly/experimental/logging/StandardLogHandler.h>
#include <folly/portability/GTest.h>

using namespace folly;
using folly::test::TemporaryFile;
using std::make_pair;

void checkAsyncWriter(
    const LogWriter* writer,
    const char* expectedPath,
    size_t expectedMaxBufferSize) {
  auto asyncWriter = dynamic_cast<const AsyncFileWriter*>(writer);
  ASSERT_TRUE(asyncWriter)
      << "FileHandlerFactory should have created an AsyncFileWriter";
  EXPECT_EQ(expectedMaxBufferSize, asyncWriter->getMaxBufferSize());

  // Make sure this refers to the expected output file
  struct stat expectedStatInfo;
  checkUnixError(stat(expectedPath, &expectedStatInfo), "stat failed");
  struct stat actualStatInfo;
  checkUnixError(
      fstat(asyncWriter->getFile().fd(), &actualStatInfo), "fstat failed");
  EXPECT_EQ(expectedStatInfo.st_dev, actualStatInfo.st_dev);
  EXPECT_EQ(expectedStatInfo.st_ino, actualStatInfo.st_ino);
}

void checkAsyncWriter(
    const LogWriter* writer,
    int expectedFD,
    size_t expectedMaxBufferSize) {
  auto asyncWriter = dynamic_cast<const AsyncFileWriter*>(writer);
  ASSERT_TRUE(asyncWriter)
      << "FileHandlerFactory should have created an AsyncFileWriter";
  EXPECT_EQ(expectedMaxBufferSize, asyncWriter->getMaxBufferSize());
  EXPECT_EQ(expectedFD, asyncWriter->getFile().fd());
}

TEST(FileHandlerFactory, pathOnly) {
  FileHandlerFactory factory;

  TemporaryFile tmpFile{"logging_test"};
  auto options = FileHandlerFactory::Options{
      make_pair("path", tmpFile.path().string()),
  };
  auto handler = factory.createHandler(options);

  auto stdHandler = std::dynamic_pointer_cast<StandardLogHandler>(handler);
  ASSERT_TRUE(stdHandler);

  auto formatter =
      std::dynamic_pointer_cast<GlogStyleFormatter>(stdHandler->getFormatter());
  EXPECT_TRUE(formatter)
      << "FileHandlerFactory should have created a GlogStyleFormatter";

  checkAsyncWriter(
      stdHandler->getWriter().get(),
      tmpFile.path().string().c_str(),
      AsyncFileWriter::kDefaultMaxBufferSize);
}

TEST(FileHandlerFactory, stderrStream) {
  FileHandlerFactory factory;

  TemporaryFile tmpFile{"logging_test"};
  auto options = FileHandlerFactory::Options{
      make_pair("stream", "stderr"),
  };
  auto handler = factory.createHandler(options);

  auto stdHandler = std::dynamic_pointer_cast<StandardLogHandler>(handler);
  ASSERT_TRUE(stdHandler);

  auto formatter =
      std::dynamic_pointer_cast<GlogStyleFormatter>(stdHandler->getFormatter());
  EXPECT_TRUE(formatter)
      << "FileHandlerFactory should have created a GlogStyleFormatter";

  checkAsyncWriter(
      stdHandler->getWriter().get(),
      STDERR_FILENO,
      AsyncFileWriter::kDefaultMaxBufferSize);
}

TEST(FileHandlerFactory, stdoutWithMaxBuffer) {
  FileHandlerFactory factory;

  TemporaryFile tmpFile{"logging_test"};
  auto options = FileHandlerFactory::Options{
      make_pair("stream", "stdout"),
      make_pair("max_buffer_size", "4096"),
  };
  auto handler = factory.createHandler(options);

  auto stdHandler = std::dynamic_pointer_cast<StandardLogHandler>(handler);
  ASSERT_TRUE(stdHandler);

  auto formatter =
      std::dynamic_pointer_cast<GlogStyleFormatter>(stdHandler->getFormatter());
  EXPECT_TRUE(formatter)
      << "FileHandlerFactory should have created a GlogStyleFormatter";

  checkAsyncWriter(stdHandler->getWriter().get(), STDOUT_FILENO, 4096);
}

TEST(FileHandlerFactory, pathWithMaxBufferSize) {
  FileHandlerFactory factory;

  TemporaryFile tmpFile{"logging_test"};
  auto options = FileHandlerFactory::Options{
      make_pair("path", tmpFile.path().string()),
      make_pair("max_buffer_size", "4096000"),
  };
  auto handler = factory.createHandler(options);

  auto stdHandler = std::dynamic_pointer_cast<StandardLogHandler>(handler);
  ASSERT_TRUE(stdHandler);

  auto formatter =
      std::dynamic_pointer_cast<GlogStyleFormatter>(stdHandler->getFormatter());
  EXPECT_TRUE(formatter)
      << "FileHandlerFactory should have created a GlogStyleFormatter";

  checkAsyncWriter(
      stdHandler->getWriter().get(), tmpFile.path().string().c_str(), 4096000);
}

TEST(FileHandlerFactory, nonAsyncStderr) {
  FileHandlerFactory factory;

  TemporaryFile tmpFile{"logging_test"};
  auto options = FileHandlerFactory::Options{
      make_pair("stream", "stderr"),
      make_pair("async", "no"),
  };
  auto handler = factory.createHandler(options);

  auto stdHandler = std::dynamic_pointer_cast<StandardLogHandler>(handler);
  ASSERT_TRUE(stdHandler);

  auto formatter =
      std::dynamic_pointer_cast<GlogStyleFormatter>(stdHandler->getFormatter());
  EXPECT_TRUE(formatter)
      << "FileHandlerFactory should have created a GlogStyleFormatter";

  auto writer =
      std::dynamic_pointer_cast<ImmediateFileWriter>(stdHandler->getWriter());
  ASSERT_TRUE(writer);
  EXPECT_EQ(STDERR_FILENO, writer->getFile().fd());
}

TEST(FileHandlerFactory, errors) {
  FileHandlerFactory factory;
  TemporaryFile tmpFile{"logging_test"};

  {
    auto options = FileHandlerFactory::Options{};
    EXPECT_THROW(factory.createHandler(options), std::invalid_argument)
        << "one of path or stream required";
  }

  {
    auto options = FileHandlerFactory::Options{
        make_pair("path", tmpFile.path().string()),
        make_pair("stream", "stderr"),
    };
    EXPECT_THROW(factory.createHandler(options), std::invalid_argument)
        << "path and stream cannot both be specified";
  }

  {
    auto options = FileHandlerFactory::Options{
        make_pair("stream", "nonstdout"),
    };
    EXPECT_THROW(factory.createHandler(options), std::invalid_argument)
        << "invalid stream";
  }

  {
    auto options = FileHandlerFactory::Options{
        make_pair("stream", "stderr"),
        make_pair("async", "foobar"),
    };
    EXPECT_THROW(factory.createHandler(options), std::invalid_argument)
        << "invalid async value";
  }

  {
    auto options = FileHandlerFactory::Options{
        make_pair("stream", "stderr"),
        make_pair("async", "false"),
        make_pair("max_buffer_size", "1234"),
    };
    EXPECT_THROW(factory.createHandler(options), std::invalid_argument)
        << "max_buffer_size only valid for async writers";
  }

  {
    auto options = FileHandlerFactory::Options{
        make_pair("stream", "stderr"),
        make_pair("max_buffer_size", "hello"),
    };
    EXPECT_THROW(factory.createHandler(options), std::invalid_argument)
        << "max_buffer_size must be an integer";
  }

  {
    auto options = FileHandlerFactory::Options{
        make_pair("stream", "stderr"),
        make_pair("max_buffer_size", "0"),
    };
    EXPECT_THROW(factory.createHandler(options), std::invalid_argument)
        << "max_buffer_size must be a positive integer";
  }

  {
    auto options = FileHandlerFactory::Options{
        make_pair("stream", "stderr"),
        make_pair("foo", "bar"),
    };
    EXPECT_THROW(factory.createHandler(options), std::invalid_argument)
        << "unknown parameter foo";
  }
}
