// Unit tests for report serialisation and parsing (no connection involved)
#include <gtest/gtest.h>

#include "CM_TEST.h"
#include "DeviceReport.h"
#include "HostReport.h"

using namespace iMS;

TEST(HostReport, SerialisesHeaderPayloadAndCrc)
{
	HostReport r(HostReport::Actions::LUT_ENTRY, HostReport::Dir::WRITE, 0x1234);
	r.Payload<Bytes>(Bytes{ 0xA1, 0xA2, 0xA3 });

	const Bytes wire = r.SerialStream();

	ASSERT_EQ(wire.size(), 7u + 3u + 2u);
	EXPECT_EQ(wire[0], static_cast<std::uint8_t>(ReportTypes::HOST_REPORT_ID_SYNTH));
	EXPECT_EQ(wire[1], 0x06);          // action, write
	EXPECT_EQ(wire[3], 3);             // length low
	EXPECT_EQ(wire[4], 0);             // length high
	EXPECT_EQ(wire[5], 0x34);          // address low
	EXPECT_EQ(wire[6], 0x12);          // address high
	EXPECT_EQ(Bytes(wire.begin() + 7, wire.begin() + 10), (Bytes{ 0xA1, 0xA2, 0xA3 }));

	// The CRC must be the same one the device side computes over the same bytes
	const Bytes rebuilt = CM_TEST::MakeFrame(ReportTypes::HOST_REPORT_ID_SYNTH, 0x06, 0, 0x1234, { 0xA1, 0xA2, 0xA3 });
	EXPECT_EQ(wire, rebuilt);
}

TEST(HostReport, ControllerActionsUseControllerReportId)
{
	HostReport r(HostReport::Actions::CTRLR_REG, HostReport::Dir::READ, 7);
	const Bytes wire = r.SerialStream();
	EXPECT_EQ(wire[0], static_cast<std::uint8_t>(ReportTypes::HOST_REPORT_ID_CTRLR));
	EXPECT_EQ(wire[1], 0x80 | 0x01);   // read, CTRLR_REG action nibble
}

TEST(DeviceReport, ParsesValidFrame)
{
	const Bytes frame = CM_TEST::MakeFrame(ReportTypes::DEVICE_REPORT_ID_SYNTH, 0x47, 0, 0x0040, { 0x34, 0x12 });

	DeviceReport d;
	for (auto b : frame) d.Parse(b);

	EXPECT_TRUE(d.Done());
	EXPECT_FALSE(d.RxCRC());
	EXPECT_FALSE(d.GeneralError());
	EXPECT_EQ(d.Fields().addr, 0x0040);
	EXPECT_EQ(d.Payload<std::vector<std::uint16_t>>().at(0), 0x1234);
}

TEST(DeviceReport, FlagsCorruptedCrc)
{
	Bytes frame = CM_TEST::MakeFrame(ReportTypes::DEVICE_REPORT_ID_SYNTH, 0x47, 0, 0x0040, { 0x34, 0x12 });
	frame.back() ^= 0xFF;

	DeviceReport d;
	for (auto b : frame) d.Parse(b);

	EXPECT_TRUE(d.Done());
	EXPECT_TRUE(d.RxCRC());
}
