#include <gtest/gtest.h>

#include "scripting/LuaSandbox.h"

using autopilot::scripting::LuaSandbox;

TEST(LuaSandbox, RunsSimpleArithmetic) {
    LuaSandbox s;
    EXPECT_EQ(s.run("return 1 + 2"), "3");
}

TEST(LuaSandbox, RunsStringOps) {
    LuaSandbox s;
    EXPECT_EQ(s.run(R"(return "hello " .. "world")"), "hello world");
}

TEST(LuaSandbox, ApLogDoesNotThrow) {
    LuaSandbox s;
    EXPECT_NO_THROW(s.run(R"(ap.log("from lua test"); return 1)"));
}

TEST(LuaSandbox, ApVersionAvailable) {
    LuaSandbox s;
    EXPECT_EQ(s.run("return ap.version()"), "AutoPilot Lua 1");
}

TEST(LuaSandbox, BlocksOsExecute) {
    LuaSandbox s;
    EXPECT_FALSE(s.hasGlobal("os"));
    EXPECT_THROW(s.run("os.execute('echo pwned')"), std::runtime_error);
}

TEST(LuaSandbox, BlocksIoPopen) {
    LuaSandbox s;
    EXPECT_FALSE(s.hasGlobal("io"));
    EXPECT_THROW(s.run("io.popen('whoami')"), std::runtime_error);
}

TEST(LuaSandbox, BlocksLoadfile) {
    LuaSandbox s;
    EXPECT_FALSE(s.hasGlobal("loadfile"));
    EXPECT_FALSE(s.hasGlobal("dofile"));
    EXPECT_FALSE(s.hasGlobal("require"));
}

TEST(LuaSandbox, BlocksDebugGetregistry) {
    LuaSandbox s;
    EXPECT_FALSE(s.hasGlobal("debug"));
}

TEST(LuaSandbox, ThrowsOnSyntaxError) {
    LuaSandbox s;
    EXPECT_THROW(s.run("if then end"), std::runtime_error);
}
