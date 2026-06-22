#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "ocr/ReviewModel.h"

using autopilot::ocr::ReviewModel;
using autopilot::ocr::ReviewRow;
namespace fs = std::filesystem;

namespace {

// helper: เขียนไฟล์ชั่วคราว + คืน path ของไฟล์
fs::path writeTempCsv(const std::string& content) {
    auto path = fs::temp_directory_path() / "autopilot_review_test.csv";
    std::ofstream out(path, std::ios::trunc);
    out << content;
    out.close();
    return path;
}

}  // namespace

// =================== CSV helpers (pure) ===================

TEST(ReviewModel, EscapeCsv_NoSpecialChars_Untouched) {
    EXPECT_EQ(ReviewModel::escapeCsv("hello"), "hello");
    EXPECT_EQ(ReviewModel::escapeCsv("315"), "315");
    EXPECT_EQ(ReviewModel::escapeCsv(""), "");
}

TEST(ReviewModel, EscapeCsv_WithComma_Quoted) {
    EXPECT_EQ(ReviewModel::escapeCsv("a,b"), R"("a,b")");
}

TEST(ReviewModel, EscapeCsv_WithQuote_DoubledAndQuoted) {
    EXPECT_EQ(ReviewModel::escapeCsv(R"(say "hi")"), R"("say ""hi""")");
}

TEST(ReviewModel, ParseCsvLine_SimpleFields) {
    auto fields = ReviewModel::parseCsvLine("a,b,c");
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields[0], "a");
    EXPECT_EQ(fields[1], "b");
    EXPECT_EQ(fields[2], "c");
}

TEST(ReviewModel, ParseCsvLine_QuotedFieldWithComma) {
    auto fields = ReviewModel::parseCsvLine(R"("a,b",c)");
    ASSERT_EQ(fields.size(), 2u);
    EXPECT_EQ(fields[0], "a,b");
    EXPECT_EQ(fields[1], "c");
}

TEST(ReviewModel, ParseCsvLine_QuotedFieldWithEscapedQuote) {
    auto fields = ReviewModel::parseCsvLine(R"("say ""hi""",x)");
    ASSERT_EQ(fields.size(), 2u);
    EXPECT_EQ(fields[0], R"(say "hi")");
    EXPECT_EQ(fields[1], "x");
}

TEST(ReviewModel, ParseCsvLine_TrailingEmptyField) {
    auto fields = ReviewModel::parseCsvLine("a,b,");
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields[2], "");
}

// =================== load CSV from bulk_extract.csv format ===================

TEST(ReviewModel, LoadCsv_FromBulkExtractFormat) {
    // ตรงกับ header ที่ bulk_extract.py เขียน — ขาด verified/notes เพราะยังไม่ review
    const std::string csv =
        "photo_index,filename,pc_no,serial_no,batch_id,photo_date,pc_range,"
        "mean_confidence,line_count,warnings\n"
        "10,img10.jpg,315,6YW8RV2,,2026-05-17,,0.837,8,\n"
        "11,img11.jpg,,,,2026-05-17,,0.770,8,No. not found; Serial not found\n";
    auto path = writeTempCsv(csv);

    ReviewModel model;
    ASSERT_TRUE(model.loadCsv(path.string()));
    EXPECT_EQ(model.size(), 2u);

    auto r0 = model.at(0);
    ASSERT_TRUE(r0.has_value());
    EXPECT_EQ(r0->filename, "img10.jpg");
    EXPECT_EQ(r0->pcNo, "315");
    EXPECT_EQ(r0->serialNo, "6YW8RV2");
    EXPECT_EQ(r0->originalPcNo, "315");
    EXPECT_EQ(r0->originalSerialNo, "6YW8RV2");
    EXPECT_FALSE(r0->verified);

    auto r1 = model.at(1);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->pcNo, "");
    EXPECT_EQ(r1->serialNo, "");
}

TEST(ReviewModel, LoadCsv_MissingFileReturnsFalse) {
    ReviewModel model;
    EXPECT_FALSE(model.loadCsv("/nonexistent/path.csv"));
    EXPECT_EQ(model.size(), 0u);
}

TEST(ReviewModel, LoadCsv_EmptyFileReturnsFalse) {
    auto path = writeTempCsv("");
    ReviewModel model;
    EXPECT_FALSE(model.loadCsv(path.string()));
}

TEST(ReviewModel, LoadCsv_HeaderOnlyOk) {
    auto path = writeTempCsv("filename,pc_no,serial_no\n");
    ReviewModel model;
    ASSERT_TRUE(model.loadCsv(path.string()));
    EXPECT_EQ(model.size(), 0u);
}

// =================== edit + roundtrip ===================

TEST(ReviewModel, SetRow_ChangesValuesButPreservesOriginal) {
    auto path = writeTempCsv(
        "filename,pc_no,serial_no\nimg10.jpg,315,6YW8RV2\n");
    ReviewModel model;
    ASSERT_TRUE(model.loadCsv(path.string()));

    auto row = *model.at(0);
    row.pcNo = "316";  // user แก้
    row.notes = "OCR misread, sticker says 316";
    row.verified = true;
    EXPECT_TRUE(model.setRow(0, row));

    auto back = *model.at(0);
    EXPECT_EQ(back.pcNo, "316");
    EXPECT_EQ(back.originalPcNo, "315");  // เดิมไม่เปลี่ยน
    EXPECT_EQ(back.notes, "OCR misread, sticker says 316");
    EXPECT_TRUE(back.verified);
}

TEST(ReviewModel, SetRow_OutOfRangeReturnsFalse) {
    ReviewModel model;
    EXPECT_FALSE(model.setRow(5, ReviewRow{}));
}

