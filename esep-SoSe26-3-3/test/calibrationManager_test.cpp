#include <gtest/gtest.h>

#include "calibrationManager/calibrationManager.hpp"

#include <cstdio>
#include <fstream>
#include <string>

namespace {

CalibrationData makeValidCalibrationData() {
    CalibrationData data;
    data.adc_emptyBelt = 800;
    data.adc_flatCenter = 1200;
    data.limit_minSamplesForClass = 10;
    data.ticks_EntryC_to_HeightB_Normal = 200;
    data.ticks_HeightB_to_HeightC_Normal = 150;
    data.ticks_HeightC_to_EjectorB_Normal = 300;
    data.ticks_EjectorB_to_EjectorC_Normal = 80;
    data.ticks_EjectorC_to_EndB_Normal = 250;
    data.ticks_EndB_to_EndC_Normal = 100;
    data.ticks_EntryC_to_HeightB_Slow = 400;
    data.ticks_HeightB_to_HeightC_Slow = 300;
    data.ticks_HeightC_to_EjectorB_Slow = 360;
    data.ticks_EjectorB_to_EjectorC_Slow = 120;
    data.ticks_EjectorC_to_ChuteB_Slow = 280;
    return data;
}

void expectCalibrationDataEqual(const CalibrationData& expected, const CalibrationData& actual) {
    EXPECT_EQ(expected.adc_emptyBelt, actual.adc_emptyBelt);
    EXPECT_EQ(expected.adc_flatCenter, actual.adc_flatCenter);
    EXPECT_EQ(expected.limit_minSamplesForClass, actual.limit_minSamplesForClass);
    EXPECT_EQ(expected.ticks_EntryC_to_HeightB_Normal, actual.ticks_EntryC_to_HeightB_Normal);
    EXPECT_EQ(expected.ticks_HeightB_to_HeightC_Normal, actual.ticks_HeightB_to_HeightC_Normal);
    EXPECT_EQ(expected.ticks_HeightC_to_EjectorB_Normal, actual.ticks_HeightC_to_EjectorB_Normal);
    EXPECT_EQ(expected.ticks_EjectorB_to_EjectorC_Normal, actual.ticks_EjectorB_to_EjectorC_Normal);
    EXPECT_EQ(expected.ticks_EjectorC_to_EndB_Normal, actual.ticks_EjectorC_to_EndB_Normal);
    EXPECT_EQ(expected.ticks_EndB_to_EndC_Normal, actual.ticks_EndB_to_EndC_Normal);
    EXPECT_EQ(expected.ticks_EntryC_to_HeightB_Slow, actual.ticks_EntryC_to_HeightB_Slow);
    EXPECT_EQ(expected.ticks_HeightB_to_HeightC_Slow, actual.ticks_HeightB_to_HeightC_Slow);
    EXPECT_EQ(expected.ticks_HeightC_to_EjectorB_Slow, actual.ticks_HeightC_to_EjectorB_Slow);
    EXPECT_EQ(expected.ticks_EjectorB_to_EjectorC_Slow, actual.ticks_EjectorB_to_EjectorC_Slow);
    EXPECT_EQ(expected.ticks_EjectorC_to_ChuteB_Slow, actual.ticks_EjectorC_to_ChuteB_Slow);
}

void writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream file(path.c_str());
    ASSERT_TRUE(file.is_open()) << "Konnte Testdatei nicht schreiben: " << path;
    file << content;
}

std::string makeTempPath(const char* testName) {
    return std::string("/tmp/calib_mgr_test_") + testName + ".txt";
}

} // namespace

class CalibrationManagerTest : public ::testing::Test {
protected:
    std::string m_path;

    void SetUp() override {
        const ::testing::TestInfo* info =
            ::testing::UnitTest::GetInstance()->current_test_info();
        m_path = makeTempPath(info->name());
        std::remove(m_path.c_str());
    }

    void TearDown() override {
        std::remove(m_path.c_str());
    }
};

TEST_F(CalibrationManagerTest, MissingFile_FileExistsReturnsFalse) {
    CalibrationManager manager(m_path);
    EXPECT_FALSE(manager.fileExists());
}

TEST_F(CalibrationManagerTest, MissingFile_LoadReturnsEmptyData) {
    CalibrationManager manager(m_path);
    const CalibrationData loaded = manager.loadCalibrationData();
    expectCalibrationDataEqual(CalibrationData{}, loaded);
}

TEST_F(CalibrationManagerTest, EmptyFile_FileExistsReturnsFalse) {
    writeTextFile(m_path, "");

    CalibrationManager manager(m_path);
    EXPECT_FALSE(manager.fileExists());
}

TEST_F(CalibrationManagerTest, PartialFile_FileExistsReturnsFalse) {
    writeTextFile(m_path,
        "# nur ein Feld\n"
        "adc_emptyBelt=800\n");

    CalibrationManager manager(m_path);
    EXPECT_FALSE(manager.fileExists());
}

