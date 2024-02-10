//
// Created by root on 2/10/24.
//

#ifndef TARANTOOL_CONNECTOR_TNTPP_CRYPTO_H
#define TARANTOOL_CONNECTOR_TNTPP_CRYPTO_H

#include <algorithm>
#include <array>
#include <sstream>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/endian.hpp>
#include <boost/uuid/detail/sha1.hpp>

#include "sha1.h"

namespace tntpp::detail
{

static inline std::string ScramblePassword(const std::string& password, std::string salt)
{
  // Decode hash from base64 to binary
  using namespace boost::archive::iterators;
  using base64_text = transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;

  boost::algorithm::trim(salt);  // trim trailing spaces
  static constexpr int SCRAMBLE_SIZE = 20;

  // PiQuer, Jun 11 2012, answer on Agus, "Base64 encode using boost throw exception",
  // Stack Overflow, Jan 06 2021, https://stackoverflow.com/questions/10521581.
  auto paddChars = std::count(salt.begin(), salt.end(), '=');
  std::replace(salt.begin(), salt.end(), '=', 'A');  // replace '=' by base64 encoding of '\0'
  std::string decodedSalt(base64_text(salt.begin()), base64_text(salt.end()));  // decode
  decodedSalt.erase(decodedSalt.end() - paddChars,
                    decodedSalt.end());  // erase padding '\0' characters

  unsigned char hash1[SCRAMBLE_SIZE];
  unsigned char hash2[SCRAMBLE_SIZE];
  unsigned char out[SCRAMBLE_SIZE];
  SHA1_CTX ctx;

  SHA1Init(&ctx);
  SHA1Update(&ctx, reinterpret_cast<const unsigned char*>(password.data()), password.size());
  SHA1Final(hash1, &ctx);

  SHA1Init(&ctx);
  SHA1Update(&ctx, hash1, SCRAMBLE_SIZE);
  SHA1Final(hash2, &ctx);

  SHA1Init(&ctx);
  SHA1Update(&ctx, reinterpret_cast<const unsigned char*>(decodedSalt.data()), SCRAMBLE_SIZE);
  SHA1Update(&ctx, hash2, SCRAMBLE_SIZE);
  SHA1Final(out, &ctx);

  for (int i = 0; i < SCRAMBLE_SIZE; ++i) {
    out[i] = hash1[i] ^ out[i];
  }

  std::string scrambleStr;
  scrambleStr.resize(SCRAMBLE_SIZE);
  std::memcpy(const_cast<char*>(scrambleStr.data()), out, SCRAMBLE_SIZE);

  return scrambleStr;
}

}  // namespace tntpp::detail

#endif  // TARANTOOL_CONNECTOR_TNTPP_CRYPTO_H
