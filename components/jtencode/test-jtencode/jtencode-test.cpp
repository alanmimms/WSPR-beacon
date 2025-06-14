#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "JTEncode.h"

TEST_CASE("WSPR Encoder", "[wspr]") {
  WSPREncoder encoder;
  encoder.encode("K1ABC", "FN42", 37);

  SECTION("Output length") {
    REQUIRE(encoder.TxBufferSize == 162);
  }

  SECTION("Symbol values") {
    for (int i = 0; i < encoder.TxBufferSize; ++i) {
      REQUIRE(encoder.symbols[i] >= 0);
      REQUIRE(encoder.symbols[i] <= 3);
    }
  }
}

TEST_CASE("FT8 Encoder", "[ft8]") {
  FT8Encoder encoder;
  encoder.encode("CQ K1ABC FN42");

  SECTION("Output length") {
    REQUIRE(encoder.TxBufferSize == 79);
  }
}

TEST_CASE("JT65 Encoder", "[jt65]") {
  JT65Encoder encoder;
  encoder.encode("CQ K1ABC FN42");

  SECTION("Output length") {
    REQUIRE(encoder.TxBufferSize == 126);
  }
}

TEST_CASE("JT9 Encoder", "[jt9]") {
  JT9Encoder encoder;
  encoder.encode("CQ K1ABC FN42");

  SECTION("Output length") {
    REQUIRE(encoder.TxBufferSize == 85);
  }
}

TEST_CASE("JT4 Encoder", "[jt4]") {
  JT4Encoder encoder;
  encoder.encode("CQ K1ABC FN42");

  SECTION("Output length") {
    REQUIRE(encoder.TxBufferSize == 206);
  }
}