TEST_F(CalibrationManagerTest, InvalidAdcOrder_FileExistsReturnsFalse) {
    writeTextFile(m_path,
        "adc_emptyBelt=1500\n"
        "adc_flatCenter=1200\n"
        "limit_minSamplesForClass=10\n"
        "ticks_EntryC_to_HeightB_Normal=200\n"
        "ticks_HeightB_to_HeightC_Normal=150\n"
        "ticks_HeightC_to_EjectorB_Normal=300\n"
        "ticks_EjectorB_to_EjectorC_Normal=80\n"
        "ticks_EjectorC_to_EndB_Normal=250\n"
        "ticks_EndB_to_EndC_Normal=100\n"
        "ticks_EntryC_to_HeightB_Slow=400\n"
        "ticks_HeightB_to_HeightC_Slow=300\n"
        "ticks_HeightC_to_EjectorB_Slow=360\n"
        "ticks_EjectorB_to_EjectorC_Slow=120\n"
        "ticks_EjectorC_to_ChuteB_Slow=280\n");

    CalibrationManager manager(m_path);
    EXPECT_FALSE(manager.fileExists());
}

TEST_F(CalibrationManagerTest, SlowTicksTooSmall_FileExistsReturnsFalse) {
    writeTextFile(m_path,
        "adc_emptyBelt=800\n"
        "adc_flatCenter=1200\n"
        "limit_minSamplesForClass=10\n"
        "ticks_EntryC_to_HeightB_Normal=200\n"
        "ticks_HeightB_to_HeightC_Normal=150\n"
        "ticks_HeightC_to_EjectorB_Normal=300\n"
        "ticks_EjectorB_to_EjectorC_Normal=80\n"
        "ticks_EjectorC_to_EndB_Normal=250\n"
        "ticks_EndB_to_EndC_Normal=100\n"
        "ticks_EntryC_to_HeightB_Slow=100\n"
        "ticks_HeightB_to_HeightC_Slow=300\n");

    CalibrationManager manager(m_path);
    EXPECT_FALSE(manager.fileExists());
}

TEST_F(CalibrationManagerTest, NonNumericValue_FileExistsReturnsFalse) {
    writeTextFile(m_path,
        "adc_emptyBelt=abc\n"
        "adc_flatCenter=1200\n");

    CalibrationManager manager(m_path);
    EXPECT_FALSE(manager.fileExists());
}

TEST_F(CalibrationManagerTest, ValidSavedFile_FileExistsAndLoadMatch) {
    const CalibrationData valid = makeValidCalibrationData();

    CalibrationManager writer(m_path);
    ASSERT_TRUE(writer.saveCalibrationData(valid));

    CalibrationManager reader(m_path);
    EXPECT_TRUE(reader.fileExists());

    const CalibrationData loaded = reader.loadCalibrationData();
    expectCalibrationDataEqual(valid, loaded);
}

TEST_F(CalibrationManagerTest, SaveInvalidData_ReturnsFalse) {
    CalibrationManager manager(m_path);
    EXPECT_FALSE(manager.saveCalibrationData(CalibrationData{}));
    EXPECT_FALSE(manager.fileExists());
}

TEST_F(CalibrationManagerTest, SaveThenReloadWithSameManager_Works) {
    const CalibrationData valid = makeValidCalibrationData();
    CalibrationManager manager(m_path);

    ASSERT_TRUE(manager.saveCalibrationData(valid));
    EXPECT_TRUE(manager.fileExists());

    const CalibrationData loaded = manager.loadCalibrationData();
    expectCalibrationDataEqual(valid, loaded);
}

TEST_F(CalibrationManagerTest, FileWithSectionHeaders_IsAccepted) {
    writeTextFile(m_path,
        "# Kalibrierung\n"
        "[Analog-Thresholds]\n"
        "adc_emptyBelt=800\n"
        "adc_flatCenter=1200\n"
        "limit_minSamplesForClass=10\n"
        "[Normal-Speed-Ticks]\n"
        "ticks_EntryC_to_HeightB_Normal=200\n"
        "ticks_HeightB_to_HeightC_Normal=150\n"
        "ticks_HeightC_to_EjectorB_Normal=300\n"
        "ticks_EjectorB_to_EjectorC_Normal=80\n"
        "ticks_EjectorC_to_EndB_Normal=250\n"
        "ticks_EndB_to_EndC_Normal=100\n"
        "[Slow-Speed-Base-Ticks]\n"
        "ticks_EntryC_to_HeightB_Slow=400\n"
        "ticks_HeightB_to_HeightC_Slow=300\n"
        "ticks_HeightC_to_EjectorB_Slow=360\n"
        "ticks_EjectorB_to_EjectorC_Slow=120\n"
        "ticks_EjectorC_to_ChuteB_Slow=280\n");

    CalibrationManager manager(m_path);
    EXPECT_TRUE(manager.fileExists());

    const CalibrationData loaded = manager.loadCalibrationData();
    expectCalibrationDataEqual(makeValidCalibrationData(), loaded);
}
