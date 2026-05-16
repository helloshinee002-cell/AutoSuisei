#include <gtest/gtest.h>

#include <chrono>

#include "core/Macro.h"
#include "storage/SqliteMacroRepository.h"

using autopilot::core::Action;
using autopilot::core::ActionKind;
using autopilot::core::KeyEvent;
using autopilot::core::Macro;
using autopilot::storage::SqliteMacroRepository;

namespace {

Macro buildSimpleMacro(const std::string& name) {
    Macro m{};
    m.name = name;
    m.description = "auto-generated test macro";
    m.actions.push_back(Action{
        .kind = ActionKind::KeyDown,
        .timestamp = std::chrono::steady_clock::time_point{std::chrono::nanoseconds{1234}},
        .payload = KeyEvent{.virtualKey = 0x42, .extended = false},
    });
    return m;
}

}  // namespace

TEST(SqliteMacroRepository, InsertsAndFetchesById) {
    SqliteMacroRepository repo(":memory:");
    const auto id = repo.save(buildSimpleMacro("first"));
    EXPECT_GT(id, 0);

    const auto fetched = repo.findById(id);
    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(fetched->id, id);
    EXPECT_EQ(fetched->name, "first");
    ASSERT_EQ(fetched->actions.size(), 1u);
    EXPECT_EQ(std::get<KeyEvent>(fetched->actions[0].payload).virtualKey, 0x42);
    EXPECT_GT(fetched->createdAtUnixMs, 0);
    EXPECT_GT(fetched->updatedAtUnixMs, 0);
}

TEST(SqliteMacroRepository, FindAllReturnsAllSavedMacros) {
    SqliteMacroRepository repo(":memory:");
    repo.save(buildSimpleMacro("a"));
    repo.save(buildSimpleMacro("b"));
    repo.save(buildSimpleMacro("c"));

    const auto all = repo.findAll();
    EXPECT_EQ(all.size(), 3u);
}

TEST(SqliteMacroRepository, UpdateOnExistingIdPreservesCreatedAt) {
    SqliteMacroRepository repo(":memory:");
    auto m = buildSimpleMacro("original");
    const auto id = repo.save(m);

    auto fetched = repo.findById(id).value();
    const auto originalCreatedAt = fetched.createdAtUnixMs;

    fetched.name = "updated";
    repo.save(fetched);

    const auto reloaded = repo.findById(id).value();
    EXPECT_EQ(reloaded.name, "updated");
    EXPECT_EQ(reloaded.createdAtUnixMs, originalCreatedAt);
    EXPECT_GE(reloaded.updatedAtUnixMs, originalCreatedAt);
}

TEST(SqliteMacroRepository, RemoveDeletesRow) {
    SqliteMacroRepository repo(":memory:");
    const auto id = repo.save(buildSimpleMacro("to-delete"));

    repo.remove(id);

    EXPECT_FALSE(repo.findById(id).has_value());
}

TEST(SqliteMacroRepository, FindByIdReturnsNulloptForMissing) {
    SqliteMacroRepository repo(":memory:");
    EXPECT_FALSE(repo.findById(9999).has_value());
}
