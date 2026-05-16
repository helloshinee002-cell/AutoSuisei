#include <gtest/gtest.h>

#include "storage/SqliteOcrResultRepository.h"

using autopilot::storage::SqliteOcrResultRepository;
using autopilot::storage::StoredOcrResult;

TEST(SqliteOcrResultRepository, InsertsAndFindsAll) {
    SqliteOcrResultRepository repo(":memory:");
    StoredOcrResult r{.id = 0,
                      .filename = "weird_name_xyz.png",
                      .extractedText = "20",
                      .confidence = 96.0f,
                      .timestampMs = 1700000000000};
    const auto id = repo.insert(r);
    EXPECT_GT(id, 0);

    const auto all = repo.findAll();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].filename, "weird_name_xyz.png");
    EXPECT_EQ(all[0].extractedText, "20");
    EXPECT_FLOAT_EQ(all[0].confidence, 96.0f);
}

TEST(SqliteOcrResultRepository, FindsByFilename) {
    SqliteOcrResultRepository repo(":memory:");
    repo.insert({0, "a.png", "1", 90.0f, 1});
    repo.insert({0, "b.png", "2", 80.0f, 2});
    repo.insert({0, "a.png", "3", 70.0f, 3});

    const auto a = repo.findByFilename("a.png");
    EXPECT_EQ(a.size(), 2u);
    const auto b = repo.findByFilename("b.png");
    EXPECT_EQ(b.size(), 1u);
    const auto none = repo.findByFilename("missing.png");
    EXPECT_TRUE(none.empty());
}