TEST(ReviewModel, SaveAndReload_PreservesAllFields) {
    auto inPath = writeTempCsv(
        "filename,pc_no,serial_no\nimg10.jpg,315,6YW8RV2\nimg11.jpg,,,\n");

    ReviewModel a;
    ASSERT_TRUE(a.loadCsv(inPath.string()));

    auto r = *a.at(0);
    r.pcNo = "316";
    r.notes = "fix, contains \"quote\" and ,comma";
    r.verified = true;
    a.setRow(0, r);

    auto outPath = fs::temp_directory_path() / "autopilot_review_out.csv";
    ASSERT_TRUE(a.saveCsv(outPath.string()));

    ReviewModel b;
    ASSERT_TRUE(b.loadCsv(outPath.string()));
    EXPECT_EQ(b.size(), 2u);

    auto r0 = *b.at(0);
    EXPECT_EQ(r0.pcNo, "316");
    EXPECT_EQ(r0.originalPcNo, "315");
    EXPECT_EQ(r0.notes, "fix, contains \"quote\" and ,comma");
    EXPECT_TRUE(r0.verified);
}

// =================== navigation helpers ===================

TEST(ReviewModel, NextUnverified_SkipsVerified) {
    auto path = writeTempCsv(
        "filename,pc_no,serial_no\na.jpg,1,\nb.jpg,2,\nc.jpg,3,\n");
    ReviewModel model;
    ASSERT_TRUE(model.loadCsv(path.string()));

    auto r = *model.at(0);
    r.verified = true;
    model.setRow(0, r);

    auto next = model.nextUnverified(0);
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(*next, 1u);
}

TEST(ReviewModel, NextUnverified_FromGivenStartIndex) {
    auto path = writeTempCsv(
        "filename,pc_no,serial_no\na.jpg,1,\nb.jpg,2,\nc.jpg,3,\n");
    ReviewModel model;
    ASSERT_TRUE(model.loadCsv(path.string()));

    auto next = model.nextUnverified(1);
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(*next, 1u);
}

TEST(ReviewModel, NextUnverified_AllVerifiedReturnsNullopt) {
    auto path = writeTempCsv(
        "filename,pc_no,serial_no\na.jpg,1,\nb.jpg,2,\n");
    ReviewModel model;
    ASSERT_TRUE(model.loadCsv(path.string()));

    for (std::size_t i = 0; i < model.size(); ++i) {
        auto r = *model.at(i);
        r.verified = true;
        model.setRow(i, r);
    }
    EXPECT_FALSE(model.nextUnverified().has_value());
}

TEST(ReviewModel, VerifiedCount_ReflectsState) {
    auto path = writeTempCsv(
        "filename,pc_no,serial_no\na.jpg,1,\nb.jpg,2,\nc.jpg,3,\n");
    ReviewModel model;
    ASSERT_TRUE(model.loadCsv(path.string()));
    EXPECT_EQ(model.verifiedCount(), 0u);

    auto r = *model.at(1);
    r.verified = true;
    model.setRow(1, r);
    EXPECT_EQ(model.verifiedCount(), 1u);
}

// =================== resume from already-reviewed CSV ===================

TEST(ReviewModel, LoadCsv_WithVerifiedAndNotesColumns) {
    // CSV ที่ user เคย save แล้วเปิดต่อ — มี verified + notes + original_*
    const std::string csv =
        "filename,pc_no,serial_no,original_pc_no,original_serial_no,verified,notes\n"
        "img.jpg,316,6YW8RV2,315,6YW8RV2,true,sticker says 316\n";
    auto path = writeTempCsv(csv);

    ReviewModel model;
    ASSERT_TRUE(model.loadCsv(path.string()));
    ASSERT_EQ(model.size(), 1u);

    auto r = *model.at(0);
    EXPECT_EQ(r.pcNo, "316");
    EXPECT_EQ(r.originalPcNo, "315");
    EXPECT_TRUE(r.verified);
    EXPECT_EQ(r.notes, "sticker says 316");
}

// =================== Unicode path (regression 2026-06-22) ===================

TEST(ReviewModel, SaveAndLoad_ThaiPath_RoundTrips) {
    // ก่อนแก้: ofstream/ifstream(narrow std::string) → MSVC ตีเป็น ANSI codepage → path/โฟลเดอร์
    // ภาษาไทยเปิด/เขียนไม่ได้ (user เจอตอน Rename/Save ในโฟลเดอร์ "ภาพ Donate/โรงเรียน...").
    // saveCsv/loadCsv ต้อง u8path เพื่อให้ path UTF-8 ถูกตีความถูก.
    const fs::path dir = fs::temp_directory_path() / fs::u8path("autopilot_ไทย_donate");
    fs::create_directories(dir);
    const fs::path p = dir / fs::u8path("ground_truth_วัดหนองคู.csv");
    const auto u8 = p.u8string();
    const std::string utf8Path(u8.begin(), u8.end());

    ReviewModel a;
    {
        const auto seed = writeTempCsv(
            "filename,pc_no,serial_no\nimg.jpg,42,6ABCDE2\n");
        ASSERT_TRUE(a.loadCsv(seed.string()));
    }
    ASSERT_TRUE(a.saveCsv(utf8Path)) << "saveCsv must write a Thai path";
    ASSERT_TRUE(fs::exists(p)) << "file must actually exist at the Thai path";

    ReviewModel b;
    ASSERT_TRUE(b.loadCsv(utf8Path)) << "loadCsv must open a Thai path";
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(b.at(0)->pcNo, "42");

    std::error_code ec;
    fs::remove_all(dir, ec);  // best-effort cleanup
}
