#include "zopfli.h"  // pulls in the generated version.h

#include <regex>
#include <string>

#include "gtest/gtest.h"
namespace {

TEST(Version, MacroSubstituted) {
  const std::string version = ZOPFLI_VERSION;
  EXPECT_FALSE(version.empty());
  // The template placeholder must have been replaced at build time.
  EXPECT_EQ(version.find('@'), std::string::npos);
}

TEST(Version, SemverShape) {
  const std::string version = ZOPFLI_VERSION;
  EXPECT_TRUE(
      std::regex_match(version, std::regex("^[0-9]+\\.[0-9]+\\.[0-9]+$")));
}

}  // namespace
