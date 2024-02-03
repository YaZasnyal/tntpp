#pragma clang diagnostic push
#pragma ide diagnostic ignored "*"
//
// Created by root on 2/1/24.
//

#include <gtest/gtest.h>

#include <tarantool_connector/detail/tntpp_mutable_buffer.h>

TEST(tntpp_mutable_buffer, single_small_message)
{
  tntpp::detail::MutableBuffer mbuf(128);
  auto ready_buf = mbuf.get_ready_buffer();
  auto recv_buf = mbuf.get_receive_buffer();
  GTEST_ASSERT_EQ(ready_buf.size(), 0);
  GTEST_ASSERT_EQ(recv_buf.size(), 128);

  std::string data = "hello";
  std::memcpy(recv_buf.data(), data.data(), data.size());
  mbuf.advance_writer(data.size());
  ready_buf = mbuf.get_ready_buffer();
  GTEST_ASSERT_EQ(ready_buf.size(), data.size());
  auto frozen_buf = mbuf.advance_reader(data.size());
  GTEST_ASSERT_NE(frozen_buf.data(), nullptr);
  GTEST_ASSERT_EQ(frozen_buf.size(), data.size());

  ready_buf = mbuf.get_ready_buffer();
  recv_buf = mbuf.get_receive_buffer();
  GTEST_ASSERT_EQ(ready_buf.size(), 0);
  GTEST_ASSERT_EQ(recv_buf.size(), 128 - data.size());
}

TEST(tntpp_mutable_buffer, prepare)
{
  tntpp::detail::MutableBuffer mbuf(8);
  auto recv_buf = mbuf.get_receive_buffer();
  GTEST_ASSERT_EQ(recv_buf.size(), 8);

  mbuf.prepare(128);
  recv_buf = mbuf.get_receive_buffer();
  GTEST_ASSERT_GE(recv_buf.size(), 128);
}

TEST(tntpp_mutable_buffer, auto_prepare)
{
  tntpp::detail::MutableBuffer mbuf(8);
  auto recv_buf = mbuf.get_receive_buffer();
  GTEST_ASSERT_EQ(recv_buf.size(), 8);

  mbuf.advance_writer(8);
  recv_buf = mbuf.get_receive_buffer();
  GTEST_ASSERT_GT(recv_buf.size(), 0);
}
